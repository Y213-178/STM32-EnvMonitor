package com.envmonitor.app.data.mqtt

import android.util.Log
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.util.Constants
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.receiveAsFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL

/**
 * OneNet API 数据查询
 *
 * 用 MQTT 同一服务器的 HTTP API 查询设备最新数据。
 * 不依赖 MQTT 订阅，每 10 秒轮询一次。
 */
class MqttManager {

    private var pollingJob: Job? = null

    private val _data = Channel<SensorData>(Channel.CONFLATED)
    val sensorDataFlow: Flow<SensorData> = _data.receiveAsFlow()

    private val _state = Channel<ConnectionState>(Channel.CONFLATED)
    val connectionStateFlow: Flow<ConnectionState> = _state.receiveAsFlow()

    enum class ConnectionState { CONNECTING, CONNECTED, DISCONNECTED, ERROR }

    fun connect(scope: CoroutineScope?) {
        pollingJob?.cancel()
        val s = scope ?: CoroutineScope(Dispatchers.IO)

        pollingJob = s.launch(Dispatchers.IO) {
            var errors = 0

            while (isActive) {
                _state.trySend(ConnectionState.CONNECTING)

                try {
                    val data = query()
                    if (data != null) {
                        _data.trySend(data)
                        _state.trySend(ConnectionState.CONNECTED)
                        errors = 0
                    } else {
                        errors++
                        if (errors >= 2) _state.trySend(ConnectionState.ERROR)
                    }
                } catch (e: Exception) {
                    errors++
                    Log.e(TAG, "Error: ${e.message}")
                    if (errors >= 2) _state.trySend(ConnectionState.ERROR)
                }

                for (i in 0 until (Constants.POLL_INTERVAL_SECONDS * 10).toInt()) { if (!isActive) break; delay(100) }
            }
        }
    }

    /**
     * 调用 OneNet HTTP API 查询设备最新数据
     *
     * 使用官方 Token 认证
     * 尝试多个 IP 和端口组合以防网络限制
     */
    private fun query(): SensorData? {
        val token = Constants.ACCESS_KEY

        // 尝试：Studio API → 旧版 API → IP 直连
        val attempts = listOf(
            // 1. Studio 最新查询 API (GET)
            "https://iot-api.heclouds.com/thingmodel/query-device-property?product_id=${Constants.PRODUCT_ID}&device_name=${Constants.DEVICE_ID}" to { conn: HttpURLConnection ->
                conn.setRequestProperty("Authorization", token)
            },
            // 2. 旧版 HTTPS APIssssxaaaasssssxddddddddddddddddxssaaaaa
            "https://api.heclouds.com/devices/${Constants.DEVICE_ID}/datapoints?datastreams=${Constants.STREAM_TEMPERATURE},${Constants.STREAM_HUMIDITY},${Constants.STREAM_LIGHT},${Constants.STREAM_PM25}&limit=1" to { conn: HttpURLConnection ->
                conn.setRequestProperty("api-key", token)
            }
        )

        for ((url, setup) in attempts) {
            try {
                Log.d(TAG, "Trying $url")
                val conn = URL(url).openConnection() as HttpURLConnection
                conn.connectTimeout = 8000
                conn.readTimeout = 8000
                setup(conn)

                val code = conn.responseCode
                if (code in 200..299) {
                    val body = BufferedReader(InputStreamReader(conn.inputStream)).readText()
                    conn.disconnect()
                    Log.d(TAG, "OK: ${body.take(300)}")

                    val data = parseResponse(body)
                    if (data != null) return data
                } else {
                    val err = conn.errorStream?.let { BufferedReader(InputStreamReader(it)).readText() }
                    Log.d(TAG, "HTTP $code: ${err?.take(200)}")
                    conn.disconnect()
                }
            } catch (e: Exception) {
                Log.d(TAG, "Fail: ${e.message}")
            }
        }

        return null
    }

    private fun parseResponse(body: String): SensorData? {
        val root = JSONObject(body)
        val code = root.optInt("code", -1)
        if (code != 0) { Log.w(TAG, "code=$code"); return null }

        // data 是直接返回的 JSON 数组，不是 data.list
        val arr = root.optJSONArray("data") ?: return null

        var t = 0f; var h = 0f; var lgt = 0f; var pm = 0
        for (i in 0 until arr.length()) {
            val item = arr.getJSONObject(i)
            val id = item.optString("identifier", "")

            // value 是 String 类型，需要转换
            val raw = item.optString("value", "0")

            when (id) {
                "temperature", "temp" -> t = raw.toFloatOrNull() ?: 0f
                "humidity", "humi"    -> h = raw.toFloatOrNull() ?: 0f
                "light", "lux"        -> lgt = raw.toFloatOrNull() ?: 0f
                "pm25", "PM2.5"       -> pm = raw.toIntOrNull() ?: 0
            }
        }
        return SensorData(temperature = t, humidity = h, lightIntensity = lgt, pm25 = pm, timestamp = System.currentTimeMillis())
    }

    fun disconnect() {
        pollingJob?.cancel(); pollingJob = null
        _state.trySend(ConnectionState.DISCONNECTED)
    }

    companion object { private const val TAG = "MqttManager" }
}
