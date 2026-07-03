#ifndef __BH1750_H
#define __BH1750_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"

#define BH1750_I2C_ADDR    (0x23 << 1)                   

/* 数据手册指令集 */
#define BH1750_POWER_DOWN   0x00                         
#define BH1750_POWER_ON     0x01                         
#define BH1750_RESET        0x07                         
#define BH1750_CONTINUOUS_HIGH_RES  0x10                 
#define BH1750_CONTINUOUS_HIGH_RES2 0x11                 
#define BH1750_CONTINUOUS_LOW_RES   0x13                 
#define BH1750_ONESHOT_HIGH_RES     0x20                 
#define BH1750_ONESHOT_HIGH_RES2    0x21
#define BH1750_ONESHOT_LOW_RES      0x23

void BH1750_Init(void);
void BH1750_Reset(void);                                 
uint8_t BH1750_SetSensitivity(uint8_t mt_value);         
uint8_t BH1750_Read(float * light_intensity);


#ifdef __cplusplus
}
#endif

#endif
