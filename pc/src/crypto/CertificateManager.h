#pragma once
// ScreenLink 证书管理（PROTOCOL.md §3.1）
// 首启生成 ECC P-256 自签证书（有效期 365 天），到期前自动续期
#include <QString>

namespace screencap::crypto {

class CertificateManager {
public:
    explicit CertificateManager(const QString& certDir);

    // 确保证书存在（缺失则用 openssl 生成）；返回是否就绪
    bool ensureCertificate();

    QString certFilePath() const;
    QString keyFilePath() const;

    // 证书 SHA-256 指纹（hex 小写，冒号分隔），用于配对分发与客户端校验
    QString fingerprintHex() const;

private:
    bool generateWithOpenSSL();
    QString certDir_;
};

} // namespace screencap::crypto
