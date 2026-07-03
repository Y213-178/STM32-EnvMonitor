//#include "data_cache.h"

//typedef struct                                  // 缓存条目结构体
//{
//    char data[DATA_MAX_LEN];
//    uint8_t is_valid;
//}   CacheItem_t;

//typedef struct                                 // 全局缓存管理结构体
//{
//    CacheItem_t items[CACHE_MAX_COUNT];
//    uint16_t write_index;
//    uint16_t read_index;
//    uint16_t valid_count;
//    SemaphoreHandle_t cache_mutex;
//}   DataCache_t;

//static DataCache_t g_data_cache;               // 静态全局变量 仅本文件可见

//void DataCache_Init(void)                                                               // 缓存初始化（在启动任务里调用1次）
//{
//    memset(&g_data_cache, 0, sizeof(DataCache_t));
//    g_data_cache.cache_mutex = xSemaphoreCreateMutex();
//}

//uint8_t DataCache_Write(char *data)                                                     // 写入数据到缓存
//{
//    if(xSemaphoreTake(g_data_cache.cache_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
//    {
//        return 1; // 拿锁失败
//    }

//    
//    if(g_data_cache.valid_count >= CACHE_MAX_COUNT)                                     // 缓存满了 不写入
//    {
//        xSemaphoreGive(g_data_cache.cache_mutex);
//        return 2;
//    }

//    strncpy(g_data_cache.items[g_data_cache.write_index].data, data, DATA_MAX_LEN-1);   // 写入数据
//    g_data_cache.items[g_data_cache.write_index].is_valid = 1;
//    g_data_cache.write_index = (g_data_cache.write_index + 1) % CACHE_MAX_COUNT;
//    g_data_cache.valid_count++;

//    xSemaphoreGive(g_data_cache.cache_mutex);
//    return 0;
//}

//uint8_t DataCache_Read(char *out_buf, uint16_t buf_len)                                  // 从缓存读取一条数据
//{
//    if(xSemaphoreTake(g_data_cache.cache_mutex, pdMS_TO_TICKS(100)) != pdTRUE)
//    {
//        return 1;
//    }

//    if(g_data_cache.valid_count == 0)                                                     // 无有效数据
//    {
//        xSemaphoreGive(g_data_cache.cache_mutex);
//        return 2;
//    }
//    
//    strncpy(out_buf, g_data_cache.items[g_data_cache.read_index].data, buf_len-1);        // 读取数据
//    g_data_cache.items[g_data_cache.read_index].is_valid = 0;
//    g_data_cache.read_index = (g_data_cache.read_index + 1) % CACHE_MAX_COUNT;
//    g_data_cache.valid_count--;

//    xSemaphoreGive(g_data_cache.cache_mutex);
//    return 0;
//}

//uint16_t DataCache_GetValidCount(void)                                                    // 获取当前有效数据条数（调试用）
//{
//    return g_data_cache.valid_count;
//}
