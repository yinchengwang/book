/**
 * @file blob_engine.h (stub)
 * @brief Forwarding stub for toast.c — declares the blob_engine_* functions
 *        referenced by toast.c. The full implementation lives in
 *        db/blob_engine.h (see blob_put/blob_get for the canonical API).
 *
 * Phase 1.5: minimal stub to unblock toast.c linkage. The semantics are
 * identical to blob_put / blob_get (one-in, one-out).
 */
#ifndef DB_STORAGE_BLOB_BLOB_ENGINE_H
#define DB_STORAGE_BLOB_BLOB_ENGINE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct blob_engine_s;
typedef struct blob_engine_s blob_engine_t;

/**
 * @brief Store data into blob engine, return content-addressed blob id.
 * @return 0 success, -1 failure.
 */
int blob_engine_put(blob_engine_t *engine,
                    const void *data, size_t len,
                    uint8_t out_blob_id[32]);

/**
 * @brief Read data from blob engine by blob id.
 * @return 0 success, -1 failure.
 */
int blob_engine_get(blob_engine_t *engine,
                    const uint8_t blob_id[32],
                    void *out_buf, size_t buf_len, size_t *out_read);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_BLOB_BLOB_ENGINE_H */
