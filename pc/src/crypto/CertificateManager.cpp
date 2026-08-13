#include "CertificateManager.h"
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSslCertificate>
#include <QHostInfo>

namespace screencap::crypto {

CertificateManager::CertificateManager(const QString& certDir)
    : certDir_(certDir)
{
}

bool CertificateManager::ensureCertificate()
{
    QDir dir(certDir_);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
    if (QFile::exists(keyFilePath()) && QFile::exists(certFilePath()))
        return true;
    return generateWithOpenSSL();
}

QString CertificateManager::certFilePath() const
{
    return QDir(certDir_).filePath(QStringLiteral("server.crt"));
}

QString CertificateManager::keyFilePath() const
{
    return QDir(certDir_).filePath(QStringLiteral("server.key"));
}

bool CertificateManager::generateWithOpenSSL()
{
    const QString cn = QStringLiteral("ScreenLink-%1").arg(QHostInfo::localHostName());
    const QString subj = QStringLiteral("/CN=%1/O=ScreenLink").arg(cn);

    // 1. 生成 ECC P-256 私钥
    QProcess gen;
    gen.start(QStringLiteral("openssl"),
              {QStringLiteral("ecparam"), QStringLiteral("-genkey"),
               QStringLiteral("-name"), QStringLiteral("prime256v1"),
               QStringLiteral("-noout"), QStringLiteral("-out"), keyFilePath()});
    if (!gen.waitForFinished(30000) || gen.exitCode() != 0)
        return false;
    QFile::setPermissions(keyFilePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    // 2. 自签证书（365 天）
    QProcess req;
    req.start(QStringLiteral("openssl"),
              {QStringLiteral("req"), QStringLiteral("-new"), QStringLiteral("-x509"),
               QStringLiteral("-key"), keyFilePath(),
               QStringLiteral("-out"), certFilePath(),
               QStringLiteral("-days"), QStringLiteral("365"),
               QStringLiteral("-subj"), subj});
    if (!req.waitForFinished(30000) || req.exitCode() != 0)
        return false;
    return QFile::exists(certFilePath());
}

QString CertificateManager::fingerprintHex() const
{
    QFile f(certFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const auto certs = QSslCertificate::fromData(f.readAll(), QSsl::Pem);
    if (certs.isEmpty())
        return {};
    return QString::fromLatin1(certs.first().digest(QCryptographicHash::Sha256).toHex(':'));
}

} // namespace screencap::crypto
