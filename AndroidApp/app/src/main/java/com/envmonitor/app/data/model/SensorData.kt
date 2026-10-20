package com.envmonitor.app.data.model

import androidx.room.Entity
import androidx.room.PrimaryKey

@Entity(tableName = "sensor_data")
data class SensorData(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0,
    val temperature: Float = 0f,
    val humidity: Float = 0f,
    val lightIntensity: Float = 0f,
    val pm25: Int = 0,
    /** 电池剩余电量百分比（0~100），来自 OneNET 的 battery 属性 */
    val battery: Int = 0,
    val timestamp: Long = System.currentTimeMillis()
)

data class HistoryPoint(
    val timestamp: String,
    val value: Float
)
