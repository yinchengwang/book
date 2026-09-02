/**
 * @file htap.h
 * @brief HTAP 混合事务分析处理接口
 *
 * Phase12 - 实现 HTAP，追赶 TiDB/SingleStore 水平。
 */
#ifndef DB_HTAP_H
#define DB_HTAP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** HTAP 不透明类型 */
typedef struct htap_engine htap_engine_t;

/** 引擎模式 */
typedef enum {
    HTAP_MODE_ROW = 0,      /**< 行式存储（OLTP）*/
    HTAP_MODE_COLUMN = 1,    /**< 列式存储（OLAP）*/
    HTAP_MODE_AUTO = 2       /**< 自动路由 */
} htap_mode_t;

/** 创建 HTAP 引擎 */
htap_engine_t *htap_create(const char *data_dir);

/** 关闭引擎 */
void htap_close(htap_engine_t *htap);

/** 事务操作 */
int htap_begin(htap_engine_t *htap);
int htap_commit(htap_engine_t *htap);
int htab_rollback(htap_engine_t *htap);

/** OLTP 操作（行式）*/
int htap_insert(htap_engine_t *htap, const char *table, const void *row, size_t row_size);
int htap_update(htap_engine_t *htap, const char *table, uint64_t key, const void *row, size_t row_size);
int htap_delete(htap_engine_t *htap, const char *table, uint64_t key);

/** OLAP 操作（列式）*/
int htap_columnar_insert(htap_engine_t *htap, const char *table, const void *data, size_t size);
void *htap_columnar_scan(htap_engine_t *htap, const char *table,
                        const char *columns, size_t *out_size);

/** 分析查询 */
void *htap_aggregate(htap_engine_t *htap, const char *sql,
                      size_t *out_size);
void *htap_join(htap_engine_t *htap, const char *table1, const char *table2,
                 const char *condition, size_t *out_size);

/** 模式切换 */
int htap_set_mode(htap_engine_t *htap, htap_mode_t mode);
htap_mode_t htap_get_mode(const htap_engine_t *htap);

#ifdef __cplusplus
}
#endif
#endif /* DB_HTAP_H */
