/**
 * @file cf_engine.h
 * @brief 列族（Column Family）存储引擎
 *
 * 提供以列族为基本组织单位的 NoSQL 数据模型。
 * 内部使用 KV 存储实现，支持动态列、宽行、批量操作。
 *
 * 数据模型：
 *   - Column Family（列族）：一级组织单位，类比关系表的"表"
 *   - Row（行）：由行键标识
 *   - Column（列）：动态列，每行可有不同列
 *
 * 存储布局（KV 复合键）：
 *   - {cf_name}\\x01{row_key}\\x01{col_name}  -> 列值
 *   - {cf_name}\\x02{row_key}                  -> 行存在标记（用于扫描）
 *   - {cf_name}\\x03__meta__                   -> CF 元数据
 */
#ifndef DB_CF_ENGINE_H
#define DB_CF_ENGINE_H

#include "db/cf/cf_column.h"
#include "db/cf/cf_row.h"
#include "db/kv.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 列族名最大长度 */
#define CF_MAX_CF_NAME_LEN 64

/** 数据库路径最大长度 */
#define CF_MAX_PATH_LEN 512

/** 默认路径后缀 */
#define CF_DEFAULT_DB_EXT ".cfdb"

/* ============================================================
 * 错误码
 * ============================================================ */

typedef enum cf_result_e {
    CF_OK = 0,            /**< 成功 */
    CF_NOT_FOUND = 1,     /**< 行或列不存在 */
    CF_ERROR = 2,         /**< 一般错误 */
    CF_NOMEM = 3,         /**< 内存不足 */
    CF_INVALID = 4,       /**< 无效参数 */
    CF_EXISTS = 5,        /**< 列族已存在 */
    CF_CORRUPT = 6,       /**< 数据损坏 */
} cf_result_t;

/* ============================================================
 * 数据库句柄
 * ============================================================ */

/**
 * @brief 列族数据库句柄
 */
typedef struct cf_db_s cf_db_t;

/* ============================================================
 * 批量操作
 * ============================================================ */

/**
 * @brief 批量操作类型
 */
typedef enum cf_batch_op_type_e {
    CF_BATCH_PUT = 1,      /**< 插入或更新列 */
    CF_BATCH_DELETE_COL = 2, /**< 删除列 */
    CF_BATCH_DELETE_ROW = 3, /**< 删除整行 */
} cf_batch_op_type_t;

/**
 * @brief 批量操作项
 */
typedef struct cf_batch_op_s {
    cf_batch_op_type_t type;     /**< 操作类型 */
    const char        *cf_name;  /**< 列族名 */
    const char        *row_key;  /**< 行键 */
    uint32_t           row_key_len; /**< 行键长度 */
    const char        *col_name; /**< 列名（PUT/DELETE_COL 时使用）*/
    uint32_t           col_name_len; /**< 列名长度 */
    const void        *value;    /**< 列值（PUT 时使用）*/
    uint32_t           value_len; /**< 列值长度 */
} cf_batch_op_t;

/* ============================================================
 * 扫描结果
 * ============================================================ */

/**
 * @brief 行扫描结果
 */
typedef struct cf_scan_result_s {
    char     *cf_name;        /**< 列族名 */
    char     *row_key;        /**< 行键 */
    uint32_t  row_key_len;
    cf_row_t *row;            /**< 完整行数据 */
} cf_scan_result_t;

/**
 * @brief 行迭代器
 */
typedef struct cf_iter_s cf_iter_t;

/* ============================================================
 * 数据库生命周期
 * ============================================================ */

/**
 * @brief 打开或创建列族数据库
 * @param path 数据库文件路径
 * @return 数据库句柄，失败返回 NULL
 */
cf_db_t *cf_open(const char *path);

/**
 * @brief 关闭数据库
 * @param db 数据库句柄
 * @return CF_OK 成功
 */
cf_result_t cf_close(cf_db_t *db);

/**
 * @brief 获取错误信息
 * @param db 数据库句柄
 * @return 错误信息字符串
 */
const char *cf_errmsg(const cf_db_t *db);

/**
 * @brief 刷脏页到磁盘
 * @param db 数据库句柄
 * @return CF_OK 成功
 */
cf_result_t cf_flush(cf_db_t *db);

/* ============================================================
 * 列族管理
 * ============================================================ */

/**
 * @brief 创建列族
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @return CF_OK 成功，CF_EXISTS 已存在
 */
cf_result_t cf_create_family(cf_db_t *db, const char *cf_name);

/**
 * @brief 删除列族（含其中所有数据）
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @return CF_OK 成功
 */
cf_result_t cf_drop_family(cf_db_t *db, const char *cf_name);

/**
 * @brief 判断列族是否存在
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @return true 存在，false 不存在
 */
bool cf_family_exists(cf_db_t *db, const char *cf_name);

/**
 * @brief 列出所有列族
 * @param db 数据库句柄
 * @param out_names 输出的列族名数组（调用者负责释放）
 * @param out_count 输出列族数量
 * @return CF_OK 成功
 */
cf_result_t cf_list_families(cf_db_t *db,
                             char ***out_names,
                             uint32_t *out_count);

/**
 * @brief 释放列族列表
 * @param names 列族名数组
 * @param count 数量
 */
void cf_free_family_list(char **names, uint32_t count);

/* ============================================================
 * 单列操作
 * ============================================================ */

/**
 * @brief 插入或更新一列
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @param value 列值
 * @param value_len 列值长度
 * @param ttl_seconds TTL 秒数（0 表示永不过期）
 * @return CF_OK 成功
 */
cf_result_t cf_put(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len,
                   const char *col_name, uint32_t col_name_len,
                   const void *value, uint32_t value_len,
                   int32_t ttl_seconds);

/**
 * @brief 获取单列值
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @param out_value 输出值（调用者负责释放）
 * @param out_len 输出值长度
 * @return CF_OK 成功，CF_NOT_FOUND 不存在
 */
cf_result_t cf_get(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len,
                   const char *col_name, uint32_t col_name_len,
                   void **out_value, uint32_t *out_len);

/**
 * @brief 删除单列
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @return CF_OK 成功，CF_NOT_FOUND 不存在
 */
cf_result_t cf_delete_column(cf_db_t *db,
                             const char *cf_name,
                             const char *row_key, uint32_t row_key_len,
                             const char *col_name, uint32_t col_name_len);

/**
 * @brief 检查列是否存在
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @return true 存在，false 不存在
 */
bool cf_exists(cf_db_t *db,
               const char *cf_name,
               const char *row_key, uint32_t row_key_len,
               const char *col_name, uint32_t col_name_len);

/* ============================================================
 * 行操作
 * ============================================================ */

/**
 * @brief 获取整行
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @param out_row 输出行（调用者负责释放）
 * @return CF_OK 成功，CF_NOT_FOUND 行不存在
 */
cf_result_t cf_get_row(cf_db_t *db,
                       const char *cf_name,
                       const char *row_key, uint32_t row_key_len,
                       cf_row_t **out_row);

/**
 * @brief 删除整行
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @return CF_OK 成功
 */
cf_result_t cf_delete_row(cf_db_t *db,
                          const char *cf_name,
                          const char *row_key, uint32_t row_key_len);

/**
 * @brief 检查行是否存在
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param row_key 行键
 * @param row_key_len 行键长度
 * @return true 存在，false 不存在
 */
bool cf_row_exists(cf_db_t *db,
                   const char *cf_name,
                   const char *row_key, uint32_t row_key_len);

/* ============================================================
 * 扫描
 * ============================================================ */

/**
 * @brief 创建行扫描迭代器
 *
 * 扫描指定列族下的所有行。
 *
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param start_row_key 起始行键（NULL 表示从头开始）
 * @param start_len 起始行键长度
 * @param end_row_key 结束行键（NULL 表示到结尾）
 * @param end_len 结束行键长度
 * @return 迭代器，失败返回 NULL
 */
cf_iter_t *cf_scan_rows(cf_db_t *db,
                        const char *cf_name,
                        const char *start_row_key, uint32_t start_len,
                        const char *end_row_key, uint32_t end_len);

/**
 * @brief 迭代器前进
 * @param iter 迭代器
 * @return CF_OK 有下一行，CF_NOT_FOUND 结束
 */
cf_result_t cf_iter_next(cf_iter_t *iter);

/**
 * @brief 获取当前行
 * @param iter 迭代器
 * @return 当前行指针（迭代器生命周期内有效）
 */
const cf_row_t *cf_iter_row(cf_iter_t *iter);

/**
 * @brief 获取当前行键
 * @param iter 迭代器
 * @return 行键指针
 */
const char *cf_iter_row_key(cf_iter_t *iter);

/**
 * @brief 获取当前行键长度
 * @param iter 迭代器
 * @return 行键长度
 */
uint32_t cf_iter_row_key_len(cf_iter_t *iter);

/**
 * @brief 释放迭代器
 * @param iter 迭代器
 */
void cf_iter_free(cf_iter_t *iter);

/* ============================================================
 * 批量操作
 * ============================================================ */

/**
 * @brief 批量执行多个操作（原子性保证见具体实现）
 *
 * 当前实现：按顺序执行，不保证跨操作原子性。
 *
 * @param db 数据库句柄
 * @param ops 操作数组
 * @param n_ops 操作数量
 * @param out_results 各操作结果数组（可为 NULL）
 * @return CF_OK 全部成功，CF_ERROR 部分失败
 */
cf_result_t cf_batch_execute(cf_db_t *db,
                             const cf_batch_op_t *ops, uint32_t n_ops,
                             cf_result_t *out_results);

/* ============================================================
 * 统计信息
 * ============================================================ */

/**
 * @brief 列族统计信息
 */
typedef struct cf_family_stats_s {
    uint64_t num_rows;       /**< 行数量 */
    uint64_t num_columns;    /**< 列总数 */
    uint64_t total_size;     /**< 数据总大小（字节） */
} cf_family_stats_t;

/**
 * @brief 获取列族统计信息
 * @param db 数据库句柄
 * @param cf_name 列族名
 * @param stats 输出统计
 * @return CF_OK 成功
 */
cf_result_t cf_family_stats(cf_db_t *db, const char *cf_name,
                            cf_family_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_CF_ENGINE_H */