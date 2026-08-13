package com.screencap.screenlink.protocol

import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.security.MessageDigest
import java.security.SecureRandom
import java.security.cert.CertificateException
import java.security.cert.X509Certificate
import javax.net.ssl.SSLContext
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory
import javax.net.ssl.TrustManager
import javax.net.ssl.X509TrustManager

/**
 * TLS 客户端（PROTOCOL.md §3/§5）
 * - TLS 1.2/1.3 加密传输
 * - 证书固定：首次连接（配对）记录服务器证书 SHA-256 指纹，此后每次校验
 * - 指纹不一致即拒绝（防中间人）
 */
object TlsClient {

    /** 创建到电脑端的 TLS 套接字；expectedFingerprint 为 null 时接受任意证书（仅配对首连） */
    fun connect(host: String, port: Int, expectedFingerprint: String?): SSLSocket {
        val tm = object : X509TrustManager {
            override fun checkClientTrusted(chain: Array<out X509Certificate>?, authType: String?) {}
            override fun checkServerTrusted(chain: Array<out X509Certificate>?, authType: String?) {
                if (chain == null || chain.isEmpty()) throw CertificateException("无服务器证书")
                val actual = fingerprint(chain[0])
                if (expectedFingerprint == null) return // TOFU：配对首连接受并记录
                if (!actual.equals(expectedFingerprint, ignoreCase = true))
                    throw CertificateException("证书指纹不匹配！\n期望: $expectedFingerprint\n实际: $actual\n可能遭遇中间人攻击")
            }
            override fun getAcceptedIssuers(): Array<X509Certificate> = arrayOf()
        }
        val ctx = SSLContext.getInstance("TLS")
        ctx.init(null, arrayOf<TrustManager>(tm), SecureRandom())

        val factory: SSLSocketFactory = ctx.socketFactory
        val socket = factory.createSocket(host, port) as SSLSocket
        socket.startHandshake()
        return socket
    }

    /** 证书 SHA-256 指纹（十六进制小写，冒号分隔，95 字符） */
    fun fingerprint(cert: X509Certificate): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(cert.encoded)
        return digest.joinToString(":") { "%02x".format(it) }
    }

    /** 写完整帧 */
    fun writeFrame(socket: SSLSocket, frame: ByteArray) {
        socket.getOutputStream().write(frame)
        socket.getOutputStream().flush()
    }

    /** 读一帧（阻塞）；EOF 返回 null */
    fun readFrame(socket: SSLSocket): FrameCodec.Decoded? {
        val input = socket.getInputStream()
        val header = ByteArray(5)
        if (readFully(input, header) == -1) return null
        val len = ((header[0].toInt() and 0xFF) shl 24) or
            ((header[1].toInt() and 0xFF) shl 16) or
            ((header[2].toInt() and 0xFF) shl 8) or
            (header[3].toInt() and 0xFF)
        val payload = ByteArray(len)
        if (readFully(input, payload) == -1) return null
        return FrameCodec.Decoded(header[4], payload)
    }

    private fun readFully(input: java.io.InputStream, buf: ByteArray): Int {
        var off = 0
        while (off < buf.size) {
            val n = input.read(buf, off, buf.size - off)
            if (n == -1) return if (off == 0) -1 else -1
            off += n
        }
        return off
    }
}
