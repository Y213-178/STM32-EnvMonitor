package com.envmonitor.app.ui.components

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AcUnit
import androidx.compose.material.icons.filled.Air
import androidx.compose.material.icons.filled.BatteryFull
import androidx.compose.material.icons.filled.LightMode
import androidx.compose.material.icons.filled.WaterDrop
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.envmonitor.app.ui.theme.BatteryColor
import com.envmonitor.app.ui.theme.HumiColor
import com.envmonitor.app.ui.theme.LightColor
import com.envmonitor.app.ui.theme.Pm25Color
import com.envmonitor.app.ui.theme.TempColor

enum class SensorType(
    val label: String,
    val unit: String,
    val icon: ImageVector,
    val color: Color,
    val decimalPlaces: Int
) {
    TEMPERATURE("温度", "°C", Icons.Default.AcUnit, TempColor, 1),
    HUMIDITY("湿度", "%", Icons.Default.WaterDrop, HumiColor, 1),
    LIGHT("光照强度", "Lux", Icons.Default.LightMode, LightColor, 1),
    PM25("PM2.5", "μg/m³", Icons.Default.Air, Pm25Color, 0),
    /* 电池电量：整数百分比，不需要小数 */
    BATTERY("电池电量", "%", Icons.Default.BatteryFull, BatteryColor, 0)
}

@Composable
fun SensorCard(
    type: SensorType,
    value: Float,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 6.dp),
        shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = Color.White),
        elevation = CardDefaults.cardElevation(defaultElevation = 1.dp)
    ) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(20.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceBetween
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(14.dp)
            ) {
                Card(
                    shape = RoundedCornerShape(12.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = type.color.copy(alpha = 0.12f)
                    )
                ) {
                    Icon(
                        imageVector = type.icon,
                        contentDescription = type.label,
                        tint = type.color,
                        modifier = Modifier
                            .size(44.dp)
                            .padding(10.dp)
                    )
                }
                Column {
                    Text(
                        text = type.label,
                        style = MaterialTheme.typography.titleMedium,
                        color = MaterialTheme.colorScheme.secondary
                    )
                    Text(
                        text = type.unit,
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.secondary.copy(alpha = 0.6f)
                    )
                }
            }
            Text(
                text = if (type.decimalPlaces == 0)
                    value.toInt().toString()
                else
                    String.format("%.1f", value),
                fontSize = 28.sp,
                fontWeight = FontWeight.Bold,
                color = type.color
            )
        }
    }
}
