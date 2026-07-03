#ifndef __W25Q64_H
#define __W25Q64_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <stdint.h>

/* 硬件配置 */
#define W25Q_SPI_HANDLE    hspi1
#define W25Q_CS_PORT       CS_GPIO_Port
#define W25Q_CS_PIN        CS_Pin

/* W25Q64核心参数 */
#define W25Q64_JEDEC_ID        0xEF4017
#define W25Q64_PAGE_SIZE       256            
#define W25Q64_SECTOR_SIZE     4096           
#define W25Q64_BLOCK_SIZE      65536          
#define W25Q64_TOTAL_CAPACITY  (8*1024*1024)  

/* 把CS引脚封装成宏 */
#define W25Q_CS_LOW()  HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_RESET)
#define W25Q_CS_HIGH() HAL_GPIO_WritePin(W25Q_CS_PORT, W25Q_CS_PIN, GPIO_PIN_SET)

/* 全局SPI互斥锁声明 */
extern SemaphoreHandle_t g_SPI_Mutex;

/* 核心功能函数 */
uint8_t  W25Q64_Init(void);
uint32_t W25Q64_ReadJEDEC_ID(void);
void     W25Q64_EraseSector(uint32_t sector_addr);
void     W25Q64_WritePage(uint8_t *write_buf, uint32_t page_addr, uint16_t write_len);
void     W25Q64_ReadData(uint8_t *read_buf, uint32_t read_addr, uint32_t read_len);

/* 存储数据帧结构（大小：4 + 4 + 4 + 2 + 4 = 18 字节） */
typedef struct {
    float    temperature;                      
    float    humidity;                         
    float    light;                            
    uint16_t pm25;                             
    uint32_t timestamp;                        
}   SensorDataFrame_t;

/* Flash存储区域划分 */
#define W25Q64_DATA_START_ADDR    0x000000UL   
#define W25Q64_MGMT_ADDR          0x7FF000UL   
#define FLASH_MAGIC               0x5A5A5A5A   

/* 管理区数据结构：记录Flash的使用状态（大小：4 + 4 + 4 + 4 = 16 字节） */
typedef struct {
    uint32_t write_ptr;                        
    uint32_t read_ptr;                         
    uint32_t data_count;                       
    uint32_t magic;                            
}   FlashMgmt_t;

/* 存储管理函数声明 */
void    W25Q64_WriteBuffer(uint8_t *write_buf, uint32_t start_addr, uint32_t write_len);
void    W25Q64_StoreData(SensorDataFrame_t *data);
uint8_t W25Q64_RetrieveData(SensorDataFrame_t *data);
void    W25Q64_PrepareForSleep(void);

#ifdef __cplusplus
}
#endif

#endif
