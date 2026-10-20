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
#include "rtc.h"
#include "oled.h"
#include "sht30.h"
#include "bh1750.h"
#include "pms5003.h"
#include "w25q64.h"
#include "esp8266.h"
#include "onenet_mqtt.h"
#include "low_power.h"
#include "battery.h"
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
extern uint32_t RTC_GetSecondsSinceEpoch(void);   /* rtc.c：跨休眠连续的秒数（时间戳用） */

#define START_TASK_STACK  200
#define START_TASK_PRIORITY  6
TaskHandle_t start_task_handle;
void start_task(void * pvParameters);

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

    /* 电池检测初始化：创建 ADC 互斥量 + 配置 ADC1。
     * 必须在这里（任何任务跑起来之前）调用：
     * NetworkTask 和 SensorTask 都会用 ADC1，如果让它们各自"惰性创建"互斥量，
     * 可能创建出两个不同的互斥量，保护就失效了。 */
    Battery_Init();

    memset(&g_task_alive, 0, sizeof(TaskAlive_t));
    g_task_alive.sensor_task_alive = 1;
    g_task_alive.network_task_alive = 1;

    LowPower_Init();

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

/* WiFi -> OneNet MQTT -> upload -> sleep
 * 常亮模式（PA0/冷启动）：持续循环，每 10min 上报一次，等 KEY1 手动切休眠。
 * 周期模式（RTC 闹钟唤醒）：采集+上传一轮后自动回睡，OLED 不点屏。 */
void NetworkTask(void * pvParameters)
{
    uint8_t wifi_ok = 0;
    uint8_t mqtt_ok = 0;
    uint8_t init_ok = 0;
    SensorDataFrame_t frame;
    /* 周期模式：RTC 闹钟唤醒。联网+上传总预算 90s，超时也回睡（失败帧落盘，下轮补传） */
    uint8_t periodic_mode = (g_wakeup_reason == WAKEUP_REASON_RTC);
    uint32_t deadline = periodic_mode ? (HAL_GetTick() + 90000UL) : 0;

    g_task_alive.network_task_alive = 1;
    vTaskDelay(pdMS_TO_TICKS(1000));

    /* ===== 电池低压保护（新板 v3.0）=====
     * 电池低于 3.40V 时不再联网上传，直接进入关机模式（STANDBY，仅 PA0 唤醒），
     * 避免锂电过放损伤、同时避免 ME6211 输入不足导致 3.3V 轨跌落而随机复位。
     * 用户插 USB 充电后按 PWR(PA0) 即可重新开机。 */
    {
        float vbat = Battery_ReadVoltage();
        if (Battery_IsLow(vbat))
        {
            printf("[BAT] LOW %.2fV (< %.2fV), shutdown to protect battery\r\n",
                   vbat, BATTERY_LOW_VOLTAGE);
            g_low_power_action = LOW_POWER_ACTION_OFF;   /* 关机：无闹钟，仅 PA0 唤醒 */
            if (g_low_power_sem != NULL)
            {
                xSemaphoreGive(g_low_power_sem);
            }
            vTaskSuspend(NULL);   /* 等 LowPower_Task 进 STANDBY（芯片随即复位） */
            return;
        }
        printf("[BAT] %.2f V (%u%%)\r\n", vbat, Battery_GetPercent(vbat));
    }

    printf("[ESP8266] Init start...");
    while (init_ok == 0)
    {
        if (ESP8266_Init() == 0)
        {
            init_ok = 1;
            break;
        }

        printf("[ESP8266] Init failed, retry in 3s...");
        g_task_alive.network_task_alive = 1;

        for (int i = 0; i < 30; i++)
        {
            g_task_alive.network_task_alive = 1;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (periodic_mode && HAL_GetTick() > deadline) break;  /* 预算耗尽：放弃本轮联网 */
    }
    printf("[ESP8266] Init %s\r\n", init_ok ? "OK" : "TIMEOUT");

    if (periodic_mode)
    {
        /* ===== 周期模式：一次性上传，然后自动回睡 ===== */
        while (wifi_ok == 0)
        {
            printf("[WiFi] Connecting: %s...", WIFI_SSID);
            if (ESP8266_ConnectWiFi() == 0) wifi_ok = 1;
            else if (HAL_GetTick() > deadline) break;
            else vTaskDelay(pdMS_TO_TICKS(2000));
        }

        while (wifi_ok && mqtt_ok == 0)
        {
            printf("[OneNet] Connecting MQTT...");
            if (Onenet_Connect() == 0) mqtt_ok = 1;
            else if (HAL_GetTick() > deadline) break;
            else vTaskDelay(pdMS_TO_TICKS(2000));
        }

        if (mqtt_ok)
        {
            printf("[W25Q64] Upload offline data...");
            while (W25Q64_RetrieveData(&frame) == 0)
            {
                if (Onenet_Publish(frame.temperature, frame.humidity,
                                   frame.light, frame.pm25,
                                   Battery_GetPercent(g_battery_voltage)) == 0)
                {
                    printf("[W25Q64] Upload OK");
                }
                else
                {
                    printf("[W25Q64] Upload FAILED, stop replay");
                    break;
                }
                g_task_alive.network_task_alive = 1;
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            printf("[W25Q64] Offline data done");

            if (Onenet_Publish(g_temp, g_humi, g_light, g_pm2_5,
                               Battery_GetPercent(g_battery_voltage)) == 0)
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
                frame.timestamp   = RTC_GetSecondsSinceEpoch();
                W25Q64_StoreData(&frame);
            }
        }
        else
        {
            /* 本轮未连上云：当前帧落盘，下周期补传 */
            printf("[OneNet] MQTT failed, save to flash");
            frame.temperature = g_temp;
            frame.humidity    = g_humi;
            frame.light       = g_light;
            frame.pm25        = g_pm2_5;
            frame.timestamp   = RTC_GetSecondsSinceEpoch();
            W25Q64_StoreData(&frame);
        }

        printf("[Network] Periodic cycle done, sleeping...\r\n");
        ESP8266_EnterLightSleep();
        g_low_power_action = LOW_POWER_ACTION_PERIODIC;   /* 明确下一轮仍是周期模式 */
        if (g_low_power_sem != NULL) xSemaphoreGive(g_low_power_sem);
        vTaskSuspend(NULL);   /* 等 LowPower_Task 进 STANDBY（芯片随即复位） */
    }

    /* ===== 常亮模式：原逻辑，持续循环，等 KEY1 手动切休眠 ===== */
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
                                       frame.light, frame.pm25,
                                       Battery_GetPercent(g_battery_voltage)) == 0)
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
                frame.timestamp   = RTC_GetSecondsSinceEpoch();
                W25Q64_StoreData(&frame);

                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
        }

        if (wifi_ok && mqtt_ok)
        {
            if (Onenet_Publish(g_temp, g_humi, g_light, g_pm2_5,
                               Battery_GetPercent(g_battery_voltage)) == 0)
            {
                printf("[OneNet] Upload OK: T=%.1f H=%.1f L=%.1f PM2.5=%d",
                       g_temp, g_humi, g_light, g_pm2_5);
                /* 已成功连接并上传到云平台：让 ESP8266 进 light-sleep，等待期省电。
                 * 网络未通时绝不提前调用，否则会打断重试流程。 */
                ESP8266_EnterLightSleep();
            }
            else
            {
                printf("[OneNet] Upload failed, save to flash");
                frame.temperature = g_temp;
                frame.humidity    = g_humi;
                frame.light       = g_light;
                frame.pm25        = g_pm2_5;
                frame.timestamp   = RTC_GetSecondsSinceEpoch();
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

    /* ===== 上电顺序（新板 v3.0）=====
     *
     * 新板上 PMS5003 挂在 V5_PMS（来自升压模块），ESP8266 挂在 V3.3_SW（来自 U4），
     * 两者都依赖 PB12 拉低 -> Q3 导通 -> 升压模块工作。
     * MX_GPIO_Init() 已经把 PB12 拉低，这里再显式确认一次，保证不依赖初始化顺序。 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);   /* 打开升压（Q3 导通） */

    /* 升压模块 + ME6211 建立输出需要时间（MT3608 起振几 ms，
     * 加上 V5_MAIN/V3.3_SW 上 C4/C5/C6 的充电，50ms 足够）。 */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* ===== 主动复位 PMS5003（新板新增 PB13 控制）=====
     * PMS5003 偶发死机时（连续读不到数据），原来只能整机断电恢复。
     * 现在每次上电/唤醒先给它一个复位脉冲：
     *   RST 低 >= 1ms -> 拉高 -> 等待约 1s 让内部风扇/激光稳定并开始输出数据。
     * 复位后必须等够时间再初始化 UART 收包，否则首帧会被丢弃。 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(1000));

    SHT30_Init();
    BH1750_Init();
    PMS5003_Init();

    /* 初始化共花掉约 1.06s，重新对齐周期基准：
     * 否则 xLastWakeTime 还是任务开始时的旧值，第一次 vTaskDelayUntil 会因为
     * "目标时刻已过"而立刻返回，导致第一次循环间隔异常。 */
    xLastWakeTime = xTaskGetTickCount();

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

        /* 电池电压采集（新板新增 PA1 + R14/R15 分压）*/
        {
            float vbat = Battery_ReadVoltage();
            printf("[BAT] %.2f V (%u%%)\r\n", vbat, Battery_GetPercent(vbat));
        }

        printf("------------------------\r\n");

        g_task_alive.sensor_task_alive = 1;

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

void DisplayTask(void * pvParameters)
{
    /* 周期模式（RTC 唤醒）不点屏：省电 + 夜间无光污染 */
    if (g_wakeup_reason != WAKEUP_REASON_RTC)
    {
        OLED_Init();
        OLED_Clear();
        printf("OLED Init OK\r\n");
    }

    while(1)
    {
        if (g_wakeup_reason != WAKEUP_REASON_RTC)
        {
            char line[24];
            uint8_t bat_pct;

            /* 电量显示死区（2%）：
             * 电池内阻造成的压降 + ADC 噪声会让换算结果在相邻几个百分点之间
             * 来回跳（例如 34/37/38）。这里只有变化达到 2% 才更新显示值，
             * 视觉上就稳定了。初值 0xFF 表示"还没显示过"，第一次一定更新。 */
            {
                static uint8_t s_disp_pct = 0xFFU;
                uint8_t now  = Battery_GetPercent(g_battery_voltage);
                uint8_t diff = (now > s_disp_pct) ? (uint8_t)(now - s_disp_pct)
                                                  : (uint8_t)(s_disp_pct - now);
                if (diff >= 2U)
                {
                    s_disp_pct = now;
                    bat_pct    = now;
                }
                else
                {
                    bat_pct    = s_disp_pct;
                }
            }

            /* 第一行：T:26.1 C          34%
             *   col 1-2   : "T:"
             *   col 3-7   : 温度，%-5.1f 固定 5 字符宽（26.1 / -5.0 / 100.5）
             *   col 8     : "C"
             *   col 9-12  : 空格
             *   col 13-16 : 电量 "%3u%%" 共 4 字符 -> " 34%" / "100%"
             * 用 snprintf 生成【固定 16 字符宽】的整行再整体写入：
             * 这样温度或电量位数变化时不会残留上一次的旧字符。 */
            snprintf(line, sizeof(line), "T:%-5.1fC    %3u%%", g_temp, bat_pct);
            OLED_ShowString(1, 1, line);

            /* 第二行：H:56.5 %
             *   col 1-2  : "H:"
             *   col 3-7  : 湿度，%-5.1f 固定 5 字符宽
             *   col 8    : "%"
             *   col 9-16 : 空格补齐
             * 去掉原来第 16 列的工作模式指示（1=常亮 / 2=周期）。
             * 必须补齐到 16 列，否则上一帧残留的字符会留在屏幕上。 */
            snprintf(line, sizeof(line), "H:%-5.1f%%", g_humi);
            {
                size_t used = strlen(line);
                if (used < 16U)
                {
                    memset(line + used, ' ', 16U - used);
                }
                line[16] = '\0';
            }
            OLED_ShowString(2, 1, line);

            OLED_ShowString(3, 1, "PM2.5:");
            OLED_ShowNum(3, 7, g_pm2_5, 3);
            OLED_ShowString(3, 11, "ug");

            /* 第四行：L:136.7 Lux
             *   col 1-2  : "L:"
             *   col 3-7  : 光照值，%-5.1f 固定 5 字符宽
             *   col 8    : 空格
             *   col 9-11 : "Lux"
             *   col 12-16: 空格补齐（避免光照值位数变化时残留旧字符）*/
            snprintf(line, sizeof(line), "L:%-5.1f Lux", g_light);
            {
                size_t used = strlen(line);
                if (used < 16U)
                {
                    memset(line + used, ' ', 16U - used);
                }
                line[16] = '\0';
            }
            OLED_ShowString(4, 1, line);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
