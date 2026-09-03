/*
 * bitmap.c
 *
 * Bitmap 位图索引实现
 *
 * 核心功能：
 * - 低基数列的高效索引
 * - 位运算组合查询 (AND/OR/NOT)
 * - 可选的 RLE 压缩
 *
 * 设计要点：
 * 1. 使用 uint32_t 数组存储位图，每 32 位为一个 word
 * 2. 每个不同的值对应一个独立的位图
 * 3. 查询时直接对位图进行位运算
 */

#include <db/index/bitmap/bitmap_index.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── 宏定义 ── */
#define BITS_PER_WORD 32
#define WORD_INDEX(bit) ((bit) / BITS_PER_WORD)
#define BIT_OFFSET(bit) ((bit) % BITS_PER_WORD)

/* ── 位图核心 API ── */

bitmap_t *bitmap_create_empty(int n_bits)
{
    if (n_bits <= 0) return NULL;

    bitmap_t *bm = (bitmap_t *)malloc(sizeof(bitmap_t));
    if (!bm) return NULL;

    bm->n_bits = n_bits;
    bm->n_words = (n_bits + BITS_PER_WORD - 1) / BITS_PER_WORD;

    bm->data = (uint32_t *)calloc(bm->n_words, sizeof(uint32_t));
    if (!bm->data) {
        free(bm);
        return NULL;
    }

    return bm;
}

void bitmap_free(bitmap_t *bm)
{
    if (!bm) return;
    free(bm->data);
    free(bm);
}

void bitmap_set_bit(bitmap_t *bm, int pos)
{
    if (!bm || pos < 0 || pos >= bm->n_bits) return;
    bm->data[WORD_INDEX(pos)] |= (1U << BIT_OFFSET(pos));
}

void bitmap_clear_bit(bitmap_t *bm, int pos)
{
    if (!bm || pos < 0 || pos >= bm->n_bits) return;
    bm->data[WORD_INDEX(pos)] &= ~(1U << BIT_OFFSET(pos));
}

int bitmap_get_bit(const bitmap_t *bm, int pos)
{
    if (!bm || pos < 0 || pos >= bm->n_bits) return 0;
    return (bm->data[WORD_INDEX(pos)] >> BIT_OFFSET(pos)) & 1U;
}

void bitmap_and_into(bitmap_t *result, const bitmap_t *a, const bitmap_t *b)
{
    if (!result || !a || !b) return;
    if (result->n_bits != a->n_bits || a->n_bits != b->n_bits) return;

    for (int i = 0; i < result->n_words; i++) {
        result->data[i] = a->data[i] & b->data[i];
    }
}

void bitmap_or_into(bitmap_t *result, const bitmap_t *a, const bitmap_t *b)
{
    if (!result || !a || !b) return;
    if (result->n_bits != a->n_bits || a->n_bits != b->n_bits) return;

    for (int i = 0; i < result->n_words; i++) {
        result->data[i] = a->data[i] | b->data[i];
    }
}

void bitmap_not_into(bitmap_t *result, const bitmap_t *bm)
{
    if (!result || !bm) return;
    if (result->n_bits != bm->n_bits) return;

    /* 创建全 1 掩码 */
    uint32_t full_mask = 0xFFFFFFFFU;
    int last_word_bits = result->n_bits % BITS_PER_WORD;
    if (last_word_bits == 0) last_word_bits = BITS_PER_WORD;

    for (int i = 0; i < result->n_words; i++) {
        if (i == result->n_words - 1) {
            /* 最后一个 word，只取有效的位 */
            uint32_t mask = (full_mask >> (BITS_PER_WORD - last_word_bits));
            result->data[i] = (~bm->data[i]) & mask;
        } else {
            result->data[i] = ~bm->data[i];
        }
    }
}

int bitmap_count_ones(const bitmap_t *bm)
{
    if (!bm) return 0;

    int count = 0;
    for (int i = 0; i < bm->n_words; i++) {
        /* Brian Kernighan 算法统计 1 的个数 */
        uint32_t w = bm->data[i];
        while (w) {
            w &= (w - 1);
            count++;
        }
    }
    return count;
}

int bitmap_get_ones(const bitmap_t *bm, int *positions)
{
    if (!bm || !positions) return 0;

    int pos = 0;
    for (int i = 0; i < bm->n_words; i++) {
        uint32_t w = bm->data[i];
        int base = i * BITS_PER_WORD;

        while (w) {
            int bit = __builtin_ctz(w);  /* 获取最低位的 1 的位置 */
            positions[pos++] = base + bit;
            w &= (w - 1);  /* 清除最低位的 1 */
        }
    }
    return pos;
}

/* ── Bitmap 索引 API ── */

bitmap_index_t *bitmap_create(int n_docs, int n_values)
{
    if (n_docs <= 0 || n_values <= 0) return NULL;

    bitmap_index_t *idx = (bitmap_index_t *)calloc(1, sizeof(bitmap_index_t));
    if (!idx) return NULL;

    idx->n_docs = n_docs;
    idx->n_values = n_values;
    idx->compression = BITMAP_COMPRESS_NONE;
    idx->is_compressed = 0;

    /* 为每个值分配一个位图 */
    idx->bitmaps = (bitmap_t **)calloc(n_values, sizeof(bitmap_t *));
    idx->value_counts = (int *)calloc(n_values, sizeof(int));

    if (!idx->bitmaps || !idx->value_counts) {
        free(idx->bitmaps);
        free(idx->value_counts);
        free(idx);
        return NULL;
    }

    for (int i = 0; i < n_values; i++) {
        idx->bitmaps[i] = bitmap_create_empty(n_docs);
        if (!idx->bitmaps[i]) {
            /* 清理已分配的 */
            for (int j = 0; j < i; j++) {
                bitmap_free(idx->bitmaps[j]);
            }
            free(idx->bitmaps);
            free(idx->value_counts);
            free(idx);
            return NULL;
        }
    }

    return idx;
}

int bitmap_set(bitmap_index_t *idx, int doc_id, int value)
{
    if (!idx) return -1;
    if (doc_id < 0 || doc_id >= idx->n_docs) return -1;
    if (value < 0 || value >= idx->n_values) return -1;

    bitmap_set_bit(idx->bitmaps[value], doc_id);
    idx->value_counts[value]++;

    return 0;
}

int bitmap_set_batch(bitmap_index_t *idx, const int *doc_ids, const int *values, int count)
{
    if (!idx || !doc_ids || !values) return -1;

    int success = 0;
    for (int i = 0; i < count; i++) {
        if (bitmap_set(idx, doc_ids[i], values[i]) == 0) {
            success++;
        }
    }
    return success;
}

int bitmap_eq(const bitmap_index_t *idx, int value, int *doc_ids, int *count)
{
    if (!idx || !count) return -1;
    if (value < 0 || value >= idx->n_values) return -1;

    /* 只获取计数时 doc_ids 可以为 NULL */
    if (doc_ids) {
        *count = bitmap_get_ones(idx->bitmaps[value], doc_ids);
    } else {
        /* 统计 1 的数量 */
        *count = bitmap_count_ones(idx->bitmaps[value]);
    }
    return 0;
}

int bitmap_count(const bitmap_index_t *idx, int value)
{
    if (!idx || value < 0 || value >= idx->n_values) return 0;
    return bitmap_count_ones(idx->bitmaps[value]);
}

int bitmap_and(const bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count)
{
    if (!idx || !doc_ids || !count) return -1;
    if (value1 < 0 || value1 >= idx->n_values) return -1;
    if (value2 < 0 || value2 >= idx->n_values) return -1;

    /* 创建临时位图存储 AND 结果 */
    bitmap_t *temp = bitmap_create_empty(idx->n_docs);
    if (!temp) return -1;

    bitmap_and_into(temp, idx->bitmaps[value1], idx->bitmaps[value2]);
    *count = bitmap_get_ones(temp, doc_ids);

    bitmap_free(temp);
    return 0;
}

int bitmap_or(const bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count)
{
    if (!idx || !doc_ids || !count) return -1;
    if (value1 < 0 || value1 >= idx->n_values) return -1;
    if (value2 < 0 || value2 >= idx->n_values) return -1;

    bitmap_t *temp = bitmap_create_empty(idx->n_docs);
    if (!temp) return -1;

    bitmap_or_into(temp, idx->bitmaps[value1], idx->bitmaps[value2]);
    *count = bitmap_get_ones(temp, doc_ids);

    bitmap_free(temp);
    return 0;
}

int bitmap_not(const bitmap_index_t *idx, int value, int *doc_ids, int *count)
{
    if (!idx || !doc_ids || !count) return -1;
    if (value < 0 || value >= idx->n_values) return -1;

    bitmap_t *temp = bitmap_create_empty(idx->n_docs);
    if (!temp) return -1;

    bitmap_not_into(temp, idx->bitmaps[value]);
    *count = bitmap_get_ones(temp, doc_ids);

    bitmap_free(temp);
    return 0;
}

int bitmap_range(const bitmap_index_t *idx, int min_val, int max_val, int *doc_ids, int *count)
{
    if (!idx || !doc_ids || !count) return -1;
    if (min_val < 0 || max_val >= idx->n_values) return -1;
    if (min_val > max_val) return -1;

    /* 创建临时位图累积结果 */
    bitmap_t *temp = bitmap_create_empty(idx->n_docs);
    if (!temp) return -1;

    for (int v = min_val; v <= max_val; v++) {
        bitmap_t *src = idx->bitmaps[v];
        for (int i = 0; i < temp->n_words; i++) {
            temp->data[i] |= src->data[i];
        }
    }

    *count = bitmap_get_ones(temp, doc_ids);
    bitmap_free(temp);

    return 0;
}

void bitmap_stats(const bitmap_index_t *idx, int *out_n_docs, int *out_n_values, int *out_total_bits)
{
    if (!idx) {
        if (out_n_docs) *out_n_docs = 0;
        if (out_n_values) *out_n_values = 0;
        if (out_total_bits) *out_total_bits = 0;
        return;
    }

    if (out_n_docs) *out_n_docs = idx->n_docs;
    if (out_n_values) *out_n_values = idx->n_values;
    if (out_total_bits) *out_total_bits = idx->n_docs * idx->n_values;
}

int bitmap_set_compression(bitmap_index_t *idx, bitmap_compression_t compress)
{
    if (!idx) return -1;

    /* 目前只支持 NONE，后续可扩展 RLE */
    if (compress != BITMAP_COMPRESS_NONE) {
        return -1;  /* 不支持的压缩类型 */
    }

    idx->compression = compress;
    return 0;
}

void bitmap_destroy(bitmap_index_t *idx)
{
    if (!idx) return;

    for (int i = 0; i < idx->n_values; i++) {
        if (idx->bitmaps[i]) {
            bitmap_free(idx->bitmaps[i]);
        }
    }

    free(idx->bitmaps);
    free(idx->value_counts);
    free(idx);
}