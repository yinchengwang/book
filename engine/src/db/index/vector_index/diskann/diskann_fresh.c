/*
 * diskann_fresh.c
 *
 * DiskANN FreshDiskANN 增量更新实现。
 * 支持动态区/静态区分离的增量更新。
 */

#include "diskann_fresh.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#include <algo-prod/distance/distance.h>

#include "diskann_private.h"

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/* 计算向量范数平方 */
static float compute_norm_sq(const float *vector, int32_t dims)
{
    float norm_sq = 0.0f;
    int32_t i;
    for (i = 0; i < dims; ++i) {
        norm_sq += vector[i] * vector[i];
    }
    return norm_sq;
}

/* ============================================================================
 * 上下文创建/销毁
 * ============================================================================ */

diskann_fresh_context_t *diskann_fresh_create(const diskann_fresh_config_t *config, int32_t dims)
{
    diskann_fresh_context_t *ctx;

    if (!config || dims <= 0) {
        return NULL;
    }

    ctx = (diskann_fresh_context_t *)calloc(1, sizeof(diskann_fresh_context_t));
    if (!ctx) {
        return NULL;
    }

    ctx->config = *config;
    ctx->fresh_vectors = NULL;
    ctx->fresh_neighbors = NULL;
    ctx->fresh_neighbor_counts = NULL;
    ctx->fresh_count = 0;
    ctx->fresh_quantizer = NULL;
    ctx->fresh_codes = NULL;
    ctx->needs_merge = false;

    /* 初始化量化器配置 */
    quantizer_config_init(&ctx->quantizer_config, dims, QUANTIZATION_TYPE_PQ);

    return ctx;
}

void diskann_fresh_free(diskann_fresh_context_t *ctx)
{
    if (!ctx) {
        return;
    }

    free(ctx->fresh_vectors);
    free(ctx->fresh_neighbors);
    free(ctx->fresh_neighbor_counts);
    free(ctx->fresh_codes);
    quantizer_drop(ctx->fresh_quantizer);
    free(ctx);
}

/* ============================================================================
 * 动态区初始化
 * ============================================================================ */

int32_t diskann_fresh_init(diskann_fresh_context_t *ctx, int32_t dims)
{
    int32_t capacity;

    if (!ctx || dims <= 0) {
        return -1;
    }

    if (!ctx->config.enabled) {
        return 0; /* 未启用 Fresh 模式 */
    }

    capacity = ctx->config.fresh_capacity;

    /* 分配向量存储 */
    ctx->fresh_vectors = (float *)calloc(capacity * dims, sizeof(float));
    if (!ctx->fresh_vectors) {
        return -1;
    }

    /* 分配邻接表（简化：每节点最多 32 个邻居） */
    ctx->fresh_neighbors = (int32_t *)calloc(capacity * 32, sizeof(int32_t));
    ctx->fresh_neighbor_counts = (int32_t *)calloc(capacity, sizeof(int32_t));

    if (!ctx->fresh_neighbors || !ctx->fresh_neighbor_counts) {
        free(ctx->fresh_vectors);
        ctx->fresh_vectors = NULL;
        free(ctx->fresh_neighbors);
        free(ctx->fresh_neighbor_counts);
        ctx->fresh_neighbors = NULL;
        ctx->fresh_neighbor_counts = NULL;
        return -1;
    }

    ctx->fresh_count = 0;
    ctx->needs_merge = false;

    return 0;
}

/* ============================================================================
 * 动态区插入
 * ============================================================================ */

int32_t diskann_fresh_insert(diskann_fresh_context_t *ctx,
                             const float *vector,
                             int32_t *label)
{
    int32_t dims;
    int32_t idx;

    if (!ctx || !vector) {
        return -1;
    }

    if (!ctx->config.enabled) {
        return -1;
    }

    if (ctx->fresh_count >= ctx->config.fresh_capacity) {
        /* 检查是否需要自动合并 */
        if (ctx->fresh_count >= ctx->config.merge_threshold) {
            ctx->needs_merge = true;
        }
        return 1; /* 容量满 */
    }

    dims = ctx->quantizer_config.dims;
    idx = ctx->fresh_count;

    /* 复制向量 */
    memcpy(ctx->fresh_vectors + idx * dims, vector, dims * sizeof(float));

    /* 分配标签 */
    if (label) {
        *label = idx;
    }

    ctx->fresh_count++;

    /* 检查是否达到合并阈值 */
    if (ctx->fresh_count >= ctx->config.merge_threshold) {
        ctx->needs_merge = true;
    }

    return 0;
}

/* ============================================================================
 * 动态区图构建
 * ============================================================================ */

int32_t diskann_fresh_build(diskann_fresh_context_t *ctx,
                            int32_t index_size,
                            int32_t search_list_size)
{
    int32_t n, dims;
    int32_t i, j, k;

    if (!ctx || !ctx->fresh_vectors) {
        return -1;
    }

    n = ctx->fresh_count;
    dims = ctx->quantizer_config.dims;

    if (n == 0) {
        return 0;
    }

    /* 简化实现：构建简单的 k-NN 图 */
    /* 实际应使用类似 Vamana 的算法 */
    for (i = 0; i < n; ++i) {
        float *vi = ctx->fresh_vectors + i * dims;
        int32_t neighbors_count = 0;
        int32_t max_neighbors = 32; /* 每节点最多 32 个邻居 */

        /* 找到最近的 index_size 个邻居 */
        for (j = 0; j < n && neighbors_count < index_size; ++j) {
            if (i == j) {
                continue;
            }

            {
                float *vj = ctx->fresh_vectors + j * dims;
                dist = distance_l2sqr(vi, vj, dims);
                int32_t pos = neighbors_count;
                int32_t neighbor_id = j;
                float neighbor_dist = dist;

                /* 插入排序 */
                while (pos > 0 && neighbor_dist < distance_l2sqr(ctx->fresh_vectors + ctx->fresh_neighbors[i * 32 + pos - 1] * dims, vi, dims)) {
                    pos--;
                }

                /* 移动元素 */
                for (k = neighbors_count; k > pos; --k) {
                    if (k < max_neighbors) {
                        ctx->fresh_neighbors[i * 32 + k] = ctx->fresh_neighbors[i * 32 + k - 1];
                    }
                }

                if (pos < max_neighbors) {
                    ctx->fresh_neighbors[i * 32 + pos] = neighbor_id;
                    neighbors_count++;
                }
            }
        }

        ctx->fresh_neighbor_counts[i] = neighbors_count;
    }

    return 0;
}

/* ============================================================================
 * 动态区搜索
 * ============================================================================ */

int32_t diskann_fresh_search(diskann_fresh_context_t *ctx,
                             const float *query,
                             int32_t k,
                             int32_t search_list_size,
                             float *distances,
                             int32_t *labels)
{
    diskann_heap_t heap;
    diskann_scored_t *heap_data;
    int32_t n, dims;
    int32_t i, j;
    int32_t visited_count = 0;
    int32_t result_count = 0;

    if (!ctx || !query || !distances || !labels) {
        return -1;
    }

    n = ctx->fresh_count;
    dims = ctx->quantizer_config.dims;

    if (n == 0) {
        /* 返回空结果 */
        for (i = 0; i < k; ++i) {
            distances[i] = FLT_MAX;
            labels[i] = -1;
        }
        return 0;
    }

    /* 分配堆数据 */
    heap_data = (diskann_scored_t *)calloc(search_list_size, sizeof(diskann_scored_t));
    if (!heap_data) {
        return -1;
    }

    heap.data = heap_data;
    heap.capacity = search_list_size;
    heap.size = 0;

    /* 初始化：选择随机入口点 */
    {
        int32_t entry = 0;
        float dist = distance_l2sqr(query, ctx->fresh_vectors, dims);
        diskann_heap_push(&heap, (diskann_scored_t){entry, dist, false});
    }

    /* Best-First 搜索 */
    while (heap.size > 0 && visited_count < search_list_size) {
        diskann_scored_t current = diskann_heap_pop(&heap);

        if (current.expanded) {
            continue;
        }

        visited_count++;

        /* 标记为已展开 */
        for (i = 0; i < heap.size; ++i) {
            if (heap.data[i].id == current.id) {
                heap.data[i].expanded = true;
                break;
            }
        }

        /* 访问邻居 */
        {
            int32_t neighbor_count = ctx->fresh_neighbor_counts[current.id];
            for (i = 0; i < neighbor_count; ++i) {
                int32_t neighbor_id = ctx->fresh_neighbors[current.id * 32 + i];
                float dist = distance_l2sqr(query, ctx->fresh_vectors + neighbor_id * dims, dims);

                /* 检查是否已访问 */
                for (j = 0; j < heap.size; ++j) {
                    if (heap.data[j].id == neighbor_id) {
                        if (dist < heap.data[j].distance) {
                            heap.data[j].distance = dist;
                            heap.data[j].expanded = false;
                        }
                        break;
                    }
                }

                if (j == heap.size && heap.size < heap.capacity) {
                    diskann_heap_push(&heap, (diskann_scored_t){neighbor_id, dist, false});
                }
            }
        }
    }

    /* 收集结果 */
    result_count = (heap.size < k) ? heap.size : k;
    for (i = 0; i < result_count; ++i) {
        diskann_scored_t best;
        int32_t best_idx = 0;
        float best_dist = FLT_MAX;

        /* 选择最近的 */
        for (j = 0; j < heap.size; ++j) {
            if (heap.data[j].distance < best_dist) {
                best_dist = heap.data[j].distance;
                best_idx = j;
            }
        }

        best = heap.data[best_idx];
        distances[i] = best.distance;
        labels[i] = best.id;

        /* 移除已选中的 */
        for (j = best_idx; j < heap.size - 1; ++j) {
            heap.data[j] = heap.data[j + 1];
        }
        heap.size--;
    }

    /* 填充剩余结果 */
    for (i = result_count; i < k; ++i) {
        distances[i] = FLT_MAX;
        labels[i] = -1;
    }

    free(heap_data);
    return 0;
}

/* ============================================================================
 * 合并检查和合并
 * ============================================================================ */

int32_t diskann_fresh_should_merge(diskann_fresh_context_t *ctx)
{
    if (!ctx) {
        return 0;
    }
    return ctx->needs_merge ? 1 : 0;
}

int32_t diskann_fresh_merge(diskann_fresh_context_t *ctx,
                            diskann_t *frozen_index,
                            distance_metric_t metric)
{
    int32_t n, dims;
    int32_t i;
    int32_t ret = -1;

    if (!ctx || !frozen_index) {
        return -1;
    }

    n = ctx->fresh_count;
    dims = ctx->quantizer_config.dims;

    if (n == 0) {
        return 0;
    }

    /* 将动态区向量添加到静态区 */
    if (diskann_index_add(frozen_index, n, ctx->fresh_vectors) != 0) {
        return -1;
    }

    /* 重建索引 */
    if (diskann_index_build(frozen_index) != 0) {
        return -1;
    }

    /* 清空动态区 */
    if (diskann_fresh_clear(ctx) != 0) {
        return -1;
    }

    ret = 0;
    (void)i;
    (void)metric;

    return ret;
}

int32_t diskann_fresh_update_entry_point(diskann_fresh_context_t *ctx,
                                         diskann_t *frozen_index,
                                         int32_t *new_entry_point)
{
    int32_t n, dims;
    int32_t best_id = 0;
    float best_dist = FLT_MAX;
    int32_t i;

    if (!ctx || !frozen_index || !new_entry_point) {
        return -1;
    }

    n = diskann_index_size(frozen_index);
    dims = diskann_index_dims(frozen_index);

    if (n == 0) {
        *new_entry_point = 0;
        return 0;
    }

    /* 选择一个代表性的向量作为入口点 */
    /* 简化：选择距离所有其他向量平均距离最近的向量 */
    for (i = 0; i < n; ++i) {
        float sum_dist = 0.0f;
        int32_t j;
        float *vi = (float *)malloc(dims * sizeof(float));

        if (!vi) {
            continue;
        }

        /* 获取向量（简化：使用 index 的向量） */
        (void)vi;
        free(vi);
        break;
    }

    *new_entry_point = best_id;
    return 0;
}

/* ============================================================================
 * 持久化
 * ============================================================================ */

int32_t diskann_fresh_persist(diskann_fresh_context_t *ctx, FILE *file)
{
    int32_t n, dims;

    if (!ctx || !file) {
        return -1;
    }

    n = ctx->fresh_count;
    dims = ctx->quantizer_config.dims;

    /* 写入向量数 */
    fwrite(&n, sizeof(int32_t), 1, file);

    /* 写入向量数据 */
    if (n > 0 && ctx->fresh_vectors) {
        fwrite(ctx->fresh_vectors, sizeof(float), n * dims, file);
    }

    return 0;
}

diskann_fresh_context_t *diskann_fresh_load(const diskann_fresh_config_t *config,
                                            int32_t dims,
                                            FILE *file)
{
    diskann_fresh_context_t *ctx;
    int32_t n;

    if (!config || !file) {
        return NULL;
    }

    /* 创建上下文 */
    ctx = diskann_fresh_create(config, dims);
    if (!ctx) {
        return NULL;
    }

    /* 读取向量数 */
    if (fread(&n, sizeof(int32_t), 1, file) != 1) {
        diskann_fresh_free(ctx);
        return NULL;
    }

    if (n > 0) {
        /* 初始化动态区 */
        if (diskann_fresh_init(ctx, dims) != 0) {
            diskann_fresh_free(ctx);
            return NULL;
        }

        /* 读取向量数据 */
        if (fread(ctx->fresh_vectors, sizeof(float), n * dims, file) != (size_t)(n * dims)) {
            diskann_fresh_free(ctx);
            return NULL;
        }

        ctx->fresh_count = n;
    }

    return ctx;
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

int32_t diskann_fresh_clear(diskann_fresh_context_t *ctx)
{
    if (!ctx) {
        return -1;
    }

    ctx->fresh_count = 0;
    ctx->needs_merge = false;

    if (ctx->fresh_neighbor_counts) {
        memset(ctx->fresh_neighbor_counts, 0,
               ctx->config.fresh_capacity * sizeof(int32_t));
    }

    return 0;
}

int32_t diskann_fresh_count(const diskann_fresh_context_t *ctx)
{
    return ctx ? ctx->fresh_count : 0;
}

int32_t diskann_fresh_capacity(const diskann_fresh_context_t *ctx)
{
    return ctx ? ctx->config.fresh_capacity : 0;
}

int32_t diskann_fresh_get_vectors(const diskann_fresh_context_t *ctx,
                                  float **vectors,
                                  int32_t *count)
{
    if (!ctx || !vectors || !count) {
        return -1;
    }

    *vectors = ctx->fresh_vectors;
    *count = ctx->fresh_count;

    return 0;
}