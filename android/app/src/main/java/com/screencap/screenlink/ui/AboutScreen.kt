package com.screencap.screenlink.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

/** 关于页：版本信息、使用提示、隐私说明 */
@Composable
fun AboutScreen() {
    Column(
        modifier = Modifier.fillMaxSize().padding(20.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        Text("关于 ScreenLink 屏联", style = MaterialTheme.typography.headlineSmall)
        Text("版本 0.2.0（AI 问答版）", style = MaterialTheme.typography.bodyMedium)

        OutlinedCard {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("功能", style = MaterialTheme.typography.titleMedium)
                Text("• 手机遥控电脑截图（局域网，TLS 1.3 加密）")
                Text("• 截图保存相册 / 分享")
                Text("• 提取图片文字并询问 AI（DeepSeek / 小米 MiMo）")
                Text("• 多轮对话：可对截图内容连续追问")
            }
        }

        OutlinedCard {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("使用提示", style = MaterialTheme.typography.titleMedium)
                Text("• 电脑端需运行 ScreenLink 并保持托盘常驻")
                Text("• 手机与电脑需在同一局域网（同一 Wi-Fi）")
                Text("• 首次使用请先在电脑端防火墙放行 8848 端口（add_firewall.bat）")
                Text("• 电脑 IP 可在电脑端主窗口查看（已过滤虚拟网卡）")
            }
        }

        OutlinedCard {
            Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text("隐私说明", style = MaterialTheme.typography.titleMedium)
                Text("• AI 功能会将截图内容（或 OCR 提取的文字）发送给你选择的 AI 服务商")
                Text("• API Key 仅保存在本机（系统 Keystore 加密），直连官方接口")
                Text("• 敏感内容请谨慎使用 AI 问答功能")
            }
        }
    }
}
