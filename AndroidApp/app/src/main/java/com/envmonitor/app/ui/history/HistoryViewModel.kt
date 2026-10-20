package com.envmonitor.app.ui.history

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.envmonitor.app.data.local.AppDatabase
import com.envmonitor.app.data.model.HistoryPoint
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.data.repository.SensorRepository
import com.envmonitor.app.util.Constants
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

data class HistoryState(
    val selectedStream: String = Constants.STREAM_TEMPERATURE,
    val points: List<HistoryPoint> = emptyList(),
    val allPoints: List<SensorData> = emptyList(),
    val isLoading: Boolean = false,
    val startTime: Long = 0L,
    val endTime: Long = System.currentTimeMillis(),
    val timeRangeOption: String = "24小时"
)

class HistoryViewModel(application: Application) : AndroidViewModel(application) {

    private val dao = AppDatabase.getInstance(application).sensorDao()
    private val repository = SensorRepository(dao)

    private val _state = MutableStateFlow(HistoryState())
    val state: StateFlow<HistoryState> = _state.asStateFlow()

    /** 当前正在跑的观察协程；切换数据流/时间范围时先取消再重启，避免重复写入状态 */
    private var historyJob: Job? = null

    val streamOptions = listOf(
        Constants.STREAM_TEMPERATURE to "温度",
        Constants.STREAM_HUMIDITY to "湿度",
        Constants.STREAM_LIGHT to "光照强度",
        Constants.STREAM_PM25 to "PM2.5",
        Constants.STREAM_BATTERY to "电量"
    )

    // 快捷时间范围选项
    val timeRangeOptions = listOf(
        "1小时" to 3600_000L,
        "6小时" to 6 * 3600_000L,
        "12小时" to 12 * 3600_000L,
        "24小时" to 24 * 3600_000L,
        "7天" to 7 * 24 * 3600_000L,
        "30天" to 30 * 24 * 3600_000L
    )

    // 自动删除选项
    val deleteOptions = listOf(
        "删除>1小时" to 3600_000L,
        "删除>12小时" to 12 * 3600_000L,
        "删除>24小时" to 24 * 3600_000L,
        "删除>7天" to 7 * 24 * 3600_000L,
        "删除>30天" to 30 * 24 * 3600_000L
    )

    init {
        val now = System.currentTimeMillis()
        _state.value = _state.value.copy(
            startTime = now - 24 * 3600_000L,
            endTime = now
        )
        loadHistory()
    }

    fun selectStream(streamId: String) {
        _state.value = _state.value.copy(selectedStream = streamId)
        loadHistory()
    }

    /** 选择快捷时间范围 */
    fun selectTimeRange(label: String, durationMs: Long) {
        val now = System.currentTimeMillis()
        _state.value = _state.value.copy(
            startTime = now - durationMs,
            endTime = now,
            timeRangeOption = label
        )
        loadHistory()
    }

    /** 删除指定时间之前的数据 */
    fun deleteOlderThan(durationMs: Long, label: String) {
        val cutoff = System.currentTimeMillis() - durationMs
        viewModelScope.launch {
            repository.deleteOlderThan(cutoff)
            // 刷新
            loadHistory()
        }
    }

    /** 删除全部历史数据 */
    fun deleteAll() {
        viewModelScope.launch {
            repository.deleteAll()
            loadHistory()
        }
    }

    fun loadHistory() {
        // 取消上一次观察（切换数据流/时间范围后，避免多个协程同时写状态）
        historyJob?.cancel()
        historyJob = viewModelScope.launch {
            _state.value = _state.value.copy(isLoading = true)
            // 持续观察：数据表任何变化（设备上报新记录、删除操作）都会自动重新查询
            repository.observeHistoryRange(
                _state.value.startTime,
                _state.value.endTime
            ).collect { list ->
                _state.value = _state.value.copy(
                    points = repository.toHistoryPoints(_state.value.selectedStream, list),
                    allPoints = list,
                    isLoading = false
                )
            }
        }
    }
}
