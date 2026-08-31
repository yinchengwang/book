/**
 * @file page.c
 * @brief 页面管理实现
 */

#include "db/page.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * CRC32 校验和实现
 * ============================================================ */

/** CRC32 查找表 */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

/** 初始化 CRC32 查找表 */
static void crc32_init(void) {
    if (crc32_table_initialized) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc32_table[i] = crc;
    }
    crc32_table_initialized = 1;
}

/** 计算 CRC32 校验和 */
static uint32_t calc_crc32(const void *data, size_t len) {
    crc32_init();
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

page_t *page_create(uint32_t page_id, page_type_t type) {
    page_t *page = (page_t *)calloc(1, sizeof(page_t));
    if (!page) return NULL;

    page->header.magic = PAGE_MAGIC;
    page->header.page_id = page_id;
    page->header.page_type = (uint8_t)type;
    page->header.free_space_offset = PAGE_HEADER_SIZE;
    page_set_checksum(page);

    return page;
}

void page_free(page_t *page) {
    if (page) {
        free(page);
    }
}

size_t page_get_free_space(const page_t *page) {
    if (!page) return 0;
    return PAGE_DATA_SIZE - page->header.free_space_offset;
}

size_t page_get_used_space(const page_t *page) {
    if (!page) return 0;
    return page->header.free_space_offset - PAGE_HEADER_SIZE;
}

uint16_t page_alloc_space(page_t *page, size_t size) {
    if (!page) return (uint16_t)-1;
    if (size == 0) return page->header.free_space_offset;

    size_t free = page_get_free_space(page);
    if (free < size) return (uint16_t)-1;

    uint16_t offset = page->header.free_space_offset;
    page->header.free_space_offset += (uint16_t)size;
    return offset;
}

void page_set_checksum(page_t *page) {
    if (!page) return;
    page->header.checksum = page_compute_checksum(page);
}

bool page_verify_checksum(const page_t *page) {
    if (!page) return false;
    return page->header.checksum == page_compute_checksum(page);
}

uint32_t page_compute_checksum(const page_t *page) {
    if (!page) return 0xFFFFFFFF;
    page_t temp = *page;
    temp.header.checksum = 0;
    return calc_crc32(&temp, sizeof(page_t));
}

int page_verify(const page_t *page) {
    if (!page) return -1;
    if (page->header.magic != PAGE_MAGIC) return -2;
    if (page->header.checksum != page_compute_checksum(page)) return -3;
    return 0;
}

const char *page_type_name(page_type_t type) {
    static const char *names[] = {
        "FREE", "DATA", "INDEX", "OVERFLOW", "META"
    };
    if (type >= 0 && type <= PAGE_META) {
        return names[type];
    }
    return "UNKNOWN";
}
