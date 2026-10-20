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

/* 进 STANDBY 的两种方式 */
#define LOW_POWER_ACTION_PERIODIC  0   /* 单按 KEY1 / RTC 周期：STANDBY + 30min 闹钟 */
#define LOW_POWER_ACTION_OFF       1   /* 长按 KEY1 关机：STANDBY 无闹钟，仅 PA0 唤醒 */

/* KEY1 长按判定门限（毫秒） */
#define KEY_LONG_PRESS_MS          3000

extern uint8_t g_low_power_flag;
extern SemaphoreHandle_t g_low_power_sem;
extern uint8_t g_wakeup_reason;
extern uint8_t g_low_power_action;

extern TaskHandle_t NetworkTask_handle;
extern TaskHandle_t SensorTask_handle;
extern TaskHandle_t DisplayTask_handle;

void LowPower_Init(void);
void LowPower_Task(void * pvParameters);
void SystemClock_ReConfig(void);

#ifdef __cplusplus
}
#endif

#endif
