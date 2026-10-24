/*
 * tree_page.h
 *
 * 树索引共享页面布局。
 * 磁盘格式使用固定大小页面：
 *   - 页面头部在前
 *   - 槽目录从前往后增长
 *   - 可变长数据从后往前填充
 */

#ifndef DB_INDEX_SHARED_TREE_PAGE_H
#define DB_INDEX_SHARED_TREE_PAGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TREE_INDEX_PAGE_MAGIC        0x54524958u
#define TREE_INDEX_PAGE_VERSION      1u
#define TREE_INDEX_DEFAULT_PAGE_SIZE 4096u
#define TREE_INDEX_MIN_PAGE_SIZE     512u
#define TREE_INDEX_INVALID_PAGE_ID   0xffffffffu

typedef enum tree_index_page_type {
    TREE_INDEX_PAGE_META = 1,
    TREE_INDEX_PAGE_BTREE_NODE = 2,
    TREE_INDEX_PAGE_BPTREE_NODE = 3
} tree_index_page_type_t;

typedef enum tree_index_page_flags {
    TREE_INDEX_PAGE_FLAG_LEAF = 1u << 0,
    TREE_INDEX_PAGE_FLAG_ROOT = 1u << 1
} tree_index_page_flags_t;

typedef struct tree_index_page_slot {
    uint16_t offset;
    uint16_t length;
} tree_index_page_slot_t;

typedef struct tree_index_page_header {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t page_size;
    uint32_t page_id;
    uint32_t prev_page_id;
    uint32_t next_page_id;
    uint32_t first_child_page_id;
    uint16_t page_type;
    uint16_t flags;
    uint16_t slot_count;
    uint16_t lower;
    uint16_t upper;
    uint16_t reserved;
    uint32_t item_count;
    uint32_t checksum;
} tree_index_page_header_t;

typedef struct tree_index_persist_options {
    uint32_t page_size;
} tree_index_persist_options_t;

uint32_t tree_page_compute_checksum(const void *page, uint32_t page_size);
int tree_page_verify_checksum(const void *page, uint32_t page_size);

uint32_t tree_page_contiguous_free_space(const tree_index_page_header_t *header);
uint32_t tree_page_total_free_space(const uint8_t *page,
                                    const tree_index_page_header_t *header);
uint32_t tree_page_fragmented_free_space(const uint8_t *page,
                                         const tree_index_page_header_t *header);

int tree_page_compact(uint8_t *page,
                      tree_index_page_header_t *header,
                      uint32_t page_size);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_SHARED_TREE_PAGE_H */
