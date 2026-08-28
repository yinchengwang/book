/**
 * @file wide_row.h
 * @brief 宽表抽象（C3-5 T25）：row_key + column + ts 版本模型
 */
#ifndef DB_WIDE_ROW_H
#define DB_WIDE_ROW_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wide_row_s {
    void *kv_handle;          /* 复用 KV 引擎 */
    char *name_space;         /* 列族名 */
} wide_row_t;

/* cell 数据：column:value@timestamp */
typedef struct wide_cell_s {
    char *column;
    void *value;
    size_t value_len;
    int64_t ts;
} wide_cell_t;

wide_row_t *wide_row_open(void *kv, const char *name_space);
void wide_row_close(wide_row_t *wr);

/* 写入：行级 cell 覆盖（同 column 最新写胜出） */
int wide_row_put(wide_row_t *wr, const void *row_key, size_t key_len,
                 const char *column,
                 const void *value, size_t value_len,
                 int64_t ts);

/* 读取：按 column 取最新版本 */
int wide_row_get(wide_row_t *wr, const void *row_key, size_t key_len,
                 const char *column,
                 void **out_value, size_t *out_len);

/* 范围扫描：按 row_key 字典序 */
typedef int (*wide_row_scan_cb)(const void *row_key, size_t key_len,
                               const char *column,
                               const void *value, size_t value_len,
                               int64_t ts, void *ctx);
int wide_row_scan(wide_row_t *wr,
                  const void *start_key, size_t start_len,
                  const void *end_key, size_t end_len,
                  wide_row_scan_cb cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_WIDE_ROW_H */