#pragma once
// ScreenLink 应用主类：配置加载 -> 证书 -> TLS 服务 -> 主窗口 + 托盘
#include <QObject>
#include "../core/Config.h"
#include "../crypto/CertificateManager.h"
#include "../pairing/PairingManager.h"
#include "../server/TlsServer.h"

class QSystemTrayIcon;
class QAction;

namespace screencap::ui {

class MainWindow;

class App : public QObject {
    Q_OBJECT
public:
    explicit App(QObject* parent = nullptr);
    ~App() override;

    // 初始化全链路；失败返回 false（弹窗说明）
    bool init();

private slots:
    void showSettingsDialog();
    void quitApp();

private:
    void setupTray();
    void ensureDefaultPreset();

    core::Config& config_;
    crypto::CertificateManager certMgr_;
    pairing::PairingManager pairing_;
    TlsServer server_;
    MainWindow* mainWindow_ = nullptr;
    QSystemTrayIcon* tray_ = nullptr;
    QAction* actSettings_ = nullptr;
};

} // namespace screencap::ui
