package com.envmonitor.app.ui.dashboard

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.envmonitor.app.data.mqtt.MqttManager
import com.envmonitor.app.ui.components.SensorCard
import com.envmonitor.app.ui.components.SensorType
import com.envmonitor.app.ui.theme.TextSecondary
import com.envmonitor.app.util.Constants
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun DashboardScreen(
    viewModel: DashboardViewModel = viewModel(),
    modifier: Modifier = Modifier
) {
    val state by viewModel.state.collectAsState()

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp)
    ) {
        // Header
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .padding(vertical = 12.dp)
        ) {
            Text(
                text = "环境监测",
                style = MaterialTheme.typography.headlineMedium,
                fontWeight = FontWeight.Bold
            )
            Spacer(modifier = Modifier.height(4.dp))

            // Connection status indicator
            val (statusText, statusColor) = when (state.connectionState) {
                MqttManager.ConnectionState.CONNECTED -> "云平台 已连接" to Color(0xFF4CAF50)
                MqttManager.ConnectionState.CONNECTING -> "云平台 连接中..." to Color(0xFFFFA726)
                MqttManager.ConnectionState.ERROR -> "云平台 连接失败" to Color(0xFFE53935)
                MqttManager.ConnectionState.DISCONNECTED -> "云平台 未连接" to Color(0xFF9E9E9E)
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(8.dp)
                        .background(statusColor, RoundedCornerShape(4.dp))
                )
                Spacer(modifier = Modifier.width(6.dp))
                Text(
                    text = statusText,
                    style = MaterialTheme.typography.labelSmall,
                    color = statusColor
                )
            }
            Spacer(modifier = Modifier.height(2.dp))

            Text(
                /* 设备名取自 Constants，避免在 UI 里硬编码 */
                text = "设备: ${Constants.DEVICE_ID}",
                style = MaterialTheme.typography.bodyMedium,
                color = TextSecondary
            )
            Text(
                /* 带上日期：App 关了一段时间再打开时，最新数据可能来自前一天，
                 * 只显示 HH:mm:ss 会让人误以为是刚才的数据。 */
                text = if (state.lastUpdateTime > 0L)
                    "更新于 ${SimpleDateFormat("MM-dd HH:mm:ss", Locale.getDefault()).format(Date(state.lastUpdateTime))}"
                else "等待数据...",
                style = MaterialTheme.typography.labelSmall,
                color = TextSecondary.copy(alpha = 0.7f)
            )
        }

        Spacer(modifier = Modifier.height(8.dp))

        // Sensor cards
        val data = state.data
        SensorCard(type = SensorType.TEMPERATURE, value = data.temperature)
        SensorCard(type = SensorType.HUMIDITY, value = data.humidity)
        SensorCard(type = SensorType.LIGHT, value = data.lightIntensity)
        SensorCard(type = SensorType.PM25, value = data.pm25.toFloat())
        // 电池电量：固件上报的是 0~100 的整数百分比
        SensorCard(type = SensorType.BATTERY, value = data.battery.toFloat())

        Spacer(modifier = Modifier.height(80.dp))
    }
}
