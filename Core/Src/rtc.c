/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    rtc.c
  * @brief   This file provides code for the configuration
  *          of the RTC instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "rtc.h"
#include <stdio.h>

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

RTC_HandleTypeDef hrtc;

/* RTC init function */
void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
   */
  /* 手改的生成段（CubeMX 重新生成会被还原）：STANDBY 唤醒复位后跳过时间初始化，
   * 保留 RTC 跨休眠的连续计时（只有上电/冷启动才清零）。与 main.c 的唤醒分支配合。 */
  if (__HAL_PWR_GET_FLAG(PWR_FLAG_SB) == RESET)
  {
    sTime.Hours = 0x0;
    sTime.Minutes = 0x0;
    sTime.Seconds = 0x0;

    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }
    DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
    DateToUpdate.Month = RTC_MONTH_JANUARY;
    DateToUpdate.Date = 0x1;
    DateToUpdate.Year = 0x0;

    if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
    {
      Error_Handler();
    }
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

void HAL_RTC_MspInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspInit 0 */

  /* USER CODE END RTC_MspInit 0 */
    HAL_PWR_EnableBkUpAccess();
    /* Enable BKP CLK enable for backup registers */
    __HAL_RCC_BKP_CLK_ENABLE();
    /* RTC clock enable */
    __HAL_RCC_RTC_ENABLE();

    /* RTC interrupt Init */
    HAL_NVIC_SetPriority(RTC_Alarm_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(RTC_Alarm_IRQn);
  /* USER CODE BEGIN RTC_MspInit 1 */

  /* USER CODE END RTC_MspInit 1 */
  }
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef* rtcHandle)
{

  if(rtcHandle->Instance==RTC)
  {
  /* USER CODE BEGIN RTC_MspDeInit 0 */

  /* USER CODE END RTC_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_RTC_DISABLE();

    /* RTC interrupt Deinit */
    HAL_NVIC_DisableIRQ(RTC_Alarm_IRQn);
  /* USER CODE BEGIN RTC_MspDeInit 1 */

  /* USER CODE END RTC_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

#include "FreeRTOS.h"
#include "semphr.h"

void RTC_Set_Alarm_Seconds(uint32_t seconds)
{
    RTC_AlarmTypeDef sAlarm = {0};
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // 0. 等待RTC寄存器同步（STM32F1特有，必须加）
    HAL_RTC_WaitForSynchro(&hrtc);

    // 1. 必须先读日期，再读时间！否则时间读取无效（HAL库的坑）
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    // 2. 正确计算时/分/秒，支持跨分钟/小时
    uint32_t total_sec = sTime.Hours * 3600 + sTime.Minutes * 60 + sTime.Seconds + seconds;
    uint32_t alarm_hour = (total_sec / 3600) % 24;
    uint32_t alarm_min = (total_sec % 3600) / 60;
    uint32_t alarm_sec = total_sec % 60;

    // 3. 配置闹钟参数
    sAlarm.AlarmTime.Hours = alarm_hour;
    sAlarm.AlarmTime.Minutes = alarm_min;
    sAlarm.AlarmTime.Seconds = alarm_sec;
    sAlarm.Alarm = RTC_ALARM_A;

    // 4. 先关闭旧闹钟，彻底清除所有中断标志
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);

    // 5. 设置新闹钟并开启中断
    HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN);

    printf("[RTC] 闹钟已设置，%u秒后唤醒（目标时间：%02u:%02u:%02u）\r\n",
           seconds, alarm_hour, alarm_min, alarm_sec);
}

/* 返回从 2000-01-01 00:00:00 起累计的连续秒数（RTC 在 STANDBY 期间持续计数，不被唤醒复位清零）。
 * 用于数据帧时间戳：比 xTaskGetTickCount() 更能反映真实发生时间。 */
uint32_t RTC_GetSecondsSinceEpoch(void)
{
    RTC_DateTypeDef sDate = {0};
    RTC_TimeTypeDef sTime = {0};
    static const uint8_t mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t days = 0;
    uint16_t year;

    HAL_RTC_WaitForSynchro(&hrtc);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);   /* 必须先读日期，再读时间 */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    /* 计算从 2000 年到当前年的整年天数 */
    year = 2000U + sDate.Year;
    for (uint16_t y = 2000U; y < year; y++)
    {
        days += (((y % 4U) == 0U && (y % 100U) != 0U) || (y % 400U) == 0U) ? 366U : 365U;
    }
    /* 加上当前年已过的完整月天数（含闰年二月） */
    for (uint8_t m = 1U; m < sDate.Month; m++)
    {
        days += mdays[m - 1U];
        if (m == 2U && (((year % 4U) == 0U && (year % 100U) != 0U) || (year % 400U) == 0U))
        {
            days += 1U;
        }
    }
    days += (sDate.Date - 1U);

    return days * 86400UL + sTime.Hours * 3600UL + sTime.Minutes * 60UL + sTime.Seconds;
}

/* USER CODE END 1 */
