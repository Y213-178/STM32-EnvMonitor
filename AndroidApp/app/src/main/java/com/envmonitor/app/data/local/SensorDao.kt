package com.envmonitor.app.data.local

import androidx.room.Dao
import androidx.room.Insert
import androidx.room.Query
import com.envmonitor.app.data.model.SensorData

@Dao
interface SensorDao {

    @Insert
    suspend fun insert(data: SensorData)

    @Query("SELECT * FROM sensor_data ORDER BY timestamp DESC LIMIT 1")
    suspend fun getLatest(): SensorData?

    @Query("SELECT * FROM sensor_data WHERE timestamp BETWEEN :start AND :end ORDER BY timestamp ASC")
    suspend fun getRange(start: Long, end: Long): List<SensorData>

    @Query("DELETE FROM sensor_data WHERE timestamp < :cutoff")
    suspend fun deleteOlderThan(cutoff: Long)

    @Query("DELETE FROM sensor_data")
    suspend fun deleteAll()

    @Query("SELECT MIN(timestamp) FROM sensor_data")
    suspend fun getOldestTimestamp(): Long?
}
