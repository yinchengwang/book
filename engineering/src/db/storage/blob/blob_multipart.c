#include "db/blob_engine.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

int blob_multipart_begin(blob_engine_t *engine, const char *upload_id,
                          size_t total_size) {
    (void)engine; (void)upload_id; (void)total_size;
    LOG_INFO("blob_multipart_begin: 骨架");
    return 0;
}

int blob_multipart_upload_part(blob_engine_t *engine, const char *upload_id,
                                int part_number, const void *data, size_t len) {
    (void)engine; (void)upload_id; (void)part_number; (void)data; (void)len;
    return 0;
}

int blob_multipart_complete(blob_engine_t *engine, const char *upload_id,
                             uint8_t out_blob_id[BLOB_SHA256_SIZE]) {
    (void)engine; (void)upload_id;
    memset(out_blob_id, 0, 32);
    return 0;
}

int blob_multipart_abort(blob_engine_t *engine, const char *upload_id) {
    (void)engine; (void)upload_id;
    return 0;
}
