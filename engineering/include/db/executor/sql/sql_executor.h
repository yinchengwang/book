/**
 * @file sql_executor.h
 * @brief SQL 执行器（整合存储架构）
 *
 * 整合 Catalog、Buffer Pool、Relation 和 WAL 的 SQL 执行器
 */
#ifndef DB_SQL_EXECUTOR_H
#define DB_SQL_EXECUTOR_H

#include "db/catalog.h"
#include "db/rel.h"
#include "db/buf.h"
#include "db/wal.h"
#include "db/wal_buf.h"
#include "db/parser/sql/sql.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 执行器上下文
 * ============================================================ */

/**
 * @brief SQL 执行器
 *
 * 整合所有存储组件的执行器
 */
typedef struct sql_executor_s {
    /* 存储组件 */
    void       *catalog;          /**< Catalog 指针 */
    void       *buffer_pool;      /**< Buffer Pool 指针 */
    wal_t     *wal;              /**< WAL 指针 */
    wal_buf_t *wal_buf;          /**< WAL-Buf 协调器 */

    /* 事务 */
    uint32_t   txn_id;            /**< 当前事务 ID */
    bool       in_transaction;   /**< 是否在事务中 */

    /* 统计 */
    uint64_t   queries_executed;  /**< 执行的查询数 */
    uint64_t   rows_processed;   /**< 处理的行数 */

    /* 错误信息 */
    char      *error_msg;         /**< 错误信息 */
} sql_executor_t;

/* ============================================================
 * 执行器生命周期
 * ============================================================ */

/**
 * @brief 创建执行器
 * @return 执行器句柄
 */
sql_executor_t *sql_executor_create(void);

/**
 * @brief 销毁执行器
 * @param exec 执行器
 */
void sql_executor_destroy(sql_executor_t *exec);

/**
 * @brief 初始化执行器（创建所有存储组件）
 * @param exec 执行器
 * @param db_path 数据库路径
 * @param buf_count Buffer 数量
 * @return 0 成功
 */
int sql_executor_init(sql_executor_t *exec, const char *db_path, int buf_count);

/**
 * @brief 关闭执行器
 * @param exec 执行器
 */
void sql_executor_shutdown(sql_executor_t *exec);

/* ============================================================
 * 事务管理
 * ============================================================ */

/**
 * @brief 开始事务
 * @param exec 执行器
 * @return 0 成功
 */
int sql_executor_begin(sql_executor_t *exec);

/**
 * @brief 提交事务
 * @param exec 执行器
 * @return 0 成功
 */
int sql_executor_commit(sql_executor_t *exec);

/**
 * @brief 回滚事务
 * @param exec 执行器
 * @return 0 成功
 */
int sql_executor_rollback(sql_executor_t *exec);

/**
 * @brief 获取当前事务 ID
 * @param exec 执行器
 * @return 事务 ID
 */
uint32_t sql_executor_get_txn_id(sql_executor_t *exec);

/* ============================================================
 * DDL 执行
 * ============================================================ */

/**
 * @brief 创建表
 * @param exec 执行器
 * @param name 表名
 * @param columns 列定义数组
 * @param ncolumns 列数
 * @return 表 OID，失败返回 InvalidOid
 */
Oid sql_executor_create_table(sql_executor_t *exec, const char *name,
                           column_def_t *columns, int ncolumns);

/**
 * @brief 删除表
 * @param exec 执行器
 * @param name 表名
 * @return 0 成功
 */
int sql_executor_drop_table(sql_executor_t *exec, const char *name);

/**
 * @brief 创建索引
 * @param exec 执行器
 * @param name 索引名
 * @param table_name 表名
 * @param columns 列名数组
 * @param ncolumns 列数
 * @param is_unique 是否唯一
 * @return 索引 OID
 */
Oid sql_executor_create_index(sql_executor_t *exec, const char *name,
                            const char *table_name, const char **columns,
                            int ncolumns, bool is_unique);

/**
 * @brief 删除索引
 * @param exec 执行器
 * @param name 索引名
 * @return 0 成功
 */
int sql_executor_drop_index(sql_executor_t *exec, const char *name);

/* ============================================================
 * DML 执行
 * ============================================================ */

/**
 * @brief 插入数据
 * @param exec 执行器
 * @param table_name 表名
 * @param values 值数组
 * @param nvalues 值数量
 * @return 影响的行数
 */
int sql_executor_insert(sql_executor_t *exec, const char *table_name,
                      const void **values, int nvalues);

/**
 * @brief 更新数据
 * @param exec 执行器
 * @param table_name 表名
 * @param set_cols 设置的列名数组
 * @param set_values 设置的值数组
 * @param nset 设置数量
 * @param where_expr WHERE 条件
 * @return 影响的行数
 */
int sql_executor_update(sql_executor_t *exec, const char *table_name,
                      const char **set_cols, const void **set_values, int nset,
                      const sql_node_t *where_expr);

/**
 * @brief 删除数据
 * @param exec 执行器
 * @param table_name 表名
 * @param where_expr WHERE 条件
 * @return 影响的行数
 */
int sql_executor_delete(sql_executor_t *exec, const char *table_name,
                       const sql_node_t *where_expr);

/* ============================================================
 * 查询执行
 * ============================================================ */

/** 查询结果 */
typedef struct sql_query_result_s {
    char      **columns;          /**< 列名 */
    int       *col_types;         /**< 列类型 */
    int       ncolumns;           /**< 列数 */
    char      **rows;             /**< 行数据 */
    int       nrow;               /**< 行数 */
    int       row_capacity;       /**< 行容量 */
} sql_query_result_t;

/**
 * @brief 执行 SELECT 查询
 * @param exec 执行器
 * @param query SQL 节点
 * @return 查询结果，失败返回 NULL
 */
sql_query_result_t *sql_executor_select(sql_executor_t *exec, const sql_node_t *query);

/**
 * @brief 释放查询结果
 * @param result 查询结果
 */
void sql_query_result_free(sql_query_result_t *result);

/**
 * @brief 获取结果行数
 * @param result 查询结果
 * @return 行数
 */
int sql_query_result_row_count(const sql_query_result_t *result);

/**
 * @brief 获取结果列数
 * @param result 查询结果
 * @return 列数
 */
int sql_query_result_column_count(const sql_query_result_t *result);

/**
 * @brief 获取单元格值
 * @param result 查询结果
 * @param row 行索引
 * @param col 列索引
 * @return 值字符串
 */
const char *sql_query_result_cell(const sql_query_result_t *result, int row, int col);

/* ============================================================
 * 扫描接口
 * ============================================================ */

/**
 * @brief 打开表扫描
 * @param exec 执行器
 * @param table_name 表名
 * @return Relation，失败返回 NULL
 */
Relation sql_executor_table_open(sql_executor_t *exec, const char *table_name);

/**
 * @brief 关闭表扫描
 * @param rel Relation
 */
void sql_executor_table_close(Relation rel);

/* ============================================================
 * 错误处理
 * ============================================================ */

/**
 * @brief 获取错误信息
 * @param exec 执行器
 * @return 错误信息
 */
const char *sql_executor_errmsg(sql_executor_t *exec);

/**
 * @brief 检查是否有错误
 * @param exec 执行器
 * @return true 有错误
 */
bool sql_executor_has_error(sql_executor_t *exec);

/* ============================================================
 * 统计
 * ============================================================ */

/**
 * @brief 执行器统计信息
 */
typedef struct sql_executor_stats_s {
    uint64_t   queries_executed;  /**< 执行的查询数 */
    uint64_t   rows_processed;    /**< 处理的行数 */
    uint32_t   txn_id;            /**< 当前事务 ID */
    bool       in_transaction;    /**< 是否在事务中 */
    uint32_t   dirty_pages;        /**< 脏页数 */
    uint64_t   cache_hits;         /**< 缓存命中 */
    uint64_t   cache_misses;      /**< 缓存未命中 */
} sql_executor_stats_t;

/**
 * @brief 获取统计信息
 * @param exec 执行器
 * @param stats 输出统计
 */
void sql_executor_get_stats(sql_executor_t *exec, sql_executor_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_EXECUTOR_H */
