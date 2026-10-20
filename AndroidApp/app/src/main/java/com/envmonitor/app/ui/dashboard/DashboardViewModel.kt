package com.envmonitor.app.ui.dashboard

import android.app.Application
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.envmonitor.app.data.local.AppDatabase
import com.envmonitor.app.data.local.SyncPrefs
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.data.mqtt.MqttManager
import com.envmonitor.app.data.repository.SensorRepository
import com.envmonitor.app.util.Constants
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

data class DashboardState(
    val data: SensorData = SensorData(),
    val connectionState: MqttManager.ConnectionState = MqttManager.ConnectionState.DISCONNECTED,
    val lastUpdateTime: Long = 0L,
    /** 本次启动是否正在回补历史（可用于界面显示"同步中…"） */
    val isBackfilling: Boolean = false
)

class DashboardViewModel(application: Application) : AndroidViewModel(application) {

    private val dao = AppDatabase.getInstance(application).sensorDao()
    private val repository = SensorRepository(dao)
    private val mqttManager = MqttManager()

    private val _state = MutableStateFlow(DashboardState())
    val state: StateFlow<DashboardState> = _state.asStateFlow()

    init {
        // ── ① 先挂上两个观察者，保证 connect() 之后的事件不会漏 ──
        viewModelScope.launch {
            mqttManager.sensorDataFlow.collect { data ->
                val isNew = repository.save(data)
                if (isNew) {
                    _state.value = _state.value.copy(
                        data = data,
                        lastUpdateTime = System.currentTimeMillis()
                    )
                }
                // isNew == false：同一帧旧数据，什么都不做
            }
        }

        viewModelScope.launch {
            mqttManager.connectionStateFlow.collect { st ->
                _state.value = _state.value.copy(connectionState = st)
            }
        }

        // ── ② 回补历史 → 读本地 → 再开始轮询 ──
        viewModelScope.launch {
            _state.value = _state.value.copy(isBackfilling = true)
            backfillFromCloud()

            repository.loadLatest()
            val latest = repository.latest.value
            _state.value = _state.value.copy(
                data = latest,
                // 有历史记录就用它的时间当"上次更新"，避免顶部一直显示"等待数据…"
                lastUpdateTime = if (latest.id != 0L) latest.timestamp else 0L,
                isBackfilling = false
            )

            // 回补完成后再开轮询（轮询只负责"当前值"）
            mqttManager.connect(viewModelScope)
        }
    }

    /**
     * 回补式同步：把「上次同步时间 → 现在」这一段云端历史补进本地。
     *
     * 背景：
     *   设备端（STM32 + ESP8266）是独立上报的，App 关着的时候数据照样进 OneNET。
     *   但 App 关闭期间本地 Room 不会更新，所以下次打开必须"补课"，
     *   否则历史页只能看到「打开 App 的那几分钟」。
     *
     * 边界处理：
     *   - 从没同步过 → 往回补 SYNC_INITIAL_LOOKBACK_MS（默认 24 小时）
     *   - 距上次同步太近 → 跳过（避免每次切页都发请求）
     *   - 起点早于 7 天前 → 截断到 7 天前（OneNET 只保留 7 天）
     *   - 请求失败 → 不推进 lastSync，下次启动重试同一区间
     */
    private suspend fun backfillFromCloud() {
        val ctx = getApplication<Application>()
        val now = System.currentTimeMillis()

        val lastSync = SyncPrefs.getLastSyncMs(ctx)
        val localEmpty = repository.isEmpty()

        // 本地是空的（首次安装 / 清过数据）-> 直接补满 7 天，不受 lastSync 限制；
        // 否则从上次同步点开始补。
        val rawStart = when {
            localEmpty -> now - Constants.SYNC_INITIAL_LOOKBACK_MS
            lastSync > 0L -> lastSync
            else -> now - Constants.SYNC_INITIAL_LOOKBACK_MS
        }

        // 间隔太短就不补（比如刚同步完又切回首页）
        if (!localEmpty && now - rawStart < Constants.SYNC_MIN_INTERVAL_MS) {
            Log.d(TAG, "backfill skipped: only ${now - rawStart} ms since last sync")
            return
        }

        // 云端只保留 RETENTION_DAYS 天，起点更早就截断
        val earliest = now - Constants.RETENTION_DAYS * 24L * 60 * 60 * 1000
        val from = maxOf(rawStart, earliest)

        Log.d(TAG, "backfill ${(now - from) / 3600_000L}h: $from -> $now (localEmpty=$localEmpty)")

        val missing = withContext(Dispatchers.IO) {
            mqttManager.queryHistory(from, now)
        }

        if (missing == null) {
            // 全部属性都失败：不推进 lastSync，下次启动重试同一区间
            Log.w(TAG, "backfill failed, keep lastSync for retry")
            return
        }

        repository.backfill(missing)
        SyncPrefs.setLastSyncMs(ctx, now)
        Log.d(TAG, "backfill done: ${missing.size} records")
    }

    fun refresh() {
        viewModelScope.launch {
            repository.loadLatest()
            _state.value = _state.value.copy(data = repository.latest.value)
        }
    }

    override fun onCleared() {
        super.onCleared()
        mqttManager.disconnect()
    }

    private companion object {
        const val TAG = "DashboardVM"
    }
}
