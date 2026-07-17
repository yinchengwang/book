/**
 * @file sql_executor.c
 * @brief SQL 执行器实现（整合存储架构）
 */

#include "db/executor/sql/sql_executor.h"
#include "db/catalog.h"
#include "db/rel.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/wal.h"
#include "db/wal_buf.h"
#include "db/table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 前向声明 */
static void sql_executor_set_error(sql_executor_t *exec, const char *msg);

/* ============================================================
 * 生命周期
 * ============================================================ */

sql_executor_t *sql_executor_create(void) {
    sql_executor_t *exec = (sql_executor_t *)calloc(1, sizeof(sql_executor_t));
    if (!exec) {
        return NULL;
    }

    exec->txn_id = 0;
    exec->in_transaction = false;
    exec->queries_executed = 0;
    exec->rows_processed = 0;
    exec->error_msg = NULL;
    exec->catalog = NULL;
    exec->buffer_pool = NULL;
    exec->wal = NULL;
    exec->wal_buf = NULL;

    return exec;
}

void sql_executor_destroy(sql_executor_t *exec) {
    if (!exec) {
        return;
    }

    sql_executor_shutdown(exec);

    if (exec->error_msg) {
        free(exec->error_msg);
    }

    free(exec);
}

int sql_executor_init(sql_executor_t *exec, const char *db_path, int buf_count) {
    if (!exec) {
        return -1;
    }

    /* 初始化 Catalog */
    if (catalog_init() != 0) {
        sql_executor_set_error(exec, "Failed to init catalog");
        return -1;
    }

    /* 初始化 Buffer Pool */
    if (buf_init(buf_count > 0 ? buf_count : BUF_DEFAULT_NBUFFERS) != 0) {
        sql_executor_set_error(exec, "Failed to init buffer pool");
        catalog_shutdown();
        return -1;
    }

    /* 初始化 WAL */
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "%s.wal", db_path ? db_path : "db");
    exec->wal = wal_create(wal_path, BUF_PAGE_SIZE);
    if (!exec->wal) {
        sql_executor_set_error(exec, "Failed to create WAL");
        buf_shutdown();
        catalog_shutdown();
        return -1;
    }

    /* 初始化 WAL-Buf 协调器 */
    exec->wal_buf = wal_buf_create(exec->wal, NULL);
    if (!exec->wal_buf) {
        sql_executor_set_error(exec, "Failed to create WAL-Buf coordinator");
        wal_close(exec->wal);
        buf_shutdown();
        catalog_shutdown();
        return -1;
    }

    return 0;
}

void sql_executor_shutdown(sql_executor_t *exec) {
    if (!exec) {
        return;
    }

    /* 提交未提交的事务 */
    if (exec->in_transaction) {
        sql_executor_rollback(exec);
    }

    /* 销毁组件 */
    if (exec->wal_buf) {
        wal_buf_destroy(exec->wal_buf);
        exec->wal_buf = NULL;
    }

    if (exec->wal) {
        wal_close(exec->wal);
        exec->wal = NULL;
    }

    buf_shutdown();
    catalog_shutdown();
}

static void sql_executor_set_error(sql_executor_t *exec, const char *msg) {
    if (!exec) return;

    if (exec->error_msg) {
        free(exec->error_msg);
    }
    exec->error_msg = msg ? strdup(msg) : NULL;
}

/* ============================================================
 * 事务管理
 * ============================================================ */

int sql_executor_begin(sql_executor_t *exec) {
    if (!exec) {
        return -1;
    }

    if (exec->in_transaction) {
        sql_executor_set_error(exec, "Already in transaction");
        return -1;
    }

    /* 生成事务 ID */
    exec->txn_id++;

    /* 写入 BEGIN 日志 */
    if (exec->wal) {
        wal_write_begin(exec->wal, exec->txn_id);
    }

    exec->in_transaction = true;
    return 0;
}

int sql_executor_commit(sql_executor_t *exec) {
    if (!exec || !exec->in_transaction) {
        return -1;
    }

    /* WAL-Buf 协调提交 */
    if (exec->wal_buf) {
        if (wal_buf_commit(exec->wal_buf, exec->txn_id) != 0) {
            sql_executor_set_error(exec, "Commit failed");
            return -1;
        }
    }

    /* 写入 COMMIT 日志 */
    if (exec->wal) {
        wal_write_commit(exec->wal, exec->txn_id);
    }

    exec->in_transaction = false;
    exec->txn_id++;
    return 0;
}

int sql_executor_rollback(sql_executor_t *exec) {
    if (!exec) {
        return -1;
    }

    /* 写入 ABORT 日志 */
    if (exec->wal) {
        wal_write_abort(exec->wal, exec->txn_id);
        wal_flush(exec->wal);
    }

    exec->in_transaction = false;
    return 0;
}

uint32_t sql_executor_get_txn_id(sql_executor_t *exec) {
    return exec ? exec->txn_id : 0;
}

/* ============================================================
 * DDL 执行
 * ============================================================ */

Oid sql_executor_create_table(sql_executor_t *exec, const char *name,
                           column_def_t *columns, int ncolumns) {
    if (!exec || !name || !columns) {
        return InvalidOid;
    }

    /* 通过 Catalog 创建表 */
    Oid table_oid = catalog_create_table(name, columns, ncolumns);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Failed to create table");
        return InvalidOid;
    }

    /* 创建 Relation */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (rel) {
        /* 可以在此初始化存储结构 */
        relation_close(rel, 0);
    }

    return table_oid;
}

int sql_executor_drop_table(sql_executor_t *exec, const char *name) {
    if (!exec || !name) {
        return -1;
    }

    /* 查找表 */
    Oid table_oid = catalog_lookup_table(name);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Table not found");
        return -1;
    }

    /* 删除表 */
    catalog_result_t result = catalog_drop_table(table_oid);
    if (result != CATALOG_SUCCESS) {
        sql_executor_set_error(exec, "Failed to drop table");
        return -1;
    }

    return 0;
}

Oid sql_executor_create_index(sql_executor_t *exec, const char *name,
                            const char *table_name, const char **columns,
                            int ncolumns, bool is_unique) {
    if (!exec || !name || !table_name || !columns) {
        return InvalidOid;
    }

    /* 查找表 */
    Oid table_oid = catalog_lookup_table(table_name);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Table not found");
        return InvalidOid;
    }

    /* 创建索引 */
    Oid index_oid = catalog_create_index(name, table_oid, columns, ncolumns, is_unique);
    if (index_oid == InvalidOid) {
        sql_executor_set_error(exec, "Failed to create index");
        return InvalidOid;
    }

    return index_oid;
}

int sql_executor_drop_index(sql_executor_t *exec, const char *name) {
    if (!exec || !name) {
        return -1;
    }

    (void)name;
    /* 需要通过 Catalog 查找并删除索引 */
    sql_executor_set_error(exec, "Not implemented");
    return -1;
}

/* ============================================================
 * DML 执行
 * ============================================================ */

int sql_executor_insert(sql_executor_t *exec, const char *table_name,
                      const void **values, int nvalues) {
    if (!exec || !table_name || !values) {
        return -1;
    }

    /* 查找表 */
    Oid table_oid = catalog_lookup_table(table_name);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Table not found");
        return -1;
    }

    /* 打开 Relation */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (!rel) {
        sql_executor_set_error(exec, "Failed to open table");
        return -1;
    }

    /* 写入 WAL（如果是新行，需要先记录 undo 信息） */
    /* 简化：直接插入 */

    /* 插入数据 */
    int rows = 0;
    for (int i = 0; i < nvalues; i++) {
        if (heap_insert(rel, values[i], strlen((const char*)values[i]), 0, 0, NULL) == 0) {
            rows++;
        }
    }

    relation_close(rel, 0);

    exec->rows_processed += rows;
    return rows;
}

int sql_executor_update(sql_executor_t *exec, const char *table_name,
                      const char **set_cols, const void **set_values, int nset,
                      const sql_node_t *where_expr) {
    if (!exec || !table_name) {
        return -1;
    }

    /* 查找表 */
    Oid table_oid = catalog_lookup_table(table_name);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Table not found");
        return -1;
    }

    /* 打开 Relation */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (!rel) {
        sql_executor_set_error(exec, "Failed to open table");
        return -1;
    }

    int updated = 0;

    /* 开始扫描 */
    TableScanDesc scan = table_beginscan_all(rel);
    if (!scan) {
        relation_close(rel, 0);
        sql_executor_set_error(exec, "Failed to begin scan");
        return -1;
    }

    /* 扫描并更新匹配行 */
    void *tuple;
    while ((tuple = table_getnext(scan)) != NULL) {
        /* TODO: 评估 WHERE 条件 */
        /* 简化：更新所有行 */

        /* 先删除旧元组 */
        /* heap_delete(rel, tuple, 0, false, false); */

        /* 构造新元组并插入 */
        /* TODO: 实现 form_new_tuple */
        updated++;
    }

    table_endscan(scan);
    relation_close(rel, 0);

    exec->rows_processed += updated;
    return updated;
}

int sql_executor_delete(sql_executor_t *exec, const char *table_name,
                      const sql_node_t *where_expr) {
    if (!exec || !table_name) {
        return -1;
    }

    /* 查找表 */
    Oid table_oid = catalog_lookup_table(table_name);
    if (table_oid == InvalidOid) {
        sql_executor_set_error(exec, "Table not found");
        return -1;
    }

    /* 打开 Relation */
    Relation rel = relation_open(table_oid, REL_OPEN_READWRITE);
    if (!rel) {
        sql_executor_set_error(exec, "Failed to open table");
        return -1;
    }

    int deleted = 0;

    /* 开始扫描 */
    TableScanDesc scan = table_beginscan_all(rel);
    if (!scan) {
        relation_close(rel, 0);
        sql_executor_set_error(exec, "Failed to begin scan");
        return -1;
    }

    /* 扫描并删除匹配行 */
    void *tuple;
    while ((tuple = table_getnext(scan)) != NULL) {
        /* TODO: 评估 WHERE 条件 */
        /* 简化：删除所有行 */

        /* 删除元组 */
        if (heap_delete(rel, tuple, 0, false, false) == 0) {
            deleted++;
        }
    }

    table_endscan(scan);
    relation_close(rel, 0);

    exec->rows_processed += deleted;
    return deleted;
}

/* ============================================================
 * 查询执行
 * ============================================================ */

/**
 * @brief 表达式求值（递归）
 */
static bool eval_expr(const sql_node_t *expr, const void *row_data,
                    const table_meta_t *meta) {
    if (!expr) return true;

    switch (expr->type) {
        case SQL_NODE_CONSTANT: {
            /* 常量表达式直接返回真 */
            return true;
        }

        case SQL_NODE_COLUMN_REF: {
            /* 列引用也返回真（简化处理） */
            return true;
        }

        case SQL_NODE_BINARY_OP: {
            /* 比较操作 */
            /* 简化：假设是 id = value 形式 */
            return true;  /* TODO: 实现真正的比较 */
        }

        case SQL_NODE_LOGICAL_OP: {
            int op = expr->u.logical_op.op;
            if (op == 0) {  /* AND */
                bool left = eval_expr(expr->u.logical_op.left, row_data, meta);
                if (!left) return false;
                return eval_expr(expr->u.logical_op.right, row_data, meta);
            } else if (op == 1) {  /* OR */
                bool left = eval_expr(expr->u.logical_op.left, row_data, meta);
                if (left) return true;
                return eval_expr(expr->u.logical_op.right, row_data, meta);
            } else if (op == 2) {  /* NOT */
                return !eval_expr(expr->u.logical_op.expr, row_data, meta);
            }
            return true;
        }

        case SQL_NODE_IN_LIST: {
            /* IN 表达式 */
            /* TODO: 实现 IN 列表检查 */
            return true;
        }

        case SQL_NODE_BETWEEN: {
            /* BETWEEN 表达式 */
            /* TODO: 实现范围检查 */
            return true;
        }

        case SQL_NODE_LIKE: {
            /* LIKE 表达式 */
            /* TODO: 实现模式匹配 */
            return true;
        }

        case SQL_NODE_IS_NULL: {
            /* IS NULL 表达式 */
            /* TODO: 实现 NULL 检查 */
            return true;
        }

        default:
            return true;
    }
}

sql_query_result_t *sql_executor_select(sql_executor_t *exec, const sql_node_t *query) {
    if (!exec || !query || query->type != SQL_NODE_SELECT) {
        return NULL;
    }

    /* 获取表名 */
    const char *table_name = query->u.select.table_name;
    if (!table_name) {
        sql_executor_set_error(exec, "No table specified");
        return NULL;
    }

    /* 打开表 */
    Relation rel = sql_executor_table_open(exec, table_name);
    if (!rel) {
        sql_executor_set_error(exec, "Table not found");
        return NULL;
    }

    /* 获取 TupleDesc */
    TupleDesc tdesc = relation_getdesc(rel);
    if (!tdesc) {
        sql_executor_table_close(rel);
        sql_executor_set_error(exec, "Failed to get tuple descriptor");
        return NULL;
    }

    /* 分配结果 */
    sql_query_result_t *result = (sql_query_result_t *)calloc(1, sizeof(sql_query_result_t));
    if (!result) {
        sql_executor_table_close(rel);
        sql_executor_set_error(exec, "Memory error");
        return NULL;
    }

    /* 初始化列信息 */
    result->ncolumns = tdesc->natts;
    result->columns = (char **)malloc(result->ncolumns * sizeof(char *));
    result->col_types = (int *)malloc(result->ncolumns * sizeof(int));

    for (int i = 0; i < result->ncolumns; i++) {
        result->columns[i] = strdup(tdesc->attrs[i].attname);
        result->col_types[i] = tdesc->attrs[i].atttypid;
    }

    /* 分配行存储 */
    result->row_capacity = 64;
    result->rows = (char **)malloc(result->row_capacity * sizeof(char *));
    result->nrow = 0;

    /* 估算行大小（简化：每列 256 字节） */
    size_t row_size = result->ncolumns * 256;

    /* 获取 WHERE 条件 */
    const sql_node_t *where_cond = query->u.select.where_cond;

    /* 获取表元数据（用于表达式求值） */
    table_meta_t *meta = NULL;  /* TODO: 从 catalog 获取 */

    /* 开始扫描 */
    TableScanDesc scan = table_beginscan_all(rel);
    if (!scan) {
        sql_query_result_free(result);
        sql_executor_table_close(rel);
        sql_executor_set_error(exec, "Failed to begin scan");
        return NULL;
    }

    /* 扫描所有行 */
    void *tuple;
    while ((tuple = table_getnext(scan)) != NULL) {
        /* 检查 WHERE 条件 */
        if (where_cond && !eval_expr(where_cond, tuple, meta)) {
            continue;  /* 不满足条件，跳过 */
        }

        /* 分配行数据 */
        char *row_data = (char *)malloc(row_size);
        if (!row_data) continue;

        /* 复制元组数据（简化处理） */
        memcpy(row_data, tuple, row_size < 1024 ? row_size : 1024);

        /* 添加到结果 */
        if (result->nrow >= result->row_capacity) {
            result->row_capacity *= 2;
            result->rows = (char **)realloc(result->rows,
                                          result->row_capacity * sizeof(char *));
        }
        result->rows[result->nrow++] = row_data;
    }

    /* 结束扫描 */
    table_endscan(scan);
    sql_executor_table_close(rel);

    exec->queries_executed++;
    return result;
}

void sql_query_result_free(sql_query_result_t *result) {
    if (!result) {
        return;
    }

    if (result->columns) free(result->columns);
    if (result->col_types) free(result->col_types);
    if (result->rows) {
        for (int i = 0; i < result->nrow; i++) {
            free(result->rows[i]);
        }
        free(result->rows);
    }
    free(result);
}

int sql_query_result_row_count(const sql_query_result_t *result) {
    return result ? result->nrow : 0;
}

int sql_query_result_column_count(const sql_query_result_t *result) {
    return result ? result->ncolumns : 0;
}

const char *sql_query_result_cell(const sql_query_result_t *result, int row, int col) {
    if (!result || row < 0 || row >= result->nrow || col < 0 || col >= result->ncolumns) {
        return NULL;
    }
    return result->rows[row] + col * 256;  /* 简化：假设每列固定 256 字节 */
}

/* ============================================================
 * 扫描接口
 * ============================================================ */

Relation sql_executor_table_open(sql_executor_t *exec, const char *table_name) {
    if (!exec || !table_name) {
        return NULL;
    }

    Oid table_oid = catalog_lookup_table(table_name);
    if (table_oid == InvalidOid) {
        return NULL;
    }

    return relation_open(table_oid, REL_OPEN_READONLY);
}

void sql_executor_table_close(Relation rel) {
    if (rel) {
        relation_close(rel, 0);
    }
}

/* ============================================================
 * 错误处理
 * ============================================================ */

const char *sql_executor_errmsg(sql_executor_t *exec) {
    return exec ? exec->error_msg : NULL;
}

bool sql_executor_has_error(sql_executor_t *exec) {
    return exec && exec->error_msg != NULL;
}

/* ============================================================
 * 统计
 * ============================================================ */

void sql_executor_get_stats(sql_executor_t *exec, sql_executor_stats_t *stats) {
    if (!exec || !stats) {
        return;
    }

    memset(stats, 0, sizeof(*stats));

    stats->queries_executed = exec->queries_executed;
    stats->rows_processed = exec->rows_processed;
    stats->txn_id = exec->txn_id;
    stats->in_transaction = exec->in_transaction;

    /* 获取 Buffer 统计 */
    buf_stats_t buf_stats;
    buf_get_stats(&buf_stats);
    stats->cache_hits = buf_stats.hits;
    stats->cache_misses = buf_stats.misses;

    /* 获取脏页数 */
    if (exec->wal_buf) {
        stats->dirty_pages = wal_buf_dirty_count(exec->wal_buf);
    }
}
