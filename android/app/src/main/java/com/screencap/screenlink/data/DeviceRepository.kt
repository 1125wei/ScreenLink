package com.screencap.screenlink.data

import android.content.Context
import android.content.SharedPreferences
import org.json.JSONArray
import org.json.JSONObject

/** 已配对设备信息（本地持久化，SharedPreferences JSON） */
data class DeviceInfo(
    val id: String,          // 本机 device_id（UUID）
    val name: String,        // 电脑端服务器名
    val host: String,        // 电脑 IP
    val port: Int,           // 监听端口
    val fingerprint: String, // 电脑端证书 SHA-256 指纹（证书固定）
)

object DeviceRepository {
    private const val PREFS = "screenlink_devices"
    private const val KEY_DEVICES = "devices"

    fun save(context: Context, device: DeviceInfo) {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val list = load(context).filterNot { it.host == device.host && it.port == device.port }.toMutableList()
        list.add(device)
        prefs.edit().putString(KEY_DEVICES, toJson(list)).apply()
    }

    fun load(context: Context): List<DeviceInfo> {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val raw = prefs.getString(KEY_DEVICES, null) ?: return emptyList()
        return try {
            val arr = JSONArray(raw)
            (0 until arr.length()).mapNotNull { i ->
                val o = arr.getJSONObject(i)
                DeviceInfo(
                    id = o.optString("id"),
                    name = o.optString("name"),
                    host = o.optString("host"),
                    port = o.optInt("port", 8848),
                    fingerprint = o.optString("fingerprint"),
                )
            }
        } catch (_: Exception) { emptyList() }
    }

    fun remove(context: Context, host: String, port: Int) {
        val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        val list = load(context).filterNot { it.host == host && it.port == port }
        prefs.edit().putString(KEY_DEVICES, toJson(list)).apply()
    }

    private fun toJson(list: List<DeviceInfo>): String {
        val arr = JSONArray()
        list.forEach { d ->
            arr.put(JSONObject().apply {
                put("id", d.id); put("name", d.name); put("host", d.host)
                put("port", d.port); put("fingerprint", d.fingerprint)
            })
        }
        return arr.toString()
    }
}
