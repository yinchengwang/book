/**
 * @file vecx_pipeline.c
 * @brief Gap#2 Task7 内存列 Source + 行→列适配器 + 批处理流水线演示
 *
 * 两种 Source 工厂：
 *   1. vecx_source_from_columns：内存列数组直接切成 VectorBlock 流
 *   2. vecx_source_from_rows：火山模型行迭代器适配为列块流
 *
 * 流水线演示：source → filter → agg
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * Source 内部结构
 * ======================================================================== */

struct vecx_source_s {
    int            ncols;
    int           *col_types;
    int           *col_elem_size;
    int64_t        total_rows;
    int64_t        cur_row;
    int            batch_size;

    /* 列数组 source */
    const void   **col_data;

    /* 行迭代 source */
    int (*row_next)(void *state, int ncols, void **col_values, int *col_types, char *isnull);
    void           *row_state;
    char           *row_buf;  /* 临时行缓冲 */

    /* 当前正在返回的块（next 成功后持有，调用方负责 destroy） */
    VectorBlock    *cur;
    int             cur_row_in_block;
};

/* ========================================================================
 * 工厂函数
 * ======================================================================== */

vecx_source_t *vecx_source_from_columns(int ncols, const int *col_types,
                                        const void **col_data, const int *col_elem_size,
                                        int64_t total_rows, int batch_size) {
    if (ncols <= 0 || !col_data || batch_size <= 0 || total_rows <= 0) return NULL;

    vecx_source_t *s = (vecx_source_t *)calloc(1, sizeof(vecx_source_t));
    if (!s) return NULL;

    s->ncols = ncols;
    s->total_rows = total_rows;
    s->cur_row = 0;
    s->batch_size = batch_size;

    /* 拷贝指针数组（不拷贝底层数据） */
    s->col_types = (int *)malloc((size_t)ncols * sizeof(int));
    s->col_elem_size = (int *)malloc((size_t)ncols * sizeof(int));
    s->col_data = (const void **)malloc((size_t)ncols * sizeof(void *));
    if (!s->col_types || !s->col_elem_size || !s->col_data) {
        free(s->col_types);
        free(s->col_elem_size);
        free((void *)s->col_data);
        free(s);
        return NULL;
    }

    for (int i = 0; i < ncols; i++) {
        s->col_types[i] = col_types ? col_types[i] : -1;
        s->col_elem_size[i] = col_elem_size ? col_elem_size[i] : 0;
        s->col_data[i] = col_data[i];
    }

    s->row_next = NULL;
    s->row_state = NULL;
    s->row_buf = NULL;
    s->cur = NULL;
    s->cur_row_in_block = 0;

    return s;
}

vecx_source_t *vecx_source_from_rows(int ncols, const int *col_types,
                                     const int *col_elem_size,
                                     int (*row_next)(void *state, int ncols,
                                                     void **col_values, int *col_types, char *isnull),
                                     void *state, int batch_size, int64_t hint_rows) {
    if (ncols <= 0 || !row_next || batch_size <= 0) return NULL;

    vecx_source_t *s = (vecx_source_t *)calloc(1, sizeof(vecx_source_t));
    if (!s) return NULL;

    s->ncols = ncols;
    s->total_rows = hint_rows; /* hint，可能不准确 */
    s->cur_row = 0;
    s->batch_size = batch_size;

    s->col_types = (int *)malloc((size_t)ncols * sizeof(int));
    s->col_elem_size = (int *)malloc((size_t)ncols * sizeof(int));
    if (!s->col_types || !s->col_elem_size) {
        free(s->col_types);
        free(s->col_elem_size);
        free(s);
        return NULL;
    }

    for (int i = 0; i < ncols; i++) {
        s->col_types[i] = col_types ? col_types[i] : -1;
        s->col_elem_size[i] = col_elem_size ? col_elem_size[i] : 0;
    }

    s->col_data = NULL;
    s->row_next = row_next;
    s->row_state = state;

    /* 分配临时行缓冲用于迭代 */
    size_t row_buf_size = 0;
    for (int i = 0; i < ncols; i++) {
        row_buf_size += (size_t)col_elem_size[i];
    }
    s->row_buf = (char *)malloc(row_buf_size);
    if (!s->row_buf) {
        free(s->col_types);
        free(s->col_elem_size);
        free(s);
        return NULL;
    }

    s->cur = NULL;
    s->cur_row_in_block = 0;

    return s;
}

/* ========================================================================
 * 取下一块
 * ======================================================================== */

/** 从列数组 source 产块 */
static VectorBlock *source_columns_next(vecx_source_t *s) {
    if (s->cur_row >= s->total_rows) return NULL;

    int nrows = (int)(s->batch_size < (s->total_rows - s->cur_row)
                         ? s->batch_size
                         : (s->total_rows - s->cur_row));

    VectorBlock *blk = vector_block_create(nrows, s->ncols);
    if (!blk) return NULL;

    /* 分配列缓冲并拷贝数据 */
    for (int c = 0; c < s->ncols; c++) {
        if (s->col_elem_size[c] <= 0) {
            vector_block_set_column_type(blk, c, s->col_types[c]);
            continue;
        }

        char *buf = (char *)malloc((size_t)nrows * (size_t)s->col_elem_size[c]);
        if (!buf) {
            vector_block_destroy(blk);
            return NULL;
        }

        const char *src = (const char *)s->col_data[c] + (size_t)s->cur_row * (size_t)s->col_elem_size[c];
        memcpy(buf, src, (size_t)nrows * (size_t)s->col_elem_size[c]);
        vector_block_set_column(blk, c, buf, s->col_elem_size[c]);
        vector_block_set_column_type(blk, c, s->col_types[c]);
    }

    /* 处理 null 位图：需要检查每个列的 null 标记 */
    /* 这里简化处理：如果有 null 位图，逐行检查 */
    /* 实际上，列数组 source 的 null 信息需要从外部传入，这里暂不支持 */

    vector_block_set_num_rows(blk, nrows);
    s->cur_row += nrows;
    return blk;
}

/** 从行迭代 source 产块 */
static VectorBlock *source_rows_next(vecx_source_t *s) {
    /* 计算一行所有列的偏移量 */
    size_t col_offsets[256]; /* 假设 ncols <= 256 */
    size_t row_size = 0;
    for (int c = 0; c < s->ncols; c++) {
        col_offsets[c] = row_size;
        row_size += (size_t)s->col_elem_size[c];
    }

    /* 分配批量行缓冲 */
    char *batch_buf = (char *)malloc(row_size * (size_t)s->batch_size);
    if (!batch_buf) return NULL;

    int nrows = 0;
    /* 收集最多 batch_size 行 */
    while (nrows < s->batch_size) {
        void *col_values[256];
        int col_types_out[256];
        char isnull[256];

        /* 构建指向当前行各列的指针（指向 batch_buf 内的正确位置） */
        for (int c = 0; c < s->ncols; c++) {
            col_values[c] = batch_buf + col_offsets[c] + (size_t)nrows * row_size;
        }

        int ret = s->row_next(s->row_state, s->ncols, col_values, col_types_out, isnull);
        if (ret == 0) break; /* 迭代结束 */
        if (ret < 0) {
            /* 错误，停止迭代 */
            break;
        }
        nrows++;
    }

    if (nrows <= 0) {
        free(batch_buf);
        return NULL;
    }

    /* 构建 VectorBlock：逐列从 batch_buf 拷贝 */
    VectorBlock *blk = vector_block_create(nrows, s->ncols);
    if (!blk) {
        free(batch_buf);
        return NULL;
    }

    for (int c = 0; c < s->ncols; c++) {
        char *col_buf = (char *)malloc((size_t)nrows * (size_t)s->col_elem_size[c]);
        if (!col_buf) {
            vector_block_destroy(blk);
            free(batch_buf);
            return NULL;
        }

        /* 从 batch_buf 中提取第 c 列的所有行 */
        for (int r = 0; r < nrows; r++) {
            memcpy(col_buf + (size_t)r * (size_t)s->col_elem_size[c],
                   batch_buf + col_offsets[c] + (size_t)r * row_size,
                   (size_t)s->col_elem_size[c]);
        }

        vector_block_set_column(blk, c, col_buf, s->col_elem_size[c]);
        vector_block_set_column_type(blk, c, s->col_types[c]);
    }

    free(batch_buf);
    vector_block_set_num_rows(blk, nrows);
    return blk;
}

VectorBlock *vecx_source_next(vecx_source_t *s) {
    if (!s) return NULL;

    /* 销毁上一块 */
    if (s->cur) {
        vector_block_destroy(s->cur);
        s->cur = NULL;
    }

    if (s->col_data) {
        return source_columns_next(s);
    } else {
        return source_rows_next(s);
    }
}

/* ========================================================================
 * 销毁
 * ======================================================================== */

void vecx_source_destroy(vecx_source_t *s) {
    if (!s) return;
    if (s->cur) {
        vector_block_destroy(s->cur);
        s->cur = NULL;
    }
    free(s->col_types);
    free(s->col_elem_size);
    free((void *)s->col_data);
    free(s->row_buf);
    free(s);
}

/* ========================================================================
 * 流水线演示：source → filter → agg
 *
 * 本函数仅示范如何组合 source + filter + agg。
 * 仅 SUM 语义正确实现；COUNT 等只做简化累加。
 * ======================================================================== */

int vecx_pipeline_filter_agg(vecx_source_t *s, int filter_col, CompareOp filter_op,
                             const void *filter_val, int agg_col, vecx_agg_kind_t agg_kind,
                             double *out, int *has_result) {
    if (!s || !out || !has_result) return -1;

    *out = 0.0;
    *has_result = 0;

    double total = 0.0;
    int64_t count = 0;
    int has = 0;

    VectorBlock *blk;
    while ((blk = vecx_source_next(s)) != NULL) {
        VectorBlock *filtered = NULL;
        int n = vecx_filter_block(blk, filter_col, filter_op, filter_val, &filtered);
        if (n > 0) {
            double v = 0.0;
            int h = 0;
            vecx_agg_scalar(filtered, agg_col, agg_kind, NULL, 0, &v, &h);
            if (h) {
                if (agg_kind == VECX_AGG_SUM) {
                    total += v;
                } else if (agg_kind == VECX_AGG_COUNT) {
                    total += v; /* COUNT 返回的就是 count 值 */
                }
                has = 1;
            }
            vector_block_destroy(filtered);
        }
        vector_block_destroy(blk);
        (void)n; /* 抑制未使用警告 */
    }

    *out = total;
    *has_result = has;
    return 0;
}
