#include "PairingManager.h"
#include <QRandomGenerator>
#include <QDateTime>

namespace screencap::pairing {

QString base32Encode(const QByteArray& data)
{
    static const char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QString out;
    int bits = 0;
    quint32 acc = 0;
    for (char c : data) {
        acc = (acc << 8) | static_cast<quint8>(c);
        bits += 8;
        while (bits >= 5) {
            bits -= 5;
            out.append(QLatin1Char(kAlphabet[(acc >> bits) & 0x1F]));
        }
    }
    if (bits > 0)
        out.append(QLatin1Char(kAlphabet[(acc << (5 - bits)) & 0x1F]));
    return out;
}

QString PairingManager::generateCode()
{
    QByteArray raw(32, Qt::Uninitialized);
    QRandomGenerator::system()->fillRange(reinterpret_cast<quint32*>(raw.data()),
                                          raw.size() / static_cast<int>(sizeof(quint32)));
    const QString b32 = base32Encode(raw);
    // 32 字节 -> 52 个 Base32 字符，取前 8 位分组为 XXXX-XXXX
    code_ = b32.left(8);
    code_.insert(4, QLatin1Char('-'));
    expiresAt_ = QDateTime::currentDateTime().addSecs(kCodeMinutes * 60);
    return code_;
}

bool PairingManager::verify(const QString& code)
{
    return isActive() && code.compare(code_, Qt::CaseInsensitive) == 0;
}

void PairingManager::onPaired()
{
    code_.clear();
    expiresAt_ = {};
}

} // namespace screencap::pairing
