package com.screencap.screenlink.ui

import android.content.Context
import android.content.Intent
import android.graphics.BitmapFactory
import android.provider.MediaStore
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.screencap.screenlink.MainViewModel
import com.screencap.screenlink.ai.ChatMessage
import com.screencap.screenlink.data.AiConfig
import com.screencap.screenlink.data.DeviceInfo
import com.screencap.screenlink.protocol.ProtocolClient
import kotlinx.coroutines.launch

/** 控制台屏：连接电脑 -> 截图 -> 预览/保存/分享 -> AI 问答（多轮） */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConsoleScreen(
    device: DeviceInfo,
    aiConfig: AiConfig,
    vm: MainViewModel,
    onDisconnect: () -> Unit,
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var client by remember { mutableStateOf<ProtocolClient?>(null) }
    var presets by remember { mutableStateOf(listOf("fullscreen")) }
    var selected by remember { mutableStateOf("fullscreen") }
    var imageBytes by remember { mutableStateOf<ByteArray?>(null) }
    var busy by remember { mutableStateOf(false) }
    var connected by remember { mutableStateOf(false) }
    var status by remember { mutableStateOf("连接中…") }
    var savedMsg by remember { mutableStateOf<String?>(null) }

    // AI 问答状态（会话/忙碌标志由 ViewModel 持有）
    var aiQuestion by remember { mutableStateOf("") }

    // 连接（hello 免密重连）—— 复用配对时保存的 device_id，保证服务端识别
    LaunchedEffect(device.host, device.port) {
        val c = ProtocolClient(device.host, device.port, device.id, device.fingerprint)
        client = c
        when (val r = c.hello()) {
            is ProtocolClient.Result.Ok -> {
                connected = true
                status = "已连接 ${device.name}"
                val arr = r.json?.optJSONArray("presets")
                if (arr != null) {
                    presets = (0 until arr.length()).map { arr.getString(it) }
                    selected = presets.firstOrNull() ?: "fullscreen"
                }
            }
            is ProtocolClient.Result.Err -> status = "连接失败：${r.msg}"
        }
    }
    DisposableEffect(Unit) { onDispose { client?.close() } }

    Column(
        modifier = Modifier.fillMaxSize().padding(16.dp).verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(device.name, style = MaterialTheme.typography.titleLarge)
                Text("${device.host}:${device.port}  ·  $status", style = MaterialTheme.typography.bodySmall)
            }
            TextButton(onClick = onDisconnect) { Text("断开") }
        }

        // 预设选择 + 截图按钮
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            var menuExpanded by remember { mutableStateOf(false) }
            ExposedDropdownMenuBox(expanded = menuExpanded, onExpandedChange = { menuExpanded = it }) {
                OutlinedButton(onClick = { menuExpanded = true }, modifier = Modifier.weight(1f)) { Text(selected) }
                ExposedDropdownMenu(expanded = menuExpanded, onDismissRequest = { menuExpanded = false }) {
                    presets.forEach { p ->
                        DropdownMenuItem(text = { Text(p) }, onClick = { selected = p; menuExpanded = false })
                    }
                }
            }
            Button(
                onClick = {
                    busy = true; savedMsg = null
                    scope.launch {
                        val c = client ?: return@launch
                        when (val r = c.capture(selected, ProtocolClient.DEFAULT_QUALITY)) {
                            is ProtocolClient.Result.Ok -> {
                                imageBytes = r.image
                                status = "截图成功 ${r.image?.size?.div(1024)} KB"
                            }
                            is ProtocolClient.Result.Err -> status = "截图失败：${r.msg}"
                        }
                        busy = false
                    }
                },
                enabled = connected && !busy,
                modifier = Modifier.weight(1.2f)
            ) { Text(if (busy) "截图传输中…" else "📸 截图") }
        }

        // ---- AI 按钮（截图键下方）：提取图片文字并询问 AI ----
        Button(
            onClick = {
                val bytes = imageBytes
                if (bytes == null) { status = "请先截图，再询问 AI"; return@Button }
                vm.askAi(aiConfig, bytes, aiQuestion)
            },
            enabled = connected && !vm.aiBusy.value,
            modifier = Modifier.fillMaxWidth(),
        ) { Text(if (vm.aiBusy.value) "AI 思考中…" else "🤖 提取图片文字，并询问 AI") }

        // ---- 对话栏（AI 按钮下方） ----
        if (vm.conversation.isNotEmpty() || vm.aiBusy.value) {
            LazyColumn(
                modifier = Modifier.height(220.dp).fillMaxWidth(),
                verticalArrangement = Arrangement.spacedBy(4.dp),
            ) {
                items(vm.conversation.size) { i -> ChatBubble(vm.conversation[i]) }
                if (vm.aiBusy.value) {
                    item { Text("…", style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant) }
                }
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            OutlinedTextField(
                value = aiQuestion, onValueChange = { aiQuestion = it },
                label = { Text("追问（可空）") },
                singleLine = true, modifier = Modifier.weight(1f),
            )
            Button(
                onClick = {
                    val bytes = imageBytes
                    if (bytes == null) { status = "请先截图"; return@Button }
                    vm.askAi(aiConfig, bytes, aiQuestion)
                },
                enabled = connected && !vm.aiBusy.value,
            ) { Text("发送") }
        }

        // 截图预览
        imageBytes?.let { bytes ->
            val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size)
            if (bmp != null) {
                Image(bitmap = bmp.asImageBitmap(), contentDescription = "截图", modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = {
                        savedMsg = saveToGallery(context, bytes)
                    }, modifier = Modifier.weight(1f)) { Text("保存到相册") }
                    Button(onClick = { shareImage(context, bytes) }, modifier = Modifier.weight(1f)) { Text("分享") }
                }
                savedMsg?.let { Text(it, style = MaterialTheme.typography.bodySmall) }
            }
        }
    }
}

/** 对话气泡 */
@Composable
private fun ChatBubble(msg: ChatMessage) {
    val isUser = msg.role == "user"
    Row(
        Modifier.fillMaxWidth(),
        horizontalArrangement = if (isUser) Arrangement.End else Arrangement.Start,
    ) {
        Surface(
            shape = MaterialTheme.shapes.medium,
            color = if (isUser) MaterialTheme.colorScheme.primaryContainer else MaterialTheme.colorScheme.surfaceVariant,
            modifier = Modifier.fillMaxWidth(0.85f).padding(vertical = 4.dp),
        ) {
            Text(msg.text, Modifier.padding(10.dp), style = MaterialTheme.typography.bodyMedium)
        }
    }
}

/** 保存到系统相册（API 29+ 走 MediaStore 免权限；26-28 用 insertImage(Bitmap)） */
fun saveToGallery(context: Context, bytes: ByteArray): String {
    return try {
        val name = "ScreenLink_${System.currentTimeMillis()}.jpg"
        if (android.os.Build.VERSION.SDK_INT >= 29) {
            val values = android.content.ContentValues().apply {
                put(MediaStore.Images.Media.DISPLAY_NAME, name)
                put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg")
                put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/ScreenLink")
            }
            val uri = context.contentResolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)
                ?: return "保存失败"
            context.contentResolver.openOutputStream(uri)?.use { it.write(bytes) } ?: return "保存失败"
        } else {
            val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size) ?: return "保存失败"
            @Suppress("DEPRECATION")
            MediaStore.Images.Media.insertImage(context.contentResolver, bmp, name, "ScreenLink")
                ?: return "保存失败"
        }
        "已保存到相册"
    } catch (e: Exception) {
        "保存失败：${e.message}"
    }
}

/** 系统分享截图 */
fun shareImage(context: Context, bytes: ByteArray) {
    val name = "ScreenLink_${System.currentTimeMillis()}.jpg"
    val uri: android.net.Uri? = try {
        if (android.os.Build.VERSION.SDK_INT >= 29) {
            val values = android.content.ContentValues().apply {
                put(MediaStore.Images.Media.DISPLAY_NAME, name)
                put(MediaStore.Images.Media.MIME_TYPE, "image/jpeg")
                put(MediaStore.Images.Media.RELATIVE_PATH, "Pictures/ScreenLink")
            }
            context.contentResolver.insert(MediaStore.Images.Media.EXTERNAL_CONTENT_URI, values)?.also { u ->
                context.contentResolver.openOutputStream(u)?.use { it.write(bytes) }
            }
        } else {
            val bmp = BitmapFactory.decodeByteArray(bytes, 0, bytes.size) ?: return
            @Suppress("DEPRECATION")
            val url = MediaStore.Images.Media.insertImage(context.contentResolver, bmp, name, "ScreenLink")
            url?.let { android.net.Uri.parse(it) }
        }
    } catch (_: Exception) { null }
    if (uri != null) {
        val intent = Intent(Intent.ACTION_SEND).apply {
            type = "image/jpeg"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        context.startActivity(Intent.createChooser(intent, "分享截图"))
    }
}
