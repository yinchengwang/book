/**
 * @file blob_engine.h
 * @brief Blob 存储引擎接口（C3-1）
 */
#ifndef DB_BLOB_ENGINE_H
#define DB_BLOB_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOB_MAX_CHUNK_SIZE (4 * 1024 * 1024)  /* 4MB */
#define BLOB_SHA256_SIZE    32

typedef struct blob_engine_s blob_engine_t;

blob_engine_t *blob_engine_create(const char *data_dir);
blob_engine_t *blob_engine_open(const char *data_dir);
void blob_engine_close(blob_engine_t *engine);

/* Blob 操作 */
int blob_put(blob_engine_t *engine,
             const void *data, size_t len,
             uint8_t out_blob_id[BLOB_SHA256_SIZE]);

int blob_get(blob_engine_t *engine,
             const uint8_t blob_id[BLOB_SHA256_SIZE],
             void *out_buf, size_t buf_len, size_t *out_read);

int blob_delete(blob_engine_t *engine,
                const uint8_t blob_id[BLOB_SHA256_SIZE]);

int blob_stat(blob_engine_t *engine,
              const uint8_t blob_id[BLOB_SHA256_SIZE],
              size_t *out_len);

int blob_range_get(blob_engine_t *engine,
                   const uint8_t blob_id[BLOB_SHA256_SIZE],
                   size_t offset, size_t len,
                   void *out_buf, size_t buf_len, size_t *out_read);

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_ENGINE_H */