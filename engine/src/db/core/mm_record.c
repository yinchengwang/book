/**
 * @file mm_record.c
 * @brief mm_record_header_t 序列化契约实现
 */
#include "db/mm_record.h"

#include <string.h>

int mm_record_has_header(const void *data, size_t len) {
    if (data == NULL) return 0;
    if (len < sizeof(mm_record_header_t)) return 0;
    const mm_record_header_t *hdr = (const mm_record_header_t *)data;
    return hdr->magic == MM_RECORD_MAGIC;
}

size_t mm_record_write_header(void *buf, uint32_t model, uint32_t payload_len) {
    mm_record_header_t *hdr = (mm_record_header_t *)buf;
    hdr->magic       = MM_RECORD_MAGIC;
    hdr->version     = MM_RECORD_VERSION_V1;
    hdr->model       = model;
    hdr->payload_len = payload_len;
    return sizeof(mm_record_header_t);
}
