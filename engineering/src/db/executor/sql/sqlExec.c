/**
 * @file sql_exec.c
 * @brief SQL 查询执行器实现
 */

#include "db/executor/sql/sql_exec.h"
#include "db/executor/sql/sql_semantic.h"
#include "db/table.h"
#include "db/kv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ============================================================
 * 结果集结构
 * ============================================================ */

struct sql_result_s {
    char **column_names;       /**< 列名数组 */
    sql_data_type_t *column_types; /**< 列类型数组 */
    size_t num_columns;        /**< 列数 */
    void ***rows;              /**< 行数据数组 */
    size_t num_rows;           /**< 行数 */
    size_t capacity;            /**< 容量 */
};

/* ============================================================
 * 执行器上下文
 * ============================================================ */

struct sql_exec_s {
    kv_t           *db;             /**< KV 数据库 */
    sql_semantic_t *semantic;        /**< 语义分析器 */
    char           error_msg[256];    /**< 错误信息 */
    table_t        *current_table;   /**< 当前操作的表 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static void set_error(sql_exec_t *exec, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(exec->error_msg, sizeof(exec->error_msg), fmt, args);
    va_end(args);
}

/* ============================================================
 * 表达式求值
 * ============================================================ */

bool sql_eval_expr(const sql_node_t *expr, const void *row_data,
                  const table_meta_t *meta) {
    if (!expr || !row_data || !meta) return false;

    switch (expr->type) {
        case SQL_NODE_CONSTANT: {
            /* 简化：非零视为 true */
            if (expr->u.constant.type == SQL_TYPE_INT) {
                return expr->u.constant.int_value != 0;
            }
            return expr->u.constant.str_value != NULL;
        }
        case SQL_NODE_COLUMN_REF: {
            int col_idx = sql_semantic_find_column(meta, expr->u.column_ref.name);
            if (col_idx < 0) return false;

            const table_column_t *col = &meta->columns[col_idx];
            uint8_t *data = (uint8_t *)row_data;
            uint32_t null_mask;
            memcpy(&null_mask, data, sizeof(null_mask));

            if (null_mask & (1 << col_idx)) return false;

            if (col->type == SQL_TYPE_INT) {
                int32_t val;
                memcpy(&val, data + col->offset, sizeof(val));
                return val != 0;
            }
            return true;
        }
        case SQL_NODE_BINARY_OP: {
            /* 获取左值和右值 */
            int left_val = 0, right_val = 0;

            if (expr->u.binary_op.left) {
                if (expr->u.binary_op.left->type == SQL_NODE_COLUMN_REF) {
                    int col_idx = sql_semantic_find_column(meta,
                        expr->u.binary_op.left->u.column_ref.name);
                    if (col_idx >= 0) {
                        const table_column_t *col = &meta->columns[col_idx];
                        uint8_t *data = (uint8_t *)row_data;
                        if (col->type == SQL_TYPE_INT) {
                            memcpy(&left_val, data + col->offset, sizeof(left_val));
                        }
                    }
                } else if (expr->u.binary_op.left->type == SQL_NODE_CONSTANT) {
                    left_val = expr->u.binary_op.left->u.constant.int_value;
                }
            }

            if (expr->u.binary_op.right) {
                if (expr->u.binary_op.right->type == SQL_NODE_COLUMN_REF) {
                    int col_idx = sql_semantic_find_column(meta,
                        expr->u.binary_op.right->u.column_ref.name);
                    if (col_idx >= 0) {
                        const table_column_t *col = &meta->columns[col_idx];
                        uint8_t *data = (uint8_t *)row_data;
                        if (col->type == SQL_TYPE_INT) {
                            memcpy(&right_val, data + col->offset, sizeof(right_val));
                        }
                    }
                } else if (expr->u.binary_op.right->type == SQL_NODE_CONSTANT) {
                    right_val = expr->u.binary_op.right->u.constant.int_value;
                }
            }

            /* 根据操作符比较 */
            switch (expr->u.binary_op.op) {
                case SQL_NODE_EXPR: /* 这是简化处理，实际应使用 token 类型 */
                    return false;
                default: return false;
            }
        }
        default:
            return false;
    }
}

int sql_extract_column(const void *row_data, const table_column_t *col,
                      void *out_value) {
    if (!row_data || !col || !out_value) return -1;

    const uint8_t *data = (const uint8_t *)row_data;
    uint32_t null_mask;
    memcpy(&null_mask, data, sizeof(null_mask));

    if (null_mask & (1 << (col - col))) {
        /* NULL 值 */
        return -1;
    }

    if (col->type == SQL_TYPE_INT) {
        memcpy(out_value, data + col->offset, sizeof(int32_t));
    }
    return 0;
}

/* ============================================================
 * 结果集操作
 * ============================================================ */

sql_result_t *sql_result_create(size_t num_columns) {
    sql_result_t *r = (sql_result_t *)calloc(1, sizeof(sql_result_t));
    if (!r) return NULL;

    r->num_columns = num_columns;
    r->column_names = (char **)calloc(num_columns, sizeof(char *));
    r->column_types = (sql_data_type_t *)calloc(num_columns, sizeof(sql_data_type_t));
    r->capacity = 16;
    r->rows = (void ***)calloc(r->capacity, sizeof(void **));

    return r;
}

void sql_result_free(sql_result_t *r) {
    if (!r) return;

    if (r->column_names) {
        for (size_t i = 0; i < r->num_columns; i++) {
            if (r->column_names[i]) free(r->column_names[i]);
        }
        free(r->column_names);
    }
    if (r->column_types) free(r->column_types);

    if (r->rows) {
        for (size_t i = 0; i < r->num_rows; i++) {
            if (r->rows[i]) {
                for (size_t j = 0; j < r->num_columns; j++) {
                    if (r->rows[i][j]) free(r->rows[i][j]);
                }
                free(r->rows[i]);
            }
        }
        free(r->rows);
    }

    free(r);
}

size_t sql_result_num_columns(const sql_result_t *r) {
    return r ? r->num_columns : 0;
}

size_t sql_result_num_rows(const sql_result_t *r) {
    return r ? r->num_rows : 0;
}

const char *sql_result_column_name(const sql_result_t *r, size_t col) {
    if (!r || col >= r->num_columns) return NULL;
    return r->column_names[col];
}

sql_data_type_t sql_result_column_type(const sql_result_t *r, size_t col) {
    if (!r || col >= r->num_columns) return SQL_TYPE_INT;
    return r->column_types[col];
}

sql_exec_result_t sql_result_get_row(const sql_result_t *r, size_t row,
                                    void **out_values) {
    if (!r || row >= r->num_rows || !out_values) return SQL_EXEC_ERROR;

    for (size_t i = 0; i < r->num_columns; i++) {
        out_values[i] = r->rows[row][i];
    }
    return SQL_EXEC_OK;
}

static void result_add_row(sql_result_t *r, void **values) {
    if (!r || !values) return;

    if (r->num_rows >= r->capacity) {
        r->capacity *= 2;
        r->rows = (void ***)realloc(r->rows, r->capacity * sizeof(void **));
    }

    r->rows[r->num_rows] = (void **)calloc(r->num_columns, sizeof(void *));
    for (size_t i = 0; i < r->num_columns; i++) {
        r->rows[r->num_rows][i] = values[i];
    }
    r->num_rows++;
}

/* ============================================================
 * SELECT 执行
 * ============================================================ */

/* 获取列的存储大小 */
static size_t get_column_size(const table_column_t *col) {
    switch (col->type) {
        case SQL_TYPE_INT:    return sizeof(int32_t);
        case SQL_TYPE_REAL:   return sizeof(double);
        case SQL_TYPE_VARCHAR:
        case SQL_TYPE_TEXT:   return col->length > 0 ? col->length : 256;
        case SQL_TYPE_BLOB:   return col->length > 0 ? col->length : 1024;
        default:              return 0;
    }
}

static sql_result_t *exec_select(sql_exec_t *exec, const sql_node_t *node) {
    /* 先检查表是否存在 */
    const table_meta_t *meta;
    if (sql_semantic_analyze_select(exec->semantic, node, &meta) < 0) {
        return NULL;  /* 表不存在或语义错误 */
    }

    /* 创建结果集 */
    sql_result_t *result = sql_result_create(meta->num_columns);
    if (!result) return NULL;

    /* 设置列名和类型 */
    for (size_t i = 0; i < meta->num_columns; i++) {
        result->column_names[i] = strdup(meta->columns[i].name);
        result->column_types[i] = meta->columns[i].type;
    }

    /* 打开表并扫描数据 */
    table_t *table = table_open(exec->db, node->u.select.table_name);
    if (table) {
        table_iter_t *iter = table_scan(table);
        if (iter) {
            while (table_iter_next(iter) == 0) {
                (void)table_iter_row_id(iter);  /* 暂时忽略 row_id */
                void *row_data;
                if (table_iter_get_row(iter, &row_data) < 0) continue;

                /* 检查 WHERE 条件 */
                if (node->u.select.where_cond) {
                    if (!sql_eval_expr(node->u.select.where_cond, row_data, meta)) {
                        free(row_data);
                        continue;
                    }
                }

                /* 复制行数据到结果集 */
                void **row = (void **)calloc(meta->num_columns, sizeof(void *));
                for (size_t i = 0; i < meta->num_columns; i++) {
                    size_t col_size = get_column_size(&meta->columns[i]);
                    row[i] = malloc(col_size);
                    memcpy(row[i], (uint8_t *)row_data + meta->columns[i].offset, col_size);
                }
                result_add_row(result, row);
                free(row);  /* 只释放行指针，保留内部数据 */

                free(row_data);
            }
            table_iter_free(iter);
        }
        table_close(table);
    }

    return result;
}

/* ============================================================
 * INSERT 执行
 * ============================================================ */

static sql_result_t *exec_insert(sql_exec_t *exec, const sql_node_t *node) {
    const table_meta_t *meta;
    if (sql_semantic_analyze_insert(exec->semantic, node, &meta) < 0) {
        return NULL;
    }

    table_t *table = table_open(exec->db, node->u.insert.table_name);
    if (!table) {
        return NULL;
    }

    int inserted = 0;

    /* values 是一个表达式列表，每行一个 */
    sql_node_t *values = node->u.insert.values;
    if (values && values->type == SQL_NODE_EXPR_LIST) {
        /* 处理 VALUES 列表 */
        for (size_t row_idx = 0; row_idx < values->u.list.count; row_idx++) {
            sql_node_t *row = values->u.list.items[row_idx];
            if (!row || row->type != SQL_NODE_EXPR_LIST) continue;

            /* 分配值数组 */
            const void **val_array = (const void **)calloc(meta->num_columns, sizeof(void *));
            if (!val_array) continue;

            /* 解析每列的值并存储到临时缓冲区 */
            for (size_t col_idx = 0; col_idx < row->u.list.count && col_idx < meta->num_columns; col_idx++) {
                sql_node_t *val = row->u.list.items[col_idx];
                if (!val || val->type != SQL_NODE_CONSTANT) continue;

                size_t col_size = get_column_size(&meta->columns[col_idx]);
                void *buf = malloc(col_size);
                if (!buf) continue;

                switch (meta->columns[col_idx].type) {
                    case SQL_TYPE_INT:
                        memcpy(buf, &val->u.constant.int_value, sizeof(int32_t));
                        val_array[col_idx] = buf;
                        break;
                    case SQL_TYPE_REAL:
                        memcpy(buf, &val->u.constant.float_value, sizeof(double));
                        val_array[col_idx] = buf;
                        break;
                    case SQL_TYPE_TEXT:
                    case SQL_TYPE_VARCHAR:
                        if (val->u.constant.str_value) {
                            strncpy((char *)buf, val->u.constant.str_value, col_size - 1);
                        }
                        val_array[col_idx] = buf;
                        break;
                    default:
                        free(buf);
                        break;
                }
            }

            /* 插入到表 */
            if (table_insert(table, val_array, meta->num_columns) == 0) {
                inserted++;
            }

            /* 释放临时缓冲区 */
            for (size_t i = 0; i < meta->num_columns; i++) {
                if (val_array[i]) free((void *)val_array[i]);
            }
            free(val_array);
        }
    }

    table_close(table);

    /* 返回影响的行数 */
    sql_result_t *result = sql_result_create(1);
    if (result) {
        result->column_names[0] = strdup("rows_inserted");
        result->column_types[0] = SQL_TYPE_INT;
        int32_t *val = (int32_t *)malloc(sizeof(int32_t));
        *val = inserted;
        void **row = (void **)calloc(1, sizeof(void *));
        row[0] = val;
        result_add_row(result, row);
    }

    return result;
}

/* ============================================================
 * UPDATE 执行
 * ============================================================ */

static sql_result_t *exec_update(sql_exec_t *exec, const sql_node_t *node) {
    const table_meta_t *meta;
    if (sql_semantic_analyze_update(exec->semantic, node, &meta) < 0) {
        return NULL;
    }

    table_t *table = table_open(exec->db, node->u.update.table_name);
    if (!table) {
        return NULL;
    }

    /* 简化：先实现全表更新，再支持 WHERE */
    int updated = 0;

    table_iter_t *iter = table_scan(table);
    if (!iter) {
        table_close(table);
        return NULL;
    }

    while (table_iter_next(iter) == 0) {
        uint64_t row_id = table_iter_row_id(iter);
        void *row_data;
        if (table_iter_get_row(iter, &row_data) < 0) continue;

        /* 检查 WHERE 条件 */
        if (node->u.update.where_cond) {
            if (!sql_eval_expr(node->u.update.where_cond, row_data, meta)) {
                free(row_data);
                continue;
            }
        }

        /* 应用 SET 列表 */
        for (size_t i = 0; i < node->u.update.set_list->u.list.count; i++) {
            sql_node_t *set = node->u.update.set_list->u.list.items[i];
            if (set && set->type == SQL_NODE_BINARY_OP) {
                sql_node_t *left = set->u.binary_op.left;
                sql_node_t *right = set->u.binary_op.right;

                if (left && left->type == SQL_NODE_COLUMN_REF) {
                    int col_idx = sql_semantic_find_column(meta, left->u.column_ref.name);
                    if (col_idx >= 0 && right && right->type == SQL_NODE_CONSTANT) {
                        if (right->u.constant.type == SQL_TYPE_INT) {
                            uint8_t *data = (uint8_t *)row_data;
                            memcpy(data + meta->columns[col_idx].offset,
                                   &right->u.constant.int_value,
                                   sizeof(int32_t));
                        }
                    }
                }
            }
        }

        /* 写回 */
        char key[256];
        snprintf(key, sizeof(key), "table:%s:row:%lu",
                node->u.update.table_name, (unsigned long)row_id);
        kv_put(exec->db, key, strlen(key), row_data, meta->row_size);
        updated++;

        free(row_data);
    }

    table_iter_free(iter);
    table_close(table);

    /* 返回影响的行数 */
    sql_result_t *result = sql_result_create(1);
    if (result) {
        result->column_names[0] = strdup("rows_updated");
        result->column_types[0] = SQL_TYPE_INT;
        int32_t *val = (int32_t *)malloc(sizeof(int32_t));
        *val = updated;
        void **row = (void **)calloc(1, sizeof(void *));
        row[0] = val;
        result_add_row(result, row);
    }

    return result;
}

/* ============================================================
 * DELETE 执行
 * ============================================================ */

static sql_result_t *exec_delete(sql_exec_t *exec, const sql_node_t *node) {
    const table_meta_t *meta;
    if (sql_semantic_analyze_delete(exec->semantic, node, &meta) < 0) {
        return NULL;
    }

    table_t *table = table_open(exec->db, node->u.del.table_name);
    if (!table) {
        return NULL;
    }

    /* 简化：使用 kv_delete 直接删除 */
    int deleted = 0;

    table_iter_t *iter = table_scan(table);
    if (!iter) {
        table_close(table);
        return NULL;
    }

    while (table_iter_next(iter) == 0) {
        uint64_t row_id = table_iter_row_id(iter);
        void *row_data;
        if (table_iter_get_row(iter, &row_data) < 0) continue;

        /* 检查 WHERE 条件 */
        if (node->u.del.where_cond) {
            if (!sql_eval_expr(node->u.del.where_cond, row_data, meta)) {
                free(row_data);
                continue;
            }
        }

        /* 删除行 */
        char key[256];
        snprintf(key, sizeof(key), "table:%s:row:%lu",
                node->u.del.table_name, (unsigned long)row_id);
        kv_delete(exec->db, key, strlen(key));
        deleted++;

        free(row_data);
    }

    table_iter_free(iter);
    table_close(table);

    /* 返回影响的行数 */
    sql_result_t *result = sql_result_create(1);
    if (result) {
        result->column_names[0] = strdup("rows_deleted");
        result->column_types[0] = SQL_TYPE_INT;
        int32_t *val = (int32_t *)malloc(sizeof(int32_t));
        *val = deleted;
        void **row = (void **)calloc(1, sizeof(void *));
        row[0] = val;
        result_add_row(result, row);
    }

    return result;
}

/* ============================================================
 * 公共 API
 * ============================================================ */

sql_exec_t *sql_exec_create(kv_t *db) {
    if (!db) return NULL;
    sql_exec_t *exec = (sql_exec_t *)calloc(1, sizeof(sql_exec_t));
    if (!exec) return NULL;
    exec->db = db;
    exec->semantic = sql_semantic_create(db);
    return exec;
}

void sql_exec_destroy(sql_exec_t *exec) {
    if (exec) {
        if (exec->semantic) sql_semantic_destroy(exec->semantic);
        if (exec->current_table) table_close(exec->current_table);
        free(exec);
    }
}

const char *sql_exec_errmsg(sql_exec_t *exec) {
    if (!exec) return "Invalid executor";
    if (exec->error_msg[0]) return exec->error_msg;
    return sql_semantic_errmsg(exec->semantic);
}

sql_result_t *sql_exec(sql_exec_t *exec, const sql_node_t *node) {
    if (!exec || !node) {
        set_error(exec, "Invalid arguments");
        return NULL;
    }

    switch (node->type) {
        case SQL_NODE_SELECT: return exec_select(exec, node);
        case SQL_NODE_INSERT: return exec_insert(exec, node);
        case SQL_NODE_UPDATE: return exec_update(exec, node);
        case SQL_NODE_DELETE: return exec_delete(exec, node);
        default:
            set_error(exec, "Unsupported statement type");
            return NULL;
    }
}

sql_exec_result_t sql_exec_ddl(sql_exec_t *exec, const sql_node_t *node) {
    if (!exec || !node) return SQL_EXEC_ERROR;

    switch (node->type) {
        case SQL_NODE_CREATE_TABLE: {
            if (sql_semantic_analyze_create_table(exec->semantic, node) < 0) {
                return SQL_EXEC_ERROR;
            }

            /* 解析列定义 */
            table_column_t *columns = NULL;
            size_t num_cols = 0;

            if (node->u.create_table.columns) {
                num_cols = node->u.create_table.columns->u.list.count;
                columns = (table_column_t *)calloc(num_cols, sizeof(table_column_t));

                for (size_t i = 0; i < num_cols; i++) {
                    sql_node_t *col = node->u.create_table.columns->u.list.items[i];
                    if (col && col->type == SQL_NODE_COLUMN_DEF) {
                        columns[i].name = strdup(col->u.column_def.name);
                        columns[i].type = col->u.column_def.type;
                        columns[i].length = col->u.column_def.length;
                        columns[i].not_null = col->u.column_def.not_null;
                        columns[i].primary_key = col->u.column_def.primary_key;
                    }
                }
            }

            /* 创建表 */
            table_t *table = table_create(exec->db, node->u.create_table.table_name,
                                          columns, num_cols);

            if (columns) {
                for (size_t i = 0; i < num_cols; i++) {
                    if (columns[i].name) free(columns[i].name);
                }
                free(columns);
            }

            if (!table) {
                set_error(exec, "Failed to create table");
                return SQL_EXEC_ERROR;
            }

            /* 保存元数据到 KV */
            char meta_key[256];
            snprintf(meta_key, sizeof(meta_key), "table:%s:meta",
                    node->u.create_table.table_name);

            const table_meta_t *meta = table_get_meta(table);
            kv_put(exec->db, meta_key, strlen(meta_key),
                  meta, sizeof(table_meta_t));

            table_close(table);
            return SQL_EXEC_OK;
        }
        case SQL_NODE_DROP_TABLE: {
            if (sql_semantic_analyze_drop_table(exec->semantic, node) < 0) {
                return SQL_EXEC_ERROR;
            }

            /* 删除所有行 - 使用 kv_scan 遍历 */
            char prefix[256];
            snprintf(prefix, sizeof(prefix), "table:%s:", node->u.drop_table.table_name);

            kv_iter_t *iter = kv_scan(exec->db, prefix, strlen(prefix), NULL, 0);
            if (iter) {
                while (kv_iter_next(iter) == KV_OK) {
                    const void *key = kv_iter_key(iter);
                    size_t key_len = kv_iter_key_len(iter);
                    kv_delete(exec->db, key, key_len);
                }
                kv_iter_free(iter);
            }

            return SQL_EXEC_OK;
        }
        default:
            set_error(exec, "Unsupported DDL statement");
            return SQL_EXEC_ERROR;
    }
}
