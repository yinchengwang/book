/*
 * diskann_config.c
 *
 * DiskANN 统一配置管理实现。
 * 提供配置创建、验证、克隆、持久化等功能。
 */

#include "diskann_private.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * 配置默认值
 * ============================================================================ */

void diskann_config_init_defaults(diskann_config_t *config, int32_t dims, distance_metric_t metric)
{
    if (!config) {
        return;
    }

    /* 基础参数 */
    config->dims = dims;
    config->index_size = 32;
    config->build_search_list_size = 48;
    config->metric = metric;

    /* 量化配置默认值 */
    config->quantization.enabled = false;
    config->quantization.type = QUANTIZATION_TYPE_PQ;
    config->quantization.subquantizers = 0; /* 自动选择 */
    config->quantization.bits = 8;
    config->quantization.train_max_vectors = 4096;

    /* Merge 配置默认值 */
    config->merge.enabled = false;
    config->merge.partition_method = DISKANN_PARTITION_AUTO;
    config->merge.partition_count = 4;
    config->merge.overlap_ratio = 0.1f;

    /* SPANN 配置默认值 */
    config->spann.enabled = false;
    config->spann.max_partitions = 128;
    config->spann.min_partition_size = 10000;
    config->spann.search_partitions = 8;

    /* Fresh 配置默认值 */
    config->fresh.enabled = false;
    config->fresh.fresh_capacity = 100000;
    config->fresh.merge_threshold = 80000;

    /* 存储配置默认值 */
    config->storage.page_size = 4096;
    config->storage.frozen_point_count = 4;
}

/* ============================================================================
 * 配置创建/销毁
 * ============================================================================ */

diskann_config_t *diskann_config_default(int32_t dims, distance_metric_t metric)
{
    diskann_config_t *config;

    if (dims <= 0 || !distance_metric_is_valid(metric)) {
        return NULL;
    }

    config = (diskann_config_t *)calloc(1, sizeof(diskann_config_t));
    if (!config) {
        return NULL;
    }

    diskann_config_init_defaults(config, dims, metric);
    return config;
}

diskann_config_t *diskann_config_create(int32_t dims,
                                        int32_t index_size,
                                        int32_t build_search_list_size,
                                        distance_metric_t metric)
{
    diskann_config_t *config;

    if (dims <= 0 || index_size <= 0 || build_search_list_size <= 0) {
        return NULL;
    }
    if (!distance_metric_is_valid(metric)) {
        return NULL;
    }

    config = diskann_config_default(dims, metric);
    if (!config) {
        return NULL;
    }

    config->index_size = index_size;
    config->build_search_list_size = build_search_list_size;

    return config;
}

void diskann_config_free(diskann_config_t *config)
{
    if (config) {
        free(config);
    }
}

/* ============================================================================
 * 配置验证
 * ============================================================================ */

int32_t diskann_quantization_config_validate(const diskann_quantization_config_t *config)
{
    if (!config) {
        return 0;
    }

    if (config->enabled) {
        /* 验证量化类型 */
        if (!quantization_type_is_valid(config->type)) {
            return 0;
        }
        /* 验证 bits */
        if (config->bits != 4 && config->bits != 6 && config->bits != 8) {
            return 0;
        }
        /* 验证训练向量数 */
        if (config->train_max_vectors <= 0) {
            return 0;
        }
    }

    return 1;
}

int32_t diskann_merge_config_validate(const diskann_merge_config_t *config)
{
    if (!config) {
        return 0;
    }

    if (config->enabled) {
        /* 验证分区数 */
        if (config->partition_count < 2) {
            return 0;
        }
        /* 验证 overlap_ratio 范围 */
        if (config->overlap_ratio < 0.0f || config->overlap_ratio >= 1.0f) {
            return 0;
        }
    }

    return 1;
}

int32_t diskann_spann_config_validate(const diskann_spann_config_t *config)
{
    if (!config) {
        return 0;
    }

    if (config->enabled) {
        /* 验证最大分区数 */
        if (config->max_partitions < 1) {
            return 0;
        }
        /* 验证最小分区大小 */
        if (config->min_partition_size < 1) {
            return 0;
        }
        /* 验证搜索分区数 */
        if (config->search_partitions < 1 || config->search_partitions > config->max_partitions) {
            return 0;
        }
    }

    return 1;
}

int32_t diskann_fresh_config_validate(const diskann_fresh_config_t *config)
{
    if (!config) {
        return 0;
    }

    if (config->enabled) {
        /* 验证容量 */
        if (config->fresh_capacity <= 0) {
            return 0;
        }
        /* 验证合并阈值 */
        if (config->merge_threshold > config->fresh_capacity) {
            return 0;
        }
    }

    return 1;
}

int32_t diskann_config_validate(const diskann_config_t *config)
{
    if (!config) {
        return 0;
    }

    /* 验证基础参数 */
    if (config->dims <= 0) {
        return 0;
    }
    if (config->index_size <= 0) {
        return 0;
    }
    if (config->build_search_list_size <= 0) {
        return 0;
    }
    if (!distance_metric_is_valid(config->metric)) {
        return 0;
    }

    /* 验证存储配置 */
    if (config->storage.page_size <= 0) {
        return 0;
    }
    if (config->storage.frozen_point_count < 0) {
        return 0;
    }

    /* 验证子配置 */
    if (!diskann_quantization_config_validate(&config->quantization)) {
        return 0;
    }
    if (!diskann_merge_config_validate(&config->merge)) {
        return 0;
    }
    if (!diskann_spann_config_validate(&config->spann)) {
        return 0;
    }
    if (!diskann_fresh_config_validate(&config->fresh)) {
        return 0;
    }

    return 1;
}

/* ============================================================================
 * 配置克隆
 * ============================================================================ */

diskann_config_t *diskann_config_clone(const diskann_config_t *config)
{
    diskann_config_t *clone;

    if (!config) {
        return NULL;
    }

    clone = (diskann_config_t *)malloc(sizeof(diskann_config_t));
    if (!clone) {
        return NULL;
    }

    memcpy(clone, config, sizeof(diskann_config_t));
    return clone;
}

/* ============================================================================
 * 配置与旧参数互转
 * ============================================================================ */

diskann_config_t *diskann_config_from_params(int32_t dims,
                                             int32_t index_size,
                                             int32_t build_search_list_size,
                                             distance_metric_t metric,
                                             const diskann_quantization_params_t *pq_params,
                                             const diskann_storage_params_t *storage_params)
{
    diskann_config_t *config;

    if (dims <= 0 || index_size <= 0 || build_search_list_size <= 0) {
        return NULL;
    }
    if (!distance_metric_is_valid(metric)) {
        return NULL;
    }

    config = diskann_config_create(dims, index_size, build_search_list_size, metric);
    if (!config) {
        return NULL;
    }

    /* 从 PQ 参数转换 */
    if (pq_params) {
        config->quantization.enabled = pq_params->enabled;
        config->quantization.type = QUANTIZATION_TYPE_PQ;
        config->quantization.subquantizers = pq_params->pq_m;
        config->quantization.bits = pq_params->pq_bits;
        config->quantization.train_max_vectors = pq_params->train_max_vectors;
    }

    /* 从存储参数转换 */
    if (storage_params) {
        config->storage.page_size = storage_params->page_size;
        config->storage.frozen_point_count = storage_params->frozen_point_count;
    }

    return config;
}

int32_t diskann_config_to_params(const diskann_config_t *config,
                                 diskann_quantization_params_t *pq_params,
                                 diskann_storage_params_t *storage_params)
{
    if (!config) {
        return -1;
    }

    /* 转换为 PQ 参数 */
    if (pq_params) {
        pq_params->enabled = config->quantization.enabled;
        pq_params->pq_m = config->quantization.subquantizers > 0 ? config->quantization.subquantizers : 0;
        pq_params->pq_bits = config->quantization.bits;
        pq_params->train_max_vectors = config->quantization.train_max_vectors;
    }

    /* 转换为存储参数 */
    if (storage_params) {
        storage_params->page_size = config->storage.page_size;
        storage_params->frozen_point_count = config->storage.frozen_point_count;
    }

    return 0;
}

/* ============================================================================
 * 配置持久化 (JSON 格式)
 * ============================================================================ */

int32_t diskann_config_save(const diskann_config_t *config, const char *path)
{
    FILE *fp;
    int32_t result = -1;

    if (!config || !path) {
        return -1;
    }

    fp = fopen(path, "w");
    if (!fp) {
        return -1;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"dims\": %d,\n", config->dims);
    fprintf(fp, "  \"index_size\": %d,\n", config->index_size);
    fprintf(fp, "  \"build_search_list_size\": %d,\n", config->build_search_list_size);
    fprintf(fp, "  \"metric\": %d,\n", config->metric);

    fprintf(fp, "  \"quantization\": {\n");
    fprintf(fp, "    \"enabled\": %s,\n", config->quantization.enabled ? "true" : "false");
    fprintf(fp, "    \"type\": %d,\n", config->quantization.type);
    fprintf(fp, "    \"subquantizers\": %d,\n", config->quantization.subquantizers);
    fprintf(fp, "    \"bits\": %d,\n", config->quantization.bits);
    fprintf(fp, "    \"train_max_vectors\": %d\n", config->quantization.train_max_vectors);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"merge\": {\n");
    fprintf(fp, "    \"enabled\": %s,\n", config->merge.enabled ? "true" : "false");
    fprintf(fp, "    \"partition_method\": %d,\n", config->merge.partition_method);
    fprintf(fp, "    \"partition_count\": %d,\n", config->merge.partition_count);
    fprintf(fp, "    \"overlap_ratio\": %f\n", config->merge.overlap_ratio);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"spann\": {\n");
    fprintf(fp, "    \"enabled\": %s,\n", config->spann.enabled ? "true" : "false");
    fprintf(fp, "    \"max_partitions\": %d,\n", config->spann.max_partitions);
    fprintf(fp, "    \"min_partition_size\": %d,\n", config->spann.min_partition_size);
    fprintf(fp, "    \"search_partitions\": %d\n", config->spann.search_partitions);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"fresh\": {\n");
    fprintf(fp, "    \"enabled\": %s,\n", config->fresh.enabled ? "true" : "false");
    fprintf(fp, "    \"fresh_capacity\": %d,\n", config->fresh.fresh_capacity);
    fprintf(fp, "    \"merge_threshold\": %d\n", config->fresh.merge_threshold);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"storage\": {\n");
    fprintf(fp, "    \"page_size\": %d,\n", config->storage.page_size);
    fprintf(fp, "    \"frozen_point_count\": %d\n", config->storage.frozen_point_count);
    fprintf(fp, "  }\n");

    fprintf(fp, "}\n");

    result = 0;
    fclose(fp);
    return result;
}

diskann_config_t *diskann_config_load(const char *path)
{
    FILE *fp;
    diskann_config_t *config;
    int32_t dims, index_size, build_list_size, metric;
    int32_t q_enabled, q_type, q_subq, q_bits, q_train_max;
    int32_t m_enabled, m_method, m_count;
    float m_overlap;
    int32_t s_enabled, s_max_p, s_min_p, s_search_p;
    int32_t f_enabled, f_capacity, f_threshold;
    int32_t st_page_size, st_frozen;

    if (!path) {
        return NULL;
    }

    fp = fopen(path, "r");
    if (!fp) {
        return NULL;
    }

    /* 简单解析 JSON - 实际应用中应使用 JSON 库 */
    config = (diskann_config_t *)calloc(1, sizeof(diskann_config_t));
    if (!config) {
        fclose(fp);
        return NULL;
    }

    if (fscanf(fp,
               "{%d,%d,%d,%d"
               "%d,%d,%d,%d,%d"
               "%d,%d,%d,%f"
               "%d,%d,%d,%d"
               "%d,%d,%d"
               "%d,%d}",
               &dims, &index_size, &build_list_size, &metric,
               &q_enabled, &q_type, &q_subq, &q_bits, &q_train_max,
               &m_enabled, &m_method, &m_count, &m_overlap,
               &s_enabled, &s_max_p, &s_min_p, &s_search_p,
               &f_enabled, &f_capacity, &f_threshold,
               &st_page_size, &st_frozen) != 22) {
        /* 尝试更宽松的解析方式 */
        rewind(fp);
        if (fscanf(fp,
                   " { \"dims\" : %d , \"index_size\" : %d , \"build_search_list_size\" : %d , \"metric\" : %d }",
                   &dims, &index_size, &build_list_size, &metric) != 4) {
            fclose(fp);
            return NULL;
        }
        config->dims = dims;
        config->index_size = index_size;
        config->build_search_list_size = build_list_size;
        config->metric = (distance_metric_t)metric;
        diskann_config_init_defaults(config, dims, config->metric);
        return config;
    }

    config->dims = dims;
    config->index_size = index_size;
    config->build_search_list_size = build_list_size;
    config->metric = (distance_metric_t)metric;

    config->quantization.enabled = !!q_enabled;
    config->quantization.type = (quantization_type_t)q_type;
    config->quantization.subquantizers = q_subq;
    config->quantization.bits = q_bits;
    config->quantization.train_max_vectors = q_train_max;

    config->merge.enabled = !!m_enabled;
    config->merge.partition_method = (diskann_partition_method_t)m_method;
    config->merge.partition_count = m_count;
    config->merge.overlap_ratio = m_overlap;

    config->spann.enabled = !!s_enabled;
    config->spann.max_partitions = s_max_p;
    config->spann.min_partition_size = s_min_p;
    config->spann.search_partitions = s_search_p;

    config->fresh.enabled = !!f_enabled;
    config->fresh.fresh_capacity = f_capacity;
    config->fresh.merge_threshold = f_threshold;

    config->storage.page_size = st_page_size;
    config->storage.frozen_point_count = st_frozen;

    fclose(fp);
    return config;
}
