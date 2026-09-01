/**
 * @file vecx_block.c
 * @brief Gap#2 向量化执行引擎基础原语实现：位图操作 + 块 gather
 */

#include "db/vectorized/vectorized.h"

#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 位图 / 选择向量基础操作
 * ======================================================================== */

int vecx_bitmap_count(const uint64_t *bm, int nrows) {
    if (!bm || nrows <= 0) return 0;

    int full_words = nrows / 64;
    int count = 0;

    /* 完整字直接 popcount */
    for (int w = 0; w < full_words; w++) {
        count += __builtin_popcountll(bm[w]);
    }

    /* 尾字：只统计行号 < nrows 的位 */
    int tail_bits = nrows % 64;
    if (tail_bits > 0) {
        uint64_t tail = bm[full_words];
        /* 屏蔽 >= tail_bits 的位（1ULL << 64 为未定义行为，故特判） */
        uint64_t mask = (tail_bits == 64) ? ~(uint64_t)0 : ((1ULL << tail_bits) - 1ULL);
        count += __builtin_popcountll(tail & mask);
    }

    return count;
}

int vecx_bitmap_to_selection(const uint64_t *bm, int nrows, int *sel_out) {
    if (!bm || !sel_out || nrows <= 0) return 0;

    int k = 0;
    int nwords = (nrows + 63) / 64;
    for (int w = 0; w < nwords; w++) {
        uint64_t word = bm[w];
        while (word) {
            int bit = __builtin_ctzll(word);
            int row = w * 64 + bit;
            if (row < nrows) {
                sel_out[k++] = row;
            }
            word &= word - 1;  /* 清除最低置位 */
        }
    }
    return k;
}

void vecx_bitmap_and(const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out) {
    if (!a || !b || !out || nwords <= 0) return;
    for (int i = 0; i < nwords; i++) {
        out[i] = a[i] & b[i];
    }
}

void vecx_bitmap_or(const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out) {
    if (!a || !b || !out || nwords <= 0) return;
    for (int i = 0; i < nwords; i++) {
        out[i] = a[i] | b[i];
    }
}

/* ========================================================================
 * 块压缩 / 收集
 * ======================================================================== */

VectorBlock *vecx_block_gather(const VectorBlock *src, const int *sel, int nsel) {
    if (!src || !sel || nsel <= 0) return NULL;

    /* 新块容量取 nsel：行数即选中行数 */
    VectorBlock *dst = vector_block_create(nsel, src->num_columns);
    if (!dst) return NULL;

    /* 逐列深拷贝选定行 */
    for (int c = 0; c < src->num_columns; c++) {
        int elem = src->column_sizes[c];
        if (elem <= 0) {
            /* 源列未设置元素大小：仅复制类型标签，不分配缓冲 */
            vector_block_set_column_type(dst, c, vector_block_get_column_type(src, c));
            continue;
        }

        char *buf = (char *)malloc((size_t)nsel * (size_t)elem);
        if (!buf) {
            vector_block_destroy(dst);
            return NULL;
        }

        if (src->columns[c] != NULL) {
            const char *src_col = (const char *)src->columns[c];
            for (int j = 0; j < nsel; j++) {
                memcpy(buf + (size_t)j * (size_t)elem,
                       src_col + (size_t)sel[j] * (size_t)elem,
                       (size_t)elem);
            }
        }
        /* 源列为 NULL 时跳过 memcpy，但仍交出缓冲所有权（保持列槽位一致） */

        vector_block_set_column(dst, c, buf, elem);
        vector_block_set_column_type(dst, c, vector_block_get_column_type(src, c));
    }

    /* null 标记重映射：新块第 j 行的 null = 源块 sel[j] 行的 null */
    for (int j = 0; j < nsel; j++) {
        vector_block_set_null(dst, j, vector_block_is_null((VectorBlock *)src, sel[j]));
    }

    vector_block_set_num_rows(dst, nsel);
    return dst;
}
