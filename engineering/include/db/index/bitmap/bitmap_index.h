/**
 * @file include/db/index/bitmap/bitmap_index.h
 * @brief Bitmap 位图索引公共头文件
 */
#ifndef DB_INDEX_BITMAP_INDEX_H
#define DB_INDEX_BITMAP_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct bitmap bitmap_t;
typedef struct bitmap_index bitmap_index_t;

/* ── 压缩类型 ── */
typedef enum {
    BITMAP_COMPRESS_NONE = 0,
    BITMAP_COMPRESS_RLE,
} bitmap_compression_t;

/* ── 位图核心结构 ── */
struct bitmap {
    uint32_t *data;             /* 位数据（每 32 位为一个 word） */
    int n_bits;                 /* 位数 */
    int n_words;                /* word 数量 */
};

/* ── Bitmap 索引结构 ── */
struct bitmap_index {
    int n_docs;                 /* 文档总数 */
    int n_values;               /* 不同值的数量 */
    bitmap_t **bitmaps;         /* 每个值的位图数组 */
    int *value_counts;          /* 每个值的文档数量 */
    bitmap_compression_t compression;
    int is_compressed;
};

/* ── 生命周期 ── */
bitmap_index_t *bitmap_create(int n_docs, int n_values);
void bitmap_destroy(bitmap_index_t *idx);

/* ── 核心操作 ── */
int bitmap_set(bitmap_index_t *idx, int doc_id, int value);
int bitmap_set_batch(bitmap_index_t *idx, const int *doc_ids, const int *values, int count);

/* ── 查询操作 ── */
int bitmap_eq(const bitmap_index_t *idx, int value, int *doc_ids, int *count);
int bitmap_and(const bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count);
int bitmap_or(const bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count);
int bitmap_not(const bitmap_index_t *idx, int value, int *doc_ids, int *count);
int bitmap_range(const bitmap_index_t *idx, int min_val, int max_val, int *doc_ids, int *count);
int bitmap_count(const bitmap_index_t *idx, int value);

/* ── 统计信息 ── */
void bitmap_stats(const bitmap_index_t *idx, int *out_n_docs, int *out_n_values, int *out_total_bits);
int bitmap_set_compression(bitmap_index_t *idx, bitmap_compression_t compress);

/* ── 位运算 API（低级） ── */
bitmap_t *bitmap_create_empty(int n_bits);
void bitmap_free(bitmap_t *bm);
void bitmap_set_bit(bitmap_t *bm, int pos);
void bitmap_clear_bit(bitmap_t *bm, int pos);
int bitmap_get_bit(const bitmap_t *bm, int pos);
void bitmap_and_into(bitmap_t *result, const bitmap_t *a, const bitmap_t *b);
void bitmap_or_into(bitmap_t *result, const bitmap_t *a, const bitmap_t *b);
void bitmap_not_into(bitmap_t *result, const bitmap_t *bm);
int bitmap_count_ones(const bitmap_t *bm);
int bitmap_get_ones(const bitmap_t *bm, int *positions);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_BITMAP_INDEX_H */