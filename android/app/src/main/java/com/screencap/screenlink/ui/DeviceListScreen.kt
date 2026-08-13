package com.screencap.screenlink.ui

import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.screencap.screenlink.data.DeviceInfo
import com.screencap.screenlink.data.DeviceRepository

/** 设备列表屏：已配对设备 + 添加入口 */
@Composable
fun DeviceListScreen(onConnect: (DeviceInfo) -> Unit, onAddNew: () -> Unit) {
    val context = LocalContext.current
    var devices by remember { mutableStateOf(DeviceRepository.load(context)) }

    Column(modifier = Modifier.fillMaxSize().padding(24.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Text("ScreenLink 屏联", style = MaterialTheme.typography.headlineSmall)
        Text("已配对设备：", style = MaterialTheme.typography.titleMedium)

        if (devices.isEmpty()) {
            Text("暂无配对设备", style = MaterialTheme.typography.bodyMedium)
        } else {
            devices.forEach { d ->
                Card(modifier = Modifier.fillMaxWidth().clickable { onConnect(d) }) {
                    Column(Modifier.padding(16.dp)) {
                        Text(d.name, style = MaterialTheme.typography.titleMedium)
                        Text("${d.host}:${d.port}", style = MaterialTheme.typography.bodySmall)
                    }
                }
            }
        }

        Button(onClick = onAddNew, modifier = Modifier.fillMaxWidth()) { Text("＋ 添加设备（配对）") }
    }
}
