/**
 * @file sql_semantic.c
 * @brief SQL 语义分析器实现
 */

#include "sql_semantic.h"
#include "db/table.h"
#include "db/kv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================
 * 语义上下文结构
 * ============================================================ */

struct sql_semantic_s {
    kv_t       *db;             /**< KV 数据库 */
    char       error_msg[256];  /**< 错误信息 */
    table_t   *current_table;   /**< 当前分析的表 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static void set_error(sql_semantic_t *ctx, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(ctx->error_msg, sizeof(ctx->error_msg), fmt, args);
    va_end(args);
}

static table_t *load_table_meta(sql_semantic_t *ctx, const char *table_name) {
    char key[256];
    snprintf(key, sizeof(key), "table:%s:meta", table_name);

    size_t value_len = 0;
    void *value = NULL;
    kv_get(ctx->db, key, strlen(key), &value, &value_len);

    if (!value || value_len == 0) {
        set_error(ctx, "Table '%s' does not exist", table_name);
        return NULL;
    }

    /* 从 KV 读取扁平化的元数据 */
    table_meta_t *stored_meta = (table_meta_t *)value;

    /* 创建表 */
    table_t *table = table_open(ctx->db, table_name);
    if (!table) {
        return NULL;
    }

    /* 使用 API 设置元数据 */
    table_meta_set_num_columns(table, stored_meta->num_columns);
    table_meta_set_row_size(table, stored_meta->row_size);

    /* 复制列信息 */
    if (stored_meta->num_columns > 0 && stored_meta->columns) {
        table_column_t *stored_cols = stored_meta->columns;
        for (size_t i = 0; i < stored_meta->num_columns; i++) {
            table_meta_set_column_from_stored(table, i, &stored_cols[i]);
        }
    }

    return table;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

sql_semantic_t *sql_semantic_create(kv_t *db) {
    if (!db) return NULL;
    sql_semantic_t *ctx = (sql_semantic_t *)calloc(1, sizeof(sql_semantic_t));
    if (ctx) ctx->db = db;
    return ctx;
}

void sql_semantic_destroy(sql_semantic_t *ctx) {
    if (ctx) {
        if (ctx->current_table) table_close(ctx->current_table);
        free(ctx);
    }
}

const char *sql_semantic_errmsg(sql_semantic_t *ctx) {
    return ctx ? ctx->error_msg : "Invalid context";
}

int sql_semantic_find_column(const table_meta_t *meta, const char *name) {
    if (!meta || !name) return -1;
    for (size_t i = 0; i < meta->num_columns; i++) {
        if (strcmp(meta->columns[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

table_t *sql_semantic_get_current_table(sql_semantic_t *ctx) {
    return ctx ? ctx->current_table : NULL;
}

/* ============================================================
 * SELECT 分析
 * ============================================================ */

static int analyze_select_columns(sql_semantic_t *ctx, const sql_node_t *columns,
                                 const table_meta_t *meta) {
    if (!columns || columns->type != SQL_NODE_EXPR_LIST) return -1;

    for (size_t i = 0; i < columns->u.list.count; i++) {
        sql_node_t *col = columns->u.list.items[i];
        if (!col) continue;

        if (col->type == SQL_NODE_COLUMN_REF) {
            /* 检查是否是 * */
            if (strcmp(col->u.column_ref.name, "*") == 0) continue;

            /* 检查列是否存在 */
            if (sql_semantic_find_column(meta, col->u.column_ref.name) < 0) {
                set_error(ctx, "Column '%s' does not exist", col->u.column_ref.name);
                return -1;
            }
        }
    }
    return 0;
}

int sql_semantic_analyze_select(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_meta_t **out_meta) {
    if (!ctx || !node || node->type != SQL_NODE_SELECT) return -1;

    /* 加载表元数据 */
    if (ctx->current_table) table_close(ctx->current_table);
    ctx->current_table = load_table_meta(ctx, node->u.select.table_name);
    if (!ctx->current_table) return -1;

    const table_meta_t *meta = table_get_meta(ctx->current_table);
    if (!meta) { set_error(ctx, "Failed to get table meta"); return -1; }

    /* 分析列 */
    if (analyze_select_columns(ctx, node->u.select.columns, meta) < 0) return -1;

    if (out_meta) *out_meta = meta;
    return 0;
}

/* ============================================================
 * INSERT 分析
 * ============================================================ */

int sql_semantic_analyze_insert(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_meta_t **out_meta) {
    if (!ctx || !node || node->type != SQL_NODE_INSERT) return -1;

    /* 加载表元数据 */
    if (ctx->current_table) table_close(ctx->current_table);
    ctx->current_table = load_table_meta(ctx, node->u.insert.table_name);
    if (!ctx->current_table) return -1;

    const table_meta_t *meta = table_get_meta(ctx->current_table);
    if (!meta) { set_error(ctx, "Failed to get table meta"); return -1; }

    /* 检查列数量匹配 */
    size_t num_values = node->u.insert.values ? node->u.insert.values->u.list.count : 0;

    if (node->u.insert.columns) {
        /* 有指定列名 */
        if (node->u.insert.columns->u.list.count != num_values) {
            set_error(ctx, "Column count (%zu) does not match value count (%zu)",
                     node->u.insert.columns->u.list.count, num_values);
            return -1;
        }
        /* 检查列是否存在 */
        for (size_t i = 0; i < node->u.insert.columns->u.list.count; i++) {
            sql_node_t *col = node->u.insert.columns->u.list.items[i];
            if (col && col->type == SQL_NODE_COLUMN_REF) {
                if (sql_semantic_find_column(meta, col->u.column_ref.name) < 0) {
                    set_error(ctx, "Column '%s' does not exist", col->u.column_ref.name);
                    return -1;
                }
            }
        }
    } else {
        /* 没有指定列名，使用所有列 */
        if (num_values != meta->num_columns) {
            set_error(ctx, "Value count (%zu) does not match column count (%zu)",
                     num_values, meta->num_columns);
            return -1;
        }
    }

    if (out_meta) *out_meta = meta;
    return 0;
}

/* ============================================================
 * UPDATE 分析
 * ============================================================ */

int sql_semantic_analyze_update(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_meta_t **out_meta) {
    if (!ctx || !node || node->type != SQL_NODE_UPDATE) return -1;

    /* 加载表元数据 */
    if (ctx->current_table) table_close(ctx->current_table);
    ctx->current_table = load_table_meta(ctx, node->u.update.table_name);
    if (!ctx->current_table) return -1;

    const table_meta_t *meta = table_get_meta(ctx->current_table);
    if (!meta) { set_error(ctx, "Failed to get table meta"); return -1; }

    /* 检查 SET 列表中的列是否存在 */
    if (node->u.update.set_list) {
        for (size_t i = 0; i < node->u.update.set_list->u.list.count; i++) {
            sql_node_t *set = node->u.update.set_list->u.list.items[i];
            if (set && set->type == SQL_NODE_BINARY_OP && set->u.binary_op.left) {
                sql_node_t *left = set->u.binary_op.left;
                if (left->type == SQL_NODE_COLUMN_REF) {
                    if (sql_semantic_find_column(meta, left->u.column_ref.name) < 0) {
                        set_error(ctx, "Column '%s' does not exist", left->u.column_ref.name);
                        return -1;
                    }
                }
            }
        }
    }

    if (out_meta) *out_meta = meta;
    return 0;
}

/* ============================================================
 * DELETE 分析
 * ============================================================ */

int sql_semantic_analyze_delete(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_meta_t **out_meta) {
    if (!ctx || !node || node->type != SQL_NODE_DELETE) return -1;

    /* 加载表元数据 */
    if (ctx->current_table) table_close(ctx->current_table);
    ctx->current_table = load_table_meta(ctx, node->u.del.table_name);
    if (!ctx->current_table) return -1;

    const table_meta_t *meta = table_get_meta(ctx->current_table);
    if (!meta) { set_error(ctx, "Failed to get table meta"); return -1; }

    if (out_meta) *out_meta = meta;
    return 0;
}

/* ============================================================
 * CREATE TABLE 分析
 * ============================================================ */

int sql_semantic_analyze_create_table(sql_semantic_t *ctx, const sql_node_t *node) {
    if (!ctx || !node || node->type != SQL_NODE_CREATE_TABLE) return -1;

    const char *table_name = node->u.create_table.table_name;

    /* 检查表是否已存在 */
    char key[256];
    snprintf(key, sizeof(key), "table:%s:meta", table_name);

    if (kv_exists(ctx->db, key, strlen(key))) {
        set_error(ctx, "Table '%s' already exists", table_name);
        return -1;
    }

    /* 检查列定义 */
    if (node->u.create_table.columns) {
        for (size_t i = 0; i < node->u.create_table.columns->u.list.count; i++) {
            sql_node_t *col = node->u.create_table.columns->u.list.items[i];
            if (col && col->type == SQL_NODE_COLUMN_DEF) {
                if (!col->u.column_def.name || !col->u.column_def.name[0]) {
                    set_error(ctx, "Column name cannot be empty");
                    return -1;
                }
            }
        }
    }

    return 0;
}

/* ============================================================
 * DROP TABLE 分析
 * ============================================================ */

int sql_semantic_analyze_drop_table(sql_semantic_t *ctx, const sql_node_t *node) {
    if (!ctx || !node || node->type != SQL_NODE_DROP_TABLE) return -1;

    const char *table_name = node->u.drop_table.table_name;

    /* 检查表是否存在 */
    char key[256];
    snprintf(key, sizeof(key), "table:%s:meta", table_name);

    if (!kv_exists(ctx->db, key, strlen(key))) {
        set_error(ctx, "Table '%s' does not exist", table_name);
        return -1;
    }

    return 0;
}
