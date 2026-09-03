/*
 * tree_page.c
 *
 * 树索引共享页面操作实现。
 */

#include "tree_page_private.h"

static uint32_t tree_page_payload_bytes(const uint8_t *page,
                                        const tree_index_page_header_t *header)
{
    uint32_t i;
    uint32_t total = 0;

    if (!page || !header) {
        return 0;
    }
    for (i = 0; i < header->slot_count; i++) {
        const tree_index_page_slot_t *slot = tree_page_slot_at(page, header, (uint16_t)i);

        if (!slot) {
            return 0;
        }
        total += slot->length;
    }
    return total;
}

uint32_t tree_page_compute_checksum(const void *page, uint32_t page_size)
{
    const uint8_t *data = (const uint8_t *)page;
    uint32_t hash = 2166136261u;
    uint32_t i;
    uint32_t checksum_offset;
    uint32_t checksum_end;

    if (!page || page_size < sizeof(tree_index_page_header_t)) {
        return 0;
    }

    checksum_offset = (uint32_t)((const uint8_t *)&((const tree_index_page_header_t *)page)->checksum - data);
    checksum_end = checksum_offset + (uint32_t)sizeof(uint32_t);

    for (i = 0; i < page_size; i++) {
        uint8_t byte = (i >= checksum_offset && i < checksum_end) ? 0u : data[i];

        hash ^= (uint32_t)byte;
        hash *= 16777619u;
    }
    return hash;
}

int tree_page_verify_checksum(const void *page, uint32_t page_size)
{
    const tree_index_page_header_t *header = (const tree_index_page_header_t *)page;

    if (!page || page_size < sizeof(tree_index_page_header_t)) {
        return -1;
    }
    return header->checksum == tree_page_compute_checksum(page, page_size) ? 0 : -1;
}

uint32_t tree_page_contiguous_free_space(const tree_index_page_header_t *header)
{
    if (!header || header->upper < header->lower) {
        return 0;
    }
    return (uint32_t)(header->upper - header->lower);
}

uint32_t tree_page_total_free_space(const uint8_t *page,
                                    const tree_index_page_header_t *header)
{
    uint32_t payload_bytes;

    if (!page || !header || header->page_size < header->lower) {
        return 0;
    }
    payload_bytes = tree_page_payload_bytes(page, header);
    if (header->page_size < header->lower + payload_bytes) {
        return 0;
    }
    return header->page_size - (uint32_t)header->lower - payload_bytes;
}

uint32_t tree_page_fragmented_free_space(const uint8_t *page,
                                         const tree_index_page_header_t *header)
{
    uint32_t total_free = tree_page_total_free_space(page, header);
    uint32_t contiguous_free = tree_page_contiguous_free_space(header);

    return total_free > contiguous_free ? total_free - contiguous_free : 0;
}

int tree_page_compact(uint8_t *page,
                      tree_index_page_header_t *header,
                      uint32_t page_size)
{
    uint8_t *scratch;
    uint32_t upper;
    uint16_t i;

    if (!page || !header || page_size != header->page_size || page_size < sizeof(*header)) {
        return -1;
    }

    scratch = (uint8_t *)calloc(page_size, 1);
    if (!scratch) {
        return -1;
    }
    memcpy(scratch, page, (size_t)header->lower);
    upper = page_size;

    for (i = 0; i < header->slot_count; i++) {
        tree_index_page_slot_t *dst_slot = (tree_index_page_slot_t *)(scratch + sizeof(tree_index_page_header_t) +
                                                                      sizeof(tree_index_page_slot_t) * i);
        const tree_index_page_slot_t *src_slot = tree_page_slot_at(page, header, i);
        const uint8_t *payload = tree_page_slot_payload(page, src_slot, page_size);

        if (!src_slot || !payload || src_slot->length > upper) {
            free(scratch);
            return -1;
        }

        upper -= src_slot->length;
        if (src_slot->length > 0) {
            memcpy(scratch + upper, payload, src_slot->length);
        }
        dst_slot->offset = (uint16_t)upper;
        dst_slot->length = src_slot->length;
    }

    memcpy(page, scratch, page_size);
    free(scratch);

    header = (tree_index_page_header_t *)page;
    header->upper = (uint16_t)upper;
    tree_page_finalize_checksum(page, header);
    return 0;
}

int tree_page_init(tree_page_builder_t *builder,
                   uint32_t page_size,
                   uint32_t page_id,
                   tree_index_page_type_t page_type,
                   uint16_t flags,
                   uint32_t prev_page_id,
                   uint32_t next_page_id,
                   uint32_t first_child_page_id,
                   uint32_t item_count)
{
    if (!builder || page_size < TREE_INDEX_MIN_PAGE_SIZE) {
        return -1;
    }

    builder->page = (uint8_t *)calloc(page_size, 1);
    if (!builder->page) {
        return -1;
    }

    builder->header = (tree_index_page_header_t *)builder->page;
    builder->header->magic = TREE_INDEX_PAGE_MAGIC;
    builder->header->version = TREE_INDEX_PAGE_VERSION;
    builder->header->header_size = (uint16_t)sizeof(tree_index_page_header_t);
    builder->header->page_size = page_size;
    builder->header->page_id = page_id;
    builder->header->prev_page_id = prev_page_id;
    builder->header->next_page_id = next_page_id;
    builder->header->first_child_page_id = first_child_page_id;
    builder->header->page_type = (uint16_t)page_type;
    builder->header->flags = flags;
    builder->header->slot_count = 0;
    builder->header->lower = (uint16_t)sizeof(tree_index_page_header_t);
    builder->header->upper = (uint16_t)page_size;
    builder->header->item_count = item_count;
    builder->header->checksum = 0;
    return 0;
}

int tree_page_add_slot(tree_page_builder_t *builder,
                       const void *payload,
                       uint16_t payload_len)
{
    tree_index_page_header_t *header;
    tree_index_page_slot_t *slot;
    uint16_t slot_bytes;

    if (!builder || !builder->page || (!payload && payload_len > 0)) {
        return -1;
    }

    header = builder->header;
    slot_bytes = (uint16_t)(sizeof(tree_index_page_slot_t) * (header->slot_count + 1u));
    if ((uint32_t)header->lower + slot_bytes > header->upper ||
        (uint32_t)header->lower + slot_bytes + payload_len > header->upper) {
        return -1;
    }

    header->upper = (uint16_t)(header->upper - payload_len);
    if (payload_len > 0) {
        memcpy(builder->page + header->upper, payload, payload_len);
    }

    slot = (tree_index_page_slot_t *)(builder->page + sizeof(tree_index_page_header_t) +
                                      sizeof(tree_index_page_slot_t) * header->slot_count);
    slot->offset = header->upper;
    slot->length = payload_len;
    header->slot_count++;
    header->lower = (uint16_t)(sizeof(tree_index_page_header_t) +
                               sizeof(tree_index_page_slot_t) * header->slot_count);
    return 0;
}

void tree_page_finish(tree_page_builder_t *builder)
{
    if (!builder || !builder->page || !builder->header) {
        return;
    }
    tree_page_finalize_checksum(builder->page, builder->header);
}

void tree_page_finalize_checksum(uint8_t *page, tree_index_page_header_t *header)
{
    if (!page || !header) {
        return;
    }
    header->checksum = 0;
    header->checksum = tree_page_compute_checksum(page, header->page_size);
}

void tree_page_destroy(tree_page_builder_t *builder)
{
    if (!builder) {
        return;
    }
    free(builder->page);
    builder->page = NULL;
    builder->header = NULL;
}

int tree_page_write(FILE *file, const tree_page_builder_t *builder)
{
    uint32_t page_size;

    if (!file || !builder || !builder->page || !builder->header) {
        return -1;
    }

    page_size = builder->header->page_size;
    tree_page_finalize_checksum(builder->page, builder->header);
    if (_fseeki64(file, (__int64)builder->header->page_id * page_size, SEEK_SET) != 0) {
        return -1;
    }
    if (fwrite(builder->page, page_size, 1, file) != 1) {
        return -1;
    }
    return 0;
}

int tree_page_read(FILE *file,
                   uint32_t page_id,
                   uint8_t *page_buffer,
                   uint32_t page_size,
                   tree_index_page_header_t *header_out)
{
    if (!file || !page_buffer || !header_out) {
        return -1;
    }

    if (_fseeki64(file, (__int64)page_id * page_size, SEEK_SET) != 0) {
        return -1;
    }
    if (fread(page_buffer, page_size, 1, file) != 1) {
        return -1;
    }

    memcpy(header_out, page_buffer, sizeof(*header_out));
    if (header_out->magic != TREE_INDEX_PAGE_MAGIC ||
        header_out->version != TREE_INDEX_PAGE_VERSION ||
        header_out->page_size != page_size ||
        header_out->page_id != page_id ||
        header_out->header_size != sizeof(tree_index_page_header_t) ||
        header_out->lower < sizeof(tree_index_page_header_t) ||
        header_out->lower > header_out->upper ||
        header_out->upper > page_size ||
        tree_page_verify_checksum(page_buffer, page_size) != 0) {
        return -1;
    }
    return 0;
}

const tree_index_page_slot_t *tree_page_slot_at(const uint8_t *page,
                                                const tree_index_page_header_t *header,
                                                uint16_t slot_index)
{
    if (!page || !header || slot_index >= header->slot_count) {
        return NULL;
    }
    return (const tree_index_page_slot_t *)(page + sizeof(tree_index_page_header_t) +
                                            sizeof(tree_index_page_slot_t) * slot_index);
}

const uint8_t *tree_page_slot_payload(const uint8_t *page,
                                      const tree_index_page_slot_t *slot,
                                      uint32_t page_size)
{
    if (!page || !slot || slot->offset > page_size || slot->offset + slot->length > page_size) {
        return NULL;
    }
    return page + slot->offset;
}
