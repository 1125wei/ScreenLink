#pragma once
// ScreenLink 协议帧编解码（PROTOCOL.md §2）
// 帧格式: [4字节大端长度][1字节类型][payload]
#include <cstdint>
#include <QByteArray>
#include <QJsonObject>

namespace screencap::protocol {

enum class FrameType : std::uint8_t {
    Json   = 0x01,
    Binary = 0x02,
};

// 最大帧长 64 MiB（协议约束，超限拒绝）
constexpr std::uint32_t kMaxFrameLen = 64 * 1024 * 1024;

struct DecodedFrame {
    FrameType type = FrameType::Json;
    QByteArray payload;
};

enum class DecodeResult {
    NeedMore,   // 半包，等待更多数据
    Complete,   // 完整一帧已取出（buffer 已消费）
    Invalid,    // 非法帧（类型未知/超限），应断开连接
};

// 编码：payload + type -> 完整帧字节流
QByteArray encodeFrame(FrameType type, const QByteArray& payload);

// 解码：从累积缓冲解析一帧（处理粘包/半包）
DecodeResult decodeFrame(QByteArray& buffer, DecodedFrame& out);

// 便捷：JSON 对象 -> JSON 控制帧
QByteArray makeJsonFrame(const QJsonObject& obj);

} // namespace screencap::protocol
