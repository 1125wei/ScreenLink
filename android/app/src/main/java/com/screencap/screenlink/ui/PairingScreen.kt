package com.screencap.screenlink.ui

import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.screencap.screenlink.data.DeviceInfo
import com.screencap.screenlink.protocol.ProtocolClient
import kotlinx.coroutines.launch

/** 配对屏：输入电脑 IP / 端口 / PIN 码完成配对（PROTOCOL.md §4） */
@Composable
fun PairingScreen(onPaired: (DeviceInfo) -> Unit, onBack: () -> Unit) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var host by remember { mutableStateOf("") }
    var port by remember { mutableStateOf("8848") }
    var pin by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf<String?>(null) }

    Column(
        modifier = Modifier.fillMaxSize().padding(24.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Text("添加设备（配对）", style = MaterialTheme.typography.headlineSmall)
        Text("输入电脑端显示的配对码（10 分钟内有效），首次连接将保存电脑证书指纹", style = MaterialTheme.typography.bodySmall)

        OutlinedTextField(value = host, onValueChange = { host = it }, label = { Text("电脑 IP 地址") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = port, onValueChange = { port = it }, label = { Text("端口（默认 8848）") }, singleLine = true, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(value = pin, onValueChange = { pin = it }, label = { Text("配对码（XXXX-XXXX）") }, singleLine = true, modifier = Modifier.fillMaxWidth())

        if (error != null) Text(error!!, color = MaterialTheme.colorScheme.error)

        Button(
            onClick = {
                busy = true; error = null
                scope.launch {
                    try {
                        val client = ProtocolClient(host.trim(), port.trim().toIntOrNull() ?: 8848)
                        val (result, fp) = client.pair(pin.trim())
                        when (result) {
                            is ProtocolClient.Result.Ok -> {
                                val name = result.json?.optString("server_name") ?: host
                                onPaired(DeviceInfo(client.deviceId, name, host.trim(), port.trim().toIntOrNull() ?: 8848, fp ?: ""))
                            }
                            is ProtocolClient.Result.Err -> error = "配对失败：${result.msg}"
                        }
                        client.close()
                    } catch (e: Exception) {
                        error = "网络错误：${e.message}"
                    } finally { busy = false }
                }
            },
            enabled = !busy && host.isNotBlank() && pin.isNotBlank(),
            modifier = Modifier.fillMaxWidth()
        ) { Text(if (busy) "配对中…" else "配对") }

        TextButton(onClick = onBack) { Text("返回") }
    }
}
