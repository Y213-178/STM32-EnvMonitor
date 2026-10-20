package com.envmonitor.app.data.local

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import com.envmonitor.app.data.model.SensorData
import kotlinx.coroutines.flow.Flow

@Dao
interface SensorDao {

    @Insert
    suspend fun insert(data: SensorData)

    @Query("SELECT * FROM sensor_data ORDER BY timestamp DESC LIMIT 1")
    suspend fun getLatest(): SensorData?

    @Query("SELECT * FROM sensor_data WHERE timestamp BETWEEN :start AND :end ORDER BY timestamp ASC")
    suspend fun getRange(start: Long, end: Long): List<SensorData>

    /**
     * 可观察版本：每次数据表发生变化（新增/删除）都会自动重新查询并推送新列表。
     * 历史页用它，就能在设备上报新数据后自动出现新记录。
     */
    @Query("SELECT * FROM sensor_data WHERE timestamp BETWEEN :start AND :end ORDER BY timestamp ASC")
    fun getRangeFlow(start: Long, end: Long): Flow<List<SensorData>>

    @Query("DELETE FROM sensor_data WHERE timestamp < :cutoff")
    suspend fun deleteOlderThan(cutoff: Long)

    @Query("DELETE FROM sensor_data")
    suspend fun deleteAll()

    @Query("SELECT MIN(timestamp) FROM sensor_data")
    suspend fun getOldestTimestamp(): Long?
}
