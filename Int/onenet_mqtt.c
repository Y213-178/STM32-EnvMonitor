#include "onenet_mqtt.h"
#include "esp8266.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>


#define MQTT_KEEP_ALIVE   60


static uint8_t MQTT_RemainingLenSize(uint32_t len)
{
    if (len < 128) return 1;
    if (len < 16384) return 2;
    if (len < 2097152) return 3;
    return 4;
}

static uint8_t MQTT_EncodeRemLen(uint8_t *buf, uint32_t len)
{
    uint8_t i = 0;
    do {
        uint8_t d = len % 128;
        len /= 128;
        if (len > 0) d |= 0x80;
        buf[i++] = d;
    } while (len > 0);
    return i;
}

static void MQTT_WriteU16(uint8_t *buf, uint16_t val)
{
    buf[0] = (val >> 8) & 0xFF;                                           
    buf[1] = val & 0xFF;                                                  
}


static uint16_t Onenet_BuildConnect(uint8_t *buf)
{
    const char *username   = ONENET_PRODUCT_ID;                           
    const char *password   = ONENET_ACCESS_KEY;
    const char *client_id  = ONENET_DEVICE_ID;

    uint16_t client_len = strlen(client_id);                              
    uint16_t user_len   = strlen(username);                               
    uint16_t pass_len   = strlen(password);

    uint32_t var_len = 10 + 2 + client_len + 2 + user_len + 2 + pass_len; 

    uint8_t *p = buf;                                                     

    *p++ = 0x10;                                                          
    p += MQTT_EncodeRemLen(p, var_len);                                   

    MQTT_WriteU16(p, 4); p += 2;
    *p++ = 'M'; *p++ = 'Q'; *p++ = 'T'; *p++ = 'T';                       
    *p++ = 0x04;                                                          
    *p++ = 0xC2;                                                          
    MQTT_WriteU16(p, MQTT_KEEP_ALIVE); p += 2;                            

    MQTT_WriteU16(p, client_len); p += 2;                                 
    memcpy(p, client_id, client_len); p += client_len;

    MQTT_WriteU16(p, user_len); p += 2;
    memcpy(p, username, user_len); p += user_len;

    MQTT_WriteU16(p, pass_len); p += 2;
    memcpy(p, password, pass_len); p += pass_len;

    return p - buf;                                                       
}


static uint16_t Onenet_BuildPublish(uint8_t *buf, const char *json)
{
    char topic[128];
    snprintf(topic, sizeof(topic), "$sys/%s/%s/thing/property/post",        
             ONENET_PRODUCT_ID, ONENET_DEVICE_ID);

    uint16_t topic_len = strlen(topic);                                     
    uint16_t payload_len = strlen(json);
    uint32_t var_len = 2 + topic_len + payload_len;                         

    uint8_t *p = buf;

    *p++ = 0x30;                                                            
    p += MQTT_EncodeRemLen(p, var_len);

    MQTT_WriteU16(p, topic_len); p += 2;                                    
    memcpy(p, topic, topic_len); p += topic_len;                            

    memcpy(p, json, payload_len); p += payload_len;                         

    return p - buf;
}

static uint16_t ParseIPD(uint8_t *buf, uint16_t buf_len, uint8_t **data_out)
{
    char *ipd = "ipd,";                                                     
    uint8_t *p = BufStrstr(buf, buf_len, ipd);                              
    if (!p) 
		{                                                                   
          p = BufStrstr(buf, buf_len, "+IPD,");
        }
    if (!p) return 0;

    /* p points to '+IPD,' or 'ipd,', advance past it */
    p = (uint8_t*)strchr((char*)p, ',');                                    
    if (!p) return 0;
    p++;                                                                    

    /* Parse decimal length before ':' */                                   
    uint16_t len = 0;
    while (*p >= '0' && *p <= '9') 
		{
          len = len * 10 + (*p - '0');                                     
          p++;
        }
    if (*p != ':') return 0;
    p++;                                                                   

    *data_out = p;
    return len;
}


uint8_t Onenet_Connect(void)
{
    uint8_t buf[256];
    uint8_t result;

    /* 1. TCP connect to OneNet MQTT server */
    char cmd[64];

    ESP8266_SendCmd_WaitAck("AT+CIPCLOSE", "", 2000);
    vTaskDelay(pdMS_TO_TICKS(300));

    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%d",
             MQTT_SERVER_IP, MQTT_SERVER_PORT);

    printf("[OneNet] TCP connecting...\r\n");
    if (ESP8266_SendCmd_WaitAck(cmd, "CONNECT", 10000) != 0)             
    {
        printf("[OneNet] TCP connect FAILED\r\n");
        return 1;
    }
    printf("[OneNet] TCP connected OK\r\n");

    uint16_t len = Onenet_BuildConnect(buf);
    printf("[OneNet] Sending MQTT CONNECT (%d bytes)...\r\n", len);

    /* Dump first 80 bytes of CONNECT packet for debug */
    printf("[OneNet] CONNECT pkt: ");
    for (int _i = 0; _i < len && _i < 80; _i++)
        printf("%02X ", buf[_i]);
    printf("\r\n");

    result = ESP8266_SendRaw(buf, len);
    if (result == 0) {
        printf("[OneNet] MQTT CONNECT sent OK\r\n");

        /* Wait for CONNACK response via +IPD (server sends 4 bytes) */
        {
            extern uint8_t esp_rx_buf[];
            extern uint16_t esp_rx_len;
            extern TaskAlive_t g_task_alive;

            uint16_t base_offset = esp_rx_len;

            uint32_t tick = HAL_GetTick();
            uint16_t ipd_len = 0;
            uint8_t *ipd_data = NULL;

            while (HAL_GetTick() - tick < 8000) {
                if (esp_rx_len > 8)                                                            
					{
                      ipd_len = ParseIPD(esp_rx_buf, esp_rx_len, &ipd_data);
                      if (ipd_len > 0) break;
                    }
                g_task_alive.network_task_alive = 1;
                vTaskDelay(pdMS_TO_TICKS(50));
            }

            printf("[OneNet] Post-CONNECT rx_len=%d (base=%d)\r\n", esp_rx_len, base_offset);
            if (esp_rx_len > 0) {
                printf("[OneNet] Post-CONNECT hex: ");
                for (int _i = 0; _i < esp_rx_len && _i < 120; _i++)
                    printf("%02X ", esp_rx_buf[_i]);
                printf("\r\n");
            }

            if (ipd_len > 0 && ipd_data) {
                printf("[OneNet] +IPD data (%d bytes): ", ipd_len);
                for (int _i = 0; _i < ipd_len && _i < 20; _i++)
                    printf("%02X ", ipd_data[_i]);
                printf("\r\n");

                /* Parse CONNACK: byte0=0x20, byte1=0x02, byte2=session, byte3=rc */
                if (ipd_len >= 4 && ipd_data[0] == 0x20 && ipd_data[1] == 0x02) {
                    uint8_t connack_rc = ipd_data[3];
                    printf("[OneNet] CONNACK return code: 0x%02X\r\n", connack_rc);
                    if (connack_rc == 0x00) {
                        printf("[OneNet] CONNACK: Connection Accepted!\r\n");
                        ESP8266_ClearRxBuffer();
                        return 0;
                    } else {
                        const char *err = "Unknown";
                        if (connack_rc == 0x01) err = "Unacceptable protocol version";
                        if (connack_rc == 0x02) err = "Identifier rejected";
                        if (connack_rc == 0x03) err = "Server unavailable";
                        if (connack_rc == 0x04) err = "Bad username or password";
                        if (connack_rc == 0x05) err = "Not authorized";
                        printf("[OneNet] CONNACK REJECTED: %s\r\n", err);
                    }
                } else if (ipd_len >= 4) {
                    printf("[OneNet] Not a CONNACK (type=0x%02X, len=0x%02X)\r\n",
                           ipd_data[0], ipd_data[1]);
                }
            } else if (esp_rx_len == 0) {
                printf("[OneNet] No response from server (buffer empty)\r\n");
            } else {
                printf("[OneNet] No +IPD data found in buffer\r\n");
            }
            ESP8266_ClearRxBuffer();
            return 1;
        }
    } else
        printf("[OneNet] SendRaw FAILED, code=%d\r\n", result);
    return result;
}

uint8_t Onenet_Publish(float temp, float humi, float light, uint16_t pm25)
{
    uint8_t mqtt_buf[512];
    char json[256];

    snprintf(json, sizeof(json),                                              
        "{\"id\":\"1\",\"version\":\"1.0\",\"params\":{"
        "\"temperature\":{\"value\":%.1f},"
        "\"humidity\":{\"value\":%.1f},"
        "\"light\":{\"value\":%.1f},"
        "\"pm25\":{\"value\":%d}"
        "}}",
        temp, humi, light, pm25);

    uint16_t mqtt_len = Onenet_BuildPublish(mqtt_buf, json);                  
    return ESP8266_SendRaw(mqtt_buf, mqtt_len);                               
}
