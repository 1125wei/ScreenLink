#include "FrameCodec.h"
#include <QJsonDocument>

namespace screencap::protocol {

QByteArray encodeFrame(FrameType type, const QByteArray& payload)
{
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    QByteArray frame;
    frame.reserve(5 + payload.size());
    frame.append(static_cast<char>((len >> 24) & 0xFF));
    frame.append(static_cast<char>((len >> 16) & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(static_cast<char>(type));
    frame.append(payload);
    return frame;
}

DecodeResult decodeFrame(QByteArray& buffer, DecodedFrame& out)
{
    if (buffer.size() < 5)
        return DecodeResult::NeedMore;

    std::uint32_t len =
        (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[0])) << 24) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[1])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[2])) << 8) |
        static_cast<std::uint32_t>(static_cast<unsigned char>(buffer[3]));

    const unsigned char typeByte = static_cast<unsigned char>(buffer[4]);
    if (typeByte != static_cast<unsigned char>(FrameType::Json) &&
        typeByte != static_cast<unsigned char>(FrameType::Binary))
        return DecodeResult::Invalid;
    if (len > kMaxFrameLen)
        return DecodeResult::Invalid;

    if (buffer.size() < 5 + static_cast<qsizetype>(len))
        return DecodeResult::NeedMore;

    out.type = (typeByte == static_cast<unsigned char>(FrameType::Json))
                   ? FrameType::Json
                   : FrameType::Binary;
    out.payload = buffer.mid(5, len);
    buffer.remove(0, 5 + static_cast<qsizetype>(len));
    return DecodeResult::Complete;
}

QByteArray makeJsonFrame(const QJsonObject& obj)
{
    return encodeFrame(FrameType::Json,
                       QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

} // namespace screencap::protocol
