#pragma once
// ScreenLink 主窗口（控制面板）：IP / 端口 / 配对码 / 最小化到托盘 / 退出
#include <QWidget>
#include <QDateTime>

class QLabel;
class QTimer;

namespace screencap::core { class Config; }
namespace screencap::pairing { class PairingManager; }
namespace screencap::crypto { class CertificateManager; }

namespace screencap::ui {

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(core::Config& config,
                        pairing::PairingManager& pairing,
                        crypto::CertificateManager& certMgr,
                        QWidget* parent = nullptr);

signals:
    void settingsRequested();
    void quitRequested();

protected:
    void closeEvent(QCloseEvent* event) override; // 关窗口 = 最小化到托盘

private slots:
    void refreshCode();
    void updateCountdown();

private:
    void refreshIpList();
    void buildUi();

    core::Config& config_;
    pairing::PairingManager& pairing_;
    crypto::CertificateManager& certMgr_;

    QLabel* ipLabel_ = nullptr;
    QLabel* portLabel_ = nullptr;
    QLabel* codeLabel_ = nullptr;
    QLabel* countdownLabel_ = nullptr;
    QTimer* timer_ = nullptr;
    QDateTime expiresAt_;
};

} // namespace screencap::ui
