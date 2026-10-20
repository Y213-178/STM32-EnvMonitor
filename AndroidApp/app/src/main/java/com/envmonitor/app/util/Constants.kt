package com.envmonitor.app.util

object Constants {
    // OneNet MQTT broker
    // App 用 OneNet Studio MQTT (旧版183.230.40.96不认识新设备)
    const val MQTT_BROKER = "tcp://183.230.102.116:1883"

    // Product ID
    const val PRODUCT_ID = "YOUR_PRODUCT_ID"          // TODO: 填你的 OneNET 产品 ID

    // ESP8266 设备（用来上传数据）
    const val DEVICE_ID = "YOUR_DEVICE_NAME"          // TODO: 填上报数据的设备名

    // App 独立设备（用来读取数据）
    const val APP_DEVICE_ID = "YOUR_APP_DEVICE_NAME"  // TODO: App 侧独立设备名（可另建一个设备专门读数据）
    /* App 设备自身的 Token，res 部分要绑定到 APP_DEVICE_ID，形如：
     *   version=2018-10-31&res=products%2F{产品ID}%2Fdevices%2F{App设备名}&et=...&method=md5&sign=...
     * 注意：不能填上报设备的 Token —— OneNET 设备级 Token 只能读自己设备的数据。 */
    const val APP_DEVICE_KEY = "YOUR_APP_DEVICE_TOKEN"   // TODO: App 设备的 Token（res 要绑定 APP_DEVICE_ID）

    // 旧设备密钥保留（ESP8266 还在用）
    const val ACCESS_KEY = "YOUR_DEVICE_ACCESS_KEY"      // TODO: 上报设备（DEVICE_ID）的 AccessKey

    // Data stream identifiers
    const val STREAM_TEMPERATURE = "temperature"
    const val STREAM_HUMIDITY = "humidity"
    const val STREAM_LIGHT = "light"
    const val STREAM_PM25 = "pm25"
    // 电池剩余电量百分比（0~100），与 OneNET 物模型里的属性标识符一致
    const val STREAM_BATTERY = "battery"

    // MQTT QoS
    const val MQTT_QOS = 1
    const val MQTT_TIMEOUT = 10
    const val MQTT_KEEPALIVE = 60

    // [Polling] App获取云平台数据的间隔（秒），默认10分钟 = 600秒
    const val POLL_INTERVAL_SECONDS = 600L

    // ===================== 回补式同步（补历史）=====================
    /* 背景：设备端独立上报，App 关着的时候数据照样进云端；
     * 但 App 关闭期间本地库不会更新。所以每次打开 App 都要
     * 用 (上次同步时间, 现在) 这个区间去云端把缺的记录补回来。 */

    /** OneNET 只保留 7 天历史，本地也只留 7 天 */
    const val RETENTION_DAYS = 7

    /** 单次历史查询每页条数。
     *  ⚠️ OneNET 的 query-device-property-history 对 limit 有上限，
     *  实测 6000 会被拒（code=10418 "limit超过最大限制"），这里用 100 起步，
     *  MqttManager 里遇到 10418 会自动减半重试，并配合 offset 翻页。 */
    const val SYNC_HISTORY_LIMIT = 1000

    /** 距上次同步不足这个时长就跳过回补，避免每次切页都发请求 */
    const val SYNC_MIN_INTERVAL_MS = 60_000L

    /** 首次安装（从没同步过）时往回补多久 —— 直接补满 OneNET 的 7 天 */
    const val SYNC_INITIAL_LOOKBACK_MS = RETENTION_DAYS * 24L * 60 * 60 * 1000
}
