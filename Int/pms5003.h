#ifndef __PMS5003_H
#define __PMS5003_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "usart.h"                  
#include "main.h"

typedef struct                      
{
	uint16_t pm1_0;
	uint16_t pm2_5;
	uint16_t pm10;
}   PMS5003_Data_t;

extern PMS5003_Data_t pms_data;

void PMS5003_Init(void);
uint8_t PMS5003_ReadData(void);     
void PMS5003_UART_Handler(void);

#ifdef __cplusplus
}
#endif

#endif
