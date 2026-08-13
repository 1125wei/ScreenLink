package com.screencap.screenlink.data

/** AI 模式：DeepSeek（本地 OCR + 文本） / MiMo（图片直传） */
enum class AiMode { DEEPSEEK, MIMO }

/** AI 配置（各厂商 API key 独立存储，直连官方接口） */
data class AiConfig(
    val mode: AiMode = AiMode.MIMO,
    val deepseekKey: String = "",
    val mimoKey: String = "",
    val deepseekModel: String = "deepseek-chat",
    val mimoModel: String = "mimo-v2.5",
    val systemPrompt: String =
        "你是 ScreenLink 的 AI 助手。用户会发送电脑截图的图片或其提取的文字。" +
        "请直接回答用户提出的问题；若用户没有单独提问，请理解截图内容并直接回答其中的问题" +
        "（如题目、报错、疑问等），仅在截图确实不含任何问题时才做内容摘要。",
) {
    /** 当前模式对应的 API key */
    fun activeKey(): String = when (mode) {
        AiMode.DEEPSEEK -> deepseekKey
        AiMode.MIMO -> mimoKey
    }

    /** 当前模式对应的模型名 */
    fun activeModel(): String = when (mode) {
        AiMode.DEEPSEEK -> deepseekModel
        AiMode.MIMO -> mimoModel
    }
}
