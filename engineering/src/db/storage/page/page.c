/**
 * @file page.c
 * @brief 页面管理实现
 */

#include "db/page.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 简单校验和计算：XOR 除 checksum 外的所有字节 */
static uint16_t calc_checksum(const page_t *page) {
    uint16_t sum = 0;
    const uint8_t *bytes = (const uint8_t *)page;
    size_t len = sizeof(page_t);

    /* 计算整个页面的 XOR，跳过 checksum 字段 */
    for (size_t i = 0; i < len; i++) {
        /* 跳过 checksum 字段 (偏移 6-7，共 2 字节) */
        if (i >= 6 && i < 8) continue;
        sum ^= (uint16_t)bytes[i] << ((i % 2) ? 0 : 8);
    }
    return sum;
}

page_t *page_create(uint32_t page_id, page_type_t type) {
    page_t *page = (page_t *)calloc(1, sizeof(page_t));
    if (!page) return NULL;

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
    page->header.checksum = calc_checksum(page);
}

bool page_verify_checksum(const page_t *page) {
    if (!page) return false;
    return page->header.checksum == calc_checksum(page);
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
