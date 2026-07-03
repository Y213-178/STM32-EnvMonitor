//#ifndef __DATA_CACHE_H
//#define __DATA_CACHE_H

//#ifdef __cplusplus
//extern "C" {
//#endif

//#include <stdint.h>
//#include <string.h>
//#include "FreeRTOS.h"
//#include "semphr.h"

///* ==================== 宏定义 ==================== */
//#define CACHE_MAX_COUNT  20  // 最多缓存100条历史数据
//#define DATA_MAX_LEN     64   // 单条数据最大长度

///* ==================== 函数声明（对外接口 上层只需要调用这些） ==================== */

//void DataCache_Init(void);                                // 缓存初始化（在启动任务里调用1次）

//uint8_t DataCache_Write(char *data);                      // 写入数据到缓存（上传失败时调用）

//uint8_t DataCache_Read(char *out_buf, uint16_t buf_len);  // 从缓存读取一条数据（补传时调用）

//uint16_t DataCache_GetValidCount(void);                   // 获取当前有效数据条数（调试用）

//#ifdef __cplusplus
//}
//#endif

//#endif
