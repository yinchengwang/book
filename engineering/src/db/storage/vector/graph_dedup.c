/**
 * @file graph_dedup.c
 * @brief 图索引去重模块实现
 *
 * 使用 SimHash + Bloom Filter 实现高效的向量去重。
 */
#include "db/storage/vector/graph_dedup.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** SimHash 位数 */
#define SIMHASH_BITS 64

/** 每维度随机投影向量（简化实现用） */
#define SIMHASH_PROJECTIONS 64

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief SimHash 投影向量（预生成）
 */
typedef struct simhash_projection_s {
    float v[SIMHASH_PROJECTIONS];
} simhash_projection_t;

/* ========================================================================
 * SimHash 内部函数
 * ======================================================================== */

/**
 * @brief 生成随机投影向量
 */
static void generate_projection(float *v, int32_t dim, uint32_t seed) {
    /* 简单的伪随机生成 */
    srand(seed);
    for (int32_t i = 0; i < dim; i++) {
        v[i] = (float)((rand() % 2000) - 1000) / 1000.0f;
    }
}

/**
 * @brief 计算 SimHash
 *
 * 简化实现：使用随机投影法
 */
static uint64_t compute_simhash(const float *vector, int32_t dim, simhash_projection_t *projections) {
    if (vector == NULL || dim <= 0) return 0;

    uint64_t hash = 0;

    for (int32_t i = 0; i < SIMHASH_PROJECTIONS; i++) {
        /* 计算投影：dot(vector, projection) */
        float dot = 0.0f;
        for (int32_t j = 0; j < dim; j++) {
            dot += vector[j] * projections[i].v[j];
        }

        /* 根据投影方向设置对应位 */
        if (dot > 0) {
            hash |= (1ULL << i);
        }
    }

    return hash;
}

/**
 * @brief 计算两个向量的 L2 距离
 */
static float l2_distance(const float *a, const float *b, int32_t dim) {
    float dist = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * @brief 计算向量范数
 */
static float compute_norm(const float *vector, int32_t dim) {
    float norm = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        norm += vector[i] * vector[i];
    }
    return sqrtf(norm);
}

/* ========================================================================
 * Bloom Filter 内部函数
 * ======================================================================== */

/**
 * @brief 计算 Bloom Filter 大小（位数）
 *
 * 根据目标容量和假阳性率计算
 */
static int32_t calc_bloom_size(int32_t capacity, float fp_rate) {
    /* m = -n * ln(p) / (ln(2)^2) */
    double m = -((double)capacity * log(fp_rate)) / (0.48045);  /* ln(2)^2 ≈ 0.48045 */
    return (int32_t)(m / 8) + 8;  /* 转换为字节，再加一些余量 */
}

/**
 * @brief 计算 hash 函数数量
 *
 * k = (m/n) * ln(2)
 */
static int32_t calc_bloom_hash_count(int32_t bloom_size, int32_t capacity) {
    if (capacity <= 0) return 1;
    double k = ((double)bloom_size * 8 / capacity) * 0.693147;  /* ln(2) */
    return (int32_t)(k + 0.5);
}

/**
 * @brief 计算 hash 值
 */
static uint64_t bloom_hash(const char *data, size_t len, uint32_t seed) {
    /* 简化的 hash 函数 */
    uint64_t h = seed;
    for (size_t i = 0; i < len; i++) {
        h = h * 31 + (uint64_t)data[i];
    }
    return h;
}

/**
 * @brief Bloom Filter 检查
 */
static bool bloom_check(const uint64_t *bitset, int32_t size, uint64_t hash) {
    int32_t idx = (int32_t)(hash % (size * 8));
    return (bitset[idx / 64] & (1ULL << (idx % 64))) != 0;
}

/**
 * @brief Bloom Filter 添加
 */
static void bloom_add(uint64_t *bitset, int32_t size, uint64_t hash) {
    int32_t idx = (int32_t)(hash % (size * 8));
    bitset[idx / 64] |= (1ULL << (idx % 64));
}

/* ========================================================================
 * 生命周期 API 实现
 * ======================================================================== */

graph_dedup_t *graph_dedup_create(int32_t capacity,
                                  int32_t dims,
                                  float fp_rate,
                                  float threshold) {
    if (dims <= 0 || capacity <= 0) {
        LOG_ERROR("图去重器参数无效: dims=%d, capacity=%d", dims, capacity);
        return NULL;
    }

    if (fp_rate <= 0 || fp_rate >= 1) {
        fp_rate = GRAPH_DEDUP_DEFAULT_FP_RATE;
    }

    if (threshold < 0) {
        threshold = GRAPH_DEDUP_DEFAULT_THRESHOLD;
    }

    graph_dedup_t *dedup = (graph_dedup_t *)calloc(1, sizeof(graph_dedup_t));
    if (dedup == NULL) {
        LOG_ERROR("分配图去重器失败");
        return NULL;
    }

    /* 初始化字段 */
    dedup->dims = dims;
    dedup->threshold = threshold;
    dedup->count = 0;
    dedup->capacity = capacity;
    dedup->reverse_count = 0;
    dedup->reverse_capacity = GRAPH_DEDUP_REVERSE_INIT_CAPACITY;

    /* 分配 SimHash 数组 */
    dedup->fingerprints = (dedup_fingerprint_t *)malloc(
        (size_t)capacity * sizeof(dedup_fingerprint_t));
    if (dedup->fingerprints == NULL) {
        free(dedup);
        return NULL;
    }

    /* 分配精确向量存储 */
    dedup->exact_vectors = (float *)malloc(
        (size_t)capacity * dims * sizeof(float));
    if (dedup->exact_vectors == NULL) {
        free(dedup->fingerprints);
        free(dedup);
        return NULL;
    }

    /* 分配 Bloom Filter */
    dedup->bloom_size = calc_bloom_size(capacity, fp_rate);
    dedup->bloom_bitset = (uint64_t *)calloc(
        (size_t)(dedup->bloom_size / 8 + 1), sizeof(uint64_t));
    if (dedup->bloom_bitset == NULL) {
        free(dedup->fingerprints);
        free(dedup->exact_vectors);
        free(dedup);
        return NULL;
    }

    /* 分配反向映射 */
    dedup->reverse_map = (dedup_reverse_entry_t *)calloc(
        dedup->reverse_capacity, sizeof(dedup_reverse_entry_t));
    if (dedup->reverse_map == NULL) {
        free(dedup->fingerprints);
        free(dedup->exact_vectors);
        free(dedup->bloom_bitset);
        free(dedup);
        return NULL;
    }

    /* 初始化统计 */
    memset(&dedup->stats, 0, sizeof(dedup_stats_t));

    LOG_INFO("图去重器创建成功: capacity=%d, dims=%d, bloom_size=%d",
              capacity, dims, dedup->bloom_size);

    return dedup;
}

void graph_dedup_destroy(graph_dedup_t *dedup) {
    if (dedup == NULL) return;

    /* 释放反向映射中的数组 */
    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        free(dedup->reverse_map[i].heap_row_ids);
    }

    free(dedup->fingerprints);
    free(dedup->exact_vectors);
    free(dedup->bloom_bitset);
    free(dedup->reverse_map);
    free(dedup);
}

void graph_dedup_reset(graph_dedup_t *dedup) {
    if (dedup == NULL) return;

    dedup->count = 0;
    dedup->reverse_count = 0;

    /* 清空 Bloom Filter */
    if (dedup->bloom_bitset) {
        memset(dedup->bloom_bitset, 0,
               (size_t)(dedup->bloom_size / 8 + 1) * sizeof(uint64_t));
    }

    /* 清空统计 */
    memset(&dedup->stats, 0, sizeof(dedup_stats_t));
}

/* ========================================================================
 * 去重检测 API 实现
 * ======================================================================== */

/**
 * @brief 预生成的随机投影向量
 */
static simhash_projection_t g_projections[SIMHASH_PROJECTIONS];
static bool g_projections_init = false;

static void ensure_projections_init(int32_t dim) {
    if (g_projections_init) return;

    for (int32_t i = 0; i < SIMHASH_PROJECTIONS; i++) {
        generate_projection(g_projections[i].v, dim, (uint32_t)(i * 12345 + dim));
    }

    g_projections_init = true;
}

void graph_dedup_compute_fingerprint(graph_dedup_t *dedup,
                                     const float *vector,
                                     dedup_fingerprint_t *out_fingerprint) {
    if (dedup == NULL || vector == NULL || out_fingerprint == NULL) return;

    ensure_projections_init(dedup->dims);

    out_fingerprint->hash = compute_simhash(vector, dedup->dims, g_projections);
    out_fingerprint->norm = compute_norm(vector, dedup->dims);
}

bool graph_dedup_is_duplicate(graph_dedup_t *dedup, const float *vector) {
    if (dedup == NULL || vector == NULL) return false;

    dedup_fingerprint_t fp;
    graph_dedup_compute_fingerprint(dedup, vector, &fp);
    dedup->stats.total_checks++;

    /* 第一步：Bloom Filter 预检 */
    if (!bloom_check(dedup->bloom_bitset, dedup->bloom_size, fp.hash)) {
        /* 一定不重复 */
        return false;
    }

    dedup->stats.bloom_positive++;

    /* 第二步：精确比较 */
    for (int32_t i = 0; i < dedup->count; i++) {
        /* 快速排除：范数差异过大的一定不重复 */
        float norm_diff = fabsf(fp.norm - dedup->fingerprints[i].norm);
        if (norm_diff > dedup->threshold * 2) {
            continue;
        }

        /* 计算精确距离 */
        float *stored_vec = &dedup->exact_vectors[(size_t)i * dedup->dims];
        float dist = l2_distance(vector, stored_vec, dedup->dims);

        if (dist < dedup->threshold) {
            dedup->stats.duplicates_found++;
            return true;
        }
    }

    return false;
}

int graph_dedup_mark_inserted(graph_dedup_t *dedup,
                               const float *vector,
                               int32_t graph_node_id,
                               int32_t heap_row_id) {
    if (dedup == NULL || vector == NULL) return -1;

    /* 检查容量 */
    if (dedup->count >= dedup->capacity) {
        LOG_WARN("图去重器容量已满，需要扩展");
        return -1;
    }

    /* 计算指纹 */
    dedup_fingerprint_t fp;
    graph_dedup_compute_fingerprint(dedup, vector, &fp);

    /* 添加到 Bloom Filter */
    bloom_add(dedup->bloom_bitset, dedup->bloom_size, fp.hash);

    /* 存储 SimHash 和范数 */
    dedup->fingerprints[dedup->count] = fp;

    /* 存储精确向量 */
    float *dest = &dedup->exact_vectors[(size_t)dedup->count * dedup->dims];
    memcpy(dest, vector, (size_t)dedup->dims * sizeof(float));

    /* 添加反向映射 */
    dedup_reverse_entry_t *entry = &dedup->reverse_map[dedup->reverse_count];
    entry->graph_node_id = graph_node_id;
    entry->capacity = GRAPH_DEDUP_REVERSE_INIT_CAPACITY;
    entry->heap_row_ids = (int32_t *)malloc(
        (size_t)entry->capacity * sizeof(int32_t));
    entry->heap_row_count = 0;

    if (entry->heap_row_ids == NULL) {
        return -1;
    }

    entry->heap_row_ids[entry->heap_row_count++] = heap_row_id;
    dedup->reverse_count++;
    dedup->count++;
    dedup->stats.unique_inserted++;

    return 0;
}

int graph_dedup_mark_inserted_batch(graph_dedup_t *dedup,
                                     const float *vectors,
                                     const int32_t *graph_node_ids,
                                     const int32_t *heap_row_ids,
                                     int32_t count) {
    if (dedup == NULL || vectors == NULL || count <= 0) return -1;

    int32_t inserted = 0;
    for (int32_t i = 0; i < count; i++) {
        int32_t node_id = (graph_node_ids != NULL) ? graph_node_ids[i] : i;
        int32_t row_id = (heap_row_ids != NULL) ? heap_row_ids[i] : i;

        if (graph_dedup_mark_inserted(dedup,
                                      &vectors[(size_t)i * dedup->dims],
                                      node_id,
                                      row_id) == 0) {
            inserted++;
        }
    }

    return inserted;
}

/* ========================================================================
 * 反向映射查询 API 实现
 * ======================================================================== */

int graph_dedup_get_heap_rows(const graph_dedup_t *dedup,
                              int32_t graph_node_id,
                              int32_t *out_rows,
                              int32_t max_rows) {
    if (dedup == NULL || out_rows == NULL || max_rows <= 0) return -1;

    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        if (dedup->reverse_map[i].graph_node_id == graph_node_id) {
            int32_t count = dedup->reverse_map[i].heap_row_count;
            count = (count < max_rows) ? count : max_rows;
            memcpy(out_rows, dedup->reverse_map[i].heap_row_ids,
                   (size_t)count * sizeof(int32_t));
            return count;
        }
    }

    return 0;
}

int32_t graph_dedup_get_first_heap_row(const graph_dedup_t *dedup,
                                        int32_t graph_node_id) {
    if (dedup == NULL) return -1;

    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        if (dedup->reverse_map[i].graph_node_id == graph_node_id) {
            if (dedup->reverse_map[i].heap_row_count > 0) {
                return dedup->reverse_map[i].heap_row_ids[0];
            }
        }
    }

    return -1;
}

int graph_dedup_get_heap_row_count(const graph_dedup_t *dedup,
                                    int32_t graph_node_id) {
    if (dedup == NULL) return 0;

    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        if (dedup->reverse_map[i].graph_node_id == graph_node_id) {
            return dedup->reverse_map[i].heap_row_count;
        }
    }

    return 0;
}

/* ========================================================================
 * 持久化 API 实现
 * ======================================================================== */

int graph_dedup_save(const graph_dedup_t *dedup, const char *path) {
    if (dedup == NULL || path == NULL) return -1;

    FILE *fp = fopen(path, "wb");
    if (fp == NULL) {
        LOG_ERROR("无法创建去重状态文件: %s", path);
        return -1;
    }

    /* 写入魔数 */
    uint32_t magic = GRAPH_DEDUP_FILE_MAGIC;
    fwrite(&magic, sizeof(magic), 1, fp);

    /* 写入版本 */
    uint32_t version = GRAPH_DEDUP_FILE_VERSION;
    fwrite(&version, sizeof(version), 1, fp);

    /* 写入配置 */
    fwrite(&dedup->dims, sizeof(dedup->dims), 1, fp);
    fwrite(&dedup->threshold, sizeof(dedup->threshold), 1, fp);
    fwrite(&dedup->count, sizeof(dedup->count), 1, fp);

    /* 写入 SimHash 数据 */
    fwrite(dedup->fingerprints, sizeof(dedup_fingerprint_t), dedup->count, fp);

    /* 写入精确向量数据 */
    fwrite(dedup->exact_vectors, sizeof(float),
           (size_t)dedup->count * dedup->dims, fp);

    /* 写入反向映射 */
    fwrite(&dedup->reverse_count, sizeof(dedup->reverse_count), 1, fp);
    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        fwrite(&dedup->reverse_map[i].graph_node_id,
               sizeof(dedup->reverse_map[i].graph_node_id), 1, fp);
        fwrite(&dedup->reverse_map[i].heap_row_count,
               sizeof(dedup->reverse_map[i].heap_row_count), 1, fp);
        fwrite(dedup->reverse_map[i].heap_row_ids,
               sizeof(int32_t), dedup->reverse_map[i].heap_row_count, fp);
    }

    /* 写入统计 */
    fwrite(&dedup->stats, sizeof(dedup_stats_t), 1, fp);

    fclose(fp);
    LOG_INFO("图去重状态已保存: %s, count=%d", path, dedup->count);

    return 0;
}

graph_dedup_t *graph_dedup_load(const char *path) {
    if (path == NULL) return NULL;

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        LOG_INFO("去重状态文件不存在: %s", path);
        return NULL;
    }

    /* 读取并验证魔数 */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, fp) != 1 || magic != GRAPH_DEDUP_FILE_MAGIC) {
        LOG_WARN("去重状态文件魔数无效");
        fclose(fp);
        return NULL;
    }

    /* 读取版本 */
    uint32_t version;
    if (fread(&version, sizeof(version), 1, fp) != 1 || version != GRAPH_DEDUP_FILE_VERSION) {
        LOG_WARN("去重状态文件版本不匹配");
        fclose(fp);
        return NULL;
    }

    /* 读取配置 */
    int32_t dims, count;
    float threshold;
    fread(&dims, sizeof(dims), 1, fp);
    fread(&threshold, sizeof(threshold), 1, fp);
    fread(&count, sizeof(count), 1, fp);

    /* 创建去重器 */
    graph_dedup_t *dedup = graph_dedup_create(count, dims, GRAPH_DEDUP_DEFAULT_FP_RATE, threshold);
    if (dedup == NULL) {
        fclose(fp);
        return NULL;
    }

    /* 读取 SimHash 数据 */
    fread(dedup->fingerprints, sizeof(dedup_fingerprint_t), count, fp);
    dedup->count = count;

    /* 读取精确向量数据 */
    fread(dedup->exact_vectors, sizeof(float), (size_t)count * dims, fp);

    /* 读取反向映射 */
    fread(&dedup->reverse_count, sizeof(dedup->reverse_count), 1, fp);
    for (int32_t i = 0; i < dedup->reverse_count; i++) {
        fread(&dedup->reverse_map[i].graph_node_id,
              sizeof(dedup->reverse_map[i].graph_node_id), 1, fp);
        fread(&dedup->reverse_map[i].heap_row_count,
              sizeof(dedup->reverse_map[i].heap_row_count), 1, fp);

        dedup->reverse_map[i].capacity = dedup->reverse_map[i].heap_row_count + 4;
        dedup->reverse_map[i].heap_row_ids = (int32_t *)malloc(
            (size_t)dedup->reverse_map[i].capacity * sizeof(int32_t));
        fread(dedup->reverse_map[i].heap_row_ids,
              sizeof(int32_t), dedup->reverse_map[i].heap_row_count, fp);
    }

    /* 读取统计 */
    fread(&dedup->stats, sizeof(dedup_stats_t), 1, fp);

    fclose(fp);
    LOG_INFO("图去重状态已加载: path=%s, count=%d", path, count);

    return dedup;
}

/* ========================================================================
 * 统计 API 实现
 * ======================================================================== */

void graph_dedup_get_stats(const graph_dedup_t *dedup, dedup_stats_t *stats) {
    if (dedup == NULL || stats == NULL) return;
    *stats = dedup->stats;
}

void graph_dedup_reset_stats(graph_dedup_t *dedup) {
    if (dedup == NULL) return;
    memset(&dedup->stats, 0, sizeof(dedup_stats_t));
}

float graph_dedup_get_dedup_rate(const graph_dedup_t *dedup) {
    if (dedup == NULL) return 0.0f;

    uint64_t total = dedup->stats.total_checks;
    if (total == 0) return 0.0f;

    return (float)dedup->stats.duplicates_found / (float)total * 100.0f;
}

/* ========================================================================
 * 工具函数实现
 * ======================================================================== */

float graph_dedup_compute_norm(const float *vector, int32_t dim) {
    return compute_norm(vector, dim);
}

float graph_dedup_l2_distance(const float *a, const float *b, int32_t dim) {
    return l2_distance(a, b, dim);
}

int32_t graph_dedup_find_min_distance(const graph_dedup_t *dedup,
                                       const float *vector,
                                       float *out_min_dist) {
    if (dedup == NULL || vector == NULL || out_min_dist == NULL) return -1;

    *out_min_dist = FLT_MAX;
    int32_t min_idx = -1;

    for (int32_t i = 0; i < dedup->count; i++) {
        float *stored_vec = &dedup->exact_vectors[(size_t)i * dedup->dims];
        float dist = l2_distance(vector, stored_vec, dedup->dims);

        if (dist < *out_min_dist) {
            *out_min_dist = dist;
            min_idx = i;
        }
    }

    return min_idx;
}

int graph_dedup_reserve(graph_dedup_t *dedup, int32_t new_capacity) {
    if (dedup == NULL || new_capacity <= dedup->capacity) return 0;

    dedup_fingerprint_t *new_fingerprints = (dedup_fingerprint_t *)realloc(
        dedup->fingerprints, (size_t)new_capacity * sizeof(dedup_fingerprint_t));
    if (new_fingerprints == NULL) return -1;
    dedup->fingerprints = new_fingerprints;

    float *new_vectors = (float *)realloc(
        dedup->exact_vectors, (size_t)new_capacity * dedup->dims * sizeof(float));
    if (new_vectors == NULL) return -1;
    dedup->exact_vectors = new_vectors;

    dedup->capacity = new_capacity;

    return 0;
}
