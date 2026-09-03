/*
 * diskann_spann.c
 *
 * DiskANN SPANN (SParse ANN) 分区层次索引实现。
 * 支持分区索引以减少搜索时的 IO 操作。
 */

#include "diskann_spann.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#include "diskann_private.h"
#include "diskann_partition.h"

/* ============================================================================
 * 上下文创建/销毁
 * ============================================================================ */

diskann_spann_context_t *diskann_spann_create(const diskann_spann_config_t *config)
{
    diskann_spann_context_t *ctx;

    if (!config) {
        return NULL;
    }

    ctx = (diskann_spann_context_t *)calloc(1, sizeof(diskann_spann_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->config = *config;
    ctx->partition_result = NULL;
    ctx->query_distances = NULL;

    return ctx;
}

void diskann_spann_free(diskann_spann_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    if (ctx->partition_result) {
        diskann_partition_result_free(ctx->partition_result);
    }

    free(ctx->query_distances);
    free(ctx);
}

/* ============================================================================
 * SPANN 初始化和分区构建
 * ============================================================================ */

int32_t diskann_spann_init(diskann_spann_context_t *ctx,
                           const float *vectors,
                           int32_t n,
                           int32_t dims)
{
    if (!ctx || !vectors || n <= 0 || dims <= 0) {
        return -1;
    }

    if (!ctx->config.enabled) {
        return 0; /* 未启用 SPANN */
    }

    /* 分配距离缓存 */
    ctx->query_distances = (float *)malloc(ctx->config.max_partitions * sizeof(float));
    if (!ctx->query_distances) {
        return -1;
    }

    return diskann_spann_build_partitions(ctx, vectors, n, dims);
}

int32_t diskann_spann_build_partitions(diskann_spann_context_t *ctx,
                                       const float *vectors,
                                       int32_t n,
                                       int32_t dims)
{
    int32_t partition_count;
    int32_t *partition_ids;
    int32_t ret = -1;

    if (!ctx || !vectors || n <= 0 || dims <= 0) {
        return -1;
    }

    /* 计算分区数：确保每个分区至少有 min_partition_size 个向量 */
    partition_count = n / ctx->config.min_partition_size;
    if (partition_count < 1) {
        partition_count = 1;
    }
    if (partition_count > ctx->config.max_partitions) {
        partition_count = ctx->config.max_partitions;
    }

    /* 分配分区 ID 数组 */
    partition_ids = (int32_t *)malloc(n * sizeof(int32_t));
    if (!partition_ids) {
        return -1;
    }

    /* 使用 K-Means 分区策略构建分区 */
    if (diskann_partition_kmeans(vectors, n, dims, partition_count, partition_ids, NULL) != 0) {
        free(partition_ids);
        return -1;
    }

    /* 创建分区结果 */
    ctx->partition_result = diskann_partition_result_create(vectors, n, dims,
                                                            partition_ids, partition_count);
    if (!ctx->partition_result) {
        free(partition_ids);
        return -1;
    }

    ret = 0;
    free(partition_ids);
    return ret;
}

int32_t diskann_spann_compute_centers(diskann_spann_context_t *ctx,
                                      const float *vectors,
                                      int32_t n,
                                      int32_t dims)
{
    diskann_partition_t *partition;
    int32_t i, j;
    float *sum;
    int32_t count;

    if (!ctx || !vectors || n <= 0 || dims <= 0) {
        return -1;
    }

    if (!ctx->partition_result || !ctx->partition_result->partitions) {
        return -1;
    }

    /* 计算每个分区的中心点（如果尚未计算） */
    for (i = 0; i < ctx->partition_result->partition_count; ++i) {
        partition = &ctx->partition_result->partitions[i];

        if (!partition->center) {
            partition->center = (float *)calloc(dims, sizeof(float));
            if (!partition->center) {
                return -1;
            }
        }

        /* 统计该分区的向量并计算和 */
        sum = (float *)calloc(dims, sizeof(float));
        if (!sum) {
            return -1;
        }
        count = 0;

        for (j = 0; j < n; ++j) {
            if (ctx->partition_result->partition_ids[j] == i) {
                int32_t k;
                for (k = 0; k < dims; ++k) {
                    sum[k] += vectors[j * dims + k];
                }
                count++;
            }
        }

        /* 计算平均值 */
        if (count > 0) {
            for (j = 0; j < dims; ++j) {
                partition->center[j] = sum[j] / count;
            }
        }

        free(sum);
    }

    return 0;
}

/* ============================================================================
 * 分区选择（路由）
 * ============================================================================ */

int32_t diskann_spann_select_partitions(diskann_spann_context_t *ctx,
                                        const float *query,
                                        int32_t dims,
                                        int32_t **selected_partitions,
                                        int32_t *selected_count)
{
    diskann_partition_t *partition;
    float *distances;
    int32_t *indices;
    int32_t i, j;
    int32_t search_count;
    int32_t partition_count;
    int32_t ret = -1;

    if (!ctx || !query || !selected_partitions || !selected_count) {
        return -1;
    }

    if (!ctx->partition_result || !ctx->partition_result->partitions) {
        return -1;
    }

    partition_count = ctx->partition_result->partition_count;
    search_count = ctx->config.search_partitions;

    if (search_count > partition_count) {
        search_count = partition_count;
    }

    /* 分配临时数组 */
    distances = (float *)malloc(partition_count * sizeof(float));
    indices = (int32_t *)malloc(partition_count * sizeof(int32_t));
    *selected_partitions = (int32_t *)malloc(search_count * sizeof(int32_t));

    if (!distances || !indices || !*selected_partitions) {
        goto cleanup;
    }

    /* 计算查询到每个分区中心的距离 */
    for (i = 0; i < partition_count; ++i) {
        partition = &ctx->partition_result->partitions[i];
        if (partition->center) {
            distances[i] = diskann_partition_distance_to_center(
                query, partition->center, dims, DISTANCE_METRIC_L2);
        } else {
            distances[i] = FLT_MAX;
        }
        indices[i] = i;
    }

    /* 简单选择排序：选择最近的 search_count 个分区 */
    for (i = 0; i < search_count; ++i) {
        int32_t best_idx = i;
        float best_dist = distances[i];

        for (j = i + 1; j < partition_count; ++j) {
            if (distances[j] < best_dist) {
                best_dist = distances[j];
                best_idx = j;
            }
        }

        /* 交换 */
        {
            float tmp_dist = distances[i];
            distances[i] = distances[best_idx];
            distances[best_idx] = tmp_dist;

            int32_t tmp_idx = indices[i];
            indices[i] = indices[best_idx];
            indices[best_idx] = tmp_idx;
        }

        (*selected_partitions)[i] = indices[i];
    }

    *selected_count = search_count;
    ret = 0;

cleanup:
    free(distances);
    free(indices);

    if (ret != 0 && *selected_partitions) {
        free(*selected_partitions);
        *selected_partitions = NULL;
    }

    return ret;
}

/* ============================================================================
 * SPANN 搜索
 * ============================================================================ */

int32_t diskann_spann_search(diskann_spann_context_t *ctx,
                             const diskann_t *index,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             int32_t max_iterations,
                             const int32_t *selected_partitions,
                             int32_t selected_count,
                             float *distances,
                             int32_t *labels)
{
    /* SPANN 搜索简化实现：在选中分区上进行搜索 */
    /* 实际实现需要按分区过滤候选向量 */

    if (!ctx || !index || !query || !selected_partitions || !distances || !labels) {
        return -1;
    }

    /* 简化：直接调用底层索引搜索 */
    /* 后续可以实现按分区过滤的精确搜索 */
    (void)selected_partitions;
    (void)selected_count;
    (void)max_iterations;

    return diskann_index_search(index, query, k, search_list_size, 100,
                                distances, labels);
}

int32_t diskann_spann_merge_results(diskann_spann_context_t *ctx,
                                    float **partition_results,
                                    int32_t partition_count,
                                    int32_t k,
                                    float *final_distances,
                                    int32_t *final_labels)
{
    /* 合并多个分区的搜索结果 */
    /* 简化实现：只保留最近的 k 个结果 */

    int32_t i, j;
    int32_t result_idx = 0;
    int32_t total_results;
    float *all_distances;
    int32_t *all_labels;

    if (!ctx || !partition_results || !final_distances || !final_labels) {
        return -1;
    }

    /* 统计总结果数 */
    total_results = partition_count * k;
    if (total_results <= 0) {
        return -1;
    }

    /* 分配临时数组 */
    all_distances = (float *)malloc(total_results * sizeof(float));
    all_labels = (int32_t *)malloc(total_results * sizeof(int32_t));

    if (!all_distances || !all_labels) {
        free(all_distances);
        free(all_labels);
        return -1;
    }

    /* 收集所有结果 */
    result_idx = 0;
    for (i = 0; i < partition_count; ++i) {
        if (partition_results[i]) {
            /* 每个分区的结果存储为 [dist1, label1, dist2, label2, ...] */
            for (j = 0; j < k && result_idx < total_results; ++j) {
                /* 简化处理：假设 partition_results[i] 指向距离数组 */
                all_distances[result_idx] = partition_results[i][j];
                all_labels[result_idx] = j; /* 简化：使用索引作为标签 */
                result_idx++;
            }
        }
    }

    /* 简单选择排序：选择最近的 k 个 */
    {
        int32_t count = result_idx;
        for (i = 0; i < k && i < count; ++i) {
            int32_t best_idx = i;
            float best_dist = all_distances[i];

            for (j = i + 1; j < count; ++j) {
                if (all_distances[j] < best_dist) {
                    best_dist = all_distances[j];
                    best_idx = j;
                }
            }

            /* 交换 */
            final_distances[i] = all_distances[best_idx];
            final_labels[i] = all_labels[best_idx];

            all_distances[best_idx] = all_distances[i];
            all_labels[best_idx] = all_labels[i];
        }
    }

    free(all_distances);
    free(all_labels);

    return 0;
}

/* ============================================================================
 * 持久化
 * ============================================================================ */

int32_t diskann_spann_persist(diskann_spann_context_t *ctx, FILE *file)
{
    diskann_partition_t *partition;
    int32_t i, j;

    if (!ctx || !file) {
        return -1;
    }

    if (!ctx->partition_result) {
        return 0;
    }

    /* 写入分区数量 */
    fwrite(&ctx->partition_result->partition_count, sizeof(int32_t), 1, file);

    /* 写入每个分区的元数据 */
    for (i = 0; i < ctx->partition_result->partition_count; ++i) {
        partition = &ctx->partition_result->partitions[i];

        fwrite(&partition->id, sizeof(int32_t), 1, file);
        fwrite(&partition->start, sizeof(int32_t), 1, file);
        fwrite(&partition->count, sizeof(int32_t), 1, file);
        fwrite(&partition->radius, sizeof(float), 1, file);

        /* 写入中心点 */
        if (partition->center) {
            fwrite(partition->center, sizeof(float), 64, file); /* 假设最大 dims 为 64 */
        }
    }

    (void)j;

    return 0;
}

diskann_spann_context_t *diskann_spann_load(const diskann_spann_config_t *config, FILE *file)
{
    diskann_spann_context_t *ctx;
    int32_t partition_count;
    int32_t i;

    if (!config || !file) {
        return NULL;
    }

    ctx = diskann_spann_create(config);
    if (!ctx) {
        return NULL;
    }

    /* 读取分区数量 */
    if (fread(&partition_count, sizeof(int32_t), 1, file) != 1) {
        diskann_spann_free(ctx);
        return NULL;
    }

    /* 后续可以从文件加载完整的分区信息 */
    /* 简化：返回空上下文，后续调用 init 重建 */
    (void)i;

    return ctx;
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

int32_t diskann_spann_get_partition_count(const diskann_spann_context_t *ctx)
{
    if (!ctx || !ctx->partition_result) {
        return 0;
    }
    return ctx->partition_result->partition_count;
}

int32_t diskann_spann_get_partition_info(const diskann_spann_context_t *ctx,
                                         int32_t partition_id,
                                         float **center,
                                         int32_t *count)
{
    diskann_partition_t *partition;

    if (!ctx || !ctx->partition_result || !center || !count) {
        return -1;
    }

    if (partition_id < 0 || partition_id >= ctx->partition_result->partition_count) {
        return -1;
    }

    partition = &ctx->partition_result->partitions[partition_id];
    *center = partition->center;
    *count = partition->count;

    return 0;
}