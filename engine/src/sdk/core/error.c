/**
 * @file error.c
 * @brief 错误码字符串映射实现
 */
#include "sdk/mmdb_error.h"

const char* mmdb_strerror(int code) {
    switch (code) {
        case MMDB_OK:              return "OK";
        case MMDB_ERR_INVALID:     return "invalid argument";
        case MMDB_ERR_NOT_FOUND:   return "not found";
        case MMDB_ERR_ALREADY:     return "already exists";
        case MMDB_ERR_IO:          return "I/O error";
        case MMDB_ERR_CORRUPT:     return "corrupted data";
        case MMDB_ERR_FULL:        return "storage full";
        case MMDB_ERR_INTERNAL:    return "internal error";
        case MMDB_ERR_NOMEM:       return "out of memory";
        case MMDB_ERR_TIMEOUT:     return "operation timed out";
        case MMDB_ERR_BUSY:        return "resource busy";
        case MMDB_ERR_NOT_IMPLEMENTED: return "not implemented";
        default:                   return "unknown error";
    }
}