package com.screencap.screenlink.ai

import com.screencap.screenlink.data.AiConfig
import com.screencap.screenlink.data.AiMode
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONArray
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * 模型抽象层：统一 OpenAI 兼容 /v1/chat/completions 接口
 * - DeepSeek 模式：文本消息（配合本地 OCR）
 * - MiMo 模式：图片直传（标准 image_url，base64 data URI）
 */
object AiClient {

    private val http = OkHttpClient.Builder()
        .connectTimeout(15, TimeUnit.SECONDS)
        .readTimeout(120, TimeUnit.SECONDS)
        .writeTimeout(60, TimeUnit.SECONDS)
        .build()

    sealed class Result {
        data class Ok(val reply: String) : Result()
        data class Err(val msg: String) : Result()
    }

    /** 厂商 -> base_url（OpenAI 兼容） */
    private fun baseUrl(mode: AiMode): String = when (mode) {
        AiMode.DEEPSEEK -> "https://api.deepseek.com/v1"
        AiMode.MIMO -> "https://api.xiaomimimo.com/v1"
    }

    /**
     * 发起多轮对话
     * @param history 完整会话历史（含 system 提示词与已往问答）
     * @param imageBase64 MiMo 模式传截图（Base64，不带 data: 前缀）；DeepSeek 模式传 null（文字已并入 history）
     */
    suspend fun chat(
        cfg: AiConfig,
        history: List<ChatMessage>,
        imageBase64: String? = null,
    ): Result = withContext(Dispatchers.IO) {
        try {
            val key = cfg.activeKey()
            if (key.isBlank()) return@withContext Result.Err("未配置 ${cfg.mode.name} API Key，请到「AI 设置」填写")

            val body = JSONObject().apply {
                put("model", cfg.activeModel())
                put("messages", buildMessages(cfg, history, imageBase64, cfg.mode))
                put("max_tokens", 2048)
                put("stream", false)
            }

            val request = Request.Builder()
                .url("${baseUrl(cfg.mode)}/chat/completions")
                .header("Authorization", "Bearer $key")
                .header("Content-Type", "application/json")
                .post(body.toString().toRequestBody("application/json".toMediaType()))
                .build()

            http.newCall(request).execute().use { resp ->
                val respBody = resp.body?.string() ?: return@withContext Result.Err("空响应")
                if (!resp.isSuccessful) {
                    return@withContext Result.Err(parseApiError(respBody, resp.code))
                }
                val json = JSONObject(respBody)
                val choice = json.optJSONArray("choices")?.optJSONObject(0)
                val msgObj = choice?.optJSONObject("message")
                    ?: return@withContext Result.Err("响应格式异常")
                // content 为空时尝试 reasoning_content（deepseek-reasoner 思考内容）
                val reply = msgObj.optString("content").ifBlank { msgObj.optString("reasoning_content") }
                if (reply.isBlank()) {
                    val finish = choice.optString("finish_reason")
                    return@withContext Result.Err(
                        if (finish == "length") "回复被截断（超出 max_tokens），请重试"
                        else "AI 返回空内容，请重试"
                    )
                }
                Result.Ok(reply)
            }
        } catch (e: Exception) {
            Result.Err(e.message ?: "网络错误")
        }
    }

    /** 构建 messages 数组（system 提示词 + 多轮历史 + 当前截图） */
    private fun buildMessages(cfg: AiConfig, history: List<ChatMessage>, imageBase64: String?, mode: AiMode): JSONArray {
        val arr = JSONArray()
        // system 提示词始终置于最前（历史中不含 system）
        arr.put(JSONObject().apply {
            put("role", "system")
            put("content", cfg.systemPrompt)
        })
        // 历史全部保留（多轮上下文）
        for (m in history) {
            arr.put(JSONObject().apply {
                put("role", m.role)
                put("content", m.text)
            })
        }
        // 当前截图：MiMo 直传图片；DeepSeek 已由调用方将 OCR 文本并入最后一条 user 消息
        if (mode == AiMode.MIMO && !imageBase64.isNullOrEmpty()) {
            val last = history.lastOrNull()
            if (last?.role == "user") {
                // 替换最后一条 user 消息为多模态内容
                arr.remove(arr.length() - 1)
                arr.put(JSONObject().apply {
                    put("role", "user")
                    put("content", JSONArray().apply {
                        put(JSONObject().apply {
                            put("type", "image_url")
                            put("image_url", JSONObject().put("url", "data:image/jpeg;base64,$imageBase64"))
                        })
                        put(JSONObject().apply {
                            put("type", "text")
                            put("text", last.text)
                        })
                    })
                })
            }
        }
        return arr
    }

    /** 解析 API 错误信息（兼容各厂商错误体） */
    private fun parseApiError(body: String, code: Int): String {
        return try {
            val json = JSONObject(body)
            val msg = json.optJSONObject("error")?.optString("message")
                ?: json.optString("message")
            if (msg.isNullOrBlank()) "HTTP $code" else "HTTP $code: $msg"
        } catch (_: Exception) {
            "HTTP $code"
        }
    }
}
