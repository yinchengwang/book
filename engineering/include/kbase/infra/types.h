#ifndef KBASE_INFRA_TYPES_H
#define KBASE_INFRA_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 数据类型枚举 */
typedef enum {
    INFRA_DTYPE_F32   = 0,  /* 32-bit 浮点 */
    INFRA_DTYPE_F16   = 1,  /* 16-bit 浮点（存储用） */
    INFRA_DTYPE_Q8_0  = 2,  /* 8-bit 量化 (block 32) */
    INFRA_DTYPE_Q4_0  = 3,  /* 4-bit 量化 (block 32) */
    INFRA_DTYPE_Q4_1  = 4,  /* 4-bit 量化 (block 32, 偏移) */
    INFRA_DTYPE_I32   = 5,  /* 32-bit 整数 */
    INFRA_DTYPE_I64   = 6,  /* 64-bit 整数 */
} infra_dtype_t;

/* 状态码 */
typedef enum {
    INFRA_OK           =  0,
    INFRA_ERR_LOAD     = -1,  /* 模型加载失败 */
    INFRA_ERR_FORMAT   = -2,  /* 格式不支持 */
    INFRA_ERR_MEMORY   = -3,  /* 内存不足 */
    INFRA_ERR_GRAPH    = -4,  /* 计算图错误 */
    INFRA_ERR_OP       = -5,  /* 算子执行错误 */
    INFRA_ERR_PARAM    = -6,  /* 参数错误 */
    INFRA_ERR_NOT_FOUND = -7, /* 查找失败 */
} infra_status_t;

/* 获取数据类型字节大小 */
size_t infra_sizeof_dtype(infra_dtype_t dtype);

#ifdef __cplusplus
}
#endif

#endif /* KBASE_INFRA_TYPES_H */