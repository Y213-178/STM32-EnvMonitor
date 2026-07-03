package com.envmonitor.app.ui.dashboard

import android.app.Application
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.envmonitor.app.data.local.AppDatabase
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.data.mqtt.MqttManager
import com.envmonitor.app.data.repository.SensorRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

data class DashboardState(
    val data: SensorData = SensorData(),
    val connectionState: MqttManager.ConnectionState = MqttManager.ConnectionState.DISCONNECTED,
    val lastUpdateTime: Long = 0L
)

class DashboardViewModel(application: Application) : AndroidViewModel(application) {

    private val dao = AppDatabase.getInstance(application).sensorDao()
    private val repository = SensorRepository(dao)
    private val mqttManager = MqttManager()

    private val _state = MutableStateFlow(DashboardState())
    val state: StateFlow<DashboardState> = _state.asStateFlow()

    init {
        viewModelScope.launch {
            repository.loadLatest()
            _state.value = _state.value.copy(data = repository.latest.value)
        }

        // Observe MQTT data
        viewModelScope.launch {
            mqttManager.sensorDataFlow.collect { data ->
                repository.save(data)
                _state.value = _state.value.copy(
                    data = data,
                    lastUpdateTime = System.currentTimeMillis()
                )
            }
        }

        // Observe connection state
        viewModelScope.launch {
            mqttManager.connectionStateFlow.collect { st ->
                _state.value = _state.value.copy(connectionState = st)
            }
        }

        mqttManager.connect(viewModelScope)
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
}
