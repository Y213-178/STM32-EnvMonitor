/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gpio.c
  * @brief   This file provides code for the configuration
  *          of all used GPIO pins.
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
#include "gpio.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure GPIO                                                             */
/*----------------------------------------------------------------------------*/
/* USER CODE BEGIN 1 */

/* USER CODE END 1 */

/** Configure pins as
        * Analog
        * Input
        * Output
        * EVENT_OUT
        * EXTI
        * Free pins are configured automatically as Analog (this feature is enabled through
        * the Code Generation settings)
*/
void MX_GPIO_Init(void)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA8 PA11 PA12 PA15
   * 手改（v3.0）：PA0 和 PA1 从 ANALOG 组移出，原因见下面两段。 */
  GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_11
                          |GPIO_PIN_12|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 手改（v3.0）：PA0 = PWR 按键 + WKUP 唤醒脚。
   * 新板在 PA0 上加了 R13(100k) 下拉 + C18(100nF) 消抖，
   * 这里再开内部下拉做双保险：避免 STANDBY 期间引脚悬空被 ESP8266 的
   * 射频噪声误拉高而意外唤醒。
   * 注意：PA0 是 WKUP 脚，按键按下是接 V3.3_A（高电平触发唤醒），
   *       所以不能配 PULLUP，否则按键松开后仍是高电平。 */
  GPIO_InitStruct.Pin   = GPIO_PIN_0;
  GPIO_InitStruct.Mode  = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull  = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* 手改（v3.0）：PA1 = 电池电压检测（BAT+ 经 R14/R15 100k 分压，1/2）。
   * 必须配成 ANALOG（不是 INPUT）：ADC 采样时要求引脚处于模拟模式，
   * 否则输入保护二极管和上/下拉会影响精度。 */
  GPIO_InitStruct.Pin  = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_Pin */
  GPIO_InitStruct.Pin = CS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB14 PB15 PB3 PB4
                           PB8 PB9
   * 手改（v3.0）：PB13 从 ANALOG 组移出 —— 新板用它控制 PMS5003 的 RST。 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_14|GPIO_PIN_15
                          |GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_8|GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* 手改（v3.0）：PB13 = PMS5003 的 RST 控制脚。
   * PMS5003 数据手册要求 RST 低电平 >= 1ms 复位，平时保持高电平。
   * 原来这个脚是悬空（NC），PMS5003 偶发死机时只能整机断电恢复；
   * 接上 MCU 后可以主动复位。
   * 先写 ODR 再切模式：避免初始化瞬间出现低电平把 PMS5003 拉进复位。 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
  GPIO_InitStruct.Pin   = GPIO_PIN_13;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* ================= 手改：外设电源门控（v3.0 新板）=================
   *
   * 新板电源树：
   *   BAT+ -> Q3(AO3401A) -> BOOST_IN -> 升压模块 -> V5_MAIN ┬-> V5_PMS -> PMS5003
   *                                                          └-> U4 -> V3.3_SW -> ESP8266
   *   BAT+ -> U1(ME6211)  -> V3.3_A  -> MCU + SHT30/BH1750/W25Q64/OLED（常开，不门控）
   *
   * PB12 -> Q3 栅极：控【升压模块的输入】。
   *         Q3 栅极经 R8(100k) 上拉到 BAT+(源极) => 上电默认关断（fail-safe）。
   *         写低电平 = 导通 = 升压模块工作 = 才有 V5_MAIN / V5_PMS / V3.3_SW。
   *
   * 为什么上电就拉低（打开升压）：
   *   新板 V3.3_SW（ESP8266 供电）只能从 V5_MAIN 经 U4 得到，而 V5_MAIN 只能由
   *   升压模块产生。如果上电不开升压，ESP8266 永远没电、WiFi 功能不可用。
   *   只在进入 STANDBY 前才关断（见 low_power.c）。
   *
   * PB5：旧板用来控 Q2（V3.3_SW 门控）。新板 Q2 已删除，PB5 无对应电路，
   *      保留为普通推挽输出低电平即可（不影响任何功能）。
   *
   * 必须先写 ODR 再切模式：HAL_GPIO_Init 内部会先设置 ODR 再配置 MODER，
   * 因此下面的顺序不会产生毛刺。
   * CubeMX 重新生成会还原，改完记得重新加回。 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);   /* 打开升压（Q3 导通） */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5,  GPIO_PIN_RESET);   /* 已废弃，保持低电平 */

  GPIO_InitStruct.Pin   = GPIO_PIN_12 | GPIO_PIN_5;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;      /* 外部已有 100k 上拉，不重复配 */
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY1_Pin */
  GPIO_InitStruct.Pin = KEY1_Pin;
  /* 手改：单沿→双边沿。KEY1 需要在松手时刻判断按住时长（单按/长按），
   * 按下和松开都要进中断。CubeMX 重新生成会还原。 */
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : KEY2_Pin */
  GPIO_InitStruct.Pin = KEY2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEY2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  /* 手改：优先级 5→15。EXTI1(KEY1) 回调里调用了 FreeRTOS API（xSemaphoreGiveFromISR），
   * 优先级 5 低于 configMAX_SYSCALL_INTERRUPT_PRIORITY(191→数值11) 可抢占内核临界区，
   * 有概率破坏调度器；提到 15 与 RTC 闹钟一致后安全。CubeMX 重新生成会还原。 */
  HAL_NVIC_SetPriority(EXTI1_IRQn, 15, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

}

/* USER CODE BEGIN 2 */

/* USER CODE END 2 */
