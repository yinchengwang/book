#include "db/blob_catalog.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

struct blob_catalog_s {
    kv_t *kv;
    char ns[64];
};

blob_catalog_t *blob_catalog_create(kv_t *kv, const char *name_space) {
    if (!kv || !name_space) return NULL;
    blob_catalog_t *cat = calloc(1, sizeof(*cat));
    if (!cat) return NULL;
    cat->kv = kv;
    strncpy(cat->ns, name_space, sizeof(cat->ns) - 1);
    return cat;
}

void blob_catalog_destroy(blob_catalog_t *cat) { free(cat); }

/* key 格式："{ns}::blob_meta::{hex}" → "{size}|{content_type}" */
static void meta_key(blob_catalog_t *cat, const uint8_t *id, char *out, size_t out_len) {
    char hex[65];
    static const char hexc[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        hex[2*i]   = hexc[id[i] >> 4];
        hex[2*i+1] = hexc[id[i] & 0x0f];
    }
    hex[64] = '\0';
    snprintf(out, out_len, "%s::blob_meta::%s", cat->ns, hex);
}

int blob_catalog_put(blob_catalog_t *cat,
                    const uint8_t blob_id[BLOB_SHA256_SIZE],
                    size_t size, const char *content_type) {
    if (!cat || !blob_id) return -1;
    char key[256];
    meta_key(cat, blob_id, key, sizeof(key));
    char val[128];
    snprintf(val, sizeof(val), "%zu|%s", size, content_type ? content_type : "");
    kv_result_t rc = kv_put(cat->kv, key, strlen(key) + 1, val, strlen(val) + 1);
    return rc == KV_OK ? 0 : -1;
}

int blob_catalog_get(blob_catalog_t *cat,
                    const uint8_t blob_id[BLOB_SHA256_SIZE],
                    blob_metadata_t *out) {
    if (!cat || !blob_id || !out) return -1;
    char key[256];
    meta_key(cat, blob_id, key, sizeof(key));
    void *val = NULL;
    size_t val_len = 0;
    kv_result_t rc = kv_get(cat->kv, key, strlen(key) + 1, &val, &val_len);
    if (rc != KV_OK) { free(val); return -1; }
    /* 解析 "size|content_type" */
    char *p = (char *)val;
    char *bar = memchr(p, '|', val_len);
    if (!bar) { free(val); return -1; }
    *bar = '\0';
    out->size = (size_t)atoll(p);
    strncpy(out->content_type, bar + 1, sizeof(out->content_type) - 1);
    out->content_type[sizeof(out->content_type) - 1] = '\0';
    free(val);
    return 0;
}
