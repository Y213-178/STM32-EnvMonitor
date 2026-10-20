#include "w25q64.h"
#include "task.h"
#include <stdio.h>      /* 诊断打印用 */

/* 兼容 W25Q128（16MB）：市面上很多模块实际焊的是 W25Q128，JEDEC ID 为 0xEF4018。
 * 两者指令集、扇区大小(4KB)、页大小(256B)完全相同，只是容量不同；
 * 本工程只用低 8MB，所以按 W25Q64 的地址空间使用是完全安全的。 */
#ifndef W25Q128_JEDEC_ID
#define W25Q128_JEDEC_ID          0xEF4018
#endif
#define W25Q_JEDEC_ID_MATCH(id)   (((id) == W25Q64_JEDEC_ID) || ((id) == W25Q128_JEDEC_ID))

#define W25Q_CMD_WRITE_ENABLE     0x06
#define W25Q_CMD_WRITE_DISABLE    0x04
#define W25Q_CMD_READ_STATUS1     0x05
#define W25Q_CMD_READ_DATA        0x03
#define W25Q_CMD_PAGE_PROGRAM     0x02
#define W25Q_CMD_SECTOR_ERASE     0x20
#define W25Q_CMD_JEDEC_ID         0x9F

static void W25Q64_WaitForBusy(void)
{
    uint8_t status_reg = 0;
    W25Q_CS_LOW();
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd, 1, 100);                    
    do
    {
        HAL_SPI_Receive(&W25Q_SPI_HANDLE, &status_reg, 1, 100);          
        vTaskDelay(pdMS_TO_TICKS(1));                                    
    } while ((status_reg & 0x01) == 0x01);                               
    W25Q_CS_HIGH();                                                      
}

uint8_t W25Q64_Init(void)
{
    uint32_t chip_id = 0;
    uint8_t  retry;

    W25Q_CS_HIGH();
    /* 上电等待：W25Q64 数据手册 tPUW（Power-Up to Write/Read）最大 5ms，
     * 但 V3.3_A 要等 U1 建立输出 + C16/C17/C3 充电，这里给 50ms 充足余量。 */
    vTaskDelay(pdMS_TO_TICKS(50));

    /* 先发 0xAB（Release from Deep Power-Down），把芯片从深度掉电态唤醒。
     * 原因：W25Q64 挂在 V3.3_A（常开轨）上，MCU 复位/STANDBY 唤醒后它可能
     * 仍保持 W25Q64_PrepareForSleep() 发过的 0xB9 深度掉电状态，此时不认 0x9F。
     * 若芯片本来就不在掉电态，发 0xAB 也是无害的。 */
    {
        uint8_t cmd_release = 0xAB;   /* Release Power-Down */
        W25Q_CS_LOW();
        HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd_release, 1, 100);
        W25Q_CS_HIGH();
        vTaskDelay(pdMS_TO_TICKS(5));  /* tRES1 约 3µs，这里给足 */
    }

    if(xSemaphoreTake(g_SPI_Mutex, pdMS_TO_TICKS(1000)) != pdTRUE)       
    {
        return 1;
    }

    retry = 5;
    while(retry--)
    {
        chip_id = W25Q64_ReadJEDEC_ID();
        if(W25Q_JEDEC_ID_MATCH(chip_id)) break;                            
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    xSemaphoreGive(g_SPI_Mutex);

    /* 诊断：把实际读到的 ID 打出来
     *   0x000000 -> MISO 被拉低（没通信 / 芯片没供电）
     *   0xFFFFFF -> MISO 悬空为高（CS 没生效 / 芯片没响应）
     *   0xEF4017 -> W25Q64 (8MB)  ，正常
     *   0xEF4018 -> W25Q128(16MB) ，正常（用低 8MB）
     *   其它     -> 别的芯片或通信异常 */
    printf("[W25Q64] JEDEC ID = 0x%06lX (%s)\r\n",
           (unsigned long)chip_id,
           (chip_id == W25Q64_JEDEC_ID)  ? "W25Q64, 8MB"  :
           (chip_id == W25Q128_JEDEC_ID) ? "W25Q128, 16MB (use low 8MB)" :
                                           "UNKNOWN!");

    if(W25Q_JEDEC_ID_MATCH(chip_id))
    {
        return 0;
    }
    return 2;                                                           
}

uint32_t W25Q64_ReadJEDEC_ID(void)                                      
{
    uint8_t id_buf[3] = {0};
    uint8_t cmd = W25Q_CMD_JEDEC_ID;

    W25Q_CS_LOW();
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd, 1, 100);                   
    HAL_SPI_Receive(&W25Q_SPI_HANDLE, id_buf, 3, 100);                  
    W25Q_CS_HIGH();

    return ((uint32_t)id_buf[0] << 16) | ((uint32_t)id_buf[1] << 8) | (uint32_t)id_buf[2];  
}

void W25Q64_EraseSector(uint32_t sector_addr)                                                   
{
    sector_addr &= 0xFFFFF000;                                                 

    if(xSemaphoreTake(g_SPI_Mutex, pdMS_TO_TICKS(2000)) != pdTRUE)         
    {
        return;
    }
	
    W25Q_CS_LOW();
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd, 1, 100);                      
    W25Q_CS_HIGH();
    vTaskDelay(pdMS_TO_TICKS(1));                                          

    W25Q_CS_LOW();                                        
    uint8_t erase_cmd[4] = {
        W25Q_CMD_SECTOR_ERASE,
        (sector_addr >> 16) & 0xFF,                                        
        (sector_addr >> 8) & 0xFF,                                         
        sector_addr & 0xFF                                                 
    };
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, erase_cmd, 4, 100);                       
    W25Q_CS_HIGH();

    W25Q64_WaitForBusy();                                                   
    xSemaphoreGive(g_SPI_Mutex);
}

void W25Q64_WritePage(uint8_t *write_buf, uint32_t page_addr, uint16_t write_len) 
{
    if(write_len > W25Q64_PAGE_SIZE) 
	{
		write_len = W25Q64_PAGE_SIZE;                                        
	}
	
    if(xSemaphoreTake(g_SPI_Mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        return;
    }

    W25Q_CS_LOW();
    uint8_t cmd = W25Q_CMD_WRITE_ENABLE;
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd, 1, 100);                           
    W25Q_CS_HIGH();
    vTaskDelay(pdMS_TO_TICKS(1));

    W25Q_CS_LOW();
    uint8_t write_cmd[4] = {
        W25Q_CMD_PAGE_PROGRAM,
        (page_addr >> 16) & 0xFF,
        (page_addr >> 8) & 0xFF,
        page_addr & 0xFF
    };
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, write_cmd, 4, 100);                 
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, write_buf, write_len, 200);         
    W25Q_CS_HIGH();

    W25Q64_WaitForBusy();                                                  
    xSemaphoreGive(g_SPI_Mutex);
}

void W25Q64_ReadData(uint8_t *read_buf, uint32_t read_addr, uint32_t read_len)                        
{
    if(xSemaphoreTake(g_SPI_Mutex, pdMS_TO_TICKS(1000)) != pdTRUE)
    {
        return;
    }

    W25Q_CS_LOW();
    uint8_t read_cmd[4] = {
        W25Q_CMD_READ_DATA,
        (read_addr >> 16) & 0xFF,
        (read_addr >> 8) & 0xFF,
        read_addr & 0xFF
    };
    HAL_SPI_Transmit(&W25Q_SPI_HANDLE, read_cmd, 4, 100);
    HAL_SPI_Receive(&W25Q_SPI_HANDLE, read_buf, read_len, 500);
    W25Q_CS_HIGH();

    xSemaphoreGive(g_SPI_Mutex);
}

void W25Q64_WriteBuffer(uint8_t *write_buf, uint32_t start_addr, uint32_t write_len)                    
{
    uint16_t page_remain;
    uint32_t current_addr = start_addr;
    uint32_t len_remaining = write_len;
    uint8_t *p_buf = write_buf;

    while(len_remaining > 0)
    {
        page_remain = W25Q64_PAGE_SIZE - (current_addr % W25Q64_PAGE_SIZE);                           
        if(len_remaining < page_remain)
        {
            page_remain = len_remaining;
        }

        W25Q64_WritePage(p_buf, current_addr, page_remain);                                           

        current_addr += page_remain;
        p_buf        += page_remain;
        len_remaining -= page_remain;
    }
}

static void W25Q64_ReadMgmt(FlashMgmt_t *mgmt)                                                        
{
    W25Q64_ReadData((uint8_t*)mgmt, W25Q64_MGMT_ADDR, sizeof(FlashMgmt_t));
    
    if(mgmt->magic != FLASH_MAGIC)                                                                    
    {
        mgmt->write_ptr  = W25Q64_DATA_START_ADDR;                                                    
        mgmt->read_ptr   = W25Q64_DATA_START_ADDR;
        mgmt->data_count = 0;
        mgmt->magic      = FLASH_MAGIC;
    }
}

static void W25Q64_WriteMgmt(FlashMgmt_t *mgmt)                                                       
{
    W25Q64_EraseSector(W25Q64_MGMT_ADDR);                                                             
    W25Q64_WriteBuffer((uint8_t*)mgmt, W25Q64_MGMT_ADDR, sizeof(FlashMgmt_t));                        
}

void W25Q64_StoreData(SensorDataFrame_t *data)                                                        
{
    FlashMgmt_t mgmt;
    W25Q64_ReadMgmt(&mgmt);                                                                           

    if(mgmt.write_ptr + sizeof(SensorDataFrame_t) > W25Q64_MGMT_ADDR)                                                                      
    {
        mgmt.write_ptr = W25Q64_DATA_START_ADDR;
    }

    W25Q64_WriteBuffer((uint8_t*)data, mgmt.write_ptr, sizeof(SensorDataFrame_t));                    
                                                                                                      
    mgmt.write_ptr += sizeof(SensorDataFrame_t);                                                      
    mgmt.data_count++;                                                                                
    W25Q64_WriteMgmt(&mgmt);                                                                          
}

uint8_t W25Q64_RetrieveData(SensorDataFrame_t *data)                                                  
{
    FlashMgmt_t mgmt;
    W25Q64_ReadMgmt(&mgmt);                                                                           

    if(mgmt.data_count == 0)                                                                          
    {
        return 1;                                                                                     
    }

    W25Q64_ReadData((uint8_t*)data, mgmt.read_ptr, sizeof(SensorDataFrame_t));                        
	
                                                                                                      
    mgmt.read_ptr += sizeof(SensorDataFrame_t);                                                       
    if(mgmt.read_ptr + sizeof(SensorDataFrame_t) > W25Q64_MGMT_ADDR)                                  
    {
        mgmt.read_ptr = W25Q64_DATA_START_ADDR;
    }
    mgmt.data_count--;                                                                                
    W25Q64_WriteMgmt(&mgmt);                                                                          

    return 0;                                                                                         
}

void W25Q64_PrepareForSleep(void)
{
    /* 发 0xB9（Deep Power-Down）：待机电流从约 10~15µA 降到约 1µA。
     * 芯片不会被断电（挂在 V3.3_A 常开轨上），所以下次上电必须在
     * W25Q64_Init() 里先发 0xAB 把它唤醒。 */
    {
        uint8_t cmd_dp = 0xB9;   /* Deep Power-Down */
        W25Q_CS_LOW();
        HAL_SPI_Transmit(&W25Q_SPI_HANDLE, &cmd_dp, 1, 100);
        W25Q_CS_HIGH();
        vTaskDelay(pdMS_TO_TICKS(1));   /* tDP 约 3µs */
    }

    W25Q_CS_HIGH();                                                                                   
    __HAL_SPI_DISABLE(&W25Q_SPI_HANDLE);                                                              
    vTaskDelay(pdMS_TO_TICKS(5));                                                                     
}
