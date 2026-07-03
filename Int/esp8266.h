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
#define WIFI_SSID           1// 请更换WiFi名称
#define WIFI_PASSWD         1// 请更换WiFi密码
#define ONENET_PRODUCT_ID   1// 请更换产品ID
#define ONENET_DEVICE_ID    1// 请更换设备ID
#define ONENET_DEVICE_NAME  1// 请更换设备名称
#define ONENET_ACCESS_KEY   1// 请更换access密钥
#define MQTT_SERVER_IP      1// 请更换服务器IP
#define MQTT_SERVER_PORT    1// 请更换服务器地址



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


#ifdef __cplusplus
}
#endif

#endif