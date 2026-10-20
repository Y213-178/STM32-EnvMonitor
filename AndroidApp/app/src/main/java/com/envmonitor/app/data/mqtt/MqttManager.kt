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
import org.json.JSONArray
import org.json.JSONObject
import java.io.BufferedReader
import java.io.InputStreamReader
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

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
        /* 鉴权必须用【本设备】的 Token（App 自己的设备 APP_DEVICE_ID）：
         * OneNET 设备级 Token 的 res 绑定了设备，用数据上报设备的 Token
         * 去读 App 设备的数据会被判为未授权。
         * 原来这里直接写在函数里，现在统一引用常量，避免和 Constants 里不一致。 */
        val token = Constants.APP_DEVICE_KEY

        // 尝试：Studio API → 旧版 API → IP 直连
        val attempts = listOf(
            // 1. Studio 最新查询 API (GET)
            "https://iot-api.heclouds.com/thingmodel/query-device-property?product_id=${Constants.PRODUCT_ID}&device_name=${Constants.DEVICE_ID}" to { conn: HttpURLConnection ->
                conn.setRequestProperty("Authorization", token)
            },
            // 2. 旧版 HTTPS API
            "https://api.heclouds.com/devices/${Constants.DEVICE_ID}/datapoints?datastreams=${Constants.STREAM_TEMPERATURE},${Constants.STREAM_HUMIDITY},${Constants.STREAM_LIGHT},${Constants.STREAM_PM25},${Constants.STREAM_BATTERY}&limit=1" to { conn: HttpURLConnection ->
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

        var t = 0f; var h = 0f; var lgt = 0f; var pm = 0; var bat = 0
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
                // 电池电量百分比；用 toFloat 再取整，兼容云端返回 "34" 或 "34.0"
                "battery", "bat"      -> bat = (raw.toFloatOrNull() ?: 0f).toInt()
            }
        }
        return SensorData(
            temperature = t,
            humidity = h,
            lightIntensity = lgt,
            pm25 = pm,
            battery = bat,
            timestamp = System.currentTimeMillis()
        )
    }

    // ===================== 回补式同步：按时间区间拉历史 =====================

    /**
     * 合并用的时间桶：60 秒。
     *
     * ⚠️ 关键：OneNET 给每个属性单独记录"接收时间"，同一帧数据里
     *    temperature / humidity / light / pm25 / battery 的 time 会相差 10~20ms
     *    （实测 battery=1792395325450、humidity=1792395325432、light=1792395325438）。
     *    如果按精确时间戳合并，5 个属性会变成 5 条"只有一项有值"的残缺记录。
     *    所以按分钟做桶再合并。
     */
    private val BUCKET_MS = 60_000L

    /**
     * 从云端拉取 [startMs, endMs] 区间内的历史数据，用于回补本地缓存。
     *
     * 接口约束（2026-10-19 实测结论）：
     *   端点：GET https://iot-api.heclouds.com/thingmodel/query-device-property-history
     *   ✅ 必须带 identifier（否则 code=10001 "identifier cannot be empty"）-> 循环 5 个属性
     *   ✅ 必须带 start_time / end_time（毫秒时间戳）
     *   ✅ limit 上限在 1000 左右（6000 会被拒 code=10418 "limit超过最大限制"）
     *   ✅ offset 翻页有效
     *   ✅ 返回结构：{"code":0,"data":{"list":[{"time":..,"value":".."}, ...]},"msg":"succ"}
     *   ❌ 旧版 api.heclouds.com/devices/{id}/datapoints 已下线（nginx 404）
     *
     * @return 按时间升序；null 表示 5 个属性全部失败（保留 lastSync 下次重试）
     */
    fun queryHistory(startMs: Long, endMs: Long): List<SensorData>? {
        if (endMs <= startMs) return emptyList()

        val ids = listOf(
            Constants.STREAM_TEMPERATURE, Constants.STREAM_HUMIDITY,
            Constants.STREAM_LIGHT, Constants.STREAM_PM25, Constants.STREAM_BATTERY
        )

        // 分钟桶 -> { 属性 -> (原始时间, 原始值) }
        val buckets = sortedMapOf<Long, MutableMap<String, Pair<Long, String>>>()
        var okCount = 0

        for (id in ids) {
            val pts = fetchOneProperty(id, startMs, endMs)
            if (pts == null) {
                Log.w(TAG, "history[$id] 请求失败，跳过")
                continue
            }
            okCount++
            Log.d(TAG, "history[$id] 拿到 ${pts.size} 个点")

            // 升序遍历，保证同一桶内"后写覆盖"留下的是最新值
            for ((ts, v) in pts.sortedBy { it.first }) {
                val bucket = ts / BUCKET_MS * BUCKET_MS
                buckets.getOrPut(bucket) { mutableMapOf() }[id] = ts to v
            }
        }

        if (okCount == 0) {
            Log.w(TAG, "history: 5 个属性全部失败")
            return null
        }
        if (buckets.isEmpty()) {
            Log.d(TAG, "history: 区间内无数据")
            return emptyList()
        }

        val result = buckets.map { (bucket, m) ->
            SensorData(
                temperature    = m[Constants.STREAM_TEMPERATURE]?.second?.toFloatOrNull() ?: 0f,
                humidity       = m[Constants.STREAM_HUMIDITY]?.second?.toFloatOrNull() ?: 0f,
                lightIntensity = m[Constants.STREAM_LIGHT]?.second?.toFloatOrNull() ?: 0f,
                pm25           = m[Constants.STREAM_PM25]?.second?.toFloatOrNull()?.toInt() ?: 0,
                battery        = m[Constants.STREAM_BATTERY]?.second?.toFloatOrNull()?.toInt() ?: 0,
                timestamp      = bucket
            )
        }
        Log.d(TAG, "history: 合并成 ${result.size} 条记录")
        return result
    }

    /**
     * 拉取【单个属性】的历史点，自动 offset 翻页。
     * @return (时间戳 to 原始值)；null 表示请求失败
     */
    private fun fetchOneProperty(
        id: String,
        startMs: Long,
        endMs: Long
    ): List<Pair<Long, String>>? {

        var limit = Constants.SYNC_HISTORY_LIMIT
        var offset = 0
        val out = mutableListOf<Pair<Long, String>>()
        var prevFirstTs = -1L

        while (true) {
            val url = "https://iot-api.heclouds.com/thingmodel/query-device-property-history" +
                    "?product_id=${Constants.PRODUCT_ID}" +
                    "&device_name=${Constants.DEVICE_ID}" +
                    "&identifier=$id" +
                    "&start_time=$startMs&end_time=$endMs" +
                    "&limit=$limit&offset=$offset"

            val body = httpGet(url) ?: return if (out.isEmpty()) null else out

            val root = try {
                JSONObject(body)
            } catch (e: Exception) {
                Log.w(TAG, "history[$id] 响应不是 JSON")
                return if (out.isEmpty()) null else out
            }

            val code = root.optInt("code", -1)

            // limit 超上限 -> 减半重试（offset 不推进）
            if (code == 10418 && limit > 50) {
                limit /= 2
                Log.d(TAG, "history[$id] limit 超限，降到 $limit 重试")
                continue
            }
            if (code != 0) {
                Log.w(TAG, "history[$id] code=$code msg=${root.optString("msg")}")
                return if (out.isEmpty()) null else out
            }

            val page = parsePropertyHistory(root)
            if (page.isEmpty()) {
                Log.d(TAG, "history[$id] offset=$offset 返回 0 条，结束翻页")
                break
            }

            // 翻页保护：万一服务端忽略 offset，会出现重复页
            val firstTs = page.minOf { it.first }
            if (firstTs == prevFirstTs) {
                Log.w(TAG, "history[$id] 翻页出现重复页，停止")
                break
            }
            prevFirstTs = firstTs

            out += page
            if (page.size < limit) break          // 已是最后一页
            offset += limit
            if (offset > 20000) {
                Log.w(TAG, "history[$id] offset 触顶，停止")
                break
            }
        }
        return out
    }

    /**
     * 解析单属性历史响应。
     * 实测结构：{"code":0,"data":{"list":[{"time":1792395325426,"value":"23.3"}, ...]}}
     * 同时兼容 data 直接是数组、或键名叫 datapoints 的情况。
     */
    private fun parsePropertyHistory(root: JSONObject): List<Pair<Long, String>> {
        // 实测结构：{"data":{"list":[...]}}；同时兼容 data 直接是数组 / 键名 datapoints
        val arr = when (val d = root.opt("data")) {
            is JSONArray -> d
            is JSONObject -> d.optJSONArray("list") ?: d.optJSONArray("datapoints")
            else -> null
        } ?: return emptyList()

        val out = mutableListOf<Pair<Long, String>>()
        for (i in 0 until arr.length()) {
            val o = arr.optJSONObject(i) ?: continue
            val ts = optTime(o) ?: continue
            out += ts to o.optString("value", "")
        }
        return out
    }

    /** 统一 GET（Studio 鉴权用 Authorization 头） */
    private fun httpGet(url: String): String? {
        return try {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.connectTimeout = 12000
            conn.readTimeout = 12000
            conn.setRequestProperty("Authorization", Constants.APP_DEVICE_KEY)

            val code = conn.responseCode
            val body = (if (code in 200..299) conn.inputStream else conn.errorStream)
                ?.use { BufferedReader(InputStreamReader(it)).readText() } ?: ""
            conn.disconnect()

            Log.d(TAG, "GET $code ${url.substringAfter("identifier=").take(70)}")
            if (code in 200..299) body else null
        } catch (e: Exception) {
            Log.w(TAG, "GET 异常: ${e.message}")
            null
        }
    }

    /** 兼容 time(毫秒数字) / timestamp(毫秒) / at(字符串) 三种时间字段 */
    private fun optTime(o: JSONObject): Long? {
        for (key in listOf("time", "timestamp", "ts")) {
            if (o.has(key)) {
                val v = o.optLong(key, 0L)
                if (v > 0L) return v
            }
        }
        val at = o.optString("at", "")
        if (at.isNotEmpty()) {
            return try {
                SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.getDefault())
                    .parse(at.substringBefore('.'))?.time
            } catch (e: Exception) {
                null
            }
        }
        return null
    }

    fun disconnect() {
        pollingJob?.cancel(); pollingJob = null
        _state.trySend(ConnectionState.DISCONNECTED)
    }

    companion object { private const val TAG = "MqttManager" }
}
