#include "App.h"
#include "MainWindow.h"
#include "SettingsDialog.h"
#include "../core/Config.h"
#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>
#include <QDir>
#include <QFile>
#include <QTimer>
#ifdef Q_OS_WIN
#include <winsock2.h>
#endif

namespace screencap::ui {

// 端口占用预检：原生 socket 无 SO_REUSEADDR 的 bind 探测
// （Windows 的 SO_REUSEADDR 语义会使 QTcpServer::listen 在端口被占时仍返回成功，
//   必须用原生 bind 才能可靠检测真实占用）
static bool portInUse(quint16 port)
{
#ifdef Q_OS_WIN
    WSADATA wsa;
    const bool wsaInit = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        if (wsaInit) WSACleanup();
        return true;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    const int r = bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    closesocket(s);
    if (wsaInit) WSACleanup();
    return r == SOCKET_ERROR;
#else
    Q_UNUSED(port);
    return false;
#endif
}

App::App(QObject* parent)
    : QObject(parent)
    , config_(core::Config::instance())
    , certMgr_(QDir(core::Config::configDir()).filePath(QStringLiteral("certs")))
    , server_(config_, pairing_)
{
}

App::~App()
{
    if (tray_)
        tray_->hide();
}

bool App::init()
{
    config_.load();
    ensureDefaultPreset();

    // 证书（ECC 自签，首启生成）
    if (!certMgr_.ensureCertificate()) {
        QMessageBox::critical(nullptr, QStringLiteral("ScreenLink"),
                              QStringLiteral("TLS 证书生成失败（需要 openssl）。"));
        return false;
    }

    // TLS 服务：端口被占用时自动顺延（最多试 20 个），避免启动失败
    // 预检用原生 bind（无 SO_REUSEADDR），规避 Windows 下 listen 假成功问题
    const quint16 basePort = config_.listenPort;
    quint16 port = basePort;
    bool started = false;
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (!portInUse(port) && server_.start(port, certMgr_.certFilePath(), certMgr_.keyFilePath())) {
            started = true;
            break;
        }
        ++port;
    }
    if (!started) {
        QMessageBox::critical(nullptr, QStringLiteral("ScreenLink"),
                              QStringLiteral("端口 %1-%2 均不可用，服务启动失败。")
                                  .arg(basePort).arg(basePort + 19));
        return false;
    }
    const bool portChanged = (port != basePort);
    if (portChanged) {
        config_.listenPort = port;
        config_.save();
    }

    setupTray();

    // 主窗口（控制面板：IP/端口/配对码），关闭 = 最小化到托盘
    mainWindow_ = new MainWindow(config_, pairing_, certMgr_, nullptr);
    connect(mainWindow_, &MainWindow::settingsRequested, this, &App::showSettingsDialog);
    connect(mainWindow_, &MainWindow::quitRequested, this, &App::quitApp);
    mainWindow_->show();

    // 端口变化时提示实际端口（手机端配对需使用）
    if (portChanged && tray_) {
        tray_->showMessage(QStringLiteral("ScreenLink"),
                           QStringLiteral("原端口 %1 被占用，已改用端口 %2。\n手机端配对时请输入新端口。")
                               .arg(basePort).arg(port),
                           QSystemTrayIcon::Information, 8000);
    }
    return true;
}

void App::setupTray()
{
    tray_ = new QSystemTrayIcon(this);
    // 自定义应用图标（与窗口/任务栏一致，便于识别）
    QIcon icon(QStringLiteral(":/app_icon.ico"));
    if (icon.isNull())
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    tray_->setIcon(icon);
    tray_->setToolTip(QStringLiteral("ScreenLink 屏联 - 手机遥控截图\n端口 %1")
                          .arg(config_.listenPort));

    auto* menu = new QMenu;
    menu->addAction(QStringLiteral("显示主窗口"), this,
                    [this]() { mainWindow_->showNormal(); mainWindow_->raise(); });
    actSettings_ = menu->addAction(QStringLiteral("设置…"), this, &App::showSettingsDialog);
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出"), this, &App::quitApp);
    tray_->setContextMenu(menu);

    // 单击/双击托盘图标均恢复主窗口（Windows 用户习惯单击）
    connect(tray_, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
                    mainWindow_->showNormal();
                    mainWindow_->raise();
                }
            });
    tray_->show();
}

void App::ensureDefaultPreset()
{
    // 默认预设：全屏（w/h = -1 表示全屏该显示器），由手机端 "fullscreen" 指令覆盖；
    // 另提供 "主屏" 命名预设便于手机端下拉选择
    if (config_.presets.isEmpty()) {
        core::Preset p;
        p.name = QStringLiteral("主屏");
        p.monitor = 0;
        p.x = p.y = 0;
        p.w = p.h = -1;
        config_.presets.append(p);
        config_.save();
    }
}

void App::showSettingsDialog()
{
    SettingsDialog dlg(config_);
    dlg.exec();
}

void App::quitApp()
{
    QApplication::quit();
}

} // namespace screencap::ui
