package com.screencap.screenlink.protocol

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.util.UUID
import javax.net.ssl.SSLSocket

/**
 * ScreenLink 协议客户端（PROTOCOL.md §4-§7）
 * 状态机：pair（配对）→ hello（重连）→ capture（截图）
 * 所有网络操作在 Dispatchers.IO，UI 线程零阻塞
 */
class ProtocolClient(
    private val host: String,
    private val port: Int,
    val deviceId: String = UUID.randomUUID().toString(), // 配对时随机生成；连接时传入已保存的 ID（保证与服务端一致）
    private val savedFingerprint: String? = null, // null = 首次连接（配对模式，TOFU 记录指纹）
) {
    companion object {
        const val DEFAULT_QUALITY = 85
    }

    private var socket: SSLSocket? = null
    private var serverFingerprint: String? = null
    private var seq: Int = 0
    val deviceName: String = android.os.Build.MODEL

    sealed class Result {
        data class Ok(val json: JSONObject? = null, val image: ByteArray? = null) : Result()
        data class Err(val code: Int, val msg: String) : Result()
    }

    /** 配对（PIN）：校验配对码；成功后返回服务器信息与证书指纹 */
    suspend fun pair(pin: String): Pair<Result, String?> = withContext(Dispatchers.IO) {
        try {
            connect()
            val fp = serverFingerprint!!
            val req = JSONObject().apply {
                put("type", "pair"); put("code", pin)
                put("device_id", deviceId); put("device_name", deviceName)
            }
            writeFrame(FrameCodec.encodeJson(req))
            val resp = readJson() ?: return@withContext Pair(Result.Err(1007, "连接中断"), null)
            if (resp.optString("type") == "pair_resp" && resp.optBoolean("ok", false)) {
                Pair(Result.Ok(resp), fp)
            } else {
                Pair(Result.Err(resp.optInt("code", 1002), resp.optString("msg", "配对失败")), null)
            }
        } catch (e: Exception) {
            Pair(Result.Err(0, e.message ?: "网络错误"), null)
        }
    }

    /** 已配对设备免密重连：返回服务器预设列表 */
    suspend fun hello(): Result = withContext(Dispatchers.IO) {
        try {
            connect()
            val req = JSONObject().apply {
                put("type", "hello"); put("device_id", deviceId)
            }
            writeFrame(FrameCodec.encodeJson(req))
            val resp = readJson() ?: return@withContext Result.Err(1007, "连接中断")
            if (resp.optString("type") == "hello_ack") Result.Ok(resp)
            else Result.Err(resp.optInt("code", 1001), resp.optString("msg", "认证失败"))
        } catch (e: Exception) {
            Result.Err(0, e.message ?: "网络错误")
        }
    }

    /** 触发截图：返回 JPEG 字节（Result.Ok.image） */
    suspend fun capture(region: String, quality: Int): Result = withContext(Dispatchers.IO) {
        try {
            ensureSocket()
            val req = JSONObject().apply {
                put("type", "capture"); put("region", region)
                put("quality", quality); put("seq", ++seq)
            }
            writeFrame(FrameCodec.encodeJson(req))
            val meta = readJson() ?: return@withContext Result.Err(1007, "连接中断")
            if (meta.optString("type") == "capture_resp" && meta.optBoolean("ok", false)) {
                val img = readFrame()?.takeIf { it.type == FrameCodec.TYPE_BINARY }?.payload
                    ?: return@withContext Result.Err(1006, "未收到图片数据")
                Result.Ok(meta, img)
            } else {
                Result.Err(meta.optInt("code", 1005), meta.optString("msg", "截图失败"))
            }
        } catch (e: Exception) {
            Result.Err(0, e.message ?: "网络错误")
        }
    }

    fun close() {
        try { socket?.close() } catch (_: Exception) {}
        socket = null
    }

    // ---- 内部 ----
    private fun connect() {
        if (socket?.isConnected == true && !socket!!.isClosed) return
        socket = TlsClient.connect(host, port, savedFingerprint)
        serverFingerprint = TlsClient.fingerprint(
            socket!!.session.peerCertificates.first() as java.security.cert.X509Certificate
        )
    }

    private fun ensureSocket() {
        if (socket?.isConnected != true || socket!!.isClosed) throw IllegalStateException("未连接")
    }

    private fun writeFrame(frame: ByteArray) = TlsClient.writeFrame(socket!!, frame)

    private fun readJson(): JSONObject? {
        val frame = TlsClient.readFrame(socket!!) ?: return null
        return JSONObject(String(frame.payload, Charsets.UTF_8))
    }

    private fun readFrame(): FrameCodec.Decoded? = TlsClient.readFrame(socket!!)
}
