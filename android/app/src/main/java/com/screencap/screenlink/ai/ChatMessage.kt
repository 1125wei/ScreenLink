package com.screencap.screenlink.ai

/** 对话消息（多轮会话） */
data class ChatMessage(
    val role: String,   // "system" | "user" | "assistant"
    val text: String,
)
