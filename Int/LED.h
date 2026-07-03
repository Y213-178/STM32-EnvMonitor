#ifndef __LED_H
#define __LED_H

#include "gpio.h"

#define LED uint16_t
void LED_Turn_On(LED led);
void LED_Turn_Off(LED led);
void LED_Toggle(LED led);

void LED_Turn_Off_All(LED led[], uint8_t len);

#endif
