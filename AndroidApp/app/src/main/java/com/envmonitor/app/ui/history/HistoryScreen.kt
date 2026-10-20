package com.envmonitor.app.ui.history

import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
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
import androidx.compose.foundation.lazy.LazyRow
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Delete
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.collectAsState
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.lifecycle.viewmodel.compose.viewModel
import com.envmonitor.app.data.model.SensorData
import com.envmonitor.app.ui.components.HistoryChart
import com.envmonitor.app.ui.theme.BatteryColor
import com.envmonitor.app.ui.theme.HumiColor
import com.envmonitor.app.ui.theme.LightColor
import com.envmonitor.app.ui.theme.Pm25Color
import com.envmonitor.app.ui.theme.TempColor
import com.envmonitor.app.ui.theme.TextSecondary
import com.envmonitor.app.util.Constants
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

@Composable
fun HistoryScreen(
    viewModel: HistoryViewModel = viewModel(),
    modifier: Modifier = Modifier
) {
    val state by viewModel.state.collectAsState()
    /* 历史记录列表的时间格式：带日期。
     * 原来只有 HH:mm:ss，查 7 天时同一条 HH:mm 可能来自不同日期，无法区分。 */
    val timeFmt = SimpleDateFormat("MM-dd HH:mm:ss", Locale.getDefault())
    var showDeleteDialog by remember { mutableStateOf(false) }

    Column(
        modifier = modifier
            .fillMaxSize()
            .verticalScroll(rememberScrollState())
            .padding(horizontal = 16.dp)
    ) {
        Text(
            text = "历史数据",
            style = MaterialTheme.typography.headlineMedium,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(vertical = 12.dp)
        )

        // 时间范围快捷选择
        Text(
            text = "时间范围: ${state.timeRangeOption}",
            style = MaterialTheme.typography.labelMedium,
            color = TextSecondary
        )
        Spacer(modifier = Modifier.height(4.dp))
        Row(
            modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            viewModel.timeRangeOptions.forEach { (label, duration) ->
                val isSelected = state.timeRangeOption == label
                FilledTonalButton(
                    onClick = { viewModel.selectTimeRange(label, duration) },
                    colors = ButtonDefaults.filledTonalButtonColors(
                        containerColor = if (isSelected) TempColor else Color(0xFFF5F5F5),
                        contentColor = if (isSelected) Color.White else Color.DarkGray
                    ),
                    modifier = Modifier.height(32.dp)
                ) {
                    Text(label, style = MaterialTheme.typography.labelSmall)
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        // Stream selector chips
        LazyRow(
            horizontalArrangement = Arrangement.spacedBy(8.dp),
            modifier = Modifier.fillMaxWidth()
        ) {
            items(viewModel.streamOptions) { (id, label) ->
                val isSelected = state.selectedStream == id
                val color = when (id) {
                    Constants.STREAM_TEMPERATURE -> TempColor
                    Constants.STREAM_HUMIDITY -> HumiColor
                    Constants.STREAM_LIGHT -> LightColor
                    Constants.STREAM_PM25 -> Pm25Color
                    Constants.STREAM_BATTERY -> BatteryColor
                    else -> Color.Gray
                }
                Card(
                    shape = RoundedCornerShape(20.dp),
                    colors = CardDefaults.cardColors(
                        containerColor = if (isSelected) color else Color.White
                    ),
                    modifier = Modifier.clickable { viewModel.selectStream(id) }
                ) {
                    Text(
                        text = label,
                        modifier = Modifier.padding(horizontal = 16.dp, vertical = 8.dp),
                        color = if (isSelected) Color.White else color,
                        fontWeight = if (isSelected) FontWeight.Bold else FontWeight.Normal,
                        style = MaterialTheme.typography.labelLarge
                    )
                }
            }
        }

        Spacer(modifier = Modifier.height(12.dp))

        // Chart or loading
        if (state.isLoading) {
            Box(
                modifier = Modifier.fillMaxWidth().padding(32.dp),
                contentAlignment = Alignment.Center
            ) {
                CircularProgressIndicator(modifier = Modifier.size(28.dp), strokeWidth = 2.dp)
            }
        } else {
            val lineColor = when (state.selectedStream) {
                Constants.STREAM_TEMPERATURE -> TempColor
                Constants.STREAM_HUMIDITY -> HumiColor
                Constants.STREAM_LIGHT -> LightColor
                Constants.STREAM_PM25 -> Pm25Color
                Constants.STREAM_BATTERY -> BatteryColor
                else -> Color.Gray
            }

            Card(
                shape = RoundedCornerShape(16.dp),
                colors = CardDefaults.cardColors(containerColor = Color.White),
                elevation = CardDefaults.cardElevation(defaultElevation = 1.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    val streamLabel = viewModel.streamOptions
                        .find { it.first == state.selectedStream }?.second ?: ""

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween
                    ) {
                        Text(
                            text = "$streamLabel 趋势",
                            style = MaterialTheme.typography.titleLarge
                        )
                        if (state.points.isNotEmpty()) {
                            val avg = state.points.map { it.value }.average()
                            Text(
                                text = "均值 ${String.format("%.1f", avg)}",
                                style = MaterialTheme.typography.bodyMedium,
                                color = TextSecondary
                            )
                        }
                    }
                    Spacer(modifier = Modifier.height(12.dp))

                    HistoryChart(points = state.points, lineColor = lineColor)

                    Spacer(modifier = Modifier.height(8.dp))

                    // Stats row
                    if (state.points.isNotEmpty()) {
                        val values = state.points.map { it.value }
                        val minV = values.minOrNull() ?: 0f
                        val maxV = values.maxOrNull() ?: 0f
                        val avgV = values.average().toFloat()
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceEvenly
                        ) {
                            StatItem("最小值", String.format("%.1f", minV))
                            StatItem("平均值", String.format("%.1f", avgV))
                            StatItem("最大值", String.format("%.1f", maxV))
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(12.dp))

                    // 历史数据表格
                    Text(
                        text = "数据记录",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.Bold
                    )
                    Spacer(modifier = Modifier.height(8.dp))

                    if (state.allPoints.isEmpty()) {
                        Text("暂无记录", color = TextSecondary, style = MaterialTheme.typography.bodySmall)
                    } else {
                        val selectedId = state.selectedStream
                        val displayedPoints = state.allPoints.sortedByDescending { it.timestamp }

                        // 表头
                        Row(
                            modifier = Modifier.fillMaxWidth().padding(vertical = 4.dp),
                            horizontalArrangement = Arrangement.SpaceBetween
                        ) {
                            Text("时间", style = MaterialTheme.typography.labelSmall,
                                color = TextSecondary, fontWeight = FontWeight.Bold)
                            Text("$streamLabel 值", style = MaterialTheme.typography.labelSmall,
                                color = TextSecondary, fontWeight = FontWeight.Bold)
                        }
                        HorizontalDivider()

                        displayedPoints.forEach { data ->
                            val valueStr = when (selectedId) {
                                Constants.STREAM_TEMPERATURE -> String.format("%.1f°C", data.temperature)
                                Constants.STREAM_HUMIDITY -> String.format("%.1f%%", data.humidity)
                                Constants.STREAM_LIGHT -> String.format("%.1f Lux", data.lightIntensity)
                                Constants.STREAM_PM25 -> "${data.pm25} μg/m³"
                                Constants.STREAM_BATTERY -> "${data.battery}%"
                                else -> "-"
                            }
                            Row(
                                modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp),
                                horizontalArrangement = Arrangement.SpaceBetween
                            ) {
                                Text(
                                    text = timeFmt.format(Date(data.timestamp)),
                                    style = MaterialTheme.typography.bodySmall,
                                    color = TextSecondary
                                )
                                Text(
                                    text = valueStr,
                                    style = MaterialTheme.typography.bodySmall,
                                    fontWeight = FontWeight.Medium
                                )
                            }
                            HorizontalDivider(
                                color = Color(0xFFF0F0F0),
                                thickness = 0.5.dp
                            )
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))
                    HorizontalDivider()
                    Spacer(modifier = Modifier.height(12.dp))

                    // 自动删除功能
                    Text(
                        text = "自动删除历史数据",
                        style = MaterialTheme.typography.titleSmall,
                        fontWeight = FontWeight.Bold
                    )
                    Spacer(modifier = Modifier.height(8.dp))

                    // 删除按钮行
                    Row(
                        modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
                        horizontalArrangement = Arrangement.spacedBy(6.dp)
                    ) {
                        viewModel.deleteOptions.forEach { (label, duration) ->
                            Button(
                                onClick = { viewModel.deleteOlderThan(duration, label) },
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = Color(0xFFE53935)
                                ),
                                modifier = Modifier.height(32.dp)
                            ) {
                                Icon(Icons.Default.Delete, contentDescription = null,
                                    modifier = Modifier.size(14.dp))
                                Spacer(modifier = Modifier.width(4.dp))
                                Text(label, style = MaterialTheme.typography.labelSmall)
                            }
                        }
                    }

                    // 删除全部
                    Spacer(modifier = Modifier.height(8.dp))
                    Button(
                        onClick = { showDeleteDialog = true },
                        colors = ButtonDefaults.buttonColors(
                            containerColor = Color(0xFFB71C1C)
                        ),
                        modifier = Modifier.fillMaxWidth().height(36.dp)
                    ) {
                        Icon(Icons.Default.Delete, contentDescription = null,
                            modifier = Modifier.size(16.dp))
                        Spacer(modifier = Modifier.width(6.dp))
                        Text("删除全部历史数据", style = MaterialTheme.typography.labelLarge)
                    }

                    Spacer(modifier = Modifier.height(60.dp))
                }
            }
        }
    }

    // 删除确认对话框
    if (showDeleteDialog) {
        AlertDialog(
            onDismissRequest = { showDeleteDialog = false },
            title = { Text("确认删除") },
            text = { Text("确定要删除全部历史数据吗？此操作不可恢复。") },
            confirmButton = {
                TextButton(onClick = {
                    viewModel.deleteAll()
                    showDeleteDialog = false
                }) {
                    Text("删除", color = Color.Red)
                }
            },
            dismissButton = {
                TextButton(onClick = { showDeleteDialog = false }) {
                    Text("取消")
                }
            }
        )
    }
}

@Composable
private fun StatItem(label: String, value: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Text(
            text = label,
            style = MaterialTheme.typography.labelSmall,
            color = TextSecondary
        )
        Spacer(modifier = Modifier.height(2.dp))
        Text(
            text = value,
            style = MaterialTheme.typography.titleMedium,
            fontWeight = FontWeight.SemiBold
        )
    }
}
