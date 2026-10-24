#include "db/multimodal_object.h"
#include "db/blob_engine.h"
#include "db/core/log.h"
#include <string.h>

/* C3-4 T6: mm_multimodal_object 关联 Blob（仅记录 blob_id，不复制数据） */
int mm_multimodal_set_blob(mm_multimodal_object_t *obj,
                           const uint8_t blob_id[BLOB_SHA256_SIZE]) {
    if (!obj || !blob_id) return -1;
    memcpy(obj->blob_id, blob_id, BLOB_SHA256_SIZE);
    obj->has_blob = true;
    LOG_INFO("mm_multimodal_set_blob: 绑定 blob_id 前 8 字节 = %02x%02x%02x%02x...",
             blob_id[0], blob_id[1], blob_id[2], blob_id[3]);
    return 0;
}

/* C3-4 T8: 集合 metadata filter（占位） */
bool mm_multimodal_metadata_match(const mm_multimodal_object_t *obj,
                                  const char *filter_json) {
    if (!obj || !filter_json) return true;
    /* 简化：若 filter 包含 obj->metadata 关键词则匹配 */
    if (obj->metadata && obj->metadata_len > 0) {
        return memmem(obj->metadata, obj->metadata_len,
                      filter_json, strlen(filter_json)) != NULL;
    }
    return false;
}