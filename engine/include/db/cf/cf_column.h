/**
 * @file cf_column.h
 * @brief 列族列（Column）定义
 *
 * 列是列族存储的二级组织单位，由列名和值构成。
 * 支持动态列，行内的列数量不固定。
 */
#ifndef DB_CF_COLUMN_H
#define DB_CF_COLUMN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 列名最大长度 */
#define CF_MAX_COLUMN_NAME_LEN 256

/** 列值最大长度 */
#define CF_MAX_COLUMN_VALUE_LEN (16 * 1024 * 1024)  /* 16MB */

/* ============================================================
 * 列定义结构
 * ============================================================ */

/**
 * @brief 列族中的列
 *
 * 表示一列的完整定义，包括列名、值、时间戳。
 * 列名在同一列族同一行内唯一。
 */
typedef struct cf_column_s {
    char     *name;           /**< 列名（动态字符串） */
    uint32_t  name_len;       /**< 列名长度 */
    void     *value;          /**< 列值（二进制数据） */
    uint32_t  value_len;      /**< 列值长度 */
    int64_t   timestamp;      /**< 时间戳（写入时刻，毫秒） */
    int32_t   ttl_seconds;    /**< TTL（秒），0 表示不过期 */
} cf_column_t;

/* ============================================================
 * 列操作 API
 * ============================================================ */

/**
 * @brief 创建列
 * @param name 列名
 * @param name_len 列名长度
 * @param value 列值
 * @param value_len 列值长度
 * @param timestamp 时间戳（0 表示自动设为当前时间）
 * @param ttl_seconds TTL 秒数（0 表示永不过期）
 * @return 新创建的列，失败返回 NULL
 */
cf_column_t *cf_column_create(const char *name, uint32_t name_len,
                              const void *value, uint32_t value_len,
                              int64_t timestamp, int32_t ttl_seconds);

/**
 * @brief 复制列（深拷贝）
 * @param col 源列
 * @return 复制的列，失败返回 NULL
 */
cf_column_t *cf_column_clone(const cf_column_t *col);

/**
 * @brief 释放列
 * @param col 列句柄
 */
void cf_column_free(cf_column_t *col);

/**
 * @brief 比较两列是否相等（仅比较列名）
 * @param a 列 A
 * @param b 列 B
 * @return 0 相等，非 0 不等（memcmp 风格）
 */
int cf_column_compare(const cf_column_t *a, const cf_column_t *b);

/**
 * @brief 检查列是否过期
 * @param col 列
 * @param now_ms 当前时间（毫秒）
 * @return true 已过期，false 未过期
 */
bool cf_column_is_expired(const cf_column_t *col, int64_t now_ms);

/**
 * @brief 获取列的序列化大小
 * @param col 列
 * @return 序列化所需的字节数
 */
size_t cf_column_serialized_size(const cf_column_t *col);

/**
 * @brief 序列化列到缓冲区
 * @param col 列
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 0 成功，-1 失败
 */
int cf_column_serialize(const cf_column_t *col, void *buf, size_t buf_size);

/**
 * @brief 从缓冲区反序列化列
 * @param buf 输入缓冲区
 * @param buf_len 缓冲区长度
 * @param out_col 输出的列（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int cf_column_deserialize(const void *buf, size_t buf_len, cf_column_t **out_col);

#ifdef __cplusplus
}
#endif

#endif /* DB_CF_COLUMN_H */