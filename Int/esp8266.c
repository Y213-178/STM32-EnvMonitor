#include "esp8266.h"
#include "FreeRTOS.h"
#include "task.h"

uint8_t esp_rx_buf[ESP_RX_BUF_LEN] = {0};                          
uint16_t esp_rx_len = 0;                                           
uint8_t esp_rx_flag = 0;                                           

void ESP8266_ClearRxBuffer(void)
{
    memset(esp_rx_buf, 0, ESP_RX_BUF_LEN);                         
    esp_rx_len = 0;
    esp_rx_flag = 0;
    HAL_UART_AbortReceive_IT(&huart2);                             
    HAL_UART_Receive_IT(&huart2, esp_rx_buf, 1);                   
}

uint8_t* BufStrstr(uint8_t *buf, uint16_t buf_len, char *needle)
{
    uint16_t n = strlen(needle);                                   
    uint16_t i;
    if (n == 0) return buf;                                        
    if (n > buf_len) return NULL;                                  
    for (i = 0; i <= buf_len - n; i++)                             
    {
        if (memcmp(buf + i, needle, n) == 0)                       
            return buf + i;
    }
    return NULL;                                                   
}


uint8_t ESP8266_WaitIPD(uint32_t timeout)
{
    uint32_t tick = HAL_GetTick();
    while (HAL_GetTick() - tick < timeout)
    {
        if (esp_rx_len > 0 && BufStrstr(esp_rx_buf, esp_rx_len, "+IPD,") != NULL)
        {
            return 0;
        }
        g_task_alive.network_task_alive = 1;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return 1;
}

uint8_t ESP8266_SendCmd_WaitAck(char *cmd, char *ack, uint32_t timeout)
{
    ESP8266_ClearRxBuffer();                                                        

    if (strlen(cmd) > 0)                                                            
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)cmd, strlen(cmd), 100);                
        HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);                       
    }

    uint32_t tick = HAL_GetTick();                                                  
    while (HAL_GetTick() - tick < timeout)                                          
    {
        if (esp_rx_len > 0 && BufStrstr(esp_rx_buf, esp_rx_len, ack) != NULL)       
        {
            return 0;
        }

        g_task_alive.network_task_alive = 1;                                        
        vTaskDelay(pdMS_TO_TICKS(10));                                              
    }

    return 1;
}

uint8_t ESP8266_Init(void)
{
    HAL_UART_Receive_IT(&huart2, esp_rx_buf + esp_rx_len, 1);                       

    printf("[ESP8266] Waiting for boot (6s max)...\r\n");                          
    {
        uint32_t tick = HAL_GetTick();
        int got_ready = 0;
        while (HAL_GetTick() - tick < 6000) {
            if (!got_ready && esp_rx_len > 0 && BufStrstr(esp_rx_buf, esp_rx_len, "ready") != NULL)
			{
                printf("[ESP8266] \"ready\" detected at %lums\r\n", HAL_GetTick() - tick);
                got_ready = 1;
            }
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        /* Dump boot messages - both ASCII and HEX */
        if (esp_rx_len > 0) {
            printf("[ESP8266] Boot msg (%d bytes):\r\n", esp_rx_len);
            printf("  ASCII: ");
            for (int _i = 0; _i < esp_rx_len && _i < 256; _i++) {
                uint8_t c = esp_rx_buf[_i];
                if (c >= 0x20 && c < 0x7F) printf("%c", c);
                else if (c == '\r') printf("\\r");
                else if (c == '\n') printf("\\n");
                else printf(".");
            }
            printf("\r\n  HEX:   ");
            for (int _i = 0; _i < esp_rx_len && _i < 256; _i++)
                printf("%02X ", esp_rx_buf[_i]);
            printf("\r\n");
        } else {
            printf("[ESP8266] No boot messages received! Check wiring/power.\r\n");
        }
    }
    ESP8266_ClearRxBuffer();

    printf("[ESP8266] Sending AT+GMR...\r\n");                       
    if (ESP8266_SendCmd_WaitAck("AT+GMR", "OK", 5000) == 0) {
        printf("[ESP8266] AT+GMR OK (rx_len=%d):\r\n", esp_rx_len);
        printf("  ASCII: ");
        for (int _i = 0; _i < esp_rx_len && _i < 256; _i++) {
            uint8_t c = esp_rx_buf[_i];
            if (c >= 0x20 && c < 0x7F) printf("%c", c);
            else if (c == '\r') printf("\\r");
            else if (c == '\n') printf("\\n");
            else printf(".");
        }
        printf("\r\n  HEX:   ");            
        for (int _i = 0; _i < esp_rx_len && _i < 256; _i++)
            printf("%02X ", esp_rx_buf[_i]);
        printf("\r\n");
    } else {
        printf("[ESP8266] AT+GMR FAILED! rx_len=%d\r\n", esp_rx_len);
        printf("  HEX: ");
        for (int _i = 0; _i < esp_rx_len && _i < 256; _i++)
            printf("%02X ", esp_rx_buf[_i]);
        printf("\r\n");
    }
    ESP8266_ClearRxBuffer();

    for (uint8_t retry = 0; retry < 3; retry++)
    {
        g_task_alive.network_task_alive = 1;
        printf("[ESP8266] AT test (retry %d)...\r\n", retry + 1);
        if (ESP8266_SendCmd_WaitAck("AT", "OK", 3000) == 0)                     
        {
            printf("[ESP8266] AT OK\r\n");
            printf("[ESP8266] Disabling echo (ATE0)...\r\n");
            ESP8266_SendCmd_WaitAck("ATE0", "OK", 2000);                        
            printf("[ESP8266] Setting station mode (CWMODE=1)...\r\n");
            ESP8266_SendCmd_WaitAck("AT+CWMODE=1", "OK", 3000);                 
            for (int i = 0; i < 10; i++)                                        
			{                                  
                g_task_alive.network_task_alive = 1;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            printf("[ESP8266] Init complete!\r\n");
            return 0;
        }
        printf("[ESP8266] AT failed, rx_len=%d, retrying...\r\n", esp_rx_len);
        for (int i = 0; i < 10; i++)
        {
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    printf("[ESP8266] Init FAILED after 3 retries!\r\n");
    return 1;
}

uint8_t ESP8266_ConnectWiFi(void)
{
    char cmd[128];

    ESP8266_SendCmd_WaitAck("AT+CWQAP", "OK", 3000);                         
    vTaskDelay(pdMS_TO_TICKS(500));                                          

    sprintf(cmd, "AT+CWJAP=\"%s\",\"%s\"", WIFI_SSID, WIFI_PASSWD);          
    printf("[WiFi] Connecting to %s...\r\n", WIFI_SSID);
    uint8_t ret = ESP8266_SendCmd_WaitAck(cmd, "WIFI GOT IP", 15000);        
    if (ret == 0) {
        printf("[WiFi] Connected! rx_len=%d\r\n", esp_rx_len);
        printf("[WiFi] Response: ");
        for (int _i = 0; _i < esp_rx_len && _i < 200; _i++) {
            uint8_t c = esp_rx_buf[_i];
            if (c >= 0x20 && c < 0x7F) printf("%c", c);
            else if (c == '\r') printf("\\r");
            else if (c == '\n') printf("\\n");
            else printf(".");
        }
        printf("\r\n");
    } else {
        printf("[WiFi] Connect FAILED! rx_len=%d\r\n", esp_rx_len);
        printf("[WiFi] Response HEX: ");
        for (int _i = 0; _i < esp_rx_len && _i < 200; _i++)
            printf("%02X ", esp_rx_buf[_i]);
        printf("\r\n");
    }
    return ret;
}

uint8_t ESP8266_ConnectTCP(void)
{
    char cmd[128];
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d", MQTT_SERVER_IP, MQTT_SERVER_PORT);
    return ESP8266_SendCmd_WaitAck(cmd, "CONNECT", 5000);
}

uint8_t ESP8266_SendData(char *data)
{
    char cmd[64];
    uint16_t data_len = strlen(data);
    sprintf(cmd, "AT+CIPSEND=%d", data_len);

    if (ESP8266_SendCmd_WaitAck(cmd, ">", 1000) != 0)
    {
        return 1;
    }

    HAL_UART_Transmit(&huart2, (uint8_t*)data, data_len, 200);

    return ESP8266_SendCmd_WaitAck("", "SEND OK", 2000);
}

uint8_t ESP8266_SendRaw(uint8_t *data, uint16_t len)
{
    char cmd[32];
    uint8_t ret;
    sprintf(cmd, "AT+CIPSEND=%d", len);
    if (ESP8266_SendCmd_WaitAck(cmd, ">", 2000) != 0)
    {
        extern uint16_t esp_rx_len;
        extern uint8_t esp_rx_buf[];
        printf("[SendRaw] AT+CIPSEND timeout, no '>', rx_len=%d\r\n", esp_rx_len);
        printf("[SendRaw] RX buf hex: ");
        for (int _i = 0; _i < esp_rx_len && _i < 60; _i++)
            printf("%02X ", esp_rx_buf[_i]);
        printf("\r\n");
        return 1;
    }
    HAL_UART_Transmit(&huart2, data, len, 500);
    /* Wait for "SEND OK" or "SEND FAIL" or "CLOSED" */
    {
        uint32_t tick = HAL_GetTick();
        while (HAL_GetTick() - tick < 5000) {
            if (esp_rx_len > 0) {
                if (BufStrstr(esp_rx_buf, esp_rx_len, "SEND OK") != NULL) break;    
                if (BufStrstr(esp_rx_buf, esp_rx_len, "SEND FAIL") != NULL) break;  
                if (BufStrstr(esp_rx_buf, esp_rx_len, "CLOSED") != NULL) break;     
            }
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    /* Now check the result */
    if (BufStrstr(esp_rx_buf, esp_rx_len, "CLOSED") != NULL) {                      
        printf("[SendRaw] TCP link closed by server\r\n");                          
        ret = 1;
    } else if (BufStrstr(esp_rx_buf, esp_rx_len, "SEND FAIL") != NULL) {
        printf("[SendRaw] TCP send failed (SEND FAIL)\r\n");
        ret = 1;
    } else if (BufStrstr(esp_rx_buf, esp_rx_len, "SEND OK") != NULL) {
        ret = 0;
    } else {
        ret = 1;
    }
    if (ret != 0)
    {
        extern uint16_t esp_rx_len;
        extern uint8_t esp_rx_buf[];
        printf("[SendRaw] No SEND OK after 5s, rx_len=%d\r\n", esp_rx_len);
        printf("[SendRaw] RX buf hex: ");
        for (int _i = 0; _i < esp_rx_len && _i < 60; _i++)
            printf("%02X ", esp_rx_buf[_i]);
        printf("\r\n");
    }
    return ret;
}

/* 让 ESP8266 进入 light-sleep（芯片 ~0.5-1mA，UART 收到数据自动唤醒）。
 * 仅在成功连接并上传数据到云平台后调用；固件不支持时返回 ERROR，直接忽略（无害）。 */
void ESP8266_EnterLightSleep(void)
{
    if (ESP8266_SendCmd_WaitAck("AT+SLEEP=2", "OK", 1000) == 0)
    {
        printf("[ESP8266] Light-sleep armed\r\n");
    }
    else
    {
        printf("[ESP8266] AT+SLEEP=2 not supported, ignored\r\n");
    }
}
