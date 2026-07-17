/**
 * @file table.c
 * @brief 表管理器实现（简化版）
 *
 * 表数据直接存储在 KV 数据库中：
 * - 表元数据：table:<name>:meta
 * - 表行数据：table:<name>:row:<row_id>
 */

#include "db/table.h"
#include "db/kv.h"
#include "db/parser/sql/sql.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 表结构 */
struct table_s {
    kv_t           *db;             /**< KV 数据库 */
    table_meta_t   *meta;           /**< 表元数据 */
    char           *prefix;         /**< 键前缀 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 计算行大小 */
static size_t calc_row_size(const table_column_t *columns, size_t num_cols) {
    size_t size = sizeof(uint32_t);  /* null_mask */
    for (size_t i = 0; i < num_cols; i++) {
        switch (columns[i].type) {
            case SQL_TYPE_INT:
            case SQL_TYPE_BLOB:
                size += sizeof(int32_t);
                break;
            case SQL_TYPE_VARCHAR:
            case SQL_TYPE_TEXT:
                size += columns[i].length > 0 ? columns[i].length : 256;
                break;
        }
    }
    return size;
}

/** 计算列偏移（用于 table_open 后重建偏移） */
static void recalc_offsets_from_meta(table_column_t *columns, size_t num_cols) {
    size_t offset = sizeof(uint32_t);
    for (size_t i = 0; i < num_cols; i++) {
        columns[i].offset = offset;
        switch (columns[i].type) {
            case SQL_TYPE_INT:
            case SQL_TYPE_BLOB:
                offset += sizeof(int32_t);
                break;
            case SQL_TYPE_VARCHAR:
            case SQL_TYPE_TEXT:
                offset += columns[i].length > 0 ? columns[i].length : 256;
                break;
        }
    }
}

/** 计算列偏移 */
static void calc_offsets(table_column_t *columns, size_t num_cols) {
    size_t offset = sizeof(uint32_t);  /* null_mask */
    for (size_t i = 0; i < num_cols; i++) {
        columns[i].offset = offset;
        switch (columns[i].type) {
            case SQL_TYPE_INT:
            case SQL_TYPE_BLOB:
                offset += sizeof(int32_t);
                break;
            case SQL_TYPE_VARCHAR:
            case SQL_TYPE_TEXT:
                offset += columns[i].length > 0 ? columns[i].length : 256;
                break;
        }
    }
}

/* ============================================================
 * 表操作 API
 * ============================================================ */

table_t *table_create(kv_t *db, const char *name,
                      const table_column_t *columns, size_t num_columns) {
    if (!db || !name || !columns || num_columns == 0) return NULL;

    /* 分配表结构 */
    table_t *table = (table_t *)calloc(1, sizeof(table_t));
    if (!table) return NULL;

    table->db = db;

    /* 复制元数据 */
    table->meta = (table_meta_t *)calloc(1, sizeof(table_meta_t));
    if (!table->meta) { free(table); return NULL; }

    table->meta->name = strdup(name);
    table->meta->num_columns = num_columns;
    table->meta->columns = (table_column_t *)calloc(num_columns, sizeof(table_column_t));

    for (size_t i = 0; i < num_columns; i++) {
        table->meta->columns[i].name = strdup(columns[i].name);
        table->meta->columns[i].type = columns[i].type;
        table->meta->columns[i].length = columns[i].length;
        table->meta->columns[i].not_null = columns[i].not_null;
        table->meta->columns[i].primary_key = columns[i].primary_key;
    }

    calc_offsets(table->meta->columns, num_columns);
    table->meta->row_size = calc_row_size(table->meta->columns, num_columns);
    table->meta->num_rows = 0;

    /* 生成前缀 */
    table->prefix = (char *)malloc(strlen(name) + 16);
    snprintf(table->prefix, strlen(name) + 16, "table:%s:", name);

    return table;
}

table_t *table_open(kv_t *db, const char *name) {
    if (!db || !name) return NULL;

    /* 分配表结构 */
    table_t *table = (table_t *)calloc(1, sizeof(table_t));
    if (!table) return NULL;

    table->db = db;

    /* 从 KV 加载元数据 */
    char meta_key[256];
    snprintf(meta_key, sizeof(meta_key), "table:%s:meta", name);

    size_t meta_len = 0;
    void *meta_data = NULL;
    kv_get(db, meta_key, strlen(meta_key), &meta_data, &meta_len);

    if (!meta_data || meta_len == 0) {
        /* 元数据不存在，创建空表结构 */
        table->meta = (table_meta_t *)calloc(1, sizeof(table_meta_t));
        if (!table->meta) { free(table); return NULL; }
        table->meta->name = strdup(name);
        table->meta->num_columns = 0;
        table->meta->columns = NULL;
    } else {
        /* 解析 JSON 格式的元数据 */
        table->meta = (table_meta_t *)calloc(1, sizeof(table_meta_t));
        if (!table->meta) {
            free(meta_data);
            free(table);
            return NULL;
        }

        /* 简单的 JSON 解析 */
        char *json = (char *)meta_data;
        json[meta_len] = '\0';  /* 确保以 null 结尾 */

        /* 解析表名 */
        char *name_start = strstr(json, "\"name\":\"");
        if (name_start) {
            name_start += 7;
            char *name_end = strchr(name_start, '"');
            if (name_end) {
                table->meta->name = (char *)malloc(name_end - name_start + 1);
                strncpy(table->meta->name, name_start, name_end - name_start);
                table->meta->name[name_end - name_start] = '\0';
            }
        }
        if (!table->meta->name) {
            table->meta->name = strdup(name);
        }

        /* 解析列数 */
        table->meta->num_columns = 0;
        char *cols_start = strstr(json, "\"columns\":[");
        char *cols_end = NULL;
        if (cols_start) {
            cols_end = cols_start + 10;
            while (*cols_end) {
                if (*cols_end == '{') table->meta->num_columns++;
                if (*cols_end == ']') break;
                cols_end++;
            }
        }

        /* 分配列空间并解析 */
        if (table->meta->num_columns > 0) {
            table->meta->columns = (table_column_t *)calloc(
                table->meta->num_columns, sizeof(table_column_t));

            int col_idx = 0;
            char *p = cols_start ? cols_start + 10 : NULL;
            while (p && *p && *p != ']' && col_idx < (int)table->meta->num_columns) {
                if (*p == '{') {
                    char *col_name_start = strstr(p, "\"name\":\"");
                    char *col_type_start = strstr(p, "\"type\":");

                    if (col_name_start && col_type_start) {
                        col_name_start += 7;
                        char *col_name_end = strchr(col_name_start, '"');
                        if (col_name_end) {
                            size_t name_len = col_name_end - col_name_start;
                            table->meta->columns[col_idx].name = (char *)malloc(name_len + 1);
                            strncpy(table->meta->columns[col_idx].name, col_name_start, name_len);
                            table->meta->columns[col_idx].name[name_len] = '\0';
                        }

                        col_type_start += 7;
                        table->meta->columns[col_idx].type = (sql_data_type_t)atoi(col_type_start);

                        /* 解析 length */
                        char *col_length_start = strstr(p, "\"length\":");
                        if (col_length_start) {
                            col_length_start += 8;
                            table->meta->columns[col_idx].length = (size_t)atol(col_length_start);
                        }

                        /* 解析 not_null */
                        char *nn = strstr(p, "\"not_null\":");
                        if (nn) {
                            nn += 10;
                            table->meta->columns[col_idx].not_null = (*nn == 't' || *nn == '1');
                        }

                        /* 解析 primary_key */
                        char *pk = strstr(p, "\"primary_key\":");
                        if (pk) {
                            pk += 13;
                            table->meta->columns[col_idx].primary_key = (*pk == 't' || *pk == '1');
                        }
                    }
                    col_idx++;
                }
                p++;
            }

            /* 重建偏移量和 row_size */
            recalc_offsets_from_meta(table->meta->columns, table->meta->num_columns);
            table->meta->row_size = calc_row_size(table->meta->columns, table->meta->num_columns);
        }

        free(meta_data);
    }

    /* 生成前缀 */
    table->prefix = (char *)malloc(strlen(name) + 16);
    snprintf(table->prefix, strlen(name) + 16, "table:%s:", name);

    return table;
}

void table_close(table_t *table) {
    if (!table) return;

    if (table->meta) {
        if (table->meta->name) free(table->meta->name);
        if (table->meta->columns) {
            for (size_t i = 0; i < table->meta->num_columns; i++) {
                if (table->meta->columns[i].name) {
                    free(table->meta->columns[i].name);
                }
            }
            free(table->meta->columns);
        }
        free(table->meta);
    }

    if (table->prefix) free(table->prefix);
    free(table);
}

int table_drop(table_t *table) {
    if (!table) return -1;
    /* 简化：只关闭表，不删除数据 */
    table_close(table);
    return 0;
}

const table_meta_t *table_get_meta(const table_t *table) {
    return table ? table->meta : NULL;
}

size_t table_num_columns(const table_t *table) {
    return table && table->meta ? table->meta->num_columns : 0;
}

const table_column_t *table_get_column(const table_t *table, size_t index) {
    if (!table || !table->meta) return NULL;
    if (index >= table->meta->num_columns) return NULL;
    return &table->meta->columns[index];
}

size_t table_row_size(const table_t *table) {
    return table && table->meta ? table->meta->row_size : 0;
}

/* 元数据设置函数（内部使用） */
void table_meta_set_num_columns(table_t *table, size_t num_cols) {
    if (!table || !table->meta) return;
    table->meta->num_columns = num_cols;
    if (num_cols > 0) {
        table->meta->columns = (table_column_t *)calloc(num_cols, sizeof(table_column_t));
    }
}

void table_meta_set_row_size(table_t *table, size_t row_size) {
    if (!table || !table->meta) return;
    table->meta->row_size = row_size;
}

void table_meta_set_column(table_t *table, size_t index, const char *name,
                         sql_data_type_t type, size_t length,
                         bool not_null, bool primary_key) {
    if (!table || !table->meta || !table->meta->columns) return;
    if (index >= table->meta->num_columns) return;

    table_column_t *col = &table->meta->columns[index];
    col->name = name ? strdup(name) : NULL;
    col->type = type;
    col->length = length;
    col->not_null = not_null;
    col->primary_key = primary_key;

    /* 计算偏移量（简化版：假设所有类型都是 4 字节） */
    col->offset = sizeof(uint32_t) * (index + 1);
}

void table_meta_set_column_from_stored(table_t *table, size_t index, const table_column_t *stored_col) {
    if (!table || !table->meta || !table->meta->columns) return;
    if (index >= table->meta->num_columns) return;
    if (!stored_col) return;

    table_column_t *col = &table->meta->columns[index];
    col->name = stored_col->name ? strdup(stored_col->name) : NULL;
    col->type = stored_col->type;
    col->length = stored_col->length;
    col->not_null = stored_col->not_null;
    col->primary_key = stored_col->primary_key;
    col->offset = stored_col->offset;
}

/* ============================================================
 * 行操作 API（简化实现）
 * ============================================================ */

int table_insert(table_t *table, const void **values, size_t num_values) {
    if (!table || !values) return -1;

    /* 简化：使用递增的 row_id 作为键 */
    static uint64_t next_row_id = 0;
    uint64_t row_id = __sync_fetch_and_add(&next_row_id, 1);

    /* 序列化行数据 */
    size_t row_size = table->meta->row_size;
    uint8_t *row_data = (uint8_t *)calloc(1, row_size);
    if (!row_data) return -1;

    /* 设置 null_mask */
    uint32_t null_mask = 0;
    memcpy(row_data, &null_mask, sizeof(null_mask));

    /* 复制值数据 */
    uint8_t *ptr = row_data + sizeof(null_mask);
    for (size_t i = 0; i < num_values && i < table->meta->num_columns; i++) {
        const void *val = values[i];
        size_t col_len;

        switch (table->meta->columns[i].type) {
            case SQL_TYPE_INT:
                if (val) {
                    *(int32_t *)ptr = *(int32_t *)val;
                } else {
                    null_mask |= (1 << i);
                }
                ptr += sizeof(int32_t);
                break;
            case SQL_TYPE_VARCHAR:
            case SQL_TYPE_TEXT:
                if (val) {
                    col_len = strlen((const char *)val);
                    size_t copy_len = col_len < table->meta->columns[i].length ?
                                   col_len : table->meta->columns[i].length;
                    memcpy(ptr, val, copy_len);
                } else {
                    null_mask |= (1 << i);
                }
                ptr += table->meta->columns[i].length > 0 ?
                       table->meta->columns[i].length : 256;
                break;
            default:
                break;
        }
    }

    /* 写回 null_mask */
    memcpy(row_data, &null_mask, sizeof(null_mask));

    /* 存储到 KV */
    char key[256];
    snprintf(key, sizeof(key), "%srow:%lu", table->prefix, (unsigned long)row_id);

    int result = kv_put(table->db, key, strlen(key), row_data, row_size);

    free(row_data);

    if (result == KV_OK) {
        table->meta->num_rows++;
    }

    return result == KV_OK ? 0 : -1;
}

int table_update(table_t *table, uint64_t row_id,
                 const void **values, size_t num_values) {
    if (!table || !values) return -1;

    /* 读取现有行 */
    void *old_row = NULL;
    int ret = table_get_row(table, row_id, &old_row);
    if (ret != 0 || !old_row) return -1;

    size_t row_size = table->meta->row_size;
    uint8_t *row_data = (uint8_t *)malloc(row_size);
    if (!row_data) { free(old_row); return -1; }

    /* 复制现有数据作为基础 */
    memcpy(row_data, old_row, row_size);
    free(old_row);

    /* 读取 null_mask */
    uint32_t null_mask;
    memcpy(&null_mask, row_data, sizeof(null_mask));

    /* 更新非 NULL 值 */
    for (size_t i = 0; i < num_values && i < table->meta->num_columns; i++) {
        const void *val = values[i];
        if (!val) continue;  /* NULL 表示不更新该字段 */

        size_t offset = table->meta->columns[i].offset;
        uint8_t *ptr = row_data + offset;

        switch (table->meta->columns[i].type) {
            case SQL_TYPE_INT:
                *(int32_t *)ptr = *(int32_t *)val;
                null_mask &= ~(1 << i);  /* 清除 NULL 标记 */
                break;
            case SQL_TYPE_VARCHAR:
            case SQL_TYPE_TEXT:
                {
                    size_t len = strlen((const char *)val);
                    size_t copy_len = len < table->meta->columns[i].length ?
                                   len : table->meta->columns[i].length;
                    memset(ptr, 0, table->meta->columns[i].length);
                    memcpy(ptr, val, copy_len);
                    null_mask &= ~(1 << i);
                }
                break;
            default:
                break;
        }
    }

    /* 写回 null_mask */
    memcpy(row_data, &null_mask, sizeof(null_mask));

    /* 存储到 KV */
    char key[256];
    snprintf(key, sizeof(key), "%srow:%lu", table->prefix, (unsigned long)row_id);
    int result = kv_put(table->db, key, strlen(key), row_data, row_size);

    free(row_data);
    return result == KV_OK ? 0 : -1;
}

int table_delete(table_t *table, uint64_t row_id) {
    if (!table) return -1;

    char key[256];
    snprintf(key, sizeof(key), "%srow:%lu", table->prefix, (unsigned long)row_id);

    int result = kv_delete(table->db, key, strlen(key));
    if (result == KV_OK) {
        table->meta->num_rows--;
    }
    return result == KV_OK ? 0 : -1;
}

int table_get_row(table_t *table, uint64_t row_id, void **out_row) {
    if (!table || !out_row) return -1;

    char key[256];
    snprintf(key, sizeof(key), "%srow:%lu", table->prefix, (unsigned long)row_id);

    size_t value_len = 0;
    void *value = NULL;
    int result = kv_get(table->db, key, strlen(key), &value, &value_len);

    if (result != KV_OK || !value || value_len == 0) return -1;

    *out_row = value;  /* kv_get 分配了内存，调用者负责释放 */
    return 0;
}

/* ============================================================
 * 扫描 API（简化实现）
 * ============================================================ */

struct table_iter_s {
    kv_t    *db;
    char    *prefix;
    kv_iter_t *kv_iter;
};

table_iter_t *table_scan(table_t *table) {
    if (!table) return NULL;

    table_iter_t *iter = (table_iter_t *)calloc(1, sizeof(table_iter_t));
    if (!iter) return NULL;

    iter->db = table->db;
    iter->prefix = strdup(table->prefix);

    /* 使用 KV 扫描 */
    iter->kv_iter = kv_scan(table->db, table->prefix, strlen(table->prefix),
                             NULL, 0);

    return iter;
}

int table_iter_next(table_iter_t *iter) {
    if (!iter || !iter->kv_iter) return 1;
    return kv_iter_next(iter->kv_iter) == KV_NOT_FOUND ? 1 : 0;
}

uint64_t table_iter_row_id(const table_iter_t *iter) {
    if (!iter || !iter->kv_iter) return 0;

    const void *key = kv_iter_key(iter->kv_iter);

    /* 解析 row_id */
    const char *key_str = (const char *)key;
    const char *row_str = strstr(key_str, "row:");
    if (row_str) {
        return strtoull(row_str + 4, NULL, 10);
    }
    return 0;
}

int table_iter_get_row(table_iter_t *iter, void **out_row) {
    if (!iter || !iter->kv_iter || !out_row) return -1;

    size_t value_len = kv_iter_value_len(iter->kv_iter);
    const void *value = kv_iter_value(iter->kv_iter);

    *out_row = malloc(value_len);
    if (*out_row) {
        memcpy(*out_row, value, value_len);
        return 0;
    }
    return -1;
}

void table_iter_free(table_iter_t *iter) {
    if (!iter) return;
    if (iter->prefix) free(iter->prefix);
    if (iter->kv_iter) kv_iter_free(iter->kv_iter);
    free(iter);
}