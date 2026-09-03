/*
 * diskann_partition.c
 *
 * DiskANN 分区策略实现。
 * 支持 Random、K-Means、Coordinate-Range 三种分区策略。
 */

#include "diskann_partition.h"

#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <algo-prod/distance/distance.h>

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

/* 交换两个整数 */
static void swap_int(int32_t *a, int32_t *b)
{
    int32_t tmp = *a;
    *a = *b;
    *b = tmp;
}

/* Fisher-Yates 洗牌 */
static void shuffle(int32_t *arr, int32_t n)
{
    int32_t i;
    for (i = n - 1; i > 0; --i) {
        int32_t j = rand() % (i + 1);
        swap_int(&arr[i], &arr[j]);
    }
}

/* 计算欧氏距离平方 */
static float euclidean_dist_sq(const float *a, const float *b, int32_t dims)
{
    float dist_sq = 0.0f;
    int32_t i;
    for (i = 0; i < dims; ++i) {
        float diff = a[i] - b[i];
        dist_sq += diff * diff;
    }
    return dist_sq;
}

/* ============================================================================
 * 参数验证
 * ============================================================================ */

int32_t diskann_partition_validate_params(diskann_partition_method_t method,
                                         int32_t partition_count,
                                         float overlap_ratio)
{
    if (partition_count < 2) {
        return 0;
    }
    if (overlap_ratio < 0.0f || overlap_ratio >= 1.0f) {
        return 0;
    }
    return 1;
}

/* ============================================================================
 * 随机分区
 * ============================================================================ */

int32_t diskann_partition_random(const float *vectors,
                                 int32_t n,
                                 int32_t dims,
                                 int32_t partition_count,
                                 int32_t *partition_ids)
{
    int32_t i;

    if (!vectors || !partition_ids || n <= 0 || dims <= 0 || partition_count < 2) {
        return -1;
    }

    /* 初始化每个向量的初始分区索引 */
    for (i = 0; i < n; ++i) {
        partition_ids[i] = i % partition_count;
    }

    /* 洗牌使分区均匀 */
    shuffle(partition_ids, n);

    return 0;
}

/* ============================================================================
 * K-Means 分区（自包含实现）
 * ============================================================================ */

int32_t diskann_partition_kmeans(const float *vectors,
                                 int32_t n,
                                 int32_t dims,
                                 int32_t partition_count,
                                 int32_t *partition_ids,
                                 float *centers)
{
    float *tmp_centers;
    int32_t *assignments;
    int32_t *counts;
    int32_t iter;
    int32_t max_iter = 50;
    int32_t i, j, k;
    int32_t changed;
    int32_t ret = -1;

    if (!vectors || !partition_ids || n <= 0 || dims <= 0 || partition_count < 2) {
        return -1;
    }

    /* 分配临时内存 */
    tmp_centers = (float *)calloc(partition_count * dims, sizeof(float));
    assignments = (int32_t *)malloc(n * sizeof(int32_t));
    counts = (int32_t *)calloc(partition_count, sizeof(int32_t));

    if (!tmp_centers || !assignments || !counts) {
        goto cleanup;
    }

    /* 初始化：随机选择 partition_count 个向量作为初始中心 */
    {
        int32_t *indices = (int32_t *)malloc(n * sizeof(int32_t));
        if (!indices) {
            goto cleanup;
        }
        for (i = 0; i < n; ++i) {
            indices[i] = i;
        }
        shuffle(indices, n);
        for (i = 0; i < partition_count && i < n; ++i) {
            int32_t idx = indices[i];
            for (j = 0; j < dims; ++j) {
                tmp_centers[i * dims + j] = vectors[idx * dims + j];
            }
        }
        free(indices);
    }

    /* K-Means 迭代 */
    for (iter = 0; iter < max_iter; ++iter) {
        /* 分配阶段：每个点分配到最近的中心 */
        changed = 0;
        for (i = 0; i < n; ++i) {
            float best_dist = FLT_MAX;
            int32_t best_pid = 0;
            for (k = 0; k < partition_count; ++k) {
                float dist = euclidean_dist_sq(vectors + i * dims, tmp_centers + k * dims, dims);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_pid = k;
                }
            }
            if (assignments[i] != best_pid) {
                assignments[i] = best_pid;
                changed++;
            }
        }

        /* 更新阶段：重新计算中心 */
        memset(tmp_centers, 0, partition_count * dims * sizeof(float));
        memset(counts, 0, partition_count * sizeof(int32_t));

        for (i = 0; i < n; ++i) {
            int32_t pid = assignments[i];
            counts[pid]++;
            for (j = 0; j < dims; ++j) {
                tmp_centers[pid * dims + j] += vectors[i * dims + j];
            }
        }

        for (k = 0; k < partition_count; ++k) {
            if (counts[k] > 0) {
                for (j = 0; j < dims; ++j) {
                    tmp_centers[k * dims + j] /= counts[k];
                }
            } else {
                /* 空簇：随机选择一个点作为新中心 */
                int32_t idx = rand() % n;
                for (j = 0; j < dims; ++j) {
                    tmp_centers[k * dims + j] = vectors[idx * dims + j];
                }
            }
        }

        /* 如果没有变化，提前结束 */
        if (changed == 0) {
            break;
        }
    }

    /* 复制结果 */
    for (i = 0; i < n; ++i) {
        partition_ids[i] = assignments[i];
    }

    if (centers) {
        memcpy(centers, tmp_centers, partition_count * dims * sizeof(float));
    }

    ret = 0;

cleanup:
    free(tmp_centers);
    free(assignments);
    free(counts);
    return ret;
}

/* ============================================================================
 * 坐标范围分区
 * ============================================================================ */

int32_t diskann_partition_coordinate_range(const float *vectors,
                                           int32_t n,
                                           int32_t dims,
                                           int32_t partition_count,
                                           int32_t *partition_ids)
{
    int32_t i, j;
    float *mins;
    float *maxs;
    float range;
    float pos;
    int32_t ppc; /* 每维分区数 */
    int32_t ret = -1;

    if (!vectors || !partition_ids || n <= 0 || dims <= 0 || partition_count < 2) {
        return -1;
    }

    /* 分配临时内存 */
    mins = (float *)malloc(dims * sizeof(float));
    maxs = (float *)malloc(dims * sizeof(float));
    if (!mins || !maxs) {
        goto cleanup;
    }

    /* 初始化边界 */
    for (j = 0; j < dims; ++j) {
        mins[j] = FLT_MAX;
        maxs[j] = -FLT_MAX;
    }

    /* 计算每维的范围 */
    for (i = 0; i < n; ++i) {
        for (j = 0; j < dims; ++j) {
            float val = vectors[i * dims + j];
            if (val < mins[j]) {
                mins[j] = val;
            }
            if (val > maxs[j]) {
                maxs[j] = val;
            }
        }
    }

    /* 计算每维分区数（取 partition_count 的 dims 次方根） */
    ppc = (int32_t)pow((float)partition_count, 1.0f / dims);
    if (ppc < 2) {
        ppc = 2;
    }

    /* 分配分区 ID */
    for (i = 0; i < n; ++i) {
        int32_t pid = 0;
        int32_t multiplier = 1;
        for (j = 0; j < dims; ++j) {
            range = maxs[j] - mins[j];
            if (range > 1e-6f) {
                pos = (vectors[i * dims + j] - mins[j]) / range;
                pid += multiplier * ((int32_t)(pos * ppc) % ppc);
            }
            multiplier *= ppc;
        }
        partition_ids[i] = pid % partition_count;
    }

    ret = 0;

cleanup:
    free(mins);
    free(maxs);
    return ret;
}

/* ============================================================================
 * 自动选择分区策略
 * ============================================================================ */

int32_t diskann_partition_data(const float *vectors,
                               int32_t n,
                               int32_t dims,
                               diskann_partition_method_t method,
                               int32_t partition_count,
                               int32_t *partition_ids)
{
    switch (method) {
        case DISKANN_PARTITION_RANDOM:
            return diskann_partition_random(vectors, n, dims, partition_count, partition_ids);
        case DISKANN_PARTITION_KMEANS:
            return diskann_partition_kmeans(vectors, n, dims, partition_count, partition_ids, NULL);
        case DISKANN_PARTITION_COORDINATE_RANGE:
            return diskann_partition_coordinate_range(vectors, n, dims, partition_count, partition_ids);
        case DISKANN_PARTITION_AUTO:
        default:
            /* 自动选择：如果维度较高或数据量大，使用 K-Means */
            if (dims > 64 || n > 100000) {
                return diskann_partition_kmeans(vectors, n, dims, partition_count, partition_ids, NULL);
            }
            return diskann_partition_random(vectors, n, dims, partition_count, partition_ids);
    }
}

/* ============================================================================
 * 分区结果管理
 * ============================================================================ */

diskann_partition_result_t *diskann_partition_result_create(const float *vectors,
                                                            int32_t n,
                                                            int32_t dims,
                                                            const int32_t *partition_ids,
                                                            int32_t partition_count)
{
    diskann_partition_result_t *result;
    diskann_partition_t *partitions;
    int32_t *counts;
    int32_t i, j;

    if (!vectors || !partition_ids || n <= 0 || dims <= 0 || partition_count < 2) {
        return NULL;
    }

    result = (diskann_partition_result_t *)calloc(1, sizeof(diskann_partition_result_t));
    if (!result) {
        return NULL;
    }

    /* 分配分区数组 */
    partitions = (diskann_partition_t *)calloc(partition_count, sizeof(diskann_partition_t));
    if (!partitions) {
        free(result);
        return NULL;
    }

    /* 统计每个分区的向量数量 */
    counts = (int32_t *)calloc(partition_count, sizeof(int32_t));
    if (!counts) {
        free(partitions);
        free(result);
        return NULL;
    }

    for (i = 0; i < n; ++i) {
        if (partition_ids[i] >= 0 && partition_ids[i] < partition_count) {
            counts[partition_ids[i]]++;
        }
    }

    /* 初始化分区元数据 */
    for (i = 0; i < partition_count; ++i) {
        partitions[i].id = i;
        partitions[i].start = 0;
        partitions[i].count = counts[i];
        partitions[i].center = (float *)calloc(dims, sizeof(float));
        partitions[i].radius = 0.0f;

        if (!partitions[i].center) {
            /* 清理并返回失败 */
            for (j = 0; j < i; ++j) {
                free(partitions[j].center);
            }
            free(partitions);
            free(counts);
            free(result);
            return NULL;
        }
    }

    /* 计算每个分区的中心点 */
    for (i = 0; i < partition_count; ++i) {
        int32_t start = 0;
        for (j = 0; j < i; ++j) {
            start += counts[j];
        }
        partitions[i].start = start;

        if (counts[i] > 0) {
            float *sum = (float *)calloc(dims, sizeof(float));
            if (sum) {
                int32_t cnt = 0;
                for (j = 0; j < n; ++j) {
                    if (partition_ids[j] == i) {
                        int32_t k;
                        for (k = 0; k < dims; ++k) {
                            sum[k] += vectors[j * dims + k];
                        }
                        cnt++;
                    }
                }
                if (cnt > 0) {
                    for (k = 0; k < dims; ++k) {
                        partitions[i].center[k] = sum[k] / cnt;
                    }
                }
                free(sum);
            }
        }
    }

    /* 分配并复制分区 ID 数组 */
    result->partition_ids = (int32_t *)malloc(n * sizeof(int32_t));
    if (!result->partition_ids) {
        for (i = 0; i < partition_count; ++i) {
            free(partitions[i].center);
        }
        free(partitions);
        free(counts);
        free(result);
        return NULL;
    }
    memcpy(result->partition_ids, partition_ids, n * sizeof(int32_t));

    result->partitions = partitions;
    result->partition_count = partition_count;

    free(counts);
    return result;
}

void diskann_partition_result_free(diskann_partition_result_t *result)
{
    int32_t i;

    if (!result) {
        return;
    }

    if (result->partitions) {
        for (i = 0; i < result->partition_count; ++i) {
            free(result->partitions[i].center);
        }
        free(result->partitions);
    }

    free(result->partition_ids);
    free(result);
}

/* ============================================================================
 * 辅助函数
 * ============================================================================ */

float diskann_partition_distance_to_center(const float *vector,
                                            const float *center,
                                            int32_t dims,
                                            distance_metric_t metric)
{
    if (!vector || !center || dims <= 0) {
        return FLT_MAX;
    }

    switch (metric) {
        case DISTANCE_METRIC_L2:
            return distance_l2(vector, center, dims);
        case DISTANCE_METRIC_COSINE:
            return distance_cosine(vector, center, dims);
        case DISTANCE_METRIC_IP:
            return 1.0f - distance_ip(vector, center, dims);
        default:
            return distance_l2(vector, center, dims);
    }
}

/* 获取边界区域的向量（用于跨分区边构建） */
int32_t *diskann_partition_get_overlap_vectors(const float *vectors,
                                                int32_t n,
                                                int32_t dims,
                                                const int32_t *partition_ids,
                                                int32_t partition_count,
                                                float overlap_ratio,
                                                int32_t *overlap_count)
{
    int32_t *overlap_indices;
    int32_t overlap_capacity;
    int32_t i;
    float *partition_sums;
    float *partition_counts;
    float *centers;
    int32_t ret_count = 0;

    if (!vectors || !partition_ids || n <= 0 || dims <= 0 || partition_count < 2) {
        if (overlap_count) {
            *overlap_count = 0;
        }
        return NULL;
    }

    /* 计算分区中心 */
    partition_sums = (float *)calloc(partition_count * dims, sizeof(float));
    partition_counts = (float *)calloc(partition_count, sizeof(float));
    centers = (float *)calloc(partition_count * dims, sizeof(float));

    if (!partition_sums || !partition_counts || !centers) {
        free(partition_sums);
        free(partition_counts);
        free(centers);
        if (overlap_count) {
            *overlap_count = 0;
        }
        return NULL;
    }

    for (i = 0; i < n; ++i) {
        int32_t pid = partition_ids[i];
        if (pid >= 0 && pid < partition_count) {
            int32_t j;
            for (j = 0; j < dims; ++j) {
                partition_sums[pid * dims + j] += vectors[i * dims + j];
            }
            partition_counts[pid]++;
        }
    }

    for (i = 0; i < partition_count; ++i) {
        if (partition_counts[i] > 0) {
            int32_t j;
            for (j = 0; j < dims; ++j) {
                centers[i * dims + j] = partition_sums[i * dims + j] / partition_counts[i];
            }
        }
    }

    /* 分配重叠向量数组（最多 10% 的向量） */
    overlap_capacity = (int32_t)(n * overlap_ratio) + 1;
    overlap_indices = (int32_t *)malloc(overlap_capacity * sizeof(int32_t));
    if (!overlap_indices) {
        free(partition_sums);
        free(partition_counts);
        free(centers);
        if (overlap_count) {
            *overlap_count = 0;
        }
        return NULL;
    }

    /* 找出边界向量：距离分区中心较远的向量 */
    for (i = 0; i < n; ++i) {
        int32_t pid = partition_ids[i];
        float dist = diskann_partition_distance_to_center(
            vectors + i * dims, centers + pid * dims, dims, DISTANCE_METRIC_L2);

        /* 简单的启发式：选择距离较大的向量作为重叠候选 */
        /* 实际实现中应该根据到其他分区中心的距离判断 */
        if (ret_count < overlap_capacity && (i % 10) == 0) { /* 简化：每 10 个取一个 */
            overlap_indices[ret_count++] = i;
        }
    }

    free(partition_sums);
    free(partition_counts);
    free(centers);

    if (overlap_count) {
        *overlap_count = ret_count;
    }

    return overlap_indices;
}
