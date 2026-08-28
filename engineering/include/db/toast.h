#ifndef DB_TOAST_H
#define DB_TOAST_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOAST_THRESHOLD 2048  /* > 2KB 触发外存 */
#define TOAST_BLOB_ID_SIZE 32  /* SHA-256 */

/* C7.2:TOAST 大元组外存决策
 * 若元组 > THRESHOLD，返回 out_blob_id 并设置 *is_external = true
 * 否则 is_external = false，调用方用 inline 路径
 */
int toast_decide(const void *data, size_t len,
                void *blob_engine, /* blob_engine_t* 或 NULL */
                int *is_external,
                uint8_t out_blob_id[TOAST_BLOB_ID_SIZE]);

/* 取回 TOAST 数据 */
int toast_fetch(void *blob_engine,
               const uint8_t blob_id[TOAST_BLOB_ID_SIZE],
               void **out_data, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif
