/**
 * @file mmdb_error.h
 * @brief 错误码定义与字符串映射
 */
#ifndef SDK_MMDB_ERROR_H
#define SDK_MMDB_ERROR_H

#include "sdk/mmdb_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MMDB_OK              =  0,
    MMDB_ERR_INVALID     = -1,
    MMDB_ERR_NOT_FOUND   = -2,
    MMDB_ERR_ALREADY     = -3,
    MMDB_ERR_IO          = -4,
    MMDB_ERR_CORRUPT     = -5,
    MMDB_ERR_FULL        = -6,
    MMDB_ERR_INTERNAL    = -7,
    MMDB_ERR_NOMEM       = -8,
    MMDB_ERR_TIMEOUT     = -9,
    MMDB_ERR_BUSY        = -10,
    MMDB_ERR_NOT_IMPLEMENTED = -11,  /* 功能尚未实现（AVERAGE_POOL / OPENAI stub 等） */
} mmdb_error_t;

/**
 * @brief 返回错误码对应的可读字符串
 */
const char* mmdb_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif /* SDK_MMDB_ERROR_H */
