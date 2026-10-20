package com.envmonitor.app.data.repository

import android.util.Log
import com.envmonitor.app.data.local.SensorDao
import com.envmonitor.app.data.model.HistoryPoint
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.util.Constants
import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class SensorRepository(private val dao: SensorDao) {

    /* 曲线图 X 轴标签用的时间格式：带日期。
     * 查 7 天时只有 HH:mm 会分不清是哪一天。 */
    private val dateFormat = SimpleDateFormat("MM-dd HH:mm", Locale.getDefault())
    private val _latest = MutableStateFlow(SensorData())
    val latest: StateFlow<SensorData> = _latest.asStateFlow()

    suspend fun loadLatest() {
        dao.getLatest()?.let { _latest.value = it }
    }

    /**
     * 本地是否一条记录都没有。
     * 用于「首次安装 / 清过数据」时把回补窗口拉满 7 天。
     */
    suspend fun isEmpty(): Boolean = dao.getLatest() == null

    /**
     * 保存一帧数据，并返回它是否为【设备新上报的一帧】。
     *
     * 背景：App 每 POLL_INTERVAL_SECONDS 拉一次 OneNET 的"最新数据"接口。
     * 设备是 30 分钟才上报一次，所以大部分轮询拉回来的都是同一帧旧数据。
     * 如果无脑 insert，就会出现"每次大退重开 App 都多一条重复记录"。
     *
     * 策略：与库中最新一条做【测量值全等】比较
     *   - 完全相同  -> 同一帧旧数据，直接 return false（不写库、不更新界面、不推进时间戳）
     *   - 有一项不同 -> 设备上报了新数据，写入并 return true
     *
     * @return true = 这是新数据（已写入）；false = 重复帧（已忽略）
     */
    suspend fun save(data: SensorData): Boolean {
        // 只与【最新一条】比较：同一次上报的四个值必然一致
        val last = dao.getLatest()
        if (last != null &&
            last.temperature == data.temperature &&
            last.humidity == data.humidity &&
            last.lightIntensity == data.lightIntensity &&
            last.pm25 == data.pm25 &&
            last.battery == data.battery
        ) {
            // 同一帧旧数据，忽略
            return false
        }

        dao.insert(data)
        _latest.value = data
        val cutoff = System.currentTimeMillis() - Constants.RETENTION_DAYS * 24L * 60 * 60 * 1000
        dao.deleteOlderThan(cutoff)
        return true
    }

    /**
     * 把从云端回补拉到的历史批量写入本地。
     *
     * 与 save() 的区别：
     *   - save() 是"实时帧"，会和最新一条做去重，只留一帧；
     *   - backfill() 是"补历史"，一次几十上百条，按 timestamp 去重。
     *
     * 注意：不更新 _latest。
     *   回补的是过去的数据，界面上的"当前值"不该被旧数据顶掉。
     */
    suspend fun backfill(list: List<SensorData>) {
        if (list.isEmpty()) return

        // 先查出这个时间区间内本地已有的时间点，避免重复插入
        val existing = dao.getRange(list.first().timestamp, list.last().timestamp)
            .map { it.timestamp }
            .toSet()

        var inserted = 0
        for (item in list) {
            if (item.timestamp !in existing) {
                dao.insert(item)
                inserted++
            }
        }

        // 本地只保留 RETENTION_DAYS 天（与 OneNET 的 7 天保持一致）
        val cutoff = System.currentTimeMillis() - Constants.RETENTION_DAYS * 24L * 60 * 60 * 1000
        dao.deleteOlderThan(cutoff)

        Log.d(TAG, "backfill: 收到 ${list.size} 条，新增 $inserted 条")
    }

    /**
     * 可观察的历史数据：数据表一变化就自动重新查询。
     * 历史页 collect 它，设备上报新数据后列表会自动多出一条，无需切页或手动刷新。
     */
    fun observeHistoryRange(startTime: Long, endTime: Long): Flow<List<SensorData>> {
        return dao.getRangeFlow(startTime, endTime)
    }

    /**
     * 把原始记录映射成图表用的数据点（纯函数，供 ViewModel 在 collect 里调用）。
     */
    fun toHistoryPoints(dataStreamId: String, list: List<SensorData>): List<HistoryPoint> {
        return list.map { data ->
            val value = when (dataStreamId) {
                "temperature" -> data.temperature
                "humidity" -> data.humidity
                "light" -> data.lightIntensity
                "pm25" -> data.pm25.toFloat()
                "battery" -> data.battery.toFloat()
                else -> 0f
            }
            HistoryPoint(
                timestamp = dateFormat.format(Date(data.timestamp)),
                value = value
            )
        }
    }

    suspend fun getAllInRange(start: Long, end: Long): List<SensorData> {
        return dao.getRange(start, end)
    }

    suspend fun deleteOlderThan(cutoff: Long) {
        dao.deleteOlderThan(cutoff)
    }

    suspend fun deleteAll() {
        dao.deleteAll()
    }

    fun getDefaultTimeRange(): Pair<Long, Long> {
        val now = System.currentTimeMillis()
        val start = now - 24 * 60 * 60 * 1000L
        return Pair(start, now)
    }

    private companion object {
        const val TAG = "SensorRepository"
    }
}
