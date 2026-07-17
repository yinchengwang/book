/**
 * @file table.h
 * @brief 表管理器接口
 *
 * 表管理负责：
 * - 表元数据存储（表结构、列定义）
 * - 数据页面的分配和管理
 * - 表的创建、删除操作
 */
#ifndef DB_TABLE_H
#define DB_TABLE_H

#include "db/kv.h"
#include "db/parser/sql/sql.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 表元数据
 * ============================================================ */

/** 列信息 */
typedef struct table_column_s {
    char           *name;       /**< 列名 */
    sql_data_type_t type;       /**< 数据类型 */
    size_t         length;      /**< 长度（VARCHAR） */
    bool           not_null;    /**< 是否 NOT NULL */
    bool           primary_key; /**< 是否主键 */
    size_t         offset;      /**< 在行中的偏移 */
} table_column_t;

/** 表元数据 */
typedef struct table_meta_s {
    char          *name;           /**< 表名 */
    table_column_t *columns;       /**< 列定义数组 */
    size_t         num_columns;    /**< 列数 */
    size_t         row_size;       /**< 行大小（字节） */
    page_id_t      first_page;     /**< 数据起始页 */
    page_id_t      last_page;      /**< 数据结束页 */
    size_t         num_rows;       /**< 行数 */
} table_meta_t;

/** 表句柄 */
typedef struct table_s table_t;

/* ============================================================
 * 表操作 API
 * ============================================================ */

/**
 * @brief 创建表
 * @param db KV 数据库句柄
 * @param name 表名
 * @param columns 列定义数组
 * @param num_columns 列数
 * @return 表句柄，失败返回 NULL
 */
table_t *table_create(kv_t *db, const char *name,
                      const table_column_t *columns, size_t num_columns);

/**
 * @brief 打开表
 * @param db KV 数据库句柄
 * @param name 表名
 * @return 表句柄，失败返回 NULL
 */
table_t *table_open(kv_t *db, const char *name);

/**
 * @brief 关闭表
 * @param table 表句柄
 */
void table_close(table_t *table);

/**
 * @brief 删除表
 * @param table 表句柄
 * @return 0 成功
 */
int table_drop(table_t *table);

/**
 * @brief 获取表元数据
 * @param table 表句柄
 * @return 表元数据
 */
const table_meta_t *table_get_meta(const table_t *table);

/**
 * @brief 获取列数
 * @param table 表句柄
 * @return 列数
 */
size_t table_num_columns(const table_t *table);

/**
 * @brief 获取列定义
 * @param table 表句柄
 * @param index 列索引
 * @return 列定义，失败返回 NULL
 */
const table_column_t *table_get_column(const table_t *table, size_t index);

/**
 * @brief 获取行大小
 * @param table 表句柄
 * @return 行大小（字节）
 */
size_t table_row_size(const table_t *table);

/**
 * @brief 设置列数（内部使用）
 */
void table_meta_set_num_columns(table_t *table, size_t num_cols);

/**
 * @brief 设置行大小（内部使用）
 */
void table_meta_set_row_size(table_t *table, size_t row_size);

/**
 * @brief 设置列定义（内部使用）
 */
void table_meta_set_column(table_t *table, size_t index, const char *name,
                         sql_data_type_t type, size_t length,
                         bool not_null, bool primary_key);

/**
 * @brief 从存储的列数据设置列定义（内部使用）
 */
void table_meta_set_column_from_stored(table_t *table, size_t index, const table_column_t *stored_col);

/* ============================================================
 * 行操作 API
 * ============================================================ */

/** 行数据（变长结构，实际大小根据列定义计算） */
typedef struct table_row_s {
    uint32_t null_mask;  /**< NULL 位图 */
    /* 后面是列数据 */
} table_row_t;

/**
 * @brief 插入一行
 * @param table 表句柄
 * @param values 值数组
 * @param num_values 值数量
 * @return 0 成功
 */
int table_insert(table_t *table, const void **values, size_t num_values);

/**
 * @brief 更新一行
 * @param table 表句柄
 * @param row_id 行ID
 * @param values 值数组
 * @param num_values 值数量
 * @return 0 成功
 */
int table_update(table_t *table, uint64_t row_id,
                 const void **values, size_t num_values);

/**
 * @brief 删除一行
 * @param table 表句柄
 * @param row_id 行ID
 * @return 0 成功
 */
int table_delete(table_t *table, uint64_t row_id);

/**
 * @brief 读取一行
 * @param table 表句柄
 * @param row_id 行ID
 * @param out_row 输出行数据（调用者负责释放）
 * @return 0 成功
 */
int table_get_row(table_t *table, uint64_t row_id, void **out_row);

/* ============================================================
 * 扫描 API
 * ============================================================ */

/** 表扫描迭代器 */
typedef struct table_iter_s table_iter_t;

/**
 * @brief 创建扫描迭代器
 * @param table 表句柄
 * @return 迭代器
 */
table_iter_t *table_scan(table_t *table);

/**
 * @brief 移动到下一行
 * @param iter 迭代器
 * @return 0 成功，1 遍历结束
 */
int table_iter_next(table_iter_t *iter);

/**
 * @brief 获取当前行ID
 * @param iter 迭代器
 * @return 行ID
 */
uint64_t table_iter_row_id(const table_iter_t *iter);

/**
 * @brief 获取当前行数据
 * @param iter 迭代器
 * @param out_row 输出行数据
 * @return 0 成功
 */
int table_iter_get_row(table_iter_t *iter, void **out_row);

/**
 * @brief 释放迭代器
 * @param iter 迭代器
 */
void table_iter_free(table_iter_t *iter);

#ifdef __cplusplus
}
#endif

#endif /* DB_TABLE_H */