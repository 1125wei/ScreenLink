# ScreenLink 屏联

> 📱 手机遥控电脑截图 · 局域网加密传输 · AI 问答

ScreenLink 是一个**局域网手机遥控截图工具**：手机端一键触发 Windows 电脑截图，图片经 **TLS 1.3 加密通道**实时回传手机查看、保存、分享；还能**提取截图文字并询问 AI**（DeepSeek / 小米 MiMo），多轮对话直接回答截图中的问题。

电脑端托盘常驻、隐蔽运行，**手机端未触发时电脑端零屏幕采集**（隐私硬约束）。

## ✨ 功能特性

- 📸 **手机遥控截图**：预设区域（全屏/自定义），一键触发，电脑端本地留档
- 🔐 **安全连接**：TLS 1.3 加密 + ECC 证书指纹固定（TOFU），PIN 配对码（10 分钟有效），设备免密重连
- 🖥️ **电脑端控制面板**：本机 IP / 端口 / 配对码一目了然，最小化到托盘，单实例保护，端口占用自动顺延
- 🤖 **AI 问答**（v0.2.0 新增）：截图 → 提取文字并询问 AI，多轮对话可连续追问
  - **MiMo 模式**：截图直传小米 MiMo-V2.5（原生多模态，¥1/¥2 每百万 token）
  - **DeepSeek 模式**：本地 OCR（ML Kit 中文，离线）+ DeepSeek 文本模型
  - API Key 仅存本机（Keystore 加密），直连厂商官方接口
- 📁 **本地管理**：截图保存相册 / 系统分享，多设备管理

## 🚀 快速开始

从 **GitHub Releases** 下载最新版本：

| 平台 | 安装包 | 说明 |
|---|---|---|
| Windows | `ScreenLink-vX.Y.Z-win64.zip` | 解压即用，运行 `ScreenLink.exe` |
| Android | `ScreenLink-vX.Y.Z.apk` | Android 8.0+（minSdk 26） |

### 使用步骤

1. **电脑端**：解压 zip → 运行 `ScreenLink.exe` → 首次运行请右键 `add_firewall.bat` **以管理员身份运行**（放行 8848 端口）→ 主窗口显示本机 IP 和配对码
2. **手机端**：安装 APK → 添加设备：输入电脑 IP + 配对码（与手机同一 Wi-Fi）→ 配对成功
3. **截图**：选择区域预设 → 一键截图 → 保存/分享
4. **AI 问答**：截图后点「🤖 提取图片文字，并询问 AI」→ 对话栏直接回答截图中的问题；在「AI 设置」Tab 配置 API Key 与模式

## 🤖 AI 配置说明

| 项目 | 说明 |
|---|---|
| DeepSeek | `platform.deepseek.com` 注册获取 Key；流程：本地 OCR → 文本 → `deepseek-chat` |
| 小米 MiMo | `mimo.mi.com`（小米账号）控制台获取 Key；流程：图片直传 → `mimo-v2.5`（推荐，支持视觉） |
| 多轮对话 | 会话历史保留，可对截图内容连续追问 |
| 隐私 | Key 仅存手机本机（系统 Keystore 加密）；截图内容会发送给所选 AI 服务商，敏感内容慎用 |

## 🏗 技术架构

```
┌─────────────┐  TCP 8848 + TLS 1.3   ┌──────────────┐
│  安卓端      │ ◄────────────────────► │  Windows 端   │
│ Kotlin/Compose│  帧协议 [4B长度][1B类型][payload] │ C++20 / Qt 6.8 │
└─────────────┘  JSON 控制 + JPEG 回传 └──────────────┘
```

- **通信协议**：`docs/PROTOCOL.md` —— 语言无关的规范文档，未来可扩展 iOS 等新端
- **安全模型**：TLS 1.3（OpenSSL 后端）+ ECC P-256 自签证书 + 证书指纹固定 + 一次性配对码
- **隐私架构**：电脑端零后台采集，仅在收到截图指令时采集

## 🔧 本地构建

### 电脑端（Windows / C++20 / Qt 6.8）

```bash
# 依赖：Qt 6.8 (msvc2022_64)、Visual Studio 2022 Build Tools (C++ 工作负载)、CMake + Ninja
cd pc
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/msvc2022_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
# 部署运行库（含 OpenSSL TLS 后端）：
# 复制 exe 到发布目录后执行 windeployqt --release，并补上 plugins/tls/qopensslbackend.dll
```

### 安卓端（Kotlin / AGP 8.5）

```bash
# 依赖：JDK 17、Android SDK (platform 34 / build-tools 34)、Gradle 8.9
cd android
# 配置 local.properties: sdk.dir=<SDK路径>
gradle assembleDebug   # 产物: app/build/outputs/apk/debug/app-debug.apk
```

## 📁 目录结构

```
ScreenLink/
├── pc/          # Windows 电脑端（C++20 / Qt 6.8 Widgets）
│   ├── src/     #   protocol(帧协议) / capture(GDI截图) / crypto(证书) / server(TLS服务) / ui(界面)
│   └── tests/   #   单元测试 + 端到端协议测试
├── android/     # 安卓端（Kotlin / Jetpack Compose）
│   └── app/src/main/java/com/screencap/screenlink/
│       ├── protocol/  # TLS 客户端 + 帧编解码
│       ├── data/      # 设备/配置存储（Keystore 加密）
│       ├── ai/        # 模型抽象层（DeepSeek/MiMo）+ 本地 OCR
│       └── ui/        # 控制台 / AI 设置 / 关于
├── docs/        # REQUIREMENTS / PROTOCOL / ARCHITECTURE
└── dist/        # 发布产物（zip + apk，随 GitHub Release 发布）
```

## 📄 文档

- [需求文档](docs/REQUIREMENTS.md)
- [通信协议 v1.0](docs/PROTOCOL.md)
- [架构设计](docs/ARCHITECTURE.md)

## ⚖️ 许可证

[MIT License](LICENSE) © 2026 ScreenLink Contributors
