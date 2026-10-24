/**
 * @file cf_row.h
 * @brief 列族行（Row）结构
 *
 * 行是列族存储的核心单位，由行键唯一标识。
 * 一行可包含任意数量的列（动态列）。
 */
#ifndef DB_CF_ROW_H
#define DB_CF_ROW_H

#include "db/cf/cf_column.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 行键最大长度 */
#define CF_MAX_ROW_KEY_LEN 1024

/** 单行最大列数 */
#define CF_MAX_COLUMNS_PER_ROW 65536

/* ============================================================
 * 行结构
 * ============================================================ */

/**
 * @brief 列族中的一行
 *
 * 由行键（唯一）和列数组构成。
 * 列以字典序排列，便于二分查找。
 */
typedef struct cf_row_s {
    char          *row_key;       /**< 行键 */
    uint32_t       row_key_len;   /**< 行键长度 */
    cf_column_t  **columns;       /**< 列数组 */
    uint32_t       num_columns;   /**< 列数量 */
    uint32_t       capacity;      /**< 列数组容量 */
    int64_t        timestamp;     /**< 行更新时间戳（毫秒） */
} cf_row_t;

/* ============================================================
 * 行操作 API
 * ============================================================ */

/**
 * @brief 创建空行
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @return 新行，失败返回 NULL
 */
cf_row_t *cf_row_create(const char *row_key, uint32_t row_key_len);

/**
 * @brief 释放行
 * @param row 行句柄
 */
void cf_row_free(cf_row_t *row);

/**
 * @brief 添加或更新列（按列名）
 *
 * 若列已存在则覆盖值和时间戳。
 *
 * @param row 行
 * @param column 列（会被复制）
 * @return 0 成功，-1 失败
 */
int cf_row_add_column(cf_row_t *row, const cf_column_t *column);

/**
 * @brief 获取指定列名的列
 * @param row 行
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @return 列指针（行生命周期内有效），未找到返回 NULL
 */
const cf_column_t *cf_row_get_column(const cf_row_t *row,
                                     const char *col_name,
                                     uint32_t col_name_len);

/**
 * @brief 删除指定列名的列
 * @param row 行
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @return 0 成功，-1 失败（列不存在）
 */
int cf_row_delete_column(cf_row_t *row,
                         const char *col_name,
                         uint32_t col_name_len);

/**
 * @brief 获取行的序列化大小
 * @param row 行
 * @return 序列化所需字节数
 */
size_t cf_row_serialized_size(const cf_row_t *row);

/**
 * @brief 序列化行到缓冲区
 * @param row 行
 * @param buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 0 成功，-1 失败
 */
int cf_row_serialize(const cf_row_t *row, void *buf, size_t buf_size);

/**
 * @brief 从缓冲区反序列化行
 * @param buf 输入缓冲区
 * @param buf_len 缓冲区长度
 * @param out_row 输出的行（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int cf_row_deserialize(const void *buf, size_t buf_len, cf_row_t **out_row);

#ifdef __cplusplus
}
#endif

#endif /* DB_CF_ROW_H */