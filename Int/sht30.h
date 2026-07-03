#ifndef __SHT30_H                                           
#define __SHT30_H

#ifdef __cplusplus                                          
extern "C" {                                                
#endif                                                      

#include <stdint.h>                                         
#include "main.h"                                           

#define SHT30_I2C_ADDR    (0x44 << 1)                       

void SHT30_Init(void);
uint8_t SHT30_Read(float *temperature,float *humidity);

#ifdef __cplusplus
}
#endif

#endif
