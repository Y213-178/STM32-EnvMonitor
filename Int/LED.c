#include "LED.h"

/**
 * @description: 点亮LED
 * @param {LED} led
 */
void LED_Turn_On(LED led)
{
    HAL_GPIO_WritePin(GPIOA, led, GPIO_PIN_RESET);
}

/**
 * @description: 熄灭LED
 * @param {LED} led
 */
void LED_Turn_Off(LED led)
{
    HAL_GPIO_WritePin(GPIOA, led, GPIO_PIN_SET);
}

/**
 * @description: 翻转LED的状态
 * @param {LED} led
 */
void LED_Toggle(LED led)
{
    HAL_GPIO_TogglePin(GPIOA, led);
}

/**
 * @description: 关闭所有LED
 * @param {LED} led
 * @param {uint8_t} len
 * @return {*}
 */
void LED_Turn_Off_All(LED led[], uint8_t len)
{
    uint8_t i;
    for (i = 0; i < len; i++)
    {
        LED_Turn_Off(led[i]);
    }
}
