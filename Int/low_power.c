#include "low_power.h"
#include "freertos_demo.h"
#include "oled.h"
#include "esp8266.h"
#include "rtc.h"
#include "spi.h"
#include "i2c.h"
#include "usart.h"
#include "w25q64.h"
#include "bh1750.h"
#include <stdio.h>
#include <string.h>

#define ESP_RX_BUF_SIZE    256U
#define LOW_POWER_DEBUG    1U

/* 休眠时长（秒）= RTC 闹钟唤醒周期 = 云平台数据更新周期。
 * 电池供电取中周期 30 分钟；
 * 改这里的同时，必须同步修改：
 * ① OneNet 控制台的数据超时判定（当前 60 分钟）
 * ② Android App 的历史数据拉取间隔。 */
#define LOW_POWER_SLEEP_SECONDS   (30U * 60U)

uint8_t g_low_power_flag = LOW_POWER_FLAG_NORMAL;
SemaphoreHandle_t g_low_power_sem = NULL;
uint8_t g_wakeup_reason = WAKEUP_REASON_KEY;
uint8_t g_low_power_action = LOW_POWER_ACTION_PERIODIC;   /* 默认周期模式；进 STANDBY 前都会被重新赋值 */

extern void SystemClock_Config(void);

void SystemClock_ReConfig(void)
{
    SystemClock_Config();
}

/* 中断清场：确保进入 STANDBY 时 __WFI() 前没有任何 pending 中断。
 * STM32F1 HAL 的 HAL_PWR_EnterSTANDBYMode() 只是 PDDS + SLEEPDEEP + 裸 __WFI()，
 * 若执行瞬间有中断 pending（SysTick/TIM2/USART3 等），WFI 会立即返回，STANDBY 进不去。 */
static void PrepareInterruptCleanup(void)
{
    /* 停 FreeRTOS 时基 SysTick 与 HAL 时基 TIM2（不再产生 1ms 节拍） */
    SysTick->CTRL = 0U;
    TIM2->CR1 &= ~(uint16_t)TIM_CR1_CEN;

    /* 关闭会产生中断的外设时钟：USART2(ESP8266 RX)、USART3(PMS5003 RX)、I2C1、SPI1 */
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_USART3_CLK_DISABLE();
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_SPI1_CLK_DISABLE();

    /* 屏蔽并清空 NVIC 待处理标志（USART1 保留：printf 走它，且无 RX 中断） */
    NVIC_DisableIRQ(TIM2_IRQn);
    NVIC_DisableIRQ(USART1_IRQn);
    NVIC_DisableIRQ(USART2_IRQn);
    NVIC_DisableIRQ(USART3_IRQn);
    NVIC_DisableIRQ(EXTI1_IRQn);
    NVIC_ClearPendingIRQ(TIM2_IRQn);
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_ClearPendingIRQ(USART2_IRQn);
    NVIC_ClearPendingIRQ(USART3_IRQn);
    NVIC_ClearPendingIRQ(EXTI1_IRQn);
}

/* 用 F1 专有方式进入 STANDBY 待机模式（唤醒后系统复位，不再运行本函数之后的代码）。
 * arm_alarm：1=周期模式（设 30min 闹钟，RTC 自动唤醒）；0=关机（无闹钟，仅 PA0 唤醒）。 */
static void Enter_STANDBY_Mode(uint8_t arm_alarm)
{
    g_low_power_flag = LOW_POWER_FLAG_SLEEPING;

    /* 确保 PWR 时钟已打开（访问 PWR 寄存器必需） */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* ===== 1.5 切断外设电源（新板 v3.0）=====
     *
     * 新板电源树：
     *   BAT+ -> Q3(P-MOS) -> BOOST_IN -> 升压模块 -> V5_MAIN ┬-> V5_PMS -> PMS5003
     *                                                         └-> U4 -> V3.3_SW -> ESP8266
     *   BAT+ -> U1(ME6211) -> V3.3_A -> MCU + SHT30/BH1750/W25Q64/OLED（常开，不门控）
     *
     * PB12 拉高 => Q3 关断 => 升压模块 + U4 + ESP8266 + PMS5003 全部断电
     *           => MT3608 的约 2mA 静态电流 + LDO 的静态电流 一起消失。
     *
     * 顺序很重要：先让外设"准备好睡"、把跨域信号摆到安全电平，再断它的电。 */

    /* (1) 关显示 + W25Q64 深度掉电
     *     OLED：先清屏，再关显示(0xAE)+关电荷泵(0x8D/0x10)。
     *           只清 GDDRAM 时面板和电荷泵仍在工作，待机约 100~200µA；
     *           关显示后约 10µA，同时彻底消除残影。
     *     W25Q64：发 0xB9 进深度掉电，待机从约 10~15µA 降到约 1µA。
     *             它挂在 V3.3_A 常开轨上，下次上电由 W25Q64_Init() 发 0xAB 唤醒。 */
    OLED_Clear();
    OLED_DisplayOff();
    vTaskDelay(pdMS_TO_TICKS(20));   /* 等最后几条 I2C 命令发完 */

    /* BH1750 在 BH1750_Init() 里被设成连续测量模式(0x10)，会一直测光，
     * 数据手册典型 120µA。这里主动发 Power Down(0x00) 关掉，降到约 0.1µA。
     * 唤醒后 SensorTask 会重新 BH1750_Init() 恢复。 */
    BH1750_PowerDown();
    vTaskDelay(pdMS_TO_TICKS(5));    /* 等 I2C 传输完成 */

    W25Q64_PrepareForSleep();

    /* (2) 把跨域的 UART 引脚主动驱动为低，防止 MCU 经 PA2 向已断电的 ESP8266 倒灌。
     *     PA2 = USART2_TX -> ESP8266.RX。ESP8266 掉电后其 RX 端的 ESD 保护二极管
     *     仍然存在，若 PA2 保持高电平，电流会从 MCU 经 PA2 -> ESP8266 的 ESD 二极管
     *     -> 已断电的 V3.3_SW 轨，形成 mA 级泄漏，把睡眠电流拉高。
     *     配成推挽输出低电平后，PA2 变成 0V，没有电位差就不会倒灌。
     *     PA3 = USART2_RX（MCU 的输入），保持输入态即可，不会向外输出电流。 */
    {
        GPIO_InitTypeDef uart_pin = {0};

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);   /* 先写 ODR */
        uart_pin.Pin   = GPIO_PIN_2;
        uart_pin.Mode  = GPIO_MODE_OUTPUT_PP;                  /* 主动拉低，而不是浮空 */
        uart_pin.Pull  = GPIO_NOPULL;
        uart_pin.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOA, &uart_pin);
    }

    /* (3) SPI 的 CS 保持高（W25Q64 常开供电，CS 高 = 不选中，避免误命令） */
    HAL_GPIO_WritePin(CS_GPIO_Port, CS_Pin, GPIO_PIN_SET);

    /* (4) 关断整条升压链：PB12 拉高 => Q3(P-MOS) 关断 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

    /* 说明：PB5 在新板上已无对应电路（Q2 已删除），不再需要操作。
     * STANDBY 唤醒后系统复位、GPIO 回默认态，随后 MX_GPIO_Init() 会重新
     * 把 PB12 拉低打开升压，天然自洽，不需要写"唤醒后恢复"的代码。 */

    /* 2. RTC 闹钟：周期模式设 30min 自动唤醒；关机模式取消残留闹钟，只有 PA0 能唤醒 */
    if (arm_alarm)
    {
        printf("[LowPower] Entering STANDBY (periodic), wake in %u s\r\n", LOW_POWER_SLEEP_SECONDS);
        RTC_Set_Alarm_Seconds(LOW_POWER_SLEEP_SECONDS);
    }
    else
    {
        printf("[LowPower] Entering STANDBY (off), PA0 to wake\r\n");
        HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);
        __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
    }

    /* 3. 把 PA0 配置为 WKUP 唤醒脚（内部下拉，按键拉高触发唤醒；新 PCB 接键后用） */
    {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_0;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLDOWN;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }

    /* 4. 中断清场：确保 WFI 时无 pending 中断 */
    PrepareInterruptCleanup();

    /* 5. 清除唤醒标志 + 使能 PA0(WKUP) 硬件唤醒 */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
    HAL_PWR_EnableWakeUpPin(PWR_WAKEUP_PIN1);

    /* 6. 屏蔽所有中断后进入 STANDBY。
     *    唤醒由 RTC 闹钟 / PA0 硬件事件直接触发（绕过 NVIC/PRIMASK），不受此影响。 */
    __disable_irq();
    HAL_PWR_EnterSTANDBYMode();

    /* 7. 安全网：正常情况下进 STANDBY 后系统复位，不会执行到这里。
     *    若 WFI 因残留中断未能进入 STANDBY，主动复位恢复干净状态，避免半死。 */
    __enable_irq();
    CLEAR_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);
    PWR->CR &= ~PWR_CR_PDDS;
    printf("[LowPower] STANDBY entry FAILED, resetting to recover\r\n");
    NVIC_SystemReset();
    while (1) { }   /* 永不返回 */
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
        Enter_STANDBY_Mode(g_low_power_action == LOW_POWER_ACTION_PERIODIC);
    }
}
