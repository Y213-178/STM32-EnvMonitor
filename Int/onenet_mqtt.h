#ifndef __ONENET_MQTT_H
#define __ONENET_MQTT_H

#include <stdint.h>

uint8_t Onenet_Connect(void);
/* battery：电池剩余电量百分比（0~100）。
 * 注意它是"设备状态"而不是"环境采样值"，所以补传历史帧时也用当前电量，
 * 不需要把电量一起存进 Flash。 */
uint8_t Onenet_Publish(float temp, float humi, float light, uint16_t pm25, uint8_t battery);

#endif
