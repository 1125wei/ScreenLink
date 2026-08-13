// ScreenLink 核心模块单元测试（轻量 main 风格，后续可迁移 Qt Test）
#include "../src/protocol/FrameCodec.h"
#include "../src/capture/CaptureEngine.h"
#include "../src/crypto/CertificateManager.h"
#include "../src/pairing/PairingManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <cstdio>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, name)                                                        \
    do {                                                                         \
        if (cond) { ++g_pass; std::printf("PASS: %s\n", name); }                 \
        else { ++g_fail; std::printf("FAIL: %s\n", name); }                      \
    } while (0)

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    using namespace screencap::protocol;

    // ---- FrameCodec：完整帧 ----
    {
        QByteArray buf;
        auto j = makeJsonFrame(QJsonObject{{"type", "ping"}});
        DecodedFrame out;
        CHECK(decodeFrame(buf, out) == DecodeResult::NeedMore, "空缓冲 -> NeedMore");
        buf.append(j);
        CHECK(decodeFrame(buf, out) == DecodeResult::Complete, "完整帧解码");
        CHECK(out.type == FrameType::Json, "帧类型 Json");
        CHECK(out.payload.contains("ping"), "payload 内容正确");
        CHECK(buf.isEmpty(), "消费后缓冲清空");
    }
    // ---- FrameCodec：粘包 ----
    {
        QByteArray buf;
        auto f1 = encodeFrame(FrameType::Json, QByteArray("{\"a\":1}"));
        auto f2 = encodeFrame(FrameType::Binary, QByteArray("\xff\xd8\xff"));
        buf.append(f1).append(f2);
        DecodedFrame out;
        CHECK(decodeFrame(buf, out) == DecodeResult::Complete, "粘包第1帧");
        CHECK(decodeFrame(buf, out) == DecodeResult::Complete, "粘包第2帧");
        CHECK(out.type == FrameType::Binary, "第2帧类型 Binary");
        CHECK(out.payload == QByteArray("\xff\xd8\xff"), "二进制 payload 保真");
    }
    // ---- FrameCodec：半包 ----
    {
        QByteArray buf = encodeFrame(FrameType::Json, QByteArray("hello"));
        buf.chop(3);
        DecodedFrame out;
        CHECK(decodeFrame(buf, out) == DecodeResult::NeedMore, "半包 -> NeedMore");
    }
    // ---- FrameCodec：非法类型 ----
    {
        QByteArray buf;
        buf.append(char(0)).append(char(0)).append(char(0)).append(char(1)).append(char(0x77));
        DecodedFrame out;
        CHECK(decodeFrame(buf, out) == DecodeResult::Invalid, "未知类型 -> Invalid");
    }
    // ---- FrameCodec：超长帧 ----
    {
        QByteArray buf;
        buf.append(char(0x08)).append(char(0)).append(char(0)).append(char(0)).append(char(0x01));
        DecodedFrame out;
        CHECK(decodeFrame(buf, out) == DecodeResult::Invalid, "超长帧 -> Invalid");
    }

    // ---- CaptureEngine ----
    {
        using namespace screencap::capture;
        auto sc = CaptureEngine::screens();
        CHECK(!sc.isEmpty(), "枚举显示器非空");
        if (!sc.isEmpty())
            std::printf("INFO: 显示器数=%d 主屏=%dx%d\n", sc.size(),
                        sc[0].geometry.width(), sc[0].geometry.height());
        QImage img = CaptureEngine::captureFullscreen(0);
        CHECK(!img.isNull() && img.width() > 0 && img.height() > 0, "全屏截图非空");
        auto jpg = CaptureEngine::toJpeg(img, 85);
        CHECK(jpg.size() > 1000, "JPEG 编码产出非空");
        CHECK(jpg.startsWith("\xff\xd8"), "JPEG magic 0xFFD8");
        // 区域截图
        QImage region = CaptureEngine::captureRegion(QRect(0, 0, 320, 200));
        CHECK(region.width() == 320 && region.height() == 200, "区域截图尺寸正确");
    }

    // ---- CertificateManager ----
    {
        using namespace screencap::crypto;
        QString dir = QDir::tempPath() + QStringLiteral("/screencap_cert_test");
        CertificateManager cm(dir);
        CHECK(cm.ensureCertificate(), "证书生成成功");
        CHECK(QFile::exists(cm.certFilePath()) && QFile::exists(cm.keyFilePath()),
              "证书/私钥文件存在");
        QString fp = cm.fingerprintHex();
        std::printf("INFO: 证书指纹=%s\n", fp.toUtf8().constData());
        CHECK(fp.size() == 95, "SHA-256 指纹 95 字符（带冒号）");
        CHECK(fp.contains(':'), "指纹冒号分隔");
        // 幂等性：重复调用不重新生成
        CertificateManager cm2(dir);
        CHECK(cm2.ensureCertificate(), "二次调用幂等");
        CHECK(cm2.fingerprintHex() == fp, "指纹稳定不变");
    }

    // ---- PairingManager ----
    {
        using namespace screencap::pairing;
        PairingManager pm;
        const QString code = pm.generateCode();
        CHECK(code.size() == 9 && code[4] == '-', "配对码格式 XXXX-XXXX");
        CHECK(pm.isActive(), "配对码初始有效");
        CHECK(pm.verify(code), "正确配对码校验通过");
        CHECK(!pm.verify("WRONG-CODE"), "错误配对码拒绝");
        // 错误多次不封禁，正确码始终可用
        CHECK(!pm.verify("BAD-1"), "错误1拒绝");
        CHECK(!pm.verify("BAD-2"), "错误2拒绝");
        CHECK(!pm.verify("BAD-3"), "错误3拒绝");
        CHECK(!pm.verify("BAD-4"), "错误4拒绝");
        CHECK(pm.verify(pm.currentCode()), "连续错误后正确码仍可用（无封禁）");
        // 配对成功后清理
        PairingManager pm3;
        const QString c3 = pm3.generateCode();
        CHECK(pm3.verify(c3), "配对成功");
        pm3.onPaired();
        CHECK(!pm3.isActive(), "配对后码失效");
    }

    std::printf("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
