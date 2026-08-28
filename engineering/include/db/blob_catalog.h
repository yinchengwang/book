#ifndef DB_BLOB_CATALOG_H
#define DB_BLOB_CATALOG_H

#include "db/blob_engine.h"
#include "db/kv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct blob_catalog_s blob_catalog_t;

blob_catalog_t *blob_catalog_create(kv_t *kv, const char *name_space);
void blob_catalog_destroy(blob_catalog_t *cat);

/* 索引：blob_id → 元数据 */
int blob_catalog_put(blob_catalog_t *cat,
                    const uint8_t blob_id[BLOB_SHA256_SIZE],
                    size_t size, const char *content_type);

typedef struct blob_metadata_s {
    size_t size;
    char content_type[64];
} blob_metadata_t;

int blob_catalog_get(blob_catalog_t *cat,
                    const uint8_t blob_id[BLOB_SHA256_SIZE],
                    blob_metadata_t *out);

#ifdef __cplusplus
}
#endif

#endif
