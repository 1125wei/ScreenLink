// ScreenLink 端到端协议测试（模拟手机端 vs 电脑端 TlsServer）
// 验证 PROTOCOL.md §4-§6 全流程：配对 -> 重连 -> 截图 -> 图片回传
#include "../src/server/TlsServer.h"
#include "../src/pairing/PairingManager.h"
#include "../src/core/Config.h"
#include "../src/crypto/CertificateManager.h"
#include "../src/protocol/FrameCodec.h"
#include <QCoreApplication>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QEventLoop>
#include <QTimer>
#include <cstdio>

using namespace screencap::protocol;
using screencap::core::Config;
using screencap::pairing::PairingManager;
using screencap::crypto::CertificateManager;

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, name)                                                        \
    do {                                                                         \
        if (cond) { ++g_pass; std::printf("PASS: %s\n", name); }                 \
        else { ++g_fail; std::printf("FAIL: %s\n", name); }                      \
    } while (0)

// 简易同步客户端（QSslSocket + 帧协议）
class TestClient {
public:
    bool connectTo(quint16 port) {
        sock_.connectToHost(QStringLiteral("127.0.0.1"), port);
        if (!sock_.waitForConnected(3000)) {
            std::printf("  [diag] connect fail: %s\n", qPrintable(sock_.errorString()));
            return false;
        }
        sock_.ignoreSslErrors();
        sock_.startClientEncryption();
        if (!sock_.waitForEncrypted(3000)) {
            std::printf("  [diag] handshake fail: %s\n", qPrintable(sock_.errorString()));
            return false;
        }
        return true;
    }
    void sendJson(const QJsonObject& obj) {
        sock_.write(makeJsonFrame(obj));
        sock_.waitForBytesWritten(2000);
    }
    // 读一个 JSON 帧（同步）
    QJsonObject readJson() {
        auto f = readFrame();
        if (f.type != FrameType::Json) return {};
        return QJsonDocument::fromJson(f.payload).object();
    }
    DecodedFrame readFrame() {
        while (true) {
            DecodedFrame out;
            auto r = decodeFrame(buf_, out);
            if (r == DecodeResult::Complete) return out;
            if (r == DecodeResult::Invalid) return {};
            if (!sock_.waitForReadyRead(5000)) return {};
            buf_.append(sock_.readAll());
        }
    }
    void close() { sock_.disconnectFromHost(); }
private:
    QSslSocket sock_;
    QByteArray buf_;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    std::printf("INFO: OpenSSL 后端: %s\n", QSslSocket::sslLibraryVersionString().toUtf8().constData());
    std::printf("INFO: supportsSsl: %d\n", static_cast<int>(QSslSocket::supportsSsl()));

    QTemporaryDir tmp;
    Config& cfg = Config::instance();
    cfg.listenPort = 0; // 由系统分配
    cfg.saveEnabled = false;
    cfg.presets.clear();
    screencap::core::Preset p;
    p.name = QStringLiteral("主屏"); p.monitor = 0; p.x = p.y = 0; p.w = p.h = -1;
    cfg.presets.append(p);

    CertificateManager certMgr(tmp.path());
    CHECK(certMgr.ensureCertificate(), "测试证书生成");

    PairingManager pairing;
    TlsServer server(cfg, pairing);
    CHECK(server.start(cfg.listenPort, certMgr.certFilePath(), certMgr.keyFilePath()), "服务启动");
    if (server.serverPort() == 0) { std::printf("FAIL: 端口分配失败\n"); return 1; }
    const quint16 port = server.serverPort();
    std::printf("INFO: 测试端口 %u\n", port);

    // ---- 场景 1：未配对 hello -> 1001 ----
    {
        TestClient c;
        CHECK(c.connectTo(port), "未配对客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","hello"},{"device_id","unknown-device"}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "error" && resp.value("code").toInt() == 1001, "未配对 hello 拒绝 (1001)");
        c.close();
    }

    // ---- 场景 2：错误配对码 -> 1002 ----
    {
        TestClient c;
        CHECK(c.connectTo(port), "错误码客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","pair"},{"code","WRONG-CODE"},{"device_id","dev-2"},{"device_name","测试机2"}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "error" && resp.value("code").toInt() == 1002, "错误配对码拒绝 (1002)");
        c.close();
    }

    // ---- 场景 3：正确配对 -> pair_resp ----
    const QString code = pairing.generateCode();
    {
        TestClient c;
        CHECK(c.connectTo(port), "配对客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","pair"},{"code",code},{"device_id","dev-3"},{"device_name","我的手机"}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "pair_resp" && resp.value("ok").toBool(), "配对成功 (pair_resp ok)");
        CHECK(cfg.trustedDevices.size() == 1, "信任列表已持久化");
        c.close();
    }

    // ---- 场景 4：已配对 hello -> hello_ack（带 presets） ----
    {
        TestClient c;
        CHECK(c.connectTo(port), "重连客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","hello"},{"device_id","dev-3"}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "hello_ack", "免密重连成功 (hello_ack)");
        bool hasPreset = false;
        for (const auto& v : resp.value("presets").toArray())
            if (v.toString() == QLatin1String("主屏")) hasPreset = true;
        CHECK(hasPreset, "hello_ack 下发预设列表");
        c.close();
    }

    // ---- 场景 5：capture -> capture_resp + JPEG 帧 ----
    {
        TestClient c;
        CHECK(c.connectTo(port), "截图客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","hello"},{"device_id","dev-3"}});
        c.readJson(); // hello_ack
        c.sendJson(QJsonObject{{"type","capture"},{"region","fullscreen"},{"quality",85},{"seq",1}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "capture_resp" && resp.value("ok").toBool(), "capture_resp ok");
        auto img = c.readFrame();
        CHECK(img.type == FrameType::Binary, "收到二进制图片帧");
        CHECK(img.payload.startsWith("\xff\xd8"), "JPEG magic 0xFFD8");
        CHECK(img.payload.size() > 1000, "图片数据非空");
        std::printf("INFO: 截图 %dx%d, %lld 字节\n",
                    resp.value("width").toInt(), resp.value("height").toInt(),
                    static_cast<long long>(img.payload.size()));
        c.close();
    }

    // ---- 场景 6：seq 防重放 ----
    {
        TestClient c;
        CHECK(c.connectTo(port), "防重放客户端 TLS 连接");
        c.sendJson(QJsonObject{{"type","hello"},{"device_id","dev-3"}});
        c.readJson();
        c.sendJson(QJsonObject{{"type","capture"},{"region","fullscreen"},{"seq",5}});
        c.readJson(); // capture_resp
        c.sendJson(QJsonObject{{"type","capture"},{"region","fullscreen"},{"seq",5}});
        auto resp = c.readJson();
        CHECK(resp.value("type") == "error" && resp.value("code").toInt() == 1008, "重复 seq 拒绝 (1008)");
        c.close();
    }

    std::printf("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
