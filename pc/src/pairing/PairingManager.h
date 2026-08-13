#pragma once
// ScreenLink 配对管理（PROTOCOL.md §4）
// 配对码：32 字节 CSPRNG -> Base32 编码 -> 8 位可读分组（XXXX-XXXX），10 分钟有效期
#include <QString>
#include <QDateTime>

namespace screencap::pairing {

class PairingManager {
public:
    // 生成新配对码（旧的立即失效），返回可读分组形式
    QString generateCode();
    // 当前配对码及过期时间
    QString currentCode() const { return code_; }
    QDateTime expiresAt() const { return expiresAt_; }
    bool isActive() const { return !code_.isEmpty() && QDateTime::currentDateTime() < expiresAt_; }

    // 校验配对码：有效返回 true
    bool verify(const QString& code);

    // 配对成功后清理（配对码失效）
    void onPaired();

    static constexpr int kCodeMinutes = 10;

private:
    QString code_;
    QDateTime expiresAt_;
};

// RFC 4648 Base32 编码（无填充）
QString base32Encode(const QByteArray& data);

} // namespace screencap::pairing
