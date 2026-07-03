#include "Key.h"
#include "FreeRTOS.h"
#include "task.h"

/**
 * @description: 检测按键
 * @return {*} 按下的按键值
 */
uint8_t Key_Detect(void)
{
    uint8_t res = 0;
    if (KEY1 == GPIO_PIN_RESET || KEY2 == GPIO_PIN_RESET)  
    {
        if (KEY1 == GPIO_PIN_RESET)  res = KEY1_PRESS;
        if (KEY2 == GPIO_PIN_RESET)  res = KEY2_PRESS;
    }
    return res;
}


