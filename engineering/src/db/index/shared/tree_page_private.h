/*
 * tree_page_private.h
 *
 * 树页面内部结构。
 */

#ifndef DB_INDEX_SHARED_TREE_PAGE_PRIVATE_H
#define DB_INDEX_SHARED_TREE_PAGE_PRIVATE_H

#include "db/index/shared/tree_page.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct tree_page_builder {
    uint8_t *page;
    tree_index_page_header_t *header;
} tree_page_builder_t;

int tree_page_init(tree_page_builder_t *builder,
                   uint32_t page_size,
                   uint32_t page_id,
                   tree_index_page_type_t page_type,
                   uint16_t flags,
                   uint32_t prev_page_id,
                   uint32_t next_page_id,
                   uint32_t first_child_page_id,
                   uint32_t item_count);

int tree_page_add_slot(tree_page_builder_t *builder,
                       const void *payload,
                       uint16_t payload_len);

void tree_page_finish(tree_page_builder_t *builder);
void tree_page_destroy(tree_page_builder_t *builder);

int tree_page_write(FILE *file, const tree_page_builder_t *builder);
int tree_page_read(FILE *file,
                   uint32_t page_id,
                   uint8_t *page_buffer,
                   uint32_t page_size,
                   tree_index_page_header_t *header_out);

void tree_page_finalize_checksum(uint8_t *page, tree_index_page_header_t *header);

const tree_index_page_slot_t *tree_page_slot_at(const uint8_t *page,
                                                const tree_index_page_header_t *header,
                                                uint16_t slot_index);

const uint8_t *tree_page_slot_payload(const uint8_t *page,
                                      const tree_index_page_slot_t *slot,
                                      uint32_t page_size);

#endif /* DB_INDEX_SHARED_TREE_PAGE_PRIVATE_H */
