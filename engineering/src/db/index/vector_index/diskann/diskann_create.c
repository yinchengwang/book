/* diskann index create */

#include "diskann_private.h"

/* 暂时禁用 spann/fresh/merge 扩展功能
#include "diskann_merge.h"
#include "diskann_spann.h"
#include "diskann_fresh.h"
*/

/* lifecycle */

diskann_t *diskann_index_create_with_config(const diskann_config_t *config)
{
    diskann_t *index;

    if (!config) {
        return NULL;
    }

    if (diskann_config_validate(config) != 1) {
        return NULL;
    }

    /* 创建索引 */
    index = diskann_index_create(config->dims,
                                 config->index_size,
                                 config->build_search_list_size,
                                 config->metric);
    if (!index) {
        return NULL;
    }

    /* 设置 alpha（使用默认值） */
    index->alpha = 1.2f;

    /* 设置存储参数 */
    {
        diskann_storage_params_t params = {
            config->storage.page_size,
            config->storage.frozen_point_count
        };
        diskann_index_set_storage_params(index, &params);
    }

    /* 设置量化参数（如果启用） */
    if (config->quantization.enabled) {
        diskann_quantization_params_t params = {
            config->quantization.subquantizers,
            config->quantization.bits,
            config->quantization.train_max_vectors,
            true
        };
        diskann_index_set_quantization_params(index, &params);
    }

    /* 保存配置副本 */
    index->config = diskann_config_clone(config);
    if (!index->config) {
        diskann_index_drop(index);
        return NULL;
    }

    /* 暂时禁用 SPANN/FreshDiskANN 扩展功能
    // 初始化 SPANN（如果启用）
    if (config->spann.enabled) {
        index->spann_ctx = diskann_spann_create(&config->spann);
        if (!index->spann_ctx) {
            diskann_index_drop(index);
            return NULL;
        }
    }

    // 初始化 FreshDiskANN（如果启用）
    if (config->fresh.enabled) {
        index->fresh_ctx = diskann_fresh_create(&config->fresh, config->dims);
        if (!index->fresh_ctx) {
            diskann_index_drop(index);
            return NULL;
        }
        if (diskann_fresh_init(index->fresh_ctx, config->dims) != 0) {
            diskann_index_drop(index);
            return NULL;
        }
    }
    */

    return index;
}

diskann_t *diskann_index_create(int32_t dims,
                                int32_t index_size,
                                int32_t build_search_list_size,
                                distance_metric_t metric)
{
    diskann_t *index;

    if (dims <= 0 || index_size <= 0 || build_search_list_size <= 0) {
        return NULL;
    }
    if (!distance_metric_is_valid(metric)) {
        return NULL;
    }

    index = (diskann_t *)calloc(1, sizeof(diskann_t));
    if (!index) {
        return NULL;
    }

    index->dims = dims;
    index->index_size = index_size;
    index->build_search_list_size = diskann_max_i32(build_search_list_size, index_size);
    index->metric = metric;
    index->alpha = 1.2f;
    index->entry_point = -1;
    index->storage_params.page_size = 4096;
    index->storage_params.frozen_point_count = 4;
    index->quantization_params.enabled = false;
    index->quantization_params.train_max_vectors = 4096;
    quantizer_config_init(&index->quantizer_config, dims, QUANTIZATION_TYPE_PQ);
    index->quantization_params.pq_m = index->quantizer_config.pq_subquantizers;
    index->quantization_params.pq_bits = index->quantizer_config.pq_bits;

    if (diskann_ensure_vector_capacity(index, 8) != 0 || diskann_ensure_frozen_capacity(index) != 0) {
        diskann_index_drop(index);
        return NULL;
    }

    return index;
}

int32_t diskann_index_size(const diskann_t *index)
{
    return index ? index->n_total : -1;
}

/* basic accessors */

int32_t diskann_index_active_size(const diskann_t *index)
{
    return index ? index->active_count : -1;
}

int32_t diskann_index_dims(const diskann_t *index)
{
    return index ? index->dims : -1;
}

int32_t diskann_index_entry_point(const diskann_t *index)
{
    return index ? index->entry_point : -1;
}

int32_t diskann_index_frozen_point_count(const diskann_t *index)
{
    if (!index) {
        return -1;
    }
    return diskann_min_i32(index->active_count, index->storage_params.frozen_point_count);
}

bool diskann_index_is_built(const diskann_t *index)
{
    return index ? index->built : false;
}

bool diskann_index_has_pq(const diskann_t *index)
{
    return diskann_pq_enabled(index);
}

/* parameter accessors */

int32_t diskann_index_set_alpha(diskann_t *index, float alpha)
{
    if (!index || !diskann_float_is_valid(alpha) || alpha < 1.0f) {
        return -1;
    }

    index->alpha = alpha;
    return 0;
}

float diskann_index_get_alpha(const diskann_t *index)
{
    return index ? index->alpha : 0.0f;
}

int32_t diskann_index_set_quantization_params(diskann_t *index,
                                              const diskann_quantization_params_t *params)
{
    quantizer_config_t config;

    if (!index || !params) {
        return -1;
    }

    if (!params->enabled) {
        index->quantization_params = *params;
        index->pq_code_size = 0;
        index->pq_trained_codebook_size = 0;
        free(index->codes);
        index->codes = NULL;
        quantizer_drop(index->quantizer);
        index->quantizer = NULL;
        return 0;
    }

    quantizer_config_init(&config, index->dims, QUANTIZATION_TYPE_PQ);
    config.pq_subquantizers = params->pq_m;
    config.pq_bits = params->pq_bits;
    if (!quantizer_config_validate(&config) || params->train_max_vectors <= 0) {
        return -1;
    }

    index->quantization_params = *params;
    index->quantizer_config = config;
    index->pq_code_size = params->pq_m;
    index->pq_trained_codebook_size = 0;
    quantizer_drop(index->quantizer);
    index->quantizer = NULL;
    if (diskann_ensure_quantizer(index) != 0 || diskann_ensure_code_storage(index) != 0) {
        return -1;
    }
    return 0;
}

int32_t diskann_index_get_quantization_params(const diskann_t *index,
                                              diskann_quantization_params_t *params)
{
    if (!index || !params) {
        return -1;
    }
    *params = index->quantization_params;
    return 0;
}

int32_t diskann_index_set_storage_params(diskann_t *index,
                                         const diskann_storage_params_t *params)
{
    int32_t old_frozen_capacity;
    int32_t *old_frozen_points;
    int32_t i;

    if (!index ||
        !params ||
        params->page_size <= (int32_t)(sizeof(diskann_block_header_t) + sizeof(diskann_meta_page_t)) ||
        params->frozen_point_count < 0) {
        return -1;
    }

    old_frozen_capacity = index->storage_params.frozen_point_count;
    old_frozen_points = index->frozen_points;
    index->storage_params = *params;
    index->frozen_points = NULL;
    if (diskann_ensure_frozen_capacity(index) != 0) {
        index->frozen_points = old_frozen_points;
        index->storage_params.frozen_point_count = old_frozen_capacity;
        return -1;
    }

    for (i = 0; i < diskann_min_i32(old_frozen_capacity, index->storage_params.frozen_point_count); ++i) {
        index->frozen_points[i] = old_frozen_points[i];
    }
    for (; i < index->storage_params.frozen_point_count; ++i) {
        index->frozen_points[i] = -1;
    }
    free(old_frozen_points);

    return diskann_rebuild_frozen_points(index);
}

int32_t diskann_index_get_storage_params(const diskann_t *index,
                                         diskann_storage_params_t *params)
{
    if (!index || !params) {
        return -1;
    }
    *params = index->storage_params;
    return 0;
}

void diskann_index_drop(diskann_t *index)
{
    if (!index) {
        return;
    }

    free(index->vectors);
    free(index->norms);
    free(index->codes);
    free(index->deleted);
    free(index->neighbors);
    free(index->neighbor_counts);
    free(index->frozen_points);
    quantizer_drop(index->quantizer);

    /* 释放扩展上下文 */
    if (index->config) {
        diskann_config_free(index->config);
    }
    /* 暂时禁用扩展上下文释放
    if (index->merge_ctx) {
        diskann_merge_context_free(index->merge_ctx);
    }
    if (index->spann_ctx) {
        diskann_spann_free(index->spann_ctx);
    }
    if (index->fresh_ctx) {
        diskann_fresh_free(index->fresh_ctx);
    }
    */

    free(index);
}