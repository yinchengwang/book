#include "db/wide_row.h"
#include "db/kv.h"
#include "db/core/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

wide_row_t *wide_row_open(void *kv, const char *name_space) {
    if (!kv || !name_space) return NULL;
    wide_row_t *wr = calloc(1, sizeof(*wr));
    if (!wr) return NULL;
    wr->kv_handle = kv;
    wr->name_space = strdup(name_space);
    return wr;
}

void wide_row_close(wide_row_t *wr) {
    if (!wr) return;
    free(wr->name_space);
    free(wr);
}

/* key 编码："{ns}::{row_key}::{column}" */
static void encode_key(wide_row_t *wr, const void *rk, size_t rklen,
                      const char *col, char *out, size_t outlen) {
    snprintf(out, outlen, "%s::%.*s::%s", wr->name_space,
             (int)rklen, (const char *)rk, col ? col : "");
}

int wide_row_put(wide_row_t *wr, const void *row_key, size_t key_len,
                 const char *column,
                 const void *value, size_t value_len, int64_t ts) {
    if (!wr || !row_key || !column) return -1;
    char key[512];
    encode_key(wr, row_key, key_len, column, key, sizeof(key));
    /* value = value_len + ts（紧凑二进制前缀） */
    size_t total = sizeof(int64_t) + value_len;
    uint8_t *buf = malloc(total);
    if (!buf) return -1;
    memcpy(buf, &ts, sizeof(ts));
    if (value && value_len > 0) memcpy(buf + sizeof(ts), value, value_len);
    kv_result_t rc = kv_put((kv_t *)wr->kv_handle, key, strlen(key) + 1,
                             (const char *)buf, total);
    free(buf);
    return rc == KV_OK ? 0 : -1;
}

int wide_row_get(wide_row_t *wr, const void *row_key, size_t key_len,
                 const char *column,
                 void **out_value, size_t *out_len) {
    if (!wr || !row_key || !column) return -1;
    char key[512];
    encode_key(wr, row_key, key_len, column, key, sizeof(key));
    void *val = NULL;
    size_t val_len = 0;
    kv_result_t rc = kv_get((kv_t *)wr->kv_handle, key, strlen(key) + 1, &val, &val_len);
    if (rc != KV_OK) { free(val); return -1; }
    /* 跳过前 8B（ts） */
    if (val_len < sizeof(int64_t)) { free(val); return -1; }
    *out_len = val_len - sizeof(int64_t);
    *out_value = malloc(*out_len);
    if (!*out_value) { free(val); return -1; }
    memcpy(*out_value, (uint8_t *)val + sizeof(int64_t), *out_len);
    free(val);
    return 0;
}

int wide_row_scan(wide_row_t *wr,
                  const void *start_key, size_t start_len,
                  const void *end_key, size_t end_len,
                  wide_row_scan_cb cb, void *ctx) {
    if (!wr || !cb) return -1;
    /* 简化：直接复用 KV scan 接口（kv_iter_t）
     * 实际实现：扫描 prefix="{ns}::{row_key}::" 范围内所有 key，提取 column/value
     */
    (void)start_key; (void)start_len;
    (void)end_key; (void)end_len;
    /* 占位：未实现完整 prefix 扫描 */
    return 0;
}