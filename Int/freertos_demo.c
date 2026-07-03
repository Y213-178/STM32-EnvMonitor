/* FreeRTOS headers */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "freertos_demo.h"

/* Project headers */
#include "LED.h"
#include "Key.h"
#include "i2c.h"
#include "spi.h"
#include "usart.h"
#include "iwdg.h"
#include "rtc.h"
#include "oled.h"
#include "sht30.h"
#include "bh1750.h"
#include "pms5003.h"
#include "w25q64.h"
#include "esp8266.h"
#include "onenet_mqtt.h"
#include "low_power.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

SemaphoreHandle_t g_I2C_Mutex;
SemaphoreHandle_t g_SPI_Mutex;

TaskAlive_t g_task_alive;

float g_temp = 0.0f, g_humi = 0.0f;
float g_light = 0.0f;
uint16_t g_pm1_0 = 0, g_pm2_5 = 0, g_pm10 = 0;

extern uint8_t g_low_power_flag;
extern SemaphoreHandle_t g_low_power_sem;

#define START_TASK_STACK  200
#define START_TASK_PRIORITY  6
TaskHandle_t start_task_handle;
void start_task(void * pvParameters);

#define WATCHDOGTASK_STACK  100
#define WATCHDOGTASK_PRIORITY  7
TaskHandle_t WatchdogTask_handle;
void WatchdogTask(void * pvParameters);

#define NETWORKTASK_STACK  384
#define NETWORKTASK_PRIORITY  5
TaskHandle_t NetworkTask_handle;
void NetworkTask(void * pvParameters);

#define SENSORTASK_STACK  320
#define SENSORTASK_PRIORITY  4
TaskHandle_t SensorTask_handle;
void SensorTask(void * pvParameters);

#define DISPLAYTASK_STACK  256
#define DISPLAYTASK_PRIORITY  2
TaskHandle_t DisplayTask_handle;
void DisplayTask(void * pvParameters);

#define LOWPOWERTASK_STACK  128
#define LOWPOWERTASK_PRIORITY  6
TaskHandle_t LowPowerTask_handle;
void LowPower_Task(void * pvParameters);

#define AUTOSLEEP_TIMER_NAME       "AutoSleepTimer"
#define AUTOSLEEP_TIMER_PERIOD     pdMS_TO_TICKS(120000)
#define AUTOSLEEP_TIMER_RELOAD     pdFALSE
TimerHandle_t g_auto_sleep_timer;
void AutoSleep_Timer_Callback(TimerHandle_t xTimer);

void freertos_start(void)
{
    BaseType_t res = xTaskCreate((TaskFunction_t) start_task,
                     (char * ) "start_task",
                     (configSTACK_DEPTH_TYPE) START_TASK_STACK,
                     (void *) NULL,
                     (UBaseType_t) START_TASK_PRIORITY,
                     (TaskHandle_t *) &start_task_handle);

    if(res == pdPASS)
    {
        printf("start_task created OK\r\n");
    }
    else
    {
        printf("start_task FAILED\r\n");
        while(1);
    }

    vTaskStartScheduler();
    while(1);
}

void start_task(void * pvParameters)
{
    g_I2C_Mutex = xSemaphoreCreateMutex();
    if(g_I2C_Mutex == NULL) { printf("I2C mutex FAILED"); while(1); }

    g_SPI_Mutex = xSemaphoreCreateMutex();
    if(g_SPI_Mutex == NULL) { printf("SPI mutex FAILED"); while(1); }

    if(W25Q64_Init() == 0) { printf("[W25Q64] Init OK\r\n"); }
    else { printf("[W25Q64] Init FAILED\r\n"); }

    memset(&g_task_alive, 0, sizeof(TaskAlive_t));
    g_task_alive.sensor_task_alive = 1;
    g_task_alive.network_task_alive = 1;

    LowPower_Init();

    HAL_IWDG_Refresh(&hiwdg);

    taskENTER_CRITICAL();

    BaseType_t res2 = xTaskCreate((TaskFunction_t) NetworkTask,
                      (char * ) "NetworkTask",
                      (configSTACK_DEPTH_TYPE) NETWORKTASK_STACK,
                      (void *) NULL,
                      (UBaseType_t) NETWORKTASK_PRIORITY,
                      (TaskHandle_t *) &NetworkTask_handle);

    BaseType_t res3 = xTaskCreate((TaskFunction_t) SensorTask,
                      (char * ) "SensorTask",
                      (configSTACK_DEPTH_TYPE) SENSORTASK_STACK,
                      (void *) NULL,
                      (UBaseType_t) SENSORTASK_PRIORITY,
                      (TaskHandle_t *) &SensorTask_handle);

    BaseType_t res1 = xTaskCreate((TaskFunction_t) WatchdogTask,
                      (char * ) "WatchdogTask",
                      (configSTACK_DEPTH_TYPE) WATCHDOGTASK_STACK,
                      (void *) NULL,
                      (UBaseType_t) WATCHDOGTASK_PRIORITY,
                      (TaskHandle_t *) &WatchdogTask_handle);

    BaseType_t res4 = xTaskCreate((TaskFunction_t) DisplayTask,
                      (char * ) "DisplayTask",
                      (configSTACK_DEPTH_TYPE) DISPLAYTASK_STACK,
                      (void *) NULL,
                      (UBaseType_t) DISPLAYTASK_PRIORITY,
                      (TaskHandle_t *) &DisplayTask_handle);

    BaseType_t res5 = xTaskCreate((TaskFunction_t) LowPower_Task,
                      (char * ) "LowPower_Task",
                      (configSTACK_DEPTH_TYPE) LOWPOWERTASK_STACK,
                      (void *) NULL,
                      (UBaseType_t) LOWPOWERTASK_PRIORITY,
                      (TaskHandle_t *) &LowPowerTask_handle);

    taskEXIT_CRITICAL();

    TimerHandle_t res_timer = xTimerCreate((const char *) AUTOSLEEP_TIMER_NAME,
                                     (TickType_t) AUTOSLEEP_TIMER_PERIOD,
                                     (UBaseType_t) AUTOSLEEP_TIMER_RELOAD,
                                     (void *) NULL,
                                     (TimerCallbackFunction_t) AutoSleep_Timer_Callback);

    if(res_timer != NULL)
    {
        g_auto_sleep_timer = res_timer;
        xTimerStart(g_auto_sleep_timer, 0);
        printf("AutoSleepTimer started (120s)\r\n");
    }
    else { printf("AutoSleepTimer FAILED\r\n"); }

    if(res1 == pdPASS) printf("WatchdogTask created OK\r\n");
    else printf("WatchdogTask FAILED\r\n");

    if(res2 == pdPASS) printf("NetworkTask created OK\r\n");
    else printf("NetworkTask FAILED\r\n");

    if(res3 == pdPASS) printf("SensorTask created OK\r\n");
    else printf("SensorTask FAILED\r\n");

    if(res4 == pdPASS) printf("DisplayTask created OK\r\n");
    else printf("DisplayTask FAILED\r\n");

    if(res5 == pdPASS) printf("LowPower_Task created OK\r\n");
    else printf("LowPower_Task FAILED\r\n");

    vTaskDelete(NULL);
}

void WatchdogTask(void * pvParameters)
{
    vTaskDelay(pdMS_TO_TICKS(1000));

    while(1)
    {
        if(g_task_alive.sensor_task_alive == 1 && g_task_alive.network_task_alive == 1)
        {
            HAL_IWDG_Refresh(&hiwdg);
            printf("[Watchdog] All OK, feed\r\n");
            memset(&g_task_alive, 0, sizeof(TaskAlive_t));
        }
        else { printf("[Watchdog] ERROR! Wait reset\r\n"); }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* WiFi -> OneNet MQTT -> upload -> sleep */
void NetworkTask(void * pvParameters)
{
    uint8_t wifi_ok = 0;
    uint8_t mqtt_ok = 0;
    SensorDataFrame_t frame;

    g_task_alive.network_task_alive = 1;
    vTaskDelay(pdMS_TO_TICKS(1000));

    printf("[ESP8266] Init start...");
    while (ESP8266_Init() != 0)
    {
        printf("[ESP8266] Init failed, retry in 3s...");
        g_task_alive.network_task_alive = 1;

        for (int i = 0; i < 30; i++)
        {
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    printf("[ESP8266] Init OK");

    while (1)
    {
        if (wifi_ok == 0)
        {
            printf("[WiFi] Connecting: %s...", WIFI_SSID);

            if (ESP8266_ConnectWiFi() == 0)
            {
                printf("[WiFi] Connected");
                wifi_ok = 1;
            }
            else
            {
                printf("[WiFi] Failed, retry in 2s");
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        if (mqtt_ok == 0)
        {
            printf("[OneNet] Connecting MQTT...");

            if (Onenet_Connect() == 0)
            {
                printf("[OneNet] MQTT OK");
                mqtt_ok = 1;

                printf("[W25Q64] Upload offline data...");

                while (W25Q64_RetrieveData(&frame) == 0)
                {
                    if (Onenet_Publish(frame.temperature, frame.humidity,
                                       frame.light, frame.pm25) == 0)
                    {
                        printf("[W25Q64] Upload OK");
                    }
                    else
                    {
                        printf("[W25Q64] Upload FAILED, stop replay");
                        mqtt_ok = 0;
                        break;
                    }
                    g_task_alive.network_task_alive = 1;
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
                printf("[W25Q64] Offline data done");
            }
            else
            {
                printf("[OneNet] MQTT failed, save to flash");
                frame.temperature = g_temp;
                frame.humidity    = g_humi;
                frame.light       = g_light;
                frame.pm25        = g_pm2_5;
                frame.timestamp   = xTaskGetTickCount();
                W25Q64_StoreData(&frame);

                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        if (wifi_ok && mqtt_ok)
        {
            if (Onenet_Publish(g_temp, g_humi, g_light, g_pm2_5) == 0)
            {
                printf("[OneNet] Upload OK: T=%.1f H=%.1f L=%.1f PM2.5=%d",
                       g_temp, g_humi, g_light, g_pm2_5);
            }
            else
            {
                printf("[OneNet] Upload failed, save to flash");
                frame.temperature = g_temp;
                frame.humidity    = g_humi;
                frame.light       = g_light;
                frame.pm25        = g_pm2_5;
                frame.timestamp   = xTaskGetTickCount();
                W25Q64_StoreData(&frame);
                mqtt_ok = 0;
            }
        }

        for (int i = 0; i < 6000; i++)
        {
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void SensorTask(void * pvParameters)
{
    float temp, humi, light;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    SHT30_Init();
    BH1750_Init();
    PMS5003_Init();

    while(1)
    {
        if(SHT30_Read(&temp, &humi) == 0)
        {
            printf("[SHT30] T=%.1f C H=%.1f %%RH\r\n", temp, humi);
            g_temp = temp; g_humi = humi;
        }
        else { printf("[SHT30] FAIL\r\n"); }

        vTaskDelay(pdMS_TO_TICKS(10));

        if(BH1750_Read(&light) == 0)
        {
            printf("[BH1750] Light=%.1f Lux\r\n", light);
            g_light = light;
        }
        else { printf("[BH1750] FAIL\r\n"); }

        if(PMS5003_ReadData() == 0)
        {
            printf("[PMS5003] PM1.0=%d PM2.5=%d PM10=%d\r\n", pms_data.pm1_0, pms_data.pm2_5, pms_data.pm10);
            g_pm1_0 = pms_data.pm1_0; g_pm2_5 = pms_data.pm2_5; g_pm10 = pms_data.pm10;
        }
        else { printf("[PMS5003] FAIL\r\n"); }

        printf("------------------------\r\n");

        g_task_alive.sensor_task_alive = 1;

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

void DisplayTask(void * pvParameters)
{
    OLED_Init();
    OLED_Clear();
    printf("OLED Init OK\r\n");

    while(1)
    {
        OLED_ShowString(1, 1, "TEMP:");
        OLED_ShowFloat(1, 6, g_temp, 1);
        OLED_ShowString(1, 11, "C");

        OLED_ShowString(2, 1, "HUMI:");
        OLED_ShowFloat(2, 6, g_humi, 1);
        OLED_ShowString(2, 11, "%");

        OLED_ShowString(3, 1, "PM2.5:");
        OLED_ShowNum(3, 7, g_pm2_5, 3);
        OLED_ShowString(3, 11, "ug");

        OLED_ShowString(4, 1, "Light:");
        OLED_ShowFloat(4, 7, g_light, 1);
        OLED_ShowString(4, 13, "Lux");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void AutoSleep_Timer_Callback(TimerHandle_t xTimer)
{
    printf("[AutoSleepTimer] 120s no op, entering sleep...\r\n");

    if(g_low_power_sem != NULL)
    {
        xSemaphoreGive(g_low_power_sem);
    }
}
