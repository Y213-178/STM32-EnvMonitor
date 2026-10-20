/* ============================================================================
 *  battery.c  --  电池电压检测（新板 v3.0 新增）
 *
 *  硬件连接：
 *      BAT+ --[R14 100k]--+--[R15 100k]-- GND
 *                         |
 *                        PA1 (ADC1_IN1)  + C20(100nF) 对地滤波
 *      分压比 1/2  ->  Vbat = Vadc x 2
 *
 *  实现方式：直接操作 ADC1 寄存器。
 *  原因：本工程的 stm32f1xx_hal_conf.h 里 HAL_ADC_MODULE_ENABLED 是注释掉的
 *        （CubeMX 没勾 ADC），而 STM32F1 的 ADC 配置足够简单，
 *        寄存器直访可以避免改动 HAL 配置 + 重开 CubeMX 工程的麻烦。
 *        ADC1 也没有中断/DMA 需求，纯阻塞单次转换即可。
 *
 *  ADC1 在 APB2 上：72MHz / 6 = 12MHz（<= 14MHz 上限）
 *  校准是 STM32F1 必须做的，否则精度很差。
 *
 *  ===== 并发保护 =====
 *  ADC1 是全局单例，NetworkTask（低压保护）和 SensorTask（周期采集）都会调用
 *  Battery_ReadVoltage()。如果两个任务同时做转换，会互相抢 EOC 标志：
 *  任务 A 读走 DR 清掉 EOC，任务 B 的 while 就永远等不到下一次 EOC
 *  （因为只有主动写 ADON 的任务才会启动新的转换）-> 任务 B 死等。
 *  所以用互斥量把"启动转换 + 等 EOC + 读 DR"这一整段保护起来。
 *  Battery_Init() 必须在任务启动前（start_task 里）调用一次来创建互斥量，
 *  避免两个任务同时"惰性创建"导致创建出两个互斥量。
 * ========================================================================== */

#include "battery.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define ADC_CHANNEL_BATTERY     1U      /* PA1 = ADC1_IN1 */
#define ADC_SAMPLE_COUNT        16U
#define ADC_VREF                3.3f    /* 参考电压 = VDD */
#define ADC_FULL_SCALE          4095.0f /* 12 位 */
#define BATTERY_DIVIDER_RATIO   2.0f    /* R14 = R15 = 100k -> 1/2 */

float g_battery_voltage = 0.0f;

static SemaphoreHandle_t s_adc_mutex = NULL;

/* ADC1 单次初始化 + 校准（只做一次）
 *
 * 序列对齐 STM32F1 HAL 的 HAL_ADCEx_Calibration_Start()：
 *   Disable(ADON=0) -> 配置 -> Enable(ADON=1) -> 等 2 个 ADC 时钟周期 -> 复位校准 -> 校准
 * 先 Disable 再配置可以避免配置改动在转换中生效，也避免校准期间误触发一次转换。 */
static void Battery_ADC_Init(void)
{
    static uint8_t s_inited = 0U;
    uint32_t tmpreg;

    if (s_inited)
    {
        return;
    }
    s_inited = 1U;

    /* --- 1. 开启 ADC1 时钟（APB2）--- */
    __HAL_RCC_ADC1_CLK_ENABLE();

    /* --- 2. ADC 时钟分频：APB2(72MHz) / 6 = 12MHz（<= 14MHz 上限）---
     * 必须在使能 ADC 之前设置好。 */
    tmpreg = RCC->CFGR;
    tmpreg &= ~RCC_CFGR_ADCPRE;                 /* 清 ADCPRE 位（bit15:14） */
    tmpreg |= RCC_CFGR_ADCPRE_DIV6;             /* 11 = ÷6 */
    RCC->CFGR = tmpreg;

    /* --- 3. 先关闭 ADC（ADON=0），再改配置 --- */
    ADC1->CR2 &= ~ADC_CR2_ADON;

    /* --- 4. 转换参数：独立模式、单次、软件触发、右对齐、12 位、无扫描 --- */
    ADC1->CR1  = 0U;                            /* 独立模式，无扫描，无中断 */
    ADC1->CR2 &= ~(ADC_CR2_CONT | ADC_CR2_EXTSEL | ADC_CR2_EXTTRIG);

    /* --- 5. 采样时间：通道 1 用 239.5 周期 ---
     * 分压电阻 R14//R15 = 50kΩ，源阻抗偏高。STM32F1 的 ADC 采样电容只有 8pF，
     * 需要足够长的采样时间让采样电容充满，否则读数会系统性偏低。
     * SMPR2 的 SMP1 字段在 bit5:3，111 = 239.5 cycles */
    ADC1->SMPR2 &= ~(7U << 3);
    ADC1->SMPR2 |=  (7U << 3);

    /* --- 6. 规则序列：长度 1，第 1 个转换通道 = 通道 1 --- */
    ADC1->SQR1 &= ~(0x0FU << 20);               /* L[3:0] = 0000 -> 1 个转换 */
    ADC1->SQR3  = ADC_CHANNEL_BATTERY;          /* SQ1 = 1 */

    /* --- 7. 使能 ADC（ADON 0->1 = 上电唤醒）--- */
    ADC1->CR2 |= ADC_CR2_ADON;

    /* --- 8. 上电后必须等 >= 2 个 ADC 时钟周期才能开始校准 ---
     * 12MHz 下 2 个周期约 0.17µs，这里用软件空转给足余量（约 1ms）。 */
    for (volatile uint32_t i = 0; i < 20000U; i++) { }

    /* --- 9. 复位校准寄存器 --- */
    ADC1->CR2 |= ADC_CR2_RSTCAL;
    while (ADC1->CR2 & ADC_CR2_RSTCAL) { }

    /* --- 10. 启动校准 --- */
    ADC1->CR2 |= ADC_CR2_CAL;
    while (ADC1->CR2 & ADC_CR2_CAL) { }
}

void Battery_Init(void)
{
    /* 在 start_task 里调用：此时调度器已启动、堆可用，且还没有其它任务并发，
     * 所以这里创建互斥量不存在竞争。 */
    if (s_adc_mutex == NULL)
    {
        s_adc_mutex = xSemaphoreCreateMutex();
    }
    Battery_ADC_Init();
}

float Battery_ReadVoltage(void)
{
    uint32_t sum = 0U;
    float    vadc;
    float    raw;
    static float s_filtered = -1.0f;   /* 负值 = 尚未初始化 */

    Battery_ADC_Init();

    /* 保护"启动转换 + 等 EOC + 读 DR"整段，防止另一个任务抢走 EOC */
    if (s_adc_mutex != NULL)
    {
        (void)xSemaphoreTake(s_adc_mutex, portMAX_DELAY);
    }

    for (uint32_t i = 0U; i < ADC_SAMPLE_COUNT; i++)
    {
        ADC1->CR2 |= ADC_CR2_ADON;              /* 启动转换（ADON 写 1 触发） */
        while (!(ADC1->SR & ADC_SR_EOC)) { }
        sum += (uint16_t)(ADC1->DR & 0xFFFFU);  /* 读 DR 同时清 EOC */
    }

    if (s_adc_mutex != NULL)
    {
        (void)xSemaphoreGive(s_adc_mutex);
    }

    /* 平均后换算成电压，再乘分压比的倒数 */
    vadc = ((float)sum / (float)ADC_SAMPLE_COUNT) * ADC_VREF / ADC_FULL_SCALE;
    raw  = vadc * BATTERY_DIVIDER_RATIO;

    /* ===== 一阶低通滤波（EMA）=====
     * 电池有内阻（约 100~200mΩ），负载电流一变端电压就跟着变：
     *   ESP8266 发射 300mA -> 内阻压降 0.15Ω × 0.3A ≈ 45mV
     *   PMS5003 风扇启动   -> 再叠几十 mV
     * 45mV 在 3.30~4.20V 的映射区间里约等于 5%，
     * 直接显示就会看到 34% / 37% / 38% 乱跳。
     *
     * 这里用 EMA 平滑（α = 0.15，SensorTask 每 2 秒调一次 => 时间常数约 13 秒），
     * 既滤掉负载瞬变，又能在十几秒内跟上电池真实趋势。
     *
     * 第一次调用直接取原始值，保证开机时读数和低压保护判断都是真实值。 */
    if (s_filtered < 0.0f)
    {
        s_filtered = raw;
    }
    else
    {
        s_filtered = s_filtered * 0.85f + raw * 0.15f;
    }

    g_battery_voltage = s_filtered;
    return s_filtered;
}

uint8_t Battery_GetPercent(float voltage)
{
    if (voltage >= BATTERY_FULL_VOLTAGE)
    {
        return 100U;
    }
    if (voltage <= BATTERY_EMPTY_VOLTAGE)
    {
        return 0U;
    }
    return (uint8_t)((voltage - BATTERY_EMPTY_VOLTAGE) * 100.0f
                     / (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE));
}

uint8_t Battery_IsLow(float voltage)
{
    return (voltage < BATTERY_LOW_VOLTAGE) ? 1U : 0U;
}
