/**
 * @file cf_row.c
 * @brief 行（Row）操作实现
 *
 * 提供行的创建、释放、列增删查、序列化、反序列化。
 * 列按列名字典序排列以便二分查找。
 */

#include "db/cf/cf_row.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 行操作实现
 * ============================================================ */

cf_row_t *cf_row_create(const char *row_key, uint32_t row_key_len) {
    if (!row_key || row_key_len == 0 || row_key_len > CF_MAX_ROW_KEY_LEN) {
        return NULL;
    }

    cf_row_t *row = (cf_row_t *)calloc(1, sizeof(cf_row_t));
    if (!row) return NULL;

    row->row_key = (char *)malloc(row_key_len + 1);
    if (!row->row_key) {
        free(row);
        return NULL;
    }
    memcpy(row->row_key, row_key, row_key_len);
    row->row_key[row_key_len] = '\0';
    row->row_key_len = row_key_len;

    /* 初始列容量 */
    row->capacity = 8;
    row->columns = (cf_column_t **)calloc(row->capacity, sizeof(cf_column_t *));
    if (!row->columns) {
        free(row->row_key);
        free(row);
        return NULL;
    }

    row->num_columns = 0;
    row->timestamp = 0;
    return row;
}

void cf_row_free(cf_row_t *row) {
    if (!row) return;
    if (row->row_key) free(row->row_key);
    if (row->columns) {
        for (uint32_t i = 0; i < row->num_columns; i++) {
            cf_column_free(row->columns[i]);
        }
        free(row->columns);
    }
    free(row);
}

/**
 * @brief 在列数组中找到列的位置（按列名）
 * @param row 行
 * @param col_name 列名
 * @param col_name_len 列名长度
 * @param out_found 输出：是否找到同名列
 * @return 列索引（找到时）或插入位置（未找到时）
 */
static uint32_t find_column_index(const cf_row_t *row,
                                  const char *col_name,
                                  uint32_t col_name_len,
                                  bool *out_found) {
    *out_found = false;
    /* 列按列名字典序排列，二分查找 */
    uint32_t lo = 0, hi = row->num_columns;
    while (lo < hi) {
        uint32_t mid = lo + (hi - lo) / 2;
        cf_column_t *c = row->columns[mid];
        size_t min_len = (c->name_len < col_name_len) ? c->name_len : col_name_len;
        int cmp = memcmp(c->name, col_name, min_len);
        if (cmp == 0) {
            if (c->name_len < col_name_len) cmp = -1;
            else if (c->name_len > col_name_len) cmp = 1;
        }
        if (cmp == 0) {
            *out_found = true;
            return mid;
        }
        if (cmp < 0) lo = mid + 1;
        else hi = mid;
    }
    return lo;  /* 未找到，返回应插入的位置 */
}

/**
 * @brief 扩展列数组容量
 */
static int grow_columns(cf_row_t *row) {
    if (row->num_columns < row->capacity) return 0;
    uint32_t new_cap = row->capacity * 2;
    if (new_cap > CF_MAX_COLUMNS_PER_ROW) {
        if (row->capacity >= CF_MAX_COLUMNS_PER_ROW) return -1;
        new_cap = CF_MAX_COLUMNS_PER_ROW;
    }
    cf_column_t **new_arr = (cf_column_t **)realloc(
        row->columns, new_cap * sizeof(cf_column_t *));
    if (!new_arr) return -1;
    row->columns = new_arr;
    row->capacity = new_cap;
    return 0;
}

int cf_row_add_column(cf_row_t *row, const cf_column_t *column) {
    if (!row || !column) return -1;

    bool found = false;
    uint32_t pos = find_column_index(row, column->name, column->name_len, &found);

    if (found) {
        /* 已存在同名列，替换 */
        cf_column_t *existing = row->columns[pos];
        cf_column_free(existing);
        row->columns[pos] = cf_column_clone(column);
        if (!row->columns[pos]) {
            row->columns[pos] = NULL;
            return -1;
        }
        return 0;
    }

    /* 不存在，需插入到 pos 处 */
    if (grow_columns(row) != 0) return -1;

    cf_column_t *new_col = cf_column_clone(column);
    if (!new_col) return -1;

    /* 后移元素 */
    if (pos < row->num_columns) {
        memmove(&row->columns[pos + 1], &row->columns[pos],
                (row->num_columns - pos) * sizeof(cf_column_t *));
    }
    row->columns[pos] = new_col;
    row->num_columns++;
    return 0;
}

const cf_column_t *cf_row_get_column(const cf_row_t *row,
                                     const char *col_name,
                                     uint32_t col_name_len) {
    if (!row || !col_name) return NULL;
    bool found = false;
    uint32_t pos = find_column_index(row, col_name, col_name_len, &found);
    if (!found) return NULL;
    return row->columns[pos];
}

int cf_row_delete_column(cf_row_t *row,
                         const char *col_name,
                         uint32_t col_name_len) {
    if (!row || !col_name) return -1;
    bool found = false;
    uint32_t pos = find_column_index(row, col_name, col_name_len, &found);
    if (!found) return -1;  /* 不存在 */

    cf_column_free(row->columns[pos]);
    /* 前移后续元素 */
    if (pos + 1 < row->num_columns) {
        memmove(&row->columns[pos], &row->columns[pos + 1],
                (row->num_columns - pos - 1) * sizeof(cf_column_t *));
    }
    row->num_columns--;
    return 0;
}

size_t cf_row_serialized_size(const cf_row_t *row) {
    if (!row) return 0;
    /* row_key_len(4) + row_key + num_columns(4) + sum(col sizes) */
    size_t total = 4 + row->row_key_len + 4;
    for (uint32_t i = 0; i < row->num_columns; i++) {
        total += cf_column_serialized_size(row->columns[i]);
    }
    return total;
}

int cf_row_serialize(const cf_row_t *row, void *buf, size_t buf_size) {
    if (!row || !buf) return -1;
    size_t need = cf_row_serialized_size(row);
    if (buf_size < need) return -1;

    uint8_t *p = (uint8_t *)buf;

    /* row_key_len */
    memcpy(p, &row->row_key_len, sizeof(uint32_t));
    p += sizeof(uint32_t);
    /* row_key */
    memcpy(p, row->row_key, row->row_key_len);
    p += row->row_key_len;
    /* num_columns */
    memcpy(p, &row->num_columns, sizeof(uint32_t));
    p += sizeof(uint32_t);
    /* 各列 */
    for (uint32_t i = 0; i < row->num_columns; i++) {
        size_t col_size = cf_column_serialized_size(row->columns[i]);
        if (cf_column_serialize(row->columns[i], p, col_size) != 0) {
            return -1;
        }
        p += col_size;
    }

    return 0;
}

int cf_row_deserialize(const void *buf, size_t buf_len, cf_row_t **out_row) {
    if (!buf || !out_row || buf_len < 4 + 4) return -1;

    const uint8_t *p = (const uint8_t *)buf;

    /* row_key_len */
    uint32_t row_key_len;
    memcpy(&row_key_len, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    if (row_key_len == 0 || row_key_len > CF_MAX_ROW_KEY_LEN) return -1;
    if ((size_t)(p - (const uint8_t *)buf) + row_key_len + 4 > buf_len) return -1;

    /* row_key */
    char *row_key = (char *)malloc(row_key_len + 1);
    if (!row_key) return -1;
    memcpy(row_key, p, row_key_len);
    row_key[row_key_len] = '\0';
    p += row_key_len;

    /* num_columns */
    uint32_t num_cols;
    memcpy(&num_cols, p, sizeof(uint32_t));
    p += sizeof(uint32_t);
    if (num_cols > CF_MAX_COLUMNS_PER_ROW) {
        free(row_key);
        return -1;
    }

    cf_row_t *row = cf_row_create(row_key, row_key_len);
    free(row_key);
    if (!row) return -1;

    /* 解析各列（使用 add_column 自动排序） */
    for (uint32_t i = 0; i < num_cols; i++) {
        size_t remaining = buf_len - (size_t)(p - (const uint8_t *)buf);
        cf_column_t *col = NULL;
        if (cf_column_deserialize(p, remaining, &col) != 0) {
            cf_row_free(row);
            return -1;
        }
        /* 直接放入数组（按读入顺序暂存，最后整理） */
        if (row->num_columns >= row->capacity) {
            if (grow_columns(row) != 0) {
                cf_column_free(col);
                cf_row_free(row);
                return -1;
            }
        }
        row->columns[row->num_columns++] = col;
        p += cf_column_serialized_size(col);
    }

    /* 对列按列名排序以保证二分查找正确性 */
    /* 简单插入排序：列数通常较少 */
    for (uint32_t i = 1; i < row->num_columns; i++) {
        cf_column_t *key = row->columns[i];
        uint32_t j = i;
        while (j > 0 && cf_column_compare(row->columns[j - 1], key) > 0) {
            row->columns[j] = row->columns[j - 1];
            j--;
        }
        row->columns[j] = key;
    }

    *out_row = row;
    return 0;
}