#pragma once
// ScreenLink TLS 服务端（PROTOCOL.md §4-§7）
// 协议状态机：pair / hello / capture / ping；隐私硬约束：仅 capture 指令时截图
#include <QTcpServer>
#include <QSslConfiguration>
#include <QHash>
#include <QByteArray>

namespace screencap::protocol { struct DecodedFrame; }

namespace screencap::core { class Config; }
namespace screencap::pairing { class PairingManager; }

class QSslSocket;
class QJsonObject;

class TlsServer : public QTcpServer {
    Q_OBJECT
public:
    explicit TlsServer(screencap::core::Config& config,
                       screencap::pairing::PairingManager& pairing,
                       QObject* parent = nullptr);

    // 启动监听；certPath/keyPath 为 ECC 自签证书（PEM）
    bool start(quint16 port, const QString& certPath, const QString& keyPath);

signals:
    void logMessage(const QString& msg);

private slots:
    void onEncrypted();
    void onReadyRead();
    void onDisconnected();

protected:
    // 重写 accept 入口：直接接收描述符构造 QSslSocket（Qt 官方 SSL 服务器模式）
    void incomingConnection(qintptr socketDescriptor) override;

private:
    void handleFrame(QSslSocket* sock, const screencap::protocol::DecodedFrame& frame);
    void processJson(QSslSocket* sock, const QJsonObject& obj);
    void sendJson(QSslSocket* sock, const QJsonObject& obj);
    void sendError(QSslSocket* sock, int code, const QString& msg);

    // 各指令处理（PROTOCOL.md §4/§5/§6）
    void handlePair(QSslSocket* sock, const QJsonObject& obj);
    void handleHello(QSslSocket* sock, const QJsonObject& obj);
    void handleCapture(QSslSocket* sock, const QJsonObject& obj);
    void handlePing(QSslSocket* sock);

    void dropClient(QSslSocket* sock);  // 非法行为断开

    screencap::core::Config& config_;
    screencap::pairing::PairingManager& pairing_;
    QSslConfiguration sslConf_;
    QHash<QSslSocket*, QByteArray> buffers_;
    QHash<QSslSocket*, bool> authed_;      // 连接是否已认证
    QHash<QSslSocket*, quint32> lastSeq_;  // 防重放：已处理的最大 seq
};
