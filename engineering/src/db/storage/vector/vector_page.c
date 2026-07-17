/**
 * @file vector_page.c
 * @brief 向量页管理实现
 *
 * 负责向量数据的页面级存储和管理。
 */
#include <db/storage/vector/vector_persist.h>
#include <db/page.h>
#include <db/log.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ============================================================
 * 校验和计算
 * ============================================================ */

/**
 * @brief 计算 CRC32 校验和
 * @param data 数据指针
 * @param size 数据大小
 * @return 校验和
 */
static uint32_t vector_page_crc32(const void *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t *p = (const uint8_t *)data;

    for (size_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }

    return ~crc;
}

/* ============================================================
 * 向量页 API 实现
 * ============================================================ */

vector_persist_page_t *vector_persist_page_create(uint32_t page_id, uint32_t page_size) {
    vector_persist_page_t *page = (vector_persist_page_t *)malloc(sizeof(vector_persist_page_t));
    if (!page) {
        LOG_ERROR("分配向量页失败");
        return NULL;
    }

    memset(page, 0, sizeof(vector_persist_page_t));
    page->page_size = page_size;
    page->is_owner = true;

    /* 分配数据区 */
    page->data = (uint8_t *)malloc(page_size - sizeof(vector_persist_page_header_t));
    if (!page->data) {
        LOG_ERROR("分配向量页数据区失败");
        free(page);
        return NULL;
    }

    /* 初始化头部 */
    page->header.page_id = page_id;
    page->header.num_vectors = 0;
    page->header.free_space = (uint16_t)(page_size - sizeof(vector_persist_page_header_t));
    page->header.checksum = 0;

    return page;
}

vector_persist_page_t *vector_persist_page_from_data(void *data, size_t size) {
    vector_persist_page_t *page = (vector_persist_page_t *)malloc(sizeof(vector_persist_page_t));
    if (!page) {
        LOG_ERROR("分配向量页失败");
        return NULL;
    }


    memset(page, 0, sizeof(vector_persist_page_t));
    page->is_owner = false;

    /* 从数据中复制头部 */
    memcpy(&page->header, data, sizeof(vector_persist_page_header_t));
    page->page_size = (uint32_t)size;

    /* 数据区指向传入数据的后面部分 */
    page->data = (uint8_t *)data + sizeof(vector_persist_page_header_t);

    return page;
}

void vector_persist_page_free(vector_persist_page_t *page) {
    if (!page) return;

    if (page->is_owner && page->data) {
        free(page->data);
    }
    free(page);
}

vector_persist_page_header_t *vector_persist_page_header(vector_persist_page_t *page) {
    return &page->header;
}

void *vector_persist_page_data(vector_persist_page_t *page) {
    return page->data;
}

uint16_t vector_persist_page_free_space(const vector_persist_page_t *page) {
    return page->header.free_space;
}

/**
 * @brief 无效偏移值（用于表示追加失败）
 */
#define VECTOR_PAGE_INVALID_OFFSET UINT16_MAX

uint16_t vector_persist_page_append(vector_persist_page_t *page, const float *vector, int32_t dims) {
    uint16_t vector_size = (uint16_t)(dims * sizeof(float));
    uint16_t free = vector_persist_page_free_space(page);

    /* 检查是否有足够空间 (需要 4 字节的偏移记录) */
    if (free < vector_size + sizeof(uint16_t)) {
        LOG_DEBUG("向量页空间不足，需要 %u 字节，可用 %u 字节",
                  vector_size + (uint16_t)sizeof(uint16_t), free);
        return VECTOR_PAGE_INVALID_OFFSET;
    }

    /* 计算数据区起始偏移 */
    uint16_t data_offset = (uint16_t)(page->page_size - sizeof(vector_persist_page_header_t) - free);

    /* 写入向量数据 */
    memcpy(page->data + data_offset, vector, vector_size);

    /* 更新头部 */
    uint16_t new_free = free - vector_size;
    page->header.free_space = new_free;
    page->header.num_vectors++;

    /* 返回数据偏移（相对于数据区起始） */
    return data_offset;
}

void vector_persist_page_read(const vector_persist_page_t *page, uint16_t offset, int32_t dims, float *out_vector) {
    uint16_t vector_size = (uint16_t)(dims * sizeof(float));

    if (offset + vector_size <= page->page_size - sizeof(vector_persist_page_header_t)) {
        memcpy(out_vector, page->data + offset, vector_size);
    }
}

bool vector_persist_page_verify_checksum(const vector_persist_page_t *page) {
    uint32_t stored = page->header.checksum;

    /* 使用临时副本计算校验和，避免修改只读对象 */
    vector_persist_page_header_t temp_header = page->header;
    temp_header.checksum = 0;

    uint32_t computed = vector_page_crc32(&temp_header, sizeof(vector_persist_page_header_t));
    computed ^= vector_page_crc32(page->data, page->page_size - sizeof(vector_persist_page_header_t));

    return stored == computed;
}

void vector_persist_page_set_checksum(vector_persist_page_t *page) {
    /* 使用临时副本计算校验和 */
    vector_persist_page_header_t temp_header = page->header;
    temp_header.checksum = 0;

    uint32_t crc = vector_page_crc32(&temp_header, sizeof(vector_persist_page_header_t));
    crc ^= vector_page_crc32(page->data, page->page_size - sizeof(vector_persist_page_header_t));
    page->header.checksum = crc;
}
