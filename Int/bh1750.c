#include "bh1750.h"
#include "FreeRTOS.h"
#include "task.h"                                                        
#include "i2c.h"
#include "semphr.h"

extern SemaphoreHandle_t g_I2C_Mutex;                                    

static float g_sensitivity_factor = 1.0f;                                

void BH1750_Init(void)
{
	uint8_t cmd;                                                         
	
	cmd = BH1750_POWER_DOWN;
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) == pdPASS)              
	{
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd,1,100);      
		xSemaphoreGive(g_I2C_Mutex);
	}
	
	vTaskDelay(pdMS_TO_TICKS(10));                                       
	
	
	cmd = BH1750_POWER_ON;
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) == pdPASS)              
	{
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd,1,100);
		xSemaphoreGive(g_I2C_Mutex);
	}
	
	vTaskDelay(pdMS_TO_TICKS(10));                                       
	
	
	BH1750_Reset();                                                      
	vTaskDelay(pdMS_TO_TICKS(10));
	
	
	cmd = BH1750_CONTINUOUS_HIGH_RES;
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) == pdPASS)              
	{
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd,1,100);
		xSemaphoreGive(g_I2C_Mutex);
	}
	
	
	vTaskDelay(pdMS_TO_TICKS(180));                                      
}

void BH1750_Reset(void)
{
	uint8_t cmd = BH1750_RESET;
	
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) == pdPASS)              
	{
		uint8_t power_on = BH1750_POWER_ON;                              
		
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&power_on,1,100); 
		vTaskDelay(pdMS_TO_TICKS(1));                                    
		
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd,1,100);      
		
		xSemaphoreGive(g_I2C_Mutex);
	}
}

uint8_t BH1750_SetSensitivity(uint8_t mt_value)                                                  
{
	if(mt_value < 31 || mt_value > 254)                                    
	{
		return 1;
	}
	
	uint8_t cmd_high = 0x40 | ((mt_value >> 5) & 0x07);                    
    uint8_t cmd_low = 0x60 | (mt_value & 0x1F);                            
	
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) != pdPASS)                
	{
		return 1;
	}
	
	if(HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd_high,1,100) != HAL_OK)
	{
		xSemaphoreGive(g_I2C_Mutex);                                             
		return 1;
	}
	
	if(HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd_low,1,100) != HAL_OK) 
	{
		xSemaphoreGive(g_I2C_Mutex);                                             
		return 1;
	}
	
	xSemaphoreGive(g_I2C_Mutex);                                                       
	
	g_sensitivity_factor = 69.0f / mt_value;                              
	
	uint8_t cmd = BH1750_CONTINUOUS_HIGH_RES;
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) == pdPASS)              
	{
		HAL_I2C_Master_Transmit(&hi2c1,BH1750_I2C_ADDR,&cmd,1,100);       
		xSemaphoreGive(g_I2C_Mutex);                                      
	}
	
	vTaskDelay(pdMS_TO_TICKS(180));                                       
	
	return 0;
}

uint8_t BH1750_Read(float * light_intensity)                              
{
	uint8_t data[2];                                                      
	uint8_t success = 0;                                                  
	
	if(xSemaphoreTake(g_I2C_Mutex,portMAX_DELAY) != pdPASS)               
	{
		return 1;
	}
	
	for(uint8_t retry = 0; retry < 3; retry++)                            
	{
		if(HAL_I2C_Master_Receive(&hi2c1,BH1750_I2C_ADDR,data,2,100) == HAL_OK)
		{
			success = 1;                                                 
			break;                                                       
		}
		vTaskDelay(pdMS_TO_TICKS(10));                                   
	}
	
	xSemaphoreGive(g_I2C_Mutex);
	
	if(success == 0)                                                     
	{
		BH1750_Reset();
		return 1;
	}
	
	uint16_t raw = (data[0] << 8) | data[1];                             
    *light_intensity = (float)raw / 1.2f * g_sensitivity_factor;         
	
	return 0;
}
