package com.screencap.screenlink.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.screencap.screenlink.data.AiConfig
import com.screencap.screenlink.data.AiMode

/**
 * AI 设置页：模式切换（DeepSeek/MiMo）+ API Key + 模型名 + 系统提示词
 * Key 仅保存在本机（Keystore 加密），直连厂商官方接口
 */
@Composable
fun AiSettingsScreen(config: AiConfig, onSave: (AiConfig) -> Unit) {
    var mode by remember { mutableStateOf(config.mode) }
    var dsKey by remember { mutableStateOf(config.deepseekKey) }
    var mimoKey by remember { mutableStateOf(config.mimoKey) }
    var dsModel by remember { mutableStateOf(config.deepseekModel) }
    var mimoModel by remember { mutableStateOf(config.mimoModel) }
    var prompt by remember { mutableStateOf(config.systemPrompt) }
    var saved by remember { mutableStateOf(false) }

    Column(
        modifier = Modifier.fillMaxSize().padding(20.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Text("AI 设置", style = MaterialTheme.typography.headlineSmall)
        Text(
            "API Key 仅保存在本机（系统加密），直连厂商官方接口，不经任何第三方。",
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )

        // ---- 模式切换 ----
        Text("问答模式", style = MaterialTheme.typography.titleMedium)
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            ModeChip("DeepSeek\n(本地 OCR+文字)", mode == AiMode.DEEPSEEK) { mode = AiMode.DEEPSEEK }
            ModeChip("MiMo\n(图片直传)", mode == AiMode.MIMO) { mode = AiMode.MIMO }
        }
        Text(
            when (mode) {
                AiMode.DEEPSEEK -> "流程：截图 → 本地 OCR 提取文字 → 发给 DeepSeek。成本 ¥2/¥8 每百万 token。"
                AiMode.MIMO -> "流程：截图 → 直接发给小米 MiMo-V2.5（原生多模态）。成本 ¥1/¥2 每百万 token，无需 OCR。"
            },
            style = MaterialTheme.typography.bodySmall,
            color = MaterialTheme.colorScheme.onSurfaceVariant,
        )

        // ---- DeepSeek 配置 ----
        OutlinedCard {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("DeepSeek", style = MaterialTheme.typography.titleMedium)
                OutlinedTextField(
                    value = dsKey, onValueChange = { dsKey = it },
                    label = { Text("API Key（platform.deepseek.com 获取）") },
                    singleLine = true, modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = dsModel, onValueChange = { dsModel = it },
                    label = { Text("模型名") }, singleLine = true, modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        // ---- MiMo 配置 ----
        OutlinedCard {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("小米 MiMo", style = MaterialTheme.typography.titleMedium)
                OutlinedTextField(
                    value = mimoKey, onValueChange = { mimoKey = it },
                    label = { Text("API Key（mimo.mi.com 控制台获取）") },
                    singleLine = true, modifier = Modifier.fillMaxWidth(),
                )
                OutlinedTextField(
                    value = mimoModel, onValueChange = { mimoModel = it },
                    label = { Text("模型名（默认 mimo-v2.5）") },
                    singleLine = true, modifier = Modifier.fillMaxWidth(),
                )
            }
        }

        // ---- 系统提示词 ----
        Text("系统提示词（多轮对话的 AI 角色设定）", style = MaterialTheme.typography.titleMedium)
        OutlinedTextField(
            value = prompt, onValueChange = { prompt = it },
            label = { Text("提示词") },
            minLines = 4, maxLines = 8, modifier = Modifier.fillMaxWidth(),
        )

        // ---- 保存 ----
        Button(
            onClick = {
                onSave(
                    AiConfig(
                        mode = mode,
                        deepseekKey = dsKey.trim(),
                        mimoKey = mimoKey.trim(),
                        deepseekModel = dsModel.trim().ifEmpty { "deepseek-chat" },
                        mimoModel = mimoModel.trim().ifEmpty { "mimo-v2.5" },
                        systemPrompt = prompt,
                    )
                )
                saved = true
            },
            modifier = Modifier.fillMaxWidth(),
        ) { Text("保存配置") }
        if (saved) {
            Text("已保存 ✓", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.bodyMedium)
        }
    }
}

@Composable
private fun ModeChip(label: String, selected: Boolean, onClick: () -> Unit) {
    FilterChip(
        selected = selected,
        onClick = onClick,
        label = { Text(label, textAlign = androidx.compose.ui.text.style.TextAlign.Center) },
    )
}
