/**
 * @file log_label_index.h
 * @brief log_engine 标签倒排索引（C6.1）
 */
#ifndef DB_LOG_LABEL_INDEX_H
#define DB_LOG_LABEL_INDEX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct log_label_index_s log_label_index_t;

log_label_index_t *log_label_index_create(const char *data_dir);
void log_label_index_destroy(log_label_index_t *idx);

/* 添加 (label, value) → stream_id */
int log_label_index_put(log_label_index_t *idx,
                       const char *label, const char *value,
                       uint64_t stream_id);

/* 查询 label=value 的 stream_id 集合（回调） */
typedef int (*log_label_index_match_cb)(uint64_t stream_id, void *ctx);
int log_label_index_query(log_label_index_t *idx,
                          const char *labels[], const char *values[], int n,
                          log_label_index_match_cb cb, void *ctx);

/* 高基数保护 */
void log_label_index_set_threshold(log_label_index_t *idx, int threshold);
bool log_label_index_is_high_cardinality(log_label_index_t *idx,
                                         const char *label);

#ifdef __cplusplus
}
#endif

#endif