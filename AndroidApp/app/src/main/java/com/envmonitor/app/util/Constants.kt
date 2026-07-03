package com.envmonitor.app.util

object Constants {
    // OneNet MQTT broker
    // App 用 OneNet Studio MQTT (旧版183.230.40.96不认识新设备)
    const val MQTT_BROKER = "you mqtt broker"

    // Product ID
    const val PRODUCT_ID = "your product id"

    // ESP8266 设备（用来上传数据）
    const val DEVICE_ID = "env_monitor_01"

    // App 独立设备（用来读取数据）
    const val APP_DEVICE_ID = "env_monitor_app"
    const val APP_DEVICE_KEY = "your_app_device_key"

    // 旧设备密钥保留（ESP8266 还在用）
    const val ACCESS_KEY = "your_onenet_token"

    // Data stream identifiers
    const val STREAM_TEMPERATURE = "temperature"
    const val STREAM_HUMIDITY = "humidity"
    const val STREAM_LIGHT = "light"
    const val STREAM_PM25 = "pm25"

    // MQTT QoS
    const val MQTT_QOS = 1
    const val MQTT_TIMEOUT = 10
    const val MQTT_KEEPALIVE = 60

    // [Polling] App获取云平台数据的间隔（秒），默认10分钟 = 600秒
    const val POLL_INTERVAL_SECONDS = 600L
}
