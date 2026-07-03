#include "pms5003.h"

PMS5003_Data_t pms_data;                                          
static uint8_t pms_buf[32];                                       
static uint8_t pms_index = 0;                                     
static uint8_t data_valid = 0;                                    

void PMS5003_Init(void)
{
    pms_data.pm1_0 = 0;                                           
    pms_data.pm2_5 = 0;
    pms_data.pm10 = 0;
    pms_index = 0;
    data_valid = 0;

    HAL_UART_Receive_IT(&huart3, &pms_buf[pms_index], 1);         
}

void PMS5003_UART_Handler(void)                                   
{
    if(pms_index == 0 && pms_buf[0] != 0x42)                      
    {
        HAL_UART_Receive_IT(&huart3, &pms_buf[0], 1);             
        return;                                                   
    }

    pms_index++;                                                  

    if(pms_index >= 32)                                           
    {
        if(pms_buf[0] == 0x42 && pms_buf[1] == 0x4D)              
        {
            uint16_t sum = 0;
			
            for(int i=0; i<30; i++) sum += pms_buf[i];            
			
            uint16_t rec_sum = (pms_buf[30]<<8) | pms_buf[31];    

            if(sum == rec_sum)                                    
            {
                pms_data.pm1_0 = (pms_buf[10]<<8) | pms_buf[11];  
                pms_data.pm2_5 = (pms_buf[12]<<8) | pms_buf[13];
                pms_data.pm10  = (pms_buf[14]<<8) | pms_buf[15];
                data_valid = 1;                                   
            }
        }
        pms_index = 0;                                            
    }
    HAL_UART_Receive_IT(&huart3, &pms_buf[pms_index], 1);         
}

uint8_t PMS5003_ReadData(void)                                    
{
    if(data_valid)                                                
    {
        data_valid = 0;                                           
        return 0;                                                 
    }
    return 1;                                                     
}
