#ifndef __ONENET_MQTT_H
#define __ONENET_MQTT_H

#include <stdint.h>

uint8_t Onenet_Connect(void);
uint8_t Onenet_Publish(float temp, float humi, float light, uint16_t pm25);

#endif
