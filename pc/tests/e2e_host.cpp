// e2e_host：控制台版 ScreenLink 服务端（调试/联调用，qDebug 直接输出）
// 用法: e2e_host [port]  —— 默认 8848，从 %APPDATA%/ScreenLink 加载配置
#include "../src/server/TlsServer.h"
#include "../src/pairing/PairingManager.h"
#include "../src/core/Config.h"
#include "../src/crypto/CertificateManager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <cstdio>

using screencap::core::Config;
using screencap::pairing::PairingManager;
using screencap::crypto::CertificateManager;

static void logToFile(QtMsgType, const QMessageLogContext&, const QString& msg)
{
    QFile f(QStringLiteral("D:/ScreenLink/.tools/host_log.txt"));
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream ts(&f);
        ts << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz "))
           << msg << Qt::endl;
    }
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ScreenLink")); // 与 main.cpp 一致，配置文件路径正确
    qInstallMessageHandler(logToFile);
    setvbuf(stdout, nullptr, _IONBF, 0);
    const quint16 port = argc > 1 ? static_cast<quint16>(QString::fromLatin1(argv[1]).toUInt())
                                  : static_cast<quint16>(8848);

    Config& cfg = Config::instance();
    cfg.load();

    CertificateManager certMgr(QDir(Config::configDir()).filePath(QStringLiteral("certs")));
    if (!certMgr.ensureCertificate()) {
        std::printf("证书生成失败\n");
        return 1;
    }

    PairingManager pairing;
    TlsServer server(cfg, pairing);
    QObject::connect(&server, &TlsServer::logMessage,
                     [](const QString& m) { std::printf("[server] %s\n", qPrintable(m)); });
    if (!server.start(port, certMgr.certFilePath(), certMgr.keyFilePath())) {
        std::printf("服务启动失败\n");
        return 1;
    }
    std::printf("[host] 监听 %u，进入事件循环\n", port);
    return app.exec();
}
