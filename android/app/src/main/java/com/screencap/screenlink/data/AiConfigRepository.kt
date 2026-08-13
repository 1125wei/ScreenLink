package com.screencap.screenlink.data

import android.content.Context
import android.content.SharedPreferences

/** AI 配置持久化：API key 经 Keystore 加密后存 SharedPreferences */
class AiConfigRepository(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("screenlink_ai", Context.MODE_PRIVATE)
    private val crypto = KeyStoreCrypto()

    fun load(): AiConfig {
        // 旧版本默认提示词迁移：用户未自定义（仍是旧默认）时升级为新默认
        val savedPrompt = prefs.getString(K_PROMPT, null)
        val prompt = when {
            savedPrompt == null -> AiConfig().systemPrompt
            savedPrompt == OLD_DEFAULT_PROMPT -> AiConfig().systemPrompt
            else -> savedPrompt
        }
        return AiConfig(
            mode = runCatching { AiMode.valueOf(prefs.getString(K_MODE, "MIMO") ?: "MIMO") }
                .getOrDefault(AiMode.MIMO),
            deepseekKey = crypto.decrypt(prefs.getString(K_DS_KEY, "") ?: ""),
            mimoKey = crypto.decrypt(prefs.getString(K_MIMO_KEY, "") ?: ""),
            deepseekModel = prefs.getString(K_DS_MODEL, "deepseek-chat") ?: "deepseek-chat",
            mimoModel = prefs.getString(K_MIMO_MODEL, "mimo-v2.5") ?: "mimo-v2.5",
            systemPrompt = prompt,
        )
    }

    fun save(cfg: AiConfig) {
        prefs.edit()
            .putString(K_MODE, cfg.mode.name)
            .putString(K_DS_KEY, crypto.encrypt(cfg.deepseekKey))
            .putString(K_MIMO_KEY, crypto.encrypt(cfg.mimoKey))
            .putString(K_DS_MODEL, cfg.deepseekModel)
            .putString(K_MIMO_MODEL, cfg.mimoModel)
            .putString(K_PROMPT, cfg.systemPrompt)
            .apply()
    }

    private companion object {
        const val K_MODE = "mode"
        const val K_DS_KEY = "deepseek_key"
        const val K_MIMO_KEY = "mimo_key"
        const val K_DS_MODEL = "deepseek_model"
        const val K_MIMO_MODEL = "mimo_model"
        const val K_PROMPT = "system_prompt"
        // v0.2.0 之前的默认提示词（迁移用）
        const val OLD_DEFAULT_PROMPT =
            "你是 ScreenLink 的 AI 助手，请根据用户提供的截图内容（文字或图片）回答用户的问题。"
    }
}
