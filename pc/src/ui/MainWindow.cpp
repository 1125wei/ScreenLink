#include "MainWindow.h"
#include "../core/Config.h"
#include "../pairing/PairingManager.h"
#include "../crypto/CertificateManager.h"
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFont>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QDateTime>

namespace screencap::ui {

MainWindow::MainWindow(core::Config& config,
                       pairing::PairingManager& pairing,
                       crypto::CertificateManager& certMgr,
                       QWidget* parent)
    : QWidget(parent)
    , config_(config)
    , pairing_(pairing)
    , certMgr_(certMgr)
{
    setWindowTitle(QStringLiteral("ScreenLink 屏联"));
    setMinimumWidth(420);
    buildUi();
    refreshIpList();

    timer_ = new QTimer(this);
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &MainWindow::updateCountdown);
    timer_->start();
    refreshCode();
}

void MainWindow::buildUi()
{
    auto* layout = new QVBoxLayout(this);

    // ---- 连接信息 ----
    auto* infoBox = new QGroupBox(QStringLiteral("连接信息"), this);
    auto* form = new QFormLayout(infoBox);
    ipLabel_ = new QLabel(infoBox);
    ipLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ipLabel_->setWordWrap(true);
    portLabel_ = new QLabel(infoBox);
    portLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(QStringLiteral("本机 IP"), ipLabel_);
    form->addRow(QStringLiteral("监听端口"), portLabel_);
    layout->addWidget(infoBox);

    // ---- 配对码 ----
    auto* pairBox = new QGroupBox(QStringLiteral("手机配对"), this);
    auto* pairLayout = new QVBoxLayout(pairBox);
    auto* hint = new QLabel(
        QStringLiteral("在手机端输入以下配对码完成连接（10 分钟内有效）："), pairBox);
    hint->setWordWrap(true);
    pairLayout->addWidget(hint);

    codeLabel_ = new QLabel(pairBox);
    QFont f = codeLabel_->font();
    f.setPointSize(24);
    f.setBold(true);
    codeLabel_->setFont(f);
    codeLabel_->setAlignment(Qt::AlignCenter);
    codeLabel_->setStyleSheet(QStringLiteral("color: #1565C0;"));
    pairLayout->addWidget(codeLabel_);

    auto* countdownRow = new QHBoxLayout;
    countdownLabel_ = new QLabel(pairBox);
    auto* refreshBtn = new QPushButton(QStringLiteral("刷新配对码"), pairBox);
    countdownRow->addWidget(countdownLabel_, 1);
    countdownRow->addWidget(refreshBtn);
    pairLayout->addLayout(countdownRow);

    auto* fpLabel = new QLabel(
        QStringLiteral("证书指纹（SHA-256）：%1").arg(certMgr_.fingerprintHex()), pairBox);
    fpLabel->setWordWrap(true);
    fpLabel->setStyleSheet(QStringLiteral("color: gray;"));
    pairLayout->addWidget(fpLabel);
    layout->addWidget(pairBox);

    connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshCode);

    // ---- 底部按钮 ----
    auto* btnRow = new QHBoxLayout;
    auto* settingsBtn = new QPushButton(QStringLiteral("设置…"), this);
    auto* trayBtn = new QPushButton(QStringLiteral("最小化到托盘"), this);
    auto* quitBtn = new QPushButton(QStringLiteral("退出"), this);
    btnRow->addWidget(settingsBtn);
    btnRow->addStretch();
    btnRow->addWidget(trayBtn);
    btnRow->addWidget(quitBtn);
    layout->addLayout(btnRow);

    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::settingsRequested);
    connect(trayBtn, &QPushButton::clicked, this, &MainWindow::hide);
    connect(quitBtn, &QPushButton::clicked, this, &MainWindow::quitRequested);
}

void MainWindow::refreshIpList()
{
    // 枚举本机局域网 IPv4（排除回环 + 虚拟网卡：VMware/VirtualBox/Hyper-V/WSL 等）
    QStringList ips;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp))
            continue;
        if (iface.flags() & QNetworkInterface::IsLoopBack)
            continue;
        const QString name = iface.humanReadableName().toLower();
        if (name.contains(QStringLiteral("vmware")) || name.contains(QStringLiteral("virtualbox"))
            || name.contains(QStringLiteral("hyper-v")) || name.contains(QStringLiteral("wsl"))
            || name.contains(QStringLiteral("virtual")) || name.contains(QStringLiteral("loopback")))
            continue;
        for (const auto& e : iface.addressEntries()) {
            const auto ip = e.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback())
                ips << ip.toString();
        }
    }
    if (ips.isEmpty())
        ipLabel_->setText(QStringLiteral("（未检测到局域网 IP，请检查网络）"));
    else
        ipLabel_->setText(ips.join(QStringLiteral("，")));

    portLabel_->setText(QStringLiteral("%1").arg(config_.listenPort));
}

void MainWindow::refreshCode()
{
    codeLabel_->setText(pairing_.generateCode());
    expiresAt_ = pairing_.expiresAt();
    updateCountdown();
}

void MainWindow::updateCountdown()
{
    const int secs = static_cast<int>(QDateTime::currentDateTime().secsTo(expiresAt_));
    if (secs <= 0) {
        countdownLabel_->setText(QStringLiteral("已过期，点击「刷新配对码」重新生成"));
        return;
    }
    countdownLabel_->setText(QStringLiteral("剩余有效时间：%1 秒").arg(secs));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // 关窗口（X）默认最小化到托盘；托盘不可用时最小化到任务栏（避免"幽灵进程"）
    event->ignore();
    if (QSystemTrayIcon::isSystemTrayAvailable())
        hide();
    else
        showMinimized();
}

} // namespace screencap::ui
