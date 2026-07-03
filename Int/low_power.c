#include "low_power.h"
#include "freertos_demo.h"
#include "iwdg.h"
#include "oled.h"
#include "esp8266.h"
#include "rtc.h"
#include "spi.h"
#include "i2c.h"
#include "usart.h"
#include "w25q64.h"
#include <stdio.h>
#include <string.h>

#define ESP_RX_BUF_SIZE    256U
#define LOW_POWER_DEBUG    1U

uint8_t g_low_power_flag = LOW_POWER_FLAG_NORMAL;
SemaphoreHandle_t g_low_power_sem = NULL;
uint8_t g_wakeup_reason = WAKEUP_REASON_KEY;

extern void SystemClock_Config(void);

void SystemClock_ReConfig(void)
{
    SystemClock_Config();
}

void LowPower_Init(void)
{
    g_low_power_sem = xSemaphoreCreateBinary();
#if LOW_POWER_DEBUG
    printf("[LowPower] Init OK\r\n");
#endif
}

void LowPower_Task(void * pvParameters)
{
    while(1)
    {
        xSemaphoreTake(g_low_power_sem, portMAX_DELAY);
    }
}
