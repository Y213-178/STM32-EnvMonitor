#ifndef __ESP8266_H
#define __ESP8266_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "usart.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*********************** 宏定义 *************************/
/* ⚠️ 下面 5 项是占位符，用之前必须换成你自己的值。
 *   建议做法：把这几行挪到单独的 config.h，并把 config.h 加进 .gitignore，
 *   这样提交到公开仓库时就不会泄露凭据。 */
#define WIFI_SSID           "YOUR_WIFI_SSID"        // WiFi 名称（2.4GHz，ESP8266 不支持 5GHz）
#define WIFI_PASSWD         "YOUR_WIFI_PASSWORD"    // WiFi 密码
#define ONENET_PRODUCT_ID   "YOUR_PRODUCT_ID"       // OneNET 产品 ID
#define ONENET_DEVICE_ID    "YOUR_DEVICE_NAME"      // OneNET 设备名称
#define ONENET_DEVICE_NAME  "环境监测节点"           // OneNET 设备描述（当前代码未引用，仅作记录）
/* OneNET AccessKey：在控制台「设备详情 → 鉴权信息」里生成，形如
 *   version=2018-10-31&res=products%2F{产品ID}%2Fdevices%2F{设备名}&et=...&method=md5&sign=...
 * 注意：设备级 Token 只能访问绑定的那个设备。 */
#define ONENET_ACCESS_KEY   "YOUR_ONENET_ACCESS_KEY"
#define MQTT_SERVER_IP      "183.230.40.96"        // OneNET MQTT 服务器 IP
#define MQTT_SERVER_PORT    1883                   // OneNET MQTT 服务器端口



/************* esp8266.c全局变量 ****************/
#define ESP_RX_BUF_LEN 256
extern uint8_t esp_rx_buf[ESP_RX_BUF_LEN];
extern uint16_t esp_rx_len;
extern uint8_t esp_rx_flag;

typedef struct
{
    uint8_t sensor_task_alive;
    uint8_t network_task_alive;
}   TaskAlive_t;

extern TaskAlive_t g_task_alive;



/*************************** 相关函数 ***********************/
uint8_t ESP8266_Init(void);
uint8_t ESP8266_ConnectWiFi(void);
uint8_t ESP8266_SendCmd_WaitAck(char *cmd, char *ack, uint32_t timeout);
uint8_t ESP8266_SendRaw(uint8_t *data, uint16_t len);
void ESP8266_ClearRxBuffer(void);
uint8_t ESP8266_WaitIPD(uint32_t timeout);
uint8_t* BufStrstr(uint8_t *buf, uint16_t buf_len, char *needle);
void ESP8266_EnterLightSleep(void);


#ifdef __cplusplus
}
#endif

#endif
