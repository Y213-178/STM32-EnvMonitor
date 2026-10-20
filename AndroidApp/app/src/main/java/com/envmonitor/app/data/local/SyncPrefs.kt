package com.envmonitor.app.data.local

import android.content.Context

/**
 * 记录「上次从云端同步到哪个时间点」，用于回补式同步。
 *
 * 为什么需要它：
 *   设备端（STM32 + ESP8266）是独立上报的，App 关着的时候数据照样进 OneNET；
 *   但 App 关闭时本地 Room 不会更新，所以下次打开必须知道「从哪补到哪」。
 *
 * 用 SharedPreferences 而不是 Room：这是单值配置，没必要建表。
 */
object SyncPrefs {

    private const val PREF_NAME = "env_monitor_sync"
    private const val KEY_LAST_SYNC_MS = "last_sync_ms"

    /** @return 上次同步完成的时间戳；0 表示从未同步过（首次安装） */
    fun getLastSyncMs(context: Context): Long =
        context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            .getLong(KEY_LAST_SYNC_MS, 0L)

    fun setLastSyncMs(context: Context, ms: Long) {
        context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .putLong(KEY_LAST_SYNC_MS, ms)
            .apply()
    }

    /** 需要重新回补时调用（比如用户手动下拉刷新） */
    fun reset(context: Context) {
        context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE)
            .edit()
            .remove(KEY_LAST_SYNC_MS)
            .apply()
    }
}
