#ifndef __KEY_H
#define __KEY_H

#include "main.h"

#define KEY1        HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin)   /* ¶ÁÈ¡KEY1Òý½Å×´Ì¬£¨SW3£© */
#define KEY2        HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin)   /* ¶ÁÈ¡KEY2Òý½Å×´Ì¬£¨SW4£© */
#define KEY3        HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin)   /* ¶ÁÈ¡KEY3Òý½Å×´Ì¬£¨SW5£© */
#define KEY4        HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin)   /* ¶ÁÈ¡KEY4Òý½Å×´Ì¬£¨SW6£© */


#define KEY1_PRESS    1              
#define KEY2_PRESS    2              
#define KEY3_PRESS    3              
#define KEY4_PRESS    4              


uint8_t Key_Detect(void);


#endif
