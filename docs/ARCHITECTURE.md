# ScreenLink 架构设计

## 1. 总体架构

```
┌─────────────────────┐         LAN          ┌──────────────────────┐
│   安卓端 (Kotlin)    │  TCP + TLS 1.3       │  电脑端 (C++/Qt6)     │
│                     │◄────────────────────►│                      │
│ ┌─────────────────┐ │   帧协议 v1.0        │ ┌──────────────────┐ │
│ │ UI (Compose)    │ │                      │ │ Qt 托盘 + 设置窗  │ │
│ │  配对/设备/截图  │ │                      │ │ (ui/)            │ │
│ │  历史/预览/保存  │ │                      │ └────────┬─────────┘ │
│ └────────┬────────┘ │                      │          │ 信号槽     │
│ ┌────────▼────────┐ │                      │ ┌────────▼─────────┐ │
│ │ 协议客户端       │ │                      │ │ 协议服务端        │ │
│ │ ProtocolClient  │ │                      │ │ ProtocolServer  │ │
│ │ (帧编解码/状态机)│ │                      │ │ (帧编解码/状态机) │ │
│ └────────┬────────┘ │                      │ └───┬─────────┬───┘ │
│ ┌────────▼────────┐ │                      │ ┌───▼───┐ ┌──▼────┐ │
│ │ TLS (SSLSocket) │ │                      │ │ TLS   │ │配对/QR │ │
│ │ 证书指纹固定     │ │                      │ │QSslSoc│ │pairing│ │
│ └────────┬────────┘ │                      │ └───┬───┘ └───┬────┘ │
│ ┌────────▼────────┐ │                      │ ┌───▼─────────▼────┐ │
│ │ 本地存储         │ │                      │ │ 截图引擎 capture/ │ │
│ │ 历史/设备/设置   │ │                      │ │ 配置 config       │ │
│ └─────────────────┘ │                      │ └──────────────────┘ │
└─────────────────────┘                      └──────────────────────┘
```

## 2. 电脑端模块设计（C++20 / Qt 6.8 Widgets）

### 2.1 线程模型

```
主线程 (GUI)         工作线程 (Worker)          截图线程
─────────────       ──────────────────        ─────────────
托盘/设置窗口    ◄── QTcpServer 监听 ──► 收到 capture
协议信号处理 ◄──── 每连接 QSslSocket 信号
                  帧解析/组帧
                  JPEG 编码 (QImage→QByteArray)
```

- 网络与截图在**工作线程**（`QThread` + 事件循环），GUI 线程零阻塞
- 截图操作本身（GDI BitBlt）耗时 <50ms，在 Worker 内直接执行；未来 DXGI 可加专用线程
- 通过 Qt 信号槽（QueuedConnection）跨线程通信

### 2.2 关键类

| 类 | 职责 |
|---|---|
| `App` | 单例，QSystemTrayIcon 生命周期，配置加载/保存 |
| `Config` | config.json 读写（QJsonDocument），原子写入（临时文件+rename） |
| `TlsServer` | QTcpServer 子类，接管 newConnection 建立 QSslSocket |
| `CertificateManager` | 首启生成 ECC P-256 自签证书；读取/续期；输出 SHA-256 指纹 |
| `FrameCodec` | 帧头编解码 + JSON 序列化（纯函数，可单测） |
| `PairingManager` | 配对码生成/校验/过期/拉黑；二维码生成（qrencode → QImage） |
| `CaptureEngine` | GDI 截图：全屏/多屏/区域；返回 QImage → JPEG 编码 |
| `TrustStore` | 信任设备列表持久化（config.json 内） |
| `TrayApp` | QSystemTrayIcon 菜单：打开设置/添加设备/退出 |

### 2.3 截图引擎（capture/）

- MVP 用 GDI `BitBlt`（兼容性最好，Win10/11 全支持）
- 多屏：枚举 `EnumDisplayMonitors` 建虚拟桌面坐标系；`monitor` 编号 0..n-1
- 区域：预设 `(monitor, x, y, w, h)`；`w/h = -1` 表示全屏
- V2 预留：`CaptureBackend` 抽象接口（DXGI Desktop Duplication 实现高性能/低延迟流）

### 2.4 配置持久化

- 路径：`%APPDATA%/ScreenLink/config.json`（非程序目录，避免权限问题）
- 证书：`%APPDATA%/ScreenLink/certs/server.key / server.crt`

## 3. 安卓端模块设计（Kotlin / Compose）

### 3.1 架构

- 单 Activity + Compose Navigation（3 屏：配对引导 / 设备列表 / 控制台）
- 网络层：`ProtocolClient` 封装 `SSLSocket`（Java 原生），协程 `withContext(Dispatchers.IO)`
- 状态管理：`ViewModel` + `StateFlow`；连接状态用 sealed class

### 3.2 关键类

| 类 | 职责 |
|---|---|
| `PairingScreen` | PIN 输入（V1.1 加 ZXing 扫码） |
| `DeviceListViewModel` | 设备列表（本地存储的配对记录） |
| `ConsoleViewModel` | 连接状态机 + 截图触发 + 结果接收 |
| `ProtocolClient` | TLS 握手（校验指纹）→ 帧收发（DataInputStream/OutputStream） |
| `FingerprintValidator` | 从配对信息提取指纹，`X509TrustManager` 校验 |
| `ImageStore` | 历史记录：app 私有目录 + SQLite 索引（Room）；导出 MediaStore + 分享 Intent |

### 3.3 线程模型

- 网络 I/O 全在 `Dispatchers.IO`；帧解析后通过 `StateFlow` 推送到 UI
- 心跳：协程循环，30s 间隔
- 重连：指数退避协程

## 4. 数据流（截图全链路）

```
手机点击「截图」
  → ProtocolClient 发 capture 帧（TLS 加密）
  → 电脑 TlsServer 收到 → FrameCodec 解析
  → CaptureEngine 按预设截图 → JPEG 编码
  → 回发 capture_resp JSON 帧 + 0x02 JPEG 帧
  → 手机收帧 → 校验 seq → 显示大图 + 存历史 + 可保存相册/分享
  → 电脑按 save_dir 本地保存（save_enabled 时）
```

## 5. 关键设计决策（ADR）

### ADR-1：为什么用 TLS 而不是自研 AES-GCM
- QSslSocket / Java SSLSocket 均为平台内置，实现成本低且经过审计
- 证书指纹固定同时解决身份认证与防中间人，无需自研密钥交换
- 安全专业角度：行业标准方案，审查价值高

### ADR-2：为什么 MVP 用 PIN 配对而不是直接扫码
- 配对逻辑与输入通道解耦（配对码同一抽象），扫码只是"读码"增强
- 扫码依赖（ZXing/qrencode）后加，不阻塞核心链路

### ADR-3：为什么 GDI 而不是 DXGI（MVP）
- GDI BitBlt 对静态/低频截图完全够用（<50ms/张），零依赖
- DXGI Desktop Duplication 为 V2 实时预览保留（需要专用线程+丢帧策略）

### ADR-4：为什么电脑端默认不保存截图
- 隐私硬约束（REQ SE-04）：保存是用户显式配置的行为，默认仅传输

## 6. 测试策略

| 层 | 手段 |
|---|---|
| FrameCodec | 纯 C++ 单元测试（Qt Test），帧边界/超限/非法 JSON |
| 配对逻辑 | 配对码生成/过期/拉黑（Qt Test） |
| 端到端 | 电脑端 + 安卓模拟器（或真机）手工用例脚本（docs/E2E.md） |
| 安全验证 | 未配对连接被拒 / 错误指纹被拒 / 抓包确认无明文（Wireshark） |

## 7. 环境与工具链（本机现状 2026-08）

| 组件 | 状态 |
|---|---|
| VS Build Tools 2022 (C++ 工作负载) | 待安装（CLion 的 MSVC 工具链将指向这里） |
| Qt 6.8 LTS (win64_msvc2022_64) | 待安装 → D:\Qt（中科大镜像 + aqtinstall） |
| CMake / Ninja | CLion 捆绑可用 |
| JDK 17 (Temurin) | 待安装 → D:\dev\jdk17（清华 Adoptium 镜像） |
| Android SDK (cmdline-tools + platform 34) | 待安装 → D:\Android\Sdk（腾讯云镜像） |
| Gradle | 随工程 wrapper 下载（阿里云镜像 maven） |
