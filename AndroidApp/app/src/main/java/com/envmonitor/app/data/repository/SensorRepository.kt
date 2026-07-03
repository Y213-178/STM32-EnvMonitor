package com.envmonitor.app.data.repository

import com.envmonitor.app.data.local.SensorDao
import com.envmonitor.app.data.model.HistoryPoint
import com.envmonitor.app.data.model.SensorData
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class SensorRepository(private val dao: SensorDao) {

    private val dateFormat = SimpleDateFormat("HH:mm", Locale.getDefault())
    private val _latest = MutableStateFlow(SensorData())
    val latest: StateFlow<SensorData> = _latest.asStateFlow()

    suspend fun loadLatest() {
        dao.getLatest()?.let { _latest.value = it }
    }

    suspend fun save(data: SensorData) {
        dao.insert(data)
        _latest.value = data
        val cutoff = System.currentTimeMillis() - 7 * 24 * 60 * 60 * 1000L
        dao.deleteOlderThan(cutoff)
    }

    suspend fun getHistory(
        dataStreamId: String,
        startTime: Long,
        endTime: Long
    ): List<HistoryPoint> {
        val list = dao.getRange(startTime, endTime)
        return list.map { data ->
            val value = when (dataStreamId) {
                "temperature" -> data.temperature
                "humidity" -> data.humidity
                "light" -> data.lightIntensity
                "pm25" -> data.pm25.toFloat()
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
}
