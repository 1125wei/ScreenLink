package com.screencap.screenlink

import android.graphics.BitmapFactory
import android.util.Base64
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.screencap.screenlink.ai.AiClient
import com.screencap.screenlink.ai.ChatMessage
import com.screencap.screenlink.ai.OcrEngine
import com.screencap.screenlink.data.AiConfig
import com.screencap.screenlink.data.AiMode
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch

/**
 * AI 问答 ViewModel：请求跑在 viewModelScope（Activity 级作用域）
 * 切换 Tab / 界面重组不会取消正在进行的 AI 请求（修复"coroutine scope left the composition"）
 */
class MainViewModel : ViewModel() {

    /** 多轮对话会话（Activity 级存活，跨 Tab 保留） */
    val conversation = mutableStateListOf<ChatMessage>()
    val aiBusy = mutableStateOf(false)

    /** 发起 AI 问答：DeepSeek = 本地 OCR + 文本；MiMo = 图片直传 */
    fun askAi(cfg: AiConfig, imageBytes: ByteArray, question: String) {
        if (aiBusy.value) return
        val q = question.trim().ifEmpty { "请直接回答截图中的问题" }
        viewModelScope.launch {
            aiBusy.value = true
            try {
                when (cfg.mode) {
                    AiMode.DEEPSEEK -> {
                        val bmp = BitmapFactory.decodeByteArray(imageBytes, 0, imageBytes.size)
                        if (bmp == null) {
                            conversation.add(ChatMessage("assistant", "⚠️ 图片解析失败"))
                            return@launch
                        }
                        val ocr = OcrEngine.extractText(bmp)
                        val userText = if (ocr.isBlank())
                            "（截图未识别出文字）\n\n问题：$q"
                        else
                            "截图中的文字：\n$ocr\n\n问题：$q"
                        conversation.add(ChatMessage("user", userText))
                        when (val r = AiClient.chat(cfg, conversation.toList())) {
                            is AiClient.Result.Ok -> conversation.add(ChatMessage("assistant", r.reply))
                            is AiClient.Result.Err -> conversation.add(ChatMessage("assistant", "⚠️ ${r.msg}"))
                        }
                    }
                    AiMode.MIMO -> {
                        conversation.add(ChatMessage("user", q))
                        val b64 = Base64.encodeToString(imageBytes, Base64.NO_WRAP)
                        when (val r = AiClient.chat(cfg, conversation.toList(), b64)) {
                            is AiClient.Result.Ok -> conversation.add(ChatMessage("assistant", r.reply))
                            is AiClient.Result.Err -> conversation.add(ChatMessage("assistant", "⚠️ ${r.msg}"))
                        }
                    }
                }
            } catch (e: CancellationException) {
                throw e // 协程取消不吞掉、不当错误显示
            } catch (e: Exception) {
                conversation.add(ChatMessage("assistant", "⚠️ 处理失败：${e.message}"))
            } finally {
                aiBusy.value = false
            }
        }
    }
}
