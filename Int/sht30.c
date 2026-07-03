#include "sht30.h"         
#include "i2c.h"           
#include "FreeRTOS.h"      
#include "semphr.h"

extern SemaphoreHandle_t g_I2C_Mutex;                        

static uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len)  
{
    uint8_t crc = 0xFF;                                      
	
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];                                      
		
        for (uint8_t j = 0; j < 8; j++)                      
        {
            if (crc & 0x80)                                  
                crc = (crc << 1) ^ 0x31;                     
			
            else
                crc <<= 1;                                   
        }
    }
    return crc;
}

void SHT30_Init(void)
{
    
}

uint8_t SHT30_Read(float *temperature, float *humidity)
{
    uint8_t cmd[2] = {0x2C, 0x06};                          
    uint8_t data[6];                                        

    for (uint8_t retry = 0; retry < 3; retry++)                              
    {
        if (xSemaphoreTake(g_I2C_Mutex, portMAX_DELAY) != pdPASS)                   
            return 1;                                                               

        if (HAL_I2C_Master_Transmit(&hi2c1, SHT30_I2C_ADDR, cmd, 2, 100) != HAL_OK) 
        {
            xSemaphoreGive(g_I2C_Mutex);                                            
            vTaskDelay(pdMS_TO_TICKS(20));                                          
            continue;                                                               
        }

        vTaskDelay(pdMS_TO_TICKS(12));                                              

        if (HAL_I2C_Master_Receive(&hi2c1, SHT30_I2C_ADDR, data, 6, 100) != HAL_OK) 
        {
            xSemaphoreGive(g_I2C_Mutex);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        xSemaphoreGive(g_I2C_Mutex);                                                

        if (SHT30_CRC8(&data[0], 2) != data[2])                                     
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (SHT30_CRC8(&data[3], 2) != data[5])                                     
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        uint16_t temp_raw = (data[0] << 8) | data[1];                               
        uint16_t humi_raw = (data[3] << 8) | data[4];

        float t = -45.0f + 175.0f * ((float)temp_raw / 65535.0f);                   
        float h = 100.0f * ((float)humi_raw / 65535.0f);

        if (t < -40.0f || t > 125.0f || h < 0.0f || h > 100.0f)                     
        {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        *temperature = t;                                                           
        *humidity    = h;
        return 0;                                                                   
    }

    return 1;                                                     
}
