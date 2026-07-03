#ifndef __LOW_POWER_H
#define __LOW_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "timers.h"
#include <stdint.h>

#define LOW_POWER_FLAG_NORMAL    0           
#define LOW_POWER_FLAG_SLEEPING  2           

#define WAKEUP_REASON_RTC    0               
#define WAKEUP_REASON_KEY    1               

extern uint8_t g_low_power_flag;
extern SemaphoreHandle_t g_low_power_sem;    
extern uint8_t g_wakeup_reason;

extern TaskHandle_t NetworkTask_handle;      
extern TaskHandle_t SensorTask_handle;       
extern TaskHandle_t DisplayTask_handle;      
extern TimerHandle_t g_auto_sleep_timer;

void LowPower_Init(void);
void LowPower_Task(void * pvParameters);
void SystemClock_ReConfig(void);

#ifdef __cplusplus
}
#endif

#endif