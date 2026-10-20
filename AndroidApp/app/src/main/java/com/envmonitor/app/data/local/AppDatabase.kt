package com.envmonitor.app.data.local

import android.content.Context
import androidx.room.Database
import androidx.room.Room
import androidx.room.RoomDatabase
import com.envmonitor.app.data.model.SensorData

/* version 2：SensorData 新增了 battery 字段（电池电量）。
 * 配合 fallbackToDestructiveMigration()，升级时会清空旧库并重建 ——
 * 历史数据是本机缓存，云端还有，所以可以接受。
 * 如果以后要保留历史数据，就需要写正式的 Migration 并去掉 destructive 回退。 */
@Database(entities = [SensorData::class], version = 2, exportSchema = false)
abstract class AppDatabase : RoomDatabase() {

    abstract fun sensorDao(): SensorDao

    companion object {
        @Volatile
        private var INSTANCE: AppDatabase? = null

        fun getInstance(context: Context): AppDatabase {
            return INSTANCE ?: synchronized(this) {
                INSTANCE ?: Room.databaseBuilder(
                    context.applicationContext,
                    AppDatabase::class.java,
                    "env_monitor_db"
                ).fallbackToDestructiveMigration().build().also { INSTANCE = it }
            }
        }
    }
}
