package com.screencap.screenlink.protocol

import org.json.JSONObject

/**
 * ScreenLink 协议帧编解码（与电脑端 FrameCodec.cpp 严格对应，PROTOCOL.md §2）
 * 帧格式: [4字节大端长度][1字节类型][payload]
 * 类型: 0x01 = JSON 控制帧, 0x02 = 二进制数据帧（图片）
 */
object FrameCodec {
    const val TYPE_JSON: Byte = 0x01
    const val TYPE_BINARY: Byte = 0x02
    const val MAX_FRAME_LEN = 64 * 1024 * 1024

    class Decoded(val type: Byte, val payload: ByteArray)

    /** 编码：payload + type -> 完整帧 */
    fun encode(type: Byte, payload: ByteArray): ByteArray {
        val len = payload.size
        val frame = ByteArray(5 + len)
        frame[0] = ((len ushr 24) and 0xFF).toByte()
        frame[1] = ((len ushr 16) and 0xFF).toByte()
        frame[2] = ((len ushr 8) and 0xFF).toByte()
        frame[3] = (len and 0xFF).toByte()
        frame[4] = type
        payload.copyInto(frame, 5)
        return frame
    }

    fun encodeJson(obj: JSONObject): ByteArray = encode(TYPE_JSON, obj.toString().toByteArray(Charsets.UTF_8))

    /** 解析器：增量喂入字节流，取出完整帧（处理粘包/半包） */
    class Parser {
        private val buffer = java.io.ByteArrayOutputStream()

        /** 尝试取出一帧；半包返回 null；非法帧抛 ProtocolException */
        fun feed(data: ByteArray): Decoded? {
            buffer.write(data)
            val buf = buffer.toByteArray()
            if (buf.size < 5) return null
            val len = ((buf[0].toInt() and 0xFF) shl 24) or
                ((buf[1].toInt() and 0xFF) shl 16) or
                ((buf[2].toInt() and 0xFF) shl 8) or
                (buf[3].toInt() and 0xFF)
            val type = buf[4]
            if (type != TYPE_JSON && type != TYPE_BINARY) throw ProtocolException("非法帧类型: $type")
            if (len > MAX_FRAME_LEN) throw ProtocolException("帧超限: $len")
            if (buf.size < 5 + len) return null
            val payload = buf.copyOfRange(5, 5 + len)
            buffer.reset()
            if (buf.size > 5 + len) buffer.write(buf, 5 + len, buf.size - (5 + len))
            return Decoded(type, payload)
        }
    }

    class ProtocolException(message: String) : Exception(message)
}
