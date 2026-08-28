#include "db/toast.h"
#include "db/storage/blob/blob_engine.h"
#include "db/core/log.h"

#include <string.h>

int toast_decide(const void *data, size_t len,
                void *blob_engine,
                int *is_external,
                uint8_t out_blob_id[TOAST_BLOB_ID_SIZE]) {
    if (!is_external) return -1;
    if (len <= TOAST_THRESHOLD || !blob_engine) {
        *is_external = false;
        return 0;
    }
    /* 外存：调 blob_engine_put */
    if (blob_engine_put((blob_engine_t *)blob_engine, data, len, out_blob_id) != 0) {
        *is_external = false;
        return -1;
    }
    *is_external = true;
    return 0;
}

int toast_fetch(void *blob_engine,
               const uint8_t blob_id[TOAST_BLOB_ID_SIZE],
               void **out_data, size_t *out_len) {
    if (!blob_engine || !blob_id || !out_data || !out_len) return -1;
    return blob_engine_get((blob_engine_t *)blob_engine, blob_id,
                          *out_data, 0, out_len);
}
