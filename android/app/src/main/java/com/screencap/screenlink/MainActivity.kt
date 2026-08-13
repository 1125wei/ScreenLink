package com.screencap.screenlink

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Home
import androidx.compose.material.icons.filled.Info
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Modifier
import com.screencap.screenlink.data.AiConfig
import com.screencap.screenlink.data.AiConfigRepository
import com.screencap.screenlink.data.DeviceInfo
import com.screencap.screenlink.data.DeviceRepository
import com.screencap.screenlink.ui.AboutScreen
import com.screencap.screenlink.ui.AiSettingsScreen
import com.screencap.screenlink.ui.ConsoleScreen
import com.screencap.screenlink.ui.DeviceListScreen
import com.screencap.screenlink.ui.PairingScreen
import androidx.lifecycle.viewmodel.compose.viewModel

/** 底部导航 Tab */
enum class MainTab(val label: String) {
    Console("控制台"), AiSettings("AI 设置"), About("关于")
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContent { ScreenLinkApp() }
    }
}

@Composable
fun ScreenLinkApp() {
    MaterialTheme {
        val context = androidx.compose.ui.platform.LocalContext.current
        val aiRepo = remember { AiConfigRepository(context) }

        // 导航状态
        var screenTag by rememberSaveable { mutableStateOf("devices") } // pairing | devices | console
        var currentDevice by remember { mutableStateOf<DeviceInfo?>(null) }
        var selectedTab by rememberSaveable { mutableStateOf(MainTab.Console) }

        // AI 配置（跨 Tab 共享，修改即保存）
        var aiConfig by remember { mutableStateOf(aiRepo.load()) }

        // AI 问答 ViewModel（viewModelScope：切 Tab 不取消请求，会话跨 Tab 保留）
        val vm: MainViewModel = viewModel()

        if (screenTag == "pairing") {
            // 配对全屏（无底部导航）
            PairingScreen(
                onPaired = { device ->
                    DeviceRepository.save(context, device)
                    currentDevice = device
                    screenTag = "console"
                },
                onBack = { screenTag = "devices" },
            )
            return@MaterialTheme
        }

        Scaffold(
            bottomBar = {
                NavigationBar {
                    MainTab.entries.forEach { tab ->
                        NavigationBarItem(
                            selected = selectedTab == tab,
                            onClick = { selectedTab = tab },
                            icon = {
                                Icon(
                                    imageVector = when (tab) {
                                        MainTab.Console -> Icons.Filled.Home
                                        MainTab.AiSettings -> Icons.Filled.Settings
                                        MainTab.About -> Icons.Filled.Info
                                    },
                                    contentDescription = tab.label,
                                )
                            },
                            label = { Text(tab.label) },
                        )
                    }
                }
            }
        ) { padding ->
            Box(Modifier.padding(padding)) {
                when (selectedTab) {
                    MainTab.Console -> {
                        val device = currentDevice
                        if (screenTag == "console" && device != null) {
                            ConsoleScreen(
                                device = device,
                                aiConfig = aiConfig,
                                vm = vm,
                                onDisconnect = { screenTag = "devices" },
                            )
                        } else {
                            DeviceListScreen(
                                onConnect = { d ->
                                    currentDevice = d
                                    screenTag = "console"
                                },
                                onAddNew = { screenTag = "pairing" },
                            )
                        }
                    }
                    MainTab.AiSettings -> {
                        AiSettingsScreen(
                            config = aiConfig,
                            onSave = {
                                aiConfig = it
                                aiRepo.save(it)
                            },
                        )
                    }
                    MainTab.About -> {
                        AboutScreen()
                    }
                }
            }
        }
    }
}
