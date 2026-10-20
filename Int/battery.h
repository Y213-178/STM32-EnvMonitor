#ifndef __BATTERY_H
#define __BATTERY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ===== 电池电压检测（新板 v3.0 新增） =====
 *
 * 硬件：
 *   BAT+ --[R14 100k]--+--[R15 100k]-- GND
 *                      |
 *                     PA1 (ADC1_IN1) + C20(100nF) 对地滤波
 *
 * 分压比 = 1/2，所以：Vbat = Vadc x 2
 * 参考电压 = VDD = 3.3V（12 位 ADC：0..4095）
 *
 * 注意：ADC1 是全局共享资源，NetworkTask 和 SensorTask 都会调用
 *       Battery_ReadVoltage()，内部用互斥量保护，不要在中断里调用。
 */

/* 初始化（创建互斥量 + 配置 ADC1）。必须在 FreeRTOS 启动后的任务上下文里
 * 调用一次，建议放在 start_task 中。重复调用是安全的。 */
void Battery_Init(void);

/* 电池电压（伏特）。内部做 16 次采样平均，耗时约 0.5ms。 */
float Battery_ReadVoltage(void);

/* 最近一次读到的电压（避免重复读取，其它模块直接取这个值） */
extern float g_battery_voltage;

/* 电压 -> 百分比（3.30V=0%，4.20V=100%，线性估算，仅用于显示） */
uint8_t Battery_GetPercent(float voltage);

/* 是否低于保护阈值（1 = 电量过低，应主动关机） */
uint8_t Battery_IsLow(float voltage);

/* ===== 阈值定义 =====
 * 3.40V：锂电放电曲线在 3.4V 以下急剧下降，再放会损伤电池；
 *        而且 ME6211 输入 3.4V 时 dropout 只剩约 0.1V，再低就稳压不住 3.3V 了。
 */
#define BATTERY_LOW_VOLTAGE     3.40f
#define BATTERY_EMPTY_VOLTAGE   3.30f
#define BATTERY_FULL_VOLTAGE    4.20f

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_H */
