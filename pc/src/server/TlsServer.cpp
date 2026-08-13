#include "TlsServer.h"
#include "../protocol/FrameCodec.h"
#include "../capture/CaptureEngine.h"
#include "../core/Config.h"
#include "../pairing/PairingManager.h"
#include <QSslSocket>
#include <QSslKey>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QRandomGenerator>
#include <QTimer>
#include <QDebug>

using namespace screencap::protocol;
using screencap::core::Config;
using screencap::core::Preset;
using screencap::core::TrustedDevice;
using screencap::pairing::PairingManager;
using screencap::capture::CaptureEngine;

TlsServer::TlsServer(Config& config, PairingManager& pairing, QObject* parent)
    : QTcpServer(parent), config_(config), pairing_(pairing)
{
}

bool TlsServer::start(quint16 port, const QString& certPath, const QString& keyPath)
{
    QFile c(certPath), k(keyPath);
    if (!c.open(QIODevice::ReadOnly) || !k.open(QIODevice::ReadOnly))
        return false;
    const QSslCertificate cert(c.readAll(), QSsl::Pem);
    const QSslKey key(k.readAll(), QSsl::Ec, QSsl::Pem);
    c.close();
    k.close();
    if (cert.isNull() || key.isNull())
        return false;

    QSslConfiguration conf = QSslConfiguration::defaultConfiguration();
    conf.setProtocol(QSsl::TlsV1_2OrLater);
    conf.setLocalCertificate(cert);
    conf.setPrivateKey(key);
    sslConf_ = conf;

    if (!listen(QHostAddress::AnyIPv4, port)) {
        emit logMessage(QStringLiteral("监听失败: %1").arg(errorString()));
        return false;
    }
    emit logMessage(QStringLiteral("服务已启动，端口 %1").arg(port));
    return true;
}

void TlsServer::incomingConnection(qintptr socketDescriptor)
{
    qDebug() << "[TlsServer] new connection, fd" << socketDescriptor;
    auto* ssl = new QSslSocket(this);
    if (!ssl->setSocketDescriptor(socketDescriptor, QAbstractSocket::ConnectedState)) {
        qDebug() << "[TlsServer] setSocketDescriptor 失败:" << ssl->errorString();
        delete ssl;
        return;
    }
    ssl->setSslConfiguration(sslConf_);
    buffers_.insert(ssl, QByteArray());
    authed_.insert(ssl, false);
    connect(ssl, &QSslSocket::encrypted, this, &TlsServer::onEncrypted);
    connect(ssl, &QSslSocket::readyRead, this, &TlsServer::onReadyRead);
    connect(ssl, &QSslSocket::disconnected, this, &TlsServer::onDisconnected);
    connect(ssl, &QSslSocket::sslErrors, this,
            [](const QList<QSslError>& errs) {
                for (const auto& e : errs)
                    qDebug() << "[TlsServer] sslError:" << e.errorString();
            });
    connect(ssl, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, [ssl](QAbstractSocket::SocketError) {
                qDebug() << "[TlsServer] socketError:" << ssl->errorString();
            });
    ssl->startServerEncryption();
}

void TlsServer::onEncrypted()
{
    auto* ssl = qobject_cast<QSslSocket*>(sender());
    if (!ssl)
        return;
    emit logMessage(QStringLiteral("TLS 已加密: %1 (协议 %2)")
                        .arg(ssl->peerAddress().toString())
                        .arg(ssl->sessionProtocol() == QSsl::TlsV1_3
                                 ? QStringLiteral("TLS1.3")
                                 : QStringLiteral("TLS1.2")));
}

void TlsServer::onReadyRead()
{
    auto* ssl = qobject_cast<QSslSocket*>(sender());
    if (!ssl)
        return;
    buffers_[ssl].append(ssl->readAll());
    qDebug() << "[TlsServer] readyRead, buffer size =" << buffers_[ssl].size();

    DecodedFrame frame;
    while (decodeFrame(buffers_[ssl], frame) == DecodeResult::Complete) {
        qDebug() << "[TlsServer] frame type" << static_cast<int>(frame.type);
        if (frame.type == FrameType::Json)
            processJson(ssl, QJsonDocument::fromJson(frame.payload).object());
        // Binary 帧是电脑→手机方向（截图回传），手机不应发送；忽略即可
    }
}

void TlsServer::processJson(QSslSocket* sock, const QJsonObject& obj)
{
    if (obj.isEmpty()) {
        sendError(sock, 1007, QStringLiteral("非法 JSON"));
        return;
    }
    const QString type = obj.value(QStringLiteral("type")).toString();

    // 认证前只允许 pair/hello
    if (!authed_.value(sock, false)) {
        if (type == QLatin1String("pair"))
            return handlePair(sock, obj);
        if (type == QLatin1String("hello"))
            return handleHello(sock, obj);
        sendError(sock, 1001, QStringLiteral("未认证"));
        dropClient(sock);
        return;
    }

    if (type == QLatin1String("capture"))
        handleCapture(sock, obj);
    else if (type == QLatin1String("ping"))
        handlePing(sock);
    else if (type == QLatin1String("hello"))
        handleHello(sock, obj);
    else
        sendError(sock, 1007, QStringLiteral("未知消息类型"));
}

void TlsServer::handlePair(QSslSocket* sock, const QJsonObject& obj)
{
    const QString code = obj.value(QStringLiteral("code")).toString();
    const QString deviceId = obj.value(QStringLiteral("device_id")).toString();
    const QString deviceName = obj.value(QStringLiteral("device_name")).toString();

    if (deviceId.isEmpty() || deviceName.isEmpty()) {
        sendError(sock, 1007, QStringLiteral("参数缺失"));
        return;
    }
    // 已配对设备不应再次 pair
    for (const auto& t : config_.trustedDevices) {
        if (t.deviceId == deviceId) {
            sendError(sock, 1002, QStringLiteral("设备已配对，请直接连接"));
            return;
        }
    }
    if (!pairing_.verify(code)) {
        sendError(sock, 1002, QStringLiteral("配对码错误或已过期"));
        return;
    }

    TrustedDevice t;
    t.deviceId = deviceId;
    t.name = deviceName;
    t.pairedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    config_.trustedDevices.append(t);
    config_.save();
    pairing_.onPaired();
    authed_[sock] = true;

    QJsonObject resp;
    resp[QStringLiteral("type")] = QStringLiteral("pair_resp");
    resp[QStringLiteral("ok")] = true;
    resp[QStringLiteral("server_id")] = QStringLiteral("screencap-1");
    resp[QStringLiteral("server_name")] = config_.serverName;
    resp[QStringLiteral("allowed")] = true;
    sendJson(sock, resp);
    emit logMessage(QStringLiteral("设备配对成功: %1 (%2)").arg(deviceName, deviceId));
}

void TlsServer::handleHello(QSslSocket* sock, const QJsonObject& obj)
{
    const QString deviceId = obj.value(QStringLiteral("device_id")).toString();

    // 已认证（pair 后重发 hello）→ 直接 hello_ack
    // 未认证 → 查信任列表
    bool found = false;
    if (!authed_.value(sock, false)) {
        for (const auto& t : config_.trustedDevices) {
            if (t.deviceId == deviceId) {
                found = true;
                break;
            }
        }
        if (!found) {
            sendError(sock, 1001, QStringLiteral("未配对"));
            dropClient(sock);
            return;
        }
        authed_[sock] = true;
    }

    QJsonObject resp;
    resp[QStringLiteral("type")] = QStringLiteral("hello_ack");
    resp[QStringLiteral("session_id")] = QStringLiteral("%1-%2")
        .arg(QDateTime::currentMSecsSinceEpoch())
        .arg(QRandomGenerator::global()->generate());
    resp[QStringLiteral("server_name")] = config_.serverName;
    resp[QStringLiteral("quality_max")] = config_.qualityMax;
    resp[QStringLiteral("proto")] = 1;
    QJsonArray pa;
    for (const auto& p : config_.presets)
        pa.append(p.name);
    pa.append(QStringLiteral("fullscreen"));
    resp[QStringLiteral("presets")] = pa;
    sendJson(sock, resp);
}

void TlsServer::handleCapture(QSslSocket* sock, const QJsonObject& obj)
{
    // 隐私硬约束：仅此函数执行截图；未触发时电脑端零采集
    const QString region = obj.value(QStringLiteral("region")).toString(QStringLiteral("fullscreen"));
    int quality = obj.value(QStringLiteral("quality")).toInt(85);
    const quint32 seq = obj.value(QStringLiteral("seq")).toVariant().toUInt();

    // 防重放：seq 必须严格递增
    if (seq == 0 || seq <= lastSeq_.value(sock, 0)) {
        sendError(sock, 1008, QStringLiteral("seq 非法或重放"));
        return;
    }
    lastSeq_[sock] = seq;

    QImage img;
    if (region == QLatin1String("fullscreen")) {
        img = CaptureEngine::captureFullscreen(-1);
    } else {
        Preset p;
        if (!config_.findPreset(region, &p)) {
            sendError(sock, 1003, QStringLiteral("预设不存在: %1").arg(region));
            return;
        }
        if (p.w <= 0 || p.h <= 0)
            img = CaptureEngine::captureFullscreen(p.monitor);
        else
            img = CaptureEngine::captureRegion(QRect(p.x, p.y, p.w, p.h));
    }
    if (img.isNull()) {
        sendError(sock, 1005, QStringLiteral("截图失败"));
        return;
    }
    QByteArray jpg = CaptureEngine::toJpeg(img, qBound(1, quality, config_.qualityMax));
    if (jpg.isEmpty()) {
        sendError(sock, 1006, QStringLiteral("JPEG 编码失败"));
        return;
    }

    // 本地留档（用户显式配置开启）
    if (config_.saveEnabled && !config_.saveDir.isEmpty()) {
        QDir d(config_.saveDir);
        if (!d.exists())
            d.mkpath(QStringLiteral("."));
        const QString fn = d.filePath(QStringLiteral("SL_%1.jpg")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz"))));
        QFile f(fn);
        if (f.open(QIODevice::WriteOnly))
            f.write(jpg);
    }

    QJsonObject resp;
    resp[QStringLiteral("type")] = QStringLiteral("capture_resp");
    resp[QStringLiteral("ok")] = true;
    resp[QStringLiteral("width")] = img.width();
    resp[QStringLiteral("height")] = img.height();
    resp[QStringLiteral("size_bytes")] = static_cast<qint64>(jpg.size());
    resp[QStringLiteral("region")] = region;
    resp[QStringLiteral("ts")] = QDateTime::currentMSecsSinceEpoch();
    sendJson(sock, resp);
    sock->write(encodeFrame(FrameType::Binary, jpg));
}

void TlsServer::handlePing(QSslSocket* sock)
{
    QJsonObject pong;
    pong[QStringLiteral("type")] = QStringLiteral("pong");
    sendJson(sock, pong);
}

void TlsServer::sendJson(QSslSocket* sock, const QJsonObject& obj)
{
    sock->write(encodeFrame(FrameType::Json,
                            QJsonDocument(obj).toJson(QJsonDocument::Compact)));
}

void TlsServer::sendError(QSslSocket* sock, int code, const QString& msg)
{
    QJsonObject e;
    e[QStringLiteral("type")] = QStringLiteral("error");
    e[QStringLiteral("code")] = code;
    e[QStringLiteral("msg")] = msg;
    sendJson(sock, e);
}

void TlsServer::dropClient(QSslSocket* sock)
{
    buffers_.remove(sock);
    authed_.remove(sock);
    lastSeq_.remove(sock);
    // 延迟断开：先确保错误帧已通过 TLS 信道送达客户端
    QTimer::singleShot(200, sock, [sock]() { sock->disconnectFromHost(); });
}

void TlsServer::onDisconnected()
{
    auto* ssl = qobject_cast<QSslSocket*>(sender());
    if (!ssl)
        return;
    buffers_.remove(ssl);
    authed_.remove(ssl);
    lastSeq_.remove(ssl);
    ssl->deleteLater();
}
