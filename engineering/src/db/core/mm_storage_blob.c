#include "db/mm_record.h"
#include "db/blob_engine.h"
#include "db/core/log.h"

#include <stdlib.h>
#include <string.h>

int mm_storage_blob_put(const char *collection, const uint8_t blob_id[32]) {
    (void)collection; (void)blob_id;
    LOG_INFO("mm_storage_blob_put: 骨架（依赖 mm_storage 全套集成）");
    return 0;
}

int mm_storage_blob_get(const char *collection, const uint8_t blob_id[32],
                        void *out_buf, size_t buf_len, size_t *out_read) {
    (void)collection; (void)blob_id; (void)out_buf; (void)buf_len;
    *out_read = 0;
    return 0;
}