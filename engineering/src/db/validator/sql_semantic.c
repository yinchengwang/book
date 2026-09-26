/**
 * @file sql_semantic.c
 * @brief SQL 语义分析器实现
 *
 * T5：语义层与 KV 存储解耦，表/列存在性校验改走 catalog API
 * （catalog_lookup_table / catalog_get_table / catalog_get_columns）。
 */

#include "sql_semantic.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================
 * 语义上下文结构
 * ============================================================ */

struct sql_semantic_s {
    char          error_msg[256];   /**< 错误信息 */
    table_info_t  current_table;    /**< 当前分析的表（catalog 信息拷贝） */
    bool          has_table;        /**< current_table 是否有效 */
    column_info_t *columns;         /**< 当前表的列信息数组（本上下文持有） */
    int           ncolumns;         /**< 列数量 */
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

/* 释放当前持有的列信息 */
static void clear_columns(sql_semantic_t *ctx) {
    if (ctx->columns) {
        catalog_free_columns(ctx->columns);
        ctx->columns = NULL;
    }
    ctx->ncolumns = 0;
    ctx->has_table = false;
}

/* 从 catalog 加载表与列元数据；失败时设置错误信息并返回 -1 */
static int load_table_meta(sql_semantic_t *ctx, const char *table_name) {
    clear_columns(ctx);

    Oid oid = catalog_lookup_table(table_name);
    if (oid == InvalidOid) {
        set_error(ctx, "Table '%s' does not exist", table_name);
        return -1;
    }

    /* 拷贝表信息，避免悬挂引用 catalog 缓存内的条目 */
    table_info_t *info = catalog_get_table(oid);
    if (!info) {
        set_error(ctx, "Table '%s' does not exist", table_name);
        return -1;
    }
    ctx->current_table = *info;
    ctx->has_table = true;

    /* 列信息数组由 catalog_get_columns 分配，所有权转移到本上下文 */
    ctx->columns = catalog_get_columns(oid, &ctx->ncolumns);
    if (ctx->ncolumns > 0 && !ctx->columns) {
        set_error(ctx, "Failed to load columns of table '%s'", table_name);
        ctx->has_table = false;
        return -1;
    }

    return 0;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

sql_semantic_t *sql_semantic_create(void) {
    return (sql_semantic_t *)calloc(1, sizeof(sql_semantic_t));
}

void sql_semantic_destroy(sql_semantic_t *ctx) {
    if (ctx) {
        clear_columns(ctx);
        free(ctx);
    }
}

const char *sql_semantic_errmsg(sql_semantic_t *ctx) {
    return ctx ? ctx->error_msg : "Invalid context";
}

int sql_semantic_find_column(const column_info_t *columns, int ncolumns,
                             const char *name) {
    if (!columns || !name) return -1;
    for (int i = 0; i < ncolumns; i++) {
        if (strcmp(columns[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const table_info_t *sql_semantic_get_current_table(sql_semantic_t *ctx) {
    return (ctx && ctx->has_table) ? &ctx->current_table : NULL;
}

int sql_semantic_get_columns(sql_semantic_t *ctx, const column_info_t **out_columns) {
    if (!ctx || !ctx->has_table) return -1;
    if (out_columns) *out_columns = ctx->columns;
    return ctx->ncolumns;
}

/* ============================================================
 * SELECT 分析
 * ============================================================ */

static int analyze_select_columns(sql_semantic_t *ctx, const sql_node_t *columns) {
    if (!columns || columns->type != SQL_NODE_EXPR_LIST) return -1;

    for (size_t i = 0; i < columns->u.list.count; i++) {
        sql_node_t *col = columns->u.list.items[i];
        if (!col) continue;

        if (col->type == SQL_NODE_COLUMN_REF) {
            /* 检查是否是 * */
            if (strcmp(col->u.column_ref.name, "*") == 0) continue;

            /* 检查列是否存在 */
            if (sql_semantic_find_column(ctx->columns, ctx->ncolumns,
                                         col->u.column_ref.name) < 0) {
                set_error(ctx, "Column '%s' does not exist", col->u.column_ref.name);
                return -1;
            }
        }
    }
    return 0;
}

int sql_semantic_analyze_select(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_info_t **out_table) {
    if (!ctx || !node || node->type != SQL_NODE_SELECT) return -1;

    /* 加载表元数据 */
    if (load_table_meta(ctx, node->u.select.table_name) < 0) return -1;

    /* 分析列 */
    if (analyze_select_columns(ctx, node->u.select.columns) < 0) return -1;

    if (out_table) *out_table = &ctx->current_table;
    return 0;
}

/* ============================================================
 * INSERT 分析
 * ============================================================ */

int sql_semantic_analyze_insert(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_info_t **out_table) {
    if (!ctx || !node || node->type != SQL_NODE_INSERT) return -1;

    /* 加载表元数据 */
    if (load_table_meta(ctx, node->u.insert.table_name) < 0) return -1;

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
                if (sql_semantic_find_column(ctx->columns, ctx->ncolumns,
                                             col->u.column_ref.name) < 0) {
                    set_error(ctx, "Column '%s' does not exist", col->u.column_ref.name);
                    return -1;
                }
            }
        }
    } else {
        /* 没有指定列名，使用所有列 */
        if (num_values != (size_t)ctx->ncolumns) {
            set_error(ctx, "Value count (%zu) does not match column count (%d)",
                     num_values, ctx->ncolumns);
            return -1;
        }
    }

    if (out_table) *out_table = &ctx->current_table;
    return 0;
}

/* ============================================================
 * UPDATE 分析
 * ============================================================ */

int sql_semantic_analyze_update(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_info_t **out_table) {
    if (!ctx || !node || node->type != SQL_NODE_UPDATE) return -1;

    /* 加载表元数据 */
    if (load_table_meta(ctx, node->u.update.table_name) < 0) return -1;

    /* 检查 SET 列表中的列是否存在 */
    if (node->u.update.set_list) {
        for (size_t i = 0; i < node->u.update.set_list->u.list.count; i++) {
            sql_node_t *set = node->u.update.set_list->u.list.items[i];
            if (set && set->type == SQL_NODE_BINARY_OP && set->u.binary_op.left) {
                sql_node_t *left = set->u.binary_op.left;
                if (left->type == SQL_NODE_COLUMN_REF) {
                    if (sql_semantic_find_column(ctx->columns, ctx->ncolumns,
                                                 left->u.column_ref.name) < 0) {
                        set_error(ctx, "Column '%s' does not exist", left->u.column_ref.name);
                        return -1;
                    }
                }
            }
        }
    }

    if (out_table) *out_table = &ctx->current_table;
    return 0;
}

/* ============================================================
 * DELETE 分析
 * ============================================================ */

int sql_semantic_analyze_delete(sql_semantic_t *ctx, const sql_node_t *node,
                               const table_info_t **out_table) {
    if (!ctx || !node || node->type != SQL_NODE_DELETE) return -1;

    /* 加载表元数据 */
    if (load_table_meta(ctx, node->u.del.table_name) < 0) return -1;

    if (out_table) *out_table = &ctx->current_table;
    return 0;
}

/* ============================================================
 * CREATE TABLE 分析
 * ============================================================ */

int sql_semantic_analyze_create_table(sql_semantic_t *ctx, const sql_node_t *node) {
    if (!ctx || !node || node->type != SQL_NODE_CREATE_TABLE) return -1;

    const char *table_name = node->u.create_table.table_name;

    /* 检查表是否已存在 */
    if (catalog_lookup_table(table_name) != InvalidOid) {
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
    if (catalog_lookup_table(table_name) == InvalidOid) {
        set_error(ctx, "Table '%s' does not exist", table_name);
        return -1;
    }

    return 0;
}
