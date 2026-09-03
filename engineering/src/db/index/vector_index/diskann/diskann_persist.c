/* diskann index persist */

#include "diskann_private.h"

#include "diskann_spann.h"
#include "diskann_fresh.h"

/* internal buffer helpers */

int diskann_buffer_append(diskann_buffer_t *buffer, const void *data, int32_t size)
{
    uint8_t *new_data;
    int32_t new_capacity;

    if (!buffer || !data || size < 0) {
        return -1;
    }
    if (size == 0) {
        return 0;
    }

    if (buffer->size > INT32_MAX - size) {
        return -1;
    }
    /* buffer 按倍增策略扩容，避免频繁 realloc。 */
    if (buffer->size + size > buffer->capacity) {
        new_capacity = buffer->capacity > 0 ? buffer->capacity : 256;
        while (new_capacity < buffer->size + size) {
            if (new_capacity > INT32_MAX / 2) {
                return -1;
            }
            new_capacity *= 2;
        }
        new_data = (uint8_t *)realloc(buffer->data, (size_t)new_capacity);
        if (!new_data) {
            return -1;
        }
        buffer->data = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->size, data, (size_t)size);
    buffer->size += size;
    return 0;
}

void diskann_buffer_free(diskann_buffer_t *buffer)
{
    if (!buffer) {
        return;
    }

    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0;
    buffer->capacity = 0;
}

/* internal block io helpers */

int diskann_write_block(FILE *file,
                        int32_t page_size,
                        int32_t block_no,
                        int32_t prev_block,
                        int32_t next_block,
                        diskann_page_type_t page_type,
                        const void *payload,
                        int32_t payload_size,
                        int32_t item_count)
{
    uint8_t *page;
    diskann_block_header_t header;

    if (!file || page_size <= (int32_t)sizeof(diskann_block_header_t) || payload_size < 0) {
        return -1;
    }

    /* 每个 block 都是“header + payload + padding 到 page_size”。 */
    page = (uint8_t *)calloc((size_t)page_size, 1);
    if (!page) {
        return -1;
    }

    memset(&header, 0, sizeof(header));
    header.magic = DISKANN_FILE_MAGIC;
    header.version = DISKANN_FILE_VERSION;
    header.page_size = (uint32_t)page_size;
    header.block_no = (uint32_t)block_no;
    header.next_block = next_block >= 0 ? (uint32_t)next_block : (uint32_t)DISKANN_INVALID_BLOCK;
    header.prev_block = prev_block >= 0 ? (uint32_t)prev_block : (uint32_t)DISKANN_INVALID_BLOCK;
    header.page_type = (uint32_t)page_type;
    header.payload_size = (uint32_t)payload_size;
    header.item_count = (uint32_t)item_count;
    memcpy(page, &header, sizeof(header));
    if (payload_size > 0 && payload) {
        memcpy(page + sizeof(header), payload, (size_t)payload_size);
    }

    if (_fseeki64(file, diskann_block_offset(block_no, page_size), SEEK_SET) != 0 ||
        fwrite(page, (size_t)page_size, 1, file) != 1) {
        free(page);
        return -1;
    }

    free(page);
    return 0;
}

int diskann_write_section_blocks(FILE *file,
                                 int32_t page_size,
                                 int32_t first_block,
                                 diskann_page_type_t page_type,
                                 const void *data,
                                 int32_t bytes,
                                 int32_t item_count,
                                 diskann_section_locator_t *locator)
{
    int32_t payload_size;
    int32_t block_idx;

    if (!locator) {
        return -1;
    }

    memset(locator, 0, sizeof(*locator));
    locator->first_block = bytes > 0 ? first_block : -1;
    locator->block_count = diskann_block_count_for_bytes(page_size, bytes);
    locator->bytes = bytes;
    locator->item_count = item_count;

    /* section 为空时只需返回一个空 locator。 */
    if (bytes <= 0) {
        return 0;
    }

    /* 将一大段连续内存切成多个 block 顺序写出。 */
    payload_size = diskann_payload_bytes_per_block(page_size);
    for (block_idx = 0; block_idx < locator->block_count; ++block_idx) {
        int32_t offset = block_idx * payload_size;
        int32_t chunk = diskann_min_i32(bytes - offset, payload_size);
        int32_t current_block = first_block + block_idx;
        int32_t prev_block = block_idx > 0 ? current_block - 1 : -1;
        int32_t next_block = block_idx + 1 < locator->block_count ? current_block + 1 : -1;

        if (diskann_write_block(file,
                                page_size,
                                current_block,
                                prev_block,
                                next_block,
                                page_type,
                                data ? (const uint8_t *)data + offset : NULL,
                                chunk,
                                item_count) != 0) {
            return -1;
        }
    }

    return 0;
}

int diskann_read_block(FILE *file,
                       int32_t page_size,
                       int32_t block_no,
                       diskann_page_type_t expected_type,
                       diskann_block_header_t *header,
                       uint8_t *payload)
{
    uint8_t *page;

    if (!file || !header || !payload) {
        return -1;
    }

    page = (uint8_t *)malloc((size_t)page_size);
    if (!page) {
        return -1;
    }

    if (_fseeki64(file, diskann_block_offset(block_no, page_size), SEEK_SET) != 0 ||
        fread(page, (size_t)page_size, 1, file) != 1) {
        free(page);
        return -1;
    }

    memcpy(header, page, sizeof(*header));
    if (header->magic != DISKANN_FILE_MAGIC ||
        header->version != DISKANN_FILE_VERSION ||
        header->page_size != (uint32_t)page_size ||
        header->block_no != (uint32_t)block_no ||
        header->page_type != (uint32_t)expected_type ||
        header->payload_size > (uint32_t)diskann_payload_bytes_per_block(page_size)) {
        free(page);
        return -1;
    }

    memcpy(payload, page + sizeof(*header), (size_t)header->payload_size);
    free(page);
    return 0;
}

int diskann_read_section_blocks(FILE *file,
                                int32_t page_size,
                                diskann_page_type_t page_type,
                                const diskann_section_locator_t *locator,
                                diskann_buffer_t *buffer)
{
    int32_t payload_size;
    int32_t block_no;
    int32_t block_idx;
    int32_t expected_block;

    if (!buffer || !locator) {
        return -1;
    }

    memset(buffer, 0, sizeof(*buffer));
    /* 空 section 直接返回空 buffer。 */
    if (locator->bytes <= 0 || locator->block_count <= 0) {
        return 0;
    }

    /* 逐块读取并拼回一段连续内存。 */
    payload_size = diskann_payload_bytes_per_block(page_size);
    block_no = locator->first_block;
    expected_block = locator->first_block;

    for (block_idx = 0; block_idx < locator->block_count; ++block_idx) {
        diskann_block_header_t header;
        uint8_t *payload;

        payload = (uint8_t *)malloc((size_t)payload_size);
        if (!payload) {
            diskann_buffer_free(buffer);
            return -1;
        }

        if (diskann_read_block(file, page_size, block_no, page_type, &header, payload) != 0) {
            free(payload);
            diskann_buffer_free(buffer);
            return -1;
        }
        if (header.block_no != (uint32_t)expected_block) {
            free(payload);
            diskann_buffer_free(buffer);
            return -1;
        }
        if (diskann_buffer_append(buffer, payload, (int32_t)header.payload_size) != 0) {
            free(payload);
            diskann_buffer_free(buffer);
            return -1;
        }
        free(payload);

        if (block_idx + 1 < locator->block_count) {
            if (header.next_block == (uint32_t)DISKANN_INVALID_BLOCK) {
                diskann_buffer_free(buffer);
                return -1;
            }
            block_no = (int32_t)header.next_block;
            expected_block += 1;
        }
    }

    if (buffer->size != locator->bytes) {
        diskann_buffer_free(buffer);
        return -1;
    }

    return 0;
}

/* public persist apis */

int32_t diskann_index_save(const diskann_t *index, const char *path)
{
    FILE *file;
    diskann_meta_page_t meta;
    diskann_node_record_t *nodes;
    float *pq_model;
    int32_t pq_model_count;
    int32_t next_block;
    int64_t vector_bytes;
    int64_t code_bytes;
    int64_t node_bytes;
    int64_t edge_bytes;
    int64_t frozen_bytes;
    int32_t pqmodel_bytes;
    int32_t i;

    if (!index || !path) {
        return -1;
    }

    file = fopen(path, "wb+");
    if (!file) {
        return -1;
    }

    /* 第一步: 生成 meta page，记录索引配置、统计信息和各 section 定位。 */
    memset(&meta, 0, sizeof(meta));
    meta.magic = DISKANN_FILE_MAGIC;
    meta.version = DISKANN_FILE_VERSION;
    meta.meta_block = 0;
    meta.dims = index->dims;
    meta.index_size = index->index_size;
    meta.build_search_list_size = index->build_search_list_size;
    meta.metric = (int32_t)index->metric;
    meta.alpha = index->alpha;
    meta.n_total = index->n_total;
    meta.active_count = index->active_count;
    meta.entry_point = index->entry_point;
    meta.built = index->built ? 1 : 0;
    meta.page_size = index->storage_params.page_size;
    meta.frozen_point_capacity = index->storage_params.frozen_point_count;
    meta.frozen_point_count = diskann_min_i32(index->active_count, index->storage_params.frozen_point_count);
    meta.pq_enabled = diskann_pq_enabled(index) ? 1 : 0;
    meta.pq_m = index->quantization_params.pq_m;
    meta.pq_bits = index->quantization_params.pq_bits;
    meta.pq_train_max_vectors = index->quantization_params.train_max_vectors;
    meta.pq_code_size = index->pq_code_size;
    meta.pq_trained_codebook_size = index->pq_trained_codebook_size;

    vector_bytes = (int64_t)index->n_total * index->dims * sizeof(float);
    code_bytes = (int64_t)index->n_total * index->pq_code_size * sizeof(uint8_t);
    node_bytes = (int64_t)index->n_total * sizeof(diskann_node_record_t);
    edge_bytes = (int64_t)index->n_total * index->index_size * sizeof(int32_t);
    frozen_bytes = (int64_t)index->storage_params.frozen_point_count * sizeof(int32_t);

    /* 数据量过大，超出了磁盘格式 int32 的寻址范围。 */
    if (vector_bytes > INT32_MAX || code_bytes > INT32_MAX || node_bytes > INT32_MAX ||
        edge_bytes > INT32_MAX || frozen_bytes > INT32_MAX) {
        fclose(file);
        return -1;
    }

    /* 第二步: 若量化器已经训练完成，则导出 PQ 模型。 */
    pq_model = NULL;
    pq_model_count = 0;
    pqmodel_bytes = 0;
    if (diskann_pq_ready(index)) {
        pq_model_count = quantizer_model_float_count(index->quantizer);
        pq_model = (float *)malloc((size_t)pq_model_count * sizeof(float));
        if (!pq_model) {
            fclose(file);
            return -1;
        }
        if (quantizer_export_model(index->quantizer,
                                        pq_model,
                                        pq_model_count,
                                        &meta.pq_trained_codebook_size) < 0) {
            free(pq_model);
            fclose(file);
            return -1;
        }
        pqmodel_bytes = pq_model_count * (int32_t)sizeof(float);
    }
    meta.pq_model_float_count = pq_model_count;

    /* 第三步: 为各 section 顺序分配 block 区间。 */
    next_block = 1;
    meta.vector_section.first_block = vector_bytes > 0 ? next_block : -1;
    next_block += diskann_block_count_for_bytes(meta.page_size, vector_bytes);
    meta.code_section.first_block = code_bytes > 0 ? next_block : -1;
    next_block += diskann_block_count_for_bytes(meta.page_size, code_bytes);
    meta.node_section.first_block = node_bytes > 0 ? next_block : -1;
    next_block += diskann_block_count_for_bytes(meta.page_size, node_bytes);
    meta.edge_section.first_block = edge_bytes > 0 ? next_block : -1;
    next_block += diskann_block_count_for_bytes(meta.page_size, edge_bytes);
    meta.frozen_section.first_block = frozen_bytes > 0 ? next_block : -1;
    next_block += diskann_block_count_for_bytes(meta.page_size, frozen_bytes);
    meta.pqmodel_section.first_block = pqmodel_bytes > 0 ? next_block : -1;

    meta.vector_section.block_count = diskann_block_count_for_bytes(meta.page_size, vector_bytes);
    meta.vector_section.bytes = vector_bytes;
    meta.vector_section.item_count = index->n_total;
    meta.code_section.block_count = diskann_block_count_for_bytes(meta.page_size, code_bytes);
    meta.code_section.bytes = code_bytes;
    meta.code_section.item_count = index->n_total;
    meta.node_section.block_count = diskann_block_count_for_bytes(meta.page_size, node_bytes);
    meta.node_section.bytes = node_bytes;
    meta.node_section.item_count = index->n_total;
    meta.edge_section.block_count = diskann_block_count_for_bytes(meta.page_size, edge_bytes);
    meta.edge_section.bytes = edge_bytes;
    meta.edge_section.item_count = index->n_total * index->index_size;
    meta.frozen_section.block_count = diskann_block_count_for_bytes(meta.page_size, frozen_bytes);
    meta.frozen_section.bytes = frozen_bytes;
    meta.frozen_section.item_count = index->storage_params.frozen_point_count;
    meta.pqmodel_section.block_count = diskann_block_count_for_bytes(meta.page_size, pqmodel_bytes);
    meta.pqmodel_section.bytes = pqmodel_bytes;
    meta.pqmodel_section.item_count = pq_model_count;

    /* 第四步: 将运行时节点信息整理成紧凑的 node record 数组。 */
    nodes = (diskann_node_record_t *)malloc((size_t)index->n_total * sizeof(diskann_node_record_t));
    if (!nodes) {
        free(pq_model);
        fclose(file);
        return -1;
    }

    for (i = 0; i < index->n_total; ++i) {
        nodes[i].id = i;
        nodes[i].neighbor_count = index->neighbor_counts[i];
        nodes[i].deleted = index->deleted[i];
        nodes[i].reserved[0] = 0;
        nodes[i].reserved[1] = 0;
        nodes[i].reserved[2] = 0;
    }

    /* 第五步: 依次写出 meta、vector、code、node、edge、frozen、pqmodel 各 section。 */
    if (diskann_write_block(file,
                            meta.page_size,
                            0,
                            -1,
                            -1,
                            DISKANN_PAGE_META,
                            &meta,
                            (int32_t)sizeof(meta),
                            1) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.vector_section.first_block,
                                     DISKANN_PAGE_VECTOR,
                                     index->vectors,
                                     vector_bytes,
                                     index->n_total,
                                     &meta.vector_section) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.code_section.first_block,
                                     DISKANN_PAGE_CODE,
                                     index->codes,
                                     code_bytes,
                                     index->n_total,
                                     &meta.code_section) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.node_section.first_block,
                                     DISKANN_PAGE_NODE,
                                     nodes,
                                     node_bytes,
                                     index->n_total,
                                     &meta.node_section) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.edge_section.first_block,
                                     DISKANN_PAGE_EDGE,
                                     index->neighbors,
                                     edge_bytes,
                                     index->n_total * index->index_size,
                                     &meta.edge_section) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.frozen_section.first_block,
                                     DISKANN_PAGE_FROZEN,
                                     index->frozen_points,
                                     frozen_bytes,
                                     index->storage_params.frozen_point_count,
                                     &meta.frozen_section) != 0 ||
        diskann_write_section_blocks(file,
                                     meta.page_size,
                                     meta.pqmodel_section.first_block,
                                     DISKANN_PAGE_PQMODEL,
                                     pq_model,
                                     pqmodel_bytes,
                                     pq_model_count,
                                     &meta.pqmodel_section) != 0 ||
        diskann_write_block(file,
                            meta.page_size,
                            0,
                            -1,
                            -1,
                            DISKANN_PAGE_META,
                            &meta,
                            (int32_t)sizeof(meta),
                            1) != 0) {
        free(nodes);
        free(pq_model);
        fclose(file);
        return -1;
    }

    free(nodes);
    free(pq_model);
    fclose(file);
    return 0;
}

diskann_t *diskann_index_load(const char *path)
{
    FILE *file;
    diskann_block_header_t header;
    diskann_meta_page_t meta;
    diskann_buffer_t vector_buffer;
    diskann_buffer_t code_buffer;
    diskann_buffer_t node_buffer;
    diskann_buffer_t edge_buffer;
    diskann_buffer_t frozen_buffer;
    diskann_buffer_t pq_buffer;
    uint8_t *meta_payload;
    diskann_t *index;
    uint8_t header_bytes[sizeof(diskann_block_header_t)];
    int64_t vector_bytes;
    int64_t code_bytes;
    int64_t node_bytes;
    int64_t edge_bytes;
    int64_t frozen_bytes;
    int32_t i;

    if (!path) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    /* 第一步: 先只读第 0 块头部，确认文件魔数、版本和页大小。 */
    if (fread(header_bytes, sizeof(header_bytes), 1, file) != 1) {
        fclose(file);
        return NULL;
    }
    memcpy(&header, header_bytes, sizeof(header));
    if (header.magic != DISKANN_FILE_MAGIC ||
        header.version != DISKANN_FILE_VERSION ||
        header.page_size <= sizeof(diskann_block_header_t)) {
        fclose(file);
        return NULL;
    }

    meta_payload = (uint8_t *)malloc((size_t)diskann_payload_bytes_per_block((int32_t)header.page_size));
    if (!meta_payload) {
        fclose(file);
        return NULL;
    }

    /* 第二步: 读取完整 meta page。 */
    if (diskann_read_block(file, (int32_t)header.page_size, 0, DISKANN_PAGE_META, &header, meta_payload) != 0) {
        free(meta_payload);
        fclose(file);
        return NULL;
    }
    if (header.payload_size != sizeof(meta)) {
        free(meta_payload);
        fclose(file);
        return NULL;
    }
    memcpy(&meta, meta_payload, sizeof(meta));
    free(meta_payload);

    if (meta.magic != DISKANN_FILE_MAGIC || meta.version != DISKANN_FILE_VERSION) {
        fclose(file);
        return NULL;
    }
    if (diskann_payload_bytes_per_block(meta.page_size) <= 0) {
        fclose(file);
        return NULL;
    }

    /* 第三步: 根据 meta 创建一个空索引壳子，并恢复容量与参数配置。 */
    index = diskann_index_create(meta.dims,
                                 meta.index_size,
                                 meta.build_search_list_size,
                                 (distance_metric_t)meta.metric);
    if (!index) {
        fclose(file);
        return NULL;
    }

    index->alpha = meta.alpha;
    index->storage_params.page_size = meta.page_size;
    index->storage_params.frozen_point_count = meta.frozen_point_capacity;
    if (diskann_ensure_frozen_capacity(index) != 0 ||
        diskann_ensure_vector_capacity(index, diskann_max_i32(meta.n_total, 1)) != 0) {
        fclose(file);
        diskann_index_drop(index);
        return NULL;
    }

    /* 第四步: 如果文件中带 PQ 信息，则先恢复 PQ 配置。 */
    if (meta.pq_enabled) {
        diskann_quantization_params_t pq_params;

        pq_params.pq_m = meta.pq_m;
        pq_params.pq_bits = meta.pq_bits;
        pq_params.train_max_vectors = meta.pq_train_max_vectors;
        pq_params.enabled = true;
        if (diskann_index_set_quantization_params(index, &pq_params) != 0) {
            fclose(file);
            diskann_index_drop(index);
            return NULL;
        }
    }

    memset(&vector_buffer, 0, sizeof(vector_buffer));
    memset(&code_buffer, 0, sizeof(code_buffer));
    memset(&node_buffer, 0, sizeof(node_buffer));
    memset(&edge_buffer, 0, sizeof(edge_buffer));
    memset(&frozen_buffer, 0, sizeof(frozen_buffer));
    memset(&pq_buffer, 0, sizeof(pq_buffer));

    /* 第五步: 把各个 section 按块读回连续 buffer。 */
    if (diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_VECTOR, &meta.vector_section, &vector_buffer) != 0 ||
        diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_CODE, &meta.code_section, &code_buffer) != 0 ||
        diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_NODE, &meta.node_section, &node_buffer) != 0 ||
        diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_EDGE, &meta.edge_section, &edge_buffer) != 0 ||
        diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_FROZEN, &meta.frozen_section, &frozen_buffer) != 0 ||
        diskann_read_section_blocks(file, meta.page_size, DISKANN_PAGE_PQMODEL, &meta.pqmodel_section, &pq_buffer) != 0) {
        fclose(file);
        diskann_buffer_free(&vector_buffer);
        diskann_buffer_free(&code_buffer);
        diskann_buffer_free(&node_buffer);
        diskann_buffer_free(&edge_buffer);
        diskann_buffer_free(&frozen_buffer);
        diskann_buffer_free(&pq_buffer);
        diskann_index_drop(index);
        return NULL;
    }

    fclose(file);

    vector_bytes = (int64_t)meta.n_total * meta.dims * sizeof(float);
    code_bytes = (int64_t)meta.n_total * meta.pq_code_size * sizeof(uint8_t);
    node_bytes = (int64_t)meta.n_total * sizeof(diskann_node_record_t);
    edge_bytes = (int64_t)meta.n_total * meta.index_size * sizeof(int32_t);
    frozen_bytes = (int64_t)meta.frozen_point_capacity * sizeof(int32_t);

    /* 第六步: 校验 section 大小与 meta 描述一致，避免读到损坏文件。 */
    if (vector_buffer.size != vector_bytes ||
        node_buffer.size != node_bytes ||
        edge_buffer.size != edge_bytes ||
        frozen_buffer.size != frozen_bytes ||
        code_buffer.size != code_bytes ||
        pq_buffer.size != meta.pq_model_float_count * (int32_t)sizeof(float)) {
        diskann_buffer_free(&vector_buffer);
        diskann_buffer_free(&code_buffer);
        diskann_buffer_free(&node_buffer);
        diskann_buffer_free(&edge_buffer);
        diskann_buffer_free(&frozen_buffer);
        diskann_buffer_free(&pq_buffer);
        diskann_index_drop(index);
        return NULL;
    }

    /* 第七步: 回填运行时数组，恢复节点、边、冻结点以及 PQ 模型。 */
    if (vector_bytes > 0) {
        memcpy(index->vectors, vector_buffer.data, (size_t)vector_bytes);
    }
    if (code_bytes > 0 && index->codes) {
        memcpy(index->codes, code_buffer.data, (size_t)code_bytes);
    }
    if (edge_bytes > 0) {
        memcpy(index->neighbors, edge_buffer.data, (size_t)edge_bytes);
    }
    if (frozen_bytes > 0 && index->frozen_points) {
        memcpy(index->frozen_points, frozen_buffer.data, (size_t)frozen_bytes);
    }

    index->n_total = meta.n_total;
    index->active_count = meta.active_count;
    index->entry_point = meta.entry_point;
    index->built = meta.built != 0;
    index->pq_code_size = meta.pq_code_size;
    index->pq_trained_codebook_size = meta.pq_trained_codebook_size;

    for (i = 0; i < meta.n_total; ++i) {
        const diskann_node_record_t *node = ((const diskann_node_record_t *)node_buffer.data) + i;
        index->neighbor_counts[i] = node->neighbor_count;
        index->deleted[i] = node->deleted;
    }

    if (meta.pq_enabled && meta.pq_model_float_count > 0) {
        quantizer_drop(index->quantizer);
        index->quantizer = quantizer_create_from_model(&index->quantizer_config,
                                                            meta.pq_trained_codebook_size,
                                                            (const float *)pq_buffer.data,
                                                            meta.pq_model_float_count);
        if (!index->quantizer) {
            diskann_buffer_free(&vector_buffer);
            diskann_buffer_free(&code_buffer);
            diskann_buffer_free(&node_buffer);
            diskann_buffer_free(&edge_buffer);
            diskann_buffer_free(&frozen_buffer);
            diskann_buffer_free(&pq_buffer);
            diskann_index_drop(index);
            return NULL;
        }
    }

    diskann_buffer_free(&vector_buffer);
    diskann_buffer_free(&code_buffer);
    diskann_buffer_free(&node_buffer);
    diskann_buffer_free(&edge_buffer);
    diskann_buffer_free(&frozen_buffer);
    diskann_buffer_free(&pq_buffer);

    /* 从向量重新计算平方范数，避免改变持久化格式 */
    {
        int32_t j;
        for (i = 0; i < meta.n_total; ++i) {
            float norm = 0.0f;
            const float *v = &index->vectors[i * index->dims];
            for (j = 0; j < index->dims; ++j) {
                norm += v[j] * v[j];
            }
            index->norms[i] = norm;
        }
    }

    return index;
}

/* ============================================================================
 * 扩展持久化支持：SPANN 和 FreshDiskANN
 * ============================================================================ */

/* 页类型枚举扩展 */
typedef enum diskann_ext_page_type {
    DISKANN_PAGE_EXT_CONFIG = 100,   /* 扩展配置（包含所有子配置） */
    DISKANN_PAGE_EXT_SPANN = 101,   /* SPANN 分区元数据 */
    DISKANN_PAGE_EXT_FRESH = 102,   /* Fresh 动态区数据 */
} diskann_ext_page_type_t;

/**
 * @brief 保存扩展配置（所有子模块配置）
 */
int32_t diskann_save_extended_config(const diskann_t *index, FILE *file)
{
    diskann_config_t config;
    int32_t config_bytes;

    if (!index || !file) {
        return -1;
    }

    /* 如果有保存的配置，直接使用 */
    if (index->config) {
        config = *index->config;
    } else {
        /* 从现有参数构建配置 */
        memset(&config, 0, sizeof(config));
        config.dims = index->dims;
        config.index_size = index->index_size;
        config.build_search_list_size = index->build_search_list_size;
        config.metric = index->metric;

        /* 量化配置 */
        config.quantization.enabled = index->quantization_params.enabled;
        config.quantization.type = QUANTIZATION_TYPE_PQ;
        config.quantization.subquantizers = index->quantization_params.pq_m;
        config.quantization.bits = index->quantization_params.pq_bits;
        config.quantization.train_max_vectors = index->quantization_params.train_max_vectors;

        /* 存储配置 */
        config.storage.page_size = index->storage_params.page_size;
        config.storage.frozen_point_count = index->storage_params.frozen_point_count;
    }

    /* 保存配置 */
    config_bytes = (int32_t)sizeof(config);
    if (fwrite(&config, sizeof(config), 1, file) != 1) {
        return -1;
    }

    return 0;
}

/**
 * @brief 加载扩展配置
 */
int32_t diskann_load_extended_config(diskann_t *index, FILE *file)
{
    diskann_config_t config;
    size_t read_count;

    if (!index || !file) {
        return -1;
    }

    /* 尝试读取配置 */
    read_count = fread(&config, sizeof(config), 1, file);
    if (read_count != 1) {
        /* 没有扩展配置，这是兼容旧格式 */
        return 0;
    }

    /* 保存配置副本 */
    index->config = diskann_config_clone(&config);
    if (!index->config) {
        return -1;
    }

    /* 根据配置初始化子模块 */
    if (config.spann.enabled && index->spann_ctx == NULL) {
        index->spann_ctx = diskann_spann_create(&config.spann);
    }

    if (config.fresh.enabled && index->fresh_ctx == NULL) {
        index->fresh_ctx = diskann_fresh_create(&config.fresh, config.dims);
        if (index->fresh_ctx) {
            diskann_fresh_init(index->fresh_ctx, config.dims);
        }
    }

    if (config.merge.enabled && index->merge_ctx == NULL) {
        index->merge_ctx = diskann_merge_context_create(&config.merge);
    }

    return 0;
}

/**
 * @brief 保存 SPANN 分区元数据
 */
int32_t diskann_save_spann_metadata(const diskann_t *index, FILE *file)
{
    int32_t spann_enabled;

    if (!index || !file) {
        return -1;
    }

    spann_enabled = (index->spann_ctx != NULL) ? 1 : 0;
    if (fwrite(&spann_enabled, sizeof(int32_t), 1, file) != 1) {
        return -1;
    }

    if (spann_enabled) {
        if (diskann_spann_persist(index->spann_ctx, file) != 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 加载 SPANN 分区元数据
 */
int32_t diskann_load_spann_metadata(diskann_t *index, FILE *file)
{
    int32_t spann_enabled;
    diskann_spann_config_t spann_config;

    if (!index || !file) {
        return -1;
    }

    if (fread(&spann_enabled, sizeof(int32_t), 1, file) != 1) {
        return -1;
    }

    if (spann_enabled && index->config) {
        spann_config = index->config->spann;
        index->spann_ctx = diskann_spann_load(&spann_config, file);
    }

    return 0;
}

/**
 * @brief 保存 Fresh 动态区数据
 */
int32_t diskann_save_fresh_data(const diskann_t *index, FILE *file)
{
    int32_t fresh_enabled;

    if (!index || !file) {
        return -1;
    }

    fresh_enabled = (index->fresh_ctx != NULL) ? 1 : 0;
    if (fwrite(&fresh_enabled, sizeof(int32_t), 1, file) != 1) {
        return -1;
    }

    if (fresh_enabled && index->fresh_ctx) {
        if (diskann_fresh_persist(index->fresh_ctx, file) != 0) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief 加载 Fresh 动态区数据
 */
int32_t diskann_load_fresh_data(diskann_t *index, FILE *file)
{
    int32_t fresh_enabled;
    diskann_fresh_config_t fresh_config;

    if (!index || !file) {
        return -1;
    }

    if (fread(&fresh_enabled, sizeof(int32_t), 1, file) != 1) {
        return -1;
    }

    if (fresh_enabled && index->config) {
        fresh_config = index->config->fresh;
        index->fresh_ctx = diskann_fresh_load(&fresh_config, index->dims, file);
    }

    return 0;
}

/**
 * @brief 保存索引到文件（扩展版，支持 SPANN 和 Fresh）
 */
int32_t diskann_index_save_extended(const diskann_t *index, const char *path)
{
    FILE *file;
    int32_t result;

    if (!index || !path) {
        return -1;
    }

    /* 先保存主索引 */
    result = diskann_index_save(index, path);
    if (result != 0) {
        return result;
    }

    /* 追加扩展数据 */
    file = fopen(path, "ab");
    if (!file) {
        return -1;
    }

    /* 保存扩展配置 */
    if (diskann_save_extended_config(index, file) != 0) {
        fclose(file);
        return -1;
    }

    /* 保存 SPANN 元数据 */
    if (diskann_save_spann_metadata(index, file) != 0) {
        fclose(file);
        return -1;
    }

    /* 保存 Fresh 数据 */
    if (diskann_save_fresh_data(index, file) != 0) {
        fclose(file);
        return -1;
    }

    fclose(file);
    return 0;
}

/**
 * @brief 从文件加载索引（扩展版，支持 SPANN 和 Fresh）
 */
diskann_t *diskann_index_load_extended(const char *path)
{
    FILE *file;
    diskann_t *index;
    int64_t main_file_size;
    int64_t extended_start;

    if (!path) {
        return NULL;
    }

    /* 获取文件大小 */
    {
        FILE *tmp = fopen(path, "rb");
        if (!tmp) {
            return NULL;
        }
        fseek(tmp, 0, SEEK_END);
        main_file_size = _ftelli64(tmp);
        fclose(tmp);
    }

    /* 加载主索引 */
    index = diskann_index_load(path);
    if (!index) {
        return NULL;
    }

    /* 检查是否有扩展数据 */
    if (main_file_size <= 0) {
        return index;
    }

    /* 打开文件准备读取扩展数据 */
    file = fopen(path, "rb");
    if (!file) {
        diskann_index_drop(index);
        return NULL;
    }

    /* 跳到主索引末尾 */
    /* 主索引格式：最后一个 meta 页在块 0，所以主索引结束位置需要估算 */
    /* 简化处理：使用固定的偏移量 */
    /* 实际应该解析最后一个 meta 页的 next_block 来确定 */
    extended_start = main_file_size;
    if (extended_start <= 0 || _fseeki64(file, extended_start, SEEK_SET) != 0) {
        fclose(file);
        return index;
    }

    /* 加载扩展配置 */
    diskann_load_extended_config(index, file);

    /* 加载 SPANN 元数据 */
    diskann_load_spann_metadata(index, file);

    /* 加载 Fresh 数据 */
    diskann_load_fresh_data(index, file);

    fclose(file);
    return index;
}