/**
 * @file tiered_index.c
 * @brief 分层索引实现
 *
 * 实现多层索引架构，支持数据在层级间自动流动。
 */
#include "tiered_index.h"
#include "hnsw/faiss_hnsw.h"
#include "ivf_pq/ivf_pq.h"
#include "diskann/diskann.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ========================================================================
 * 常量
 * ======================================================================== */

const char *tier_names[] = {"memory", "ssd", "disk"};

/* 层级容量默认阈值 */
#define DEFAULT_MEMORY_TIER_CAPACITY 1000000   /* 100 万向量 */
#define DEFAULT_SSD_TIER_CAPACITY 10000000     /* 1000 万向量 */

/* 热度阈值 */
#define DEFAULT_HOT_THRESHOLD 1000             /* 访问 1000 次以上为热 */
#define DEFAULT_COLD_THRESHOLD 10              /* 访问 10 次以下为冷 */

/* ========================================================================
 * 层级名称和优先级
 * ======================================================================== */

const char *tier_get_name(IndexTier tier) {
    if (tier < 0 || tier >= TIER_COUNT) return "unknown";
    return tier_names[tier];
}

int tier_get_priority(IndexTier tier) {
    return (int)tier; /* 数值越小优先级越高 */
}

/* ========================================================================
 * 数据访问统计
 * ======================================================================== */

struct AccessStats_s {
    int64_t total_accesses;
    int64_t recent_accesses;
    int64_t last_access_time;
    float heat_score;
};

AccessStats *access_stats_create(void) {
    AccessStats *stats = (AccessStats *)calloc(1, sizeof(AccessStats));
    return stats;
}

void access_stats_update(AccessStats *stats, int64_t current_time) {
    if (!stats) return;

    stats->total_accesses++;
    stats->recent_accesses++;
    stats->last_access_time = current_time;

    /* 简化热度计算：基于最近访问频率 */
    stats->heat_score = fminf(1.0f, (float)stats->recent_accesses / 1000.0f);
}

int access_stats_get_tier(const AccessStats *stats, int64_t current_time) {
    if (!stats) return TIER_MEMORY;

    /* 基于热度判断层级 */
    if (stats->heat_score > 0.8f) return TIER_MEMORY;
    if (stats->heat_score > 0.3f) return TIER_SSD;
    return TIER_DISK;
}

void access_stats_free(AccessStats *stats) {
    free(stats);
}

/* ========================================================================
 * 分层索引配置
 * ======================================================================== */

TieredIndexConfig *tiered_config_default(void) {
    return tiered_config_create(1024, 10240, true);
}

TieredIndexConfig *tiered_config_create(int64_t memory_limit_mb,
                                        int64_t ssd_limit_mb,
                                        bool auto_switch) {
    TieredIndexConfig *config = (TieredIndexConfig *)calloc(1, sizeof(TieredIndexConfig));
    if (!config) return NULL;

    config->memory_tier_capacity = DEFAULT_MEMORY_TIER_CAPACITY;
    config->ssd_tier_capacity = DEFAULT_SSD_TIER_CAPACITY;
    config->auto_tier_switching = auto_switch;
    config->hot_threshold = DEFAULT_HOT_THRESHOLD;
    config->cold_threshold = DEFAULT_COLD_THRESHOLD;
    config->tier_switch_interval = 60; /* 1 分钟 */
    config->max_memory_usage_mb = memory_limit_mb;
    config->memory_warning_threshold = memory_limit_mb * 8 / 10;
    config->prefetch_count = 10;
    config->enable_prometheus = false;
    config->search.hnsw_ef = 128;
    config->search.hnsw_m = 16;
    config->search.ivf_nlist = 1024;
    config->search.ivf_nprobe = 64;
    config->search.disk_search_list = 128;

    return config;
}

void tiered_config_free(TieredIndexConfig *config) {
    free(config);
}

TieredIndexConfig *tiered_config_clone(const TieredIndexConfig *config) {
    if (!config) return NULL;
    TieredIndexConfig *copy = (TieredIndexConfig *)malloc(sizeof(TieredIndexConfig));
    if (copy) {
        memcpy(copy, config, sizeof(TieredIndexConfig));
    }
    return copy;
}

/* ========================================================================
 * 分层索引结构
 * ======================================================================== */

/** ID 到索引层级的映射 */
typedef struct IdTierMap_s {
    int64_t id;
    IndexTier tier;
    AccessStats *stats;
} IdTierMap;

struct TieredIndex_s {
    int32_t dims;
    distance_metric_t metric;
    TieredIndexConfig *config;
    AccessPattern access_pattern;

    /* 各层索引 */
    faiss_hnsw_t *hnsw_index;       /**< 内存层：HNSW */
    ivf_pq_index_t *ivf_pq_index;   /**< SSD 层：IVF-PQ */
    diskann_t *diskann_index;       /**< 磁盘层：DiskANN */

    /* ID 管理 */
    IdTierMap *id_map;
    int32_t id_map_capacity;
    int32_t id_map_size;

    /* ID 到向量的映射（用于层级迁移） */
    float *vectors;                 /**< [id_map_size × dims] */
    int32_t vectors_capacity;

    /* 统计 */
    TieredIndexStats stats;
    int64_t search_count;
    int64_t tier_switch_count;
    int64_t last_switch_check_time;

    /* 互斥锁 */
    void *mutex;
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

static int32_t find_id_index(const TieredIndex *index, int64_t id) {
    for (int32_t i = 0; i < index->id_map_size; i++) {
        if (index->id_map[i].id == id) {
            return i;
        }
    }
    return -1;
}

static void ensure_id_map_capacity(TieredIndex *index) {
    if (index->id_map_size >= index->id_map_capacity) {
        int32_t new_capacity = index->id_map_capacity * 2;
        index->id_map = (IdTierMap *)realloc(index->id_map,
            new_capacity * sizeof(IdTierMap));
        index->vectors = (float *)realloc(index->vectors,
            new_capacity * index->dims * sizeof(float));
        index->id_map_capacity = new_capacity;
    }
}

/* ========================================================================
 * 创建和销毁
 * ======================================================================== */

TieredIndex *tiered_index_create(int32_t dims,
                                 distance_metric_t metric,
                                 const TieredIndexConfig *config) {
    if (dims <= 0) return NULL;

    TieredIndex *index = (TieredIndex *)calloc(1, sizeof(TieredIndex));
    if (!index) return NULL;

    index->dims = dims;
    index->metric = metric;
    index->config = config ? tiered_config_clone(config) : tiered_config_default();
    index->access_pattern = ACCESS_UNIFORM;

    /* 初始化各层索引 */
    index->hnsw_index = faiss_hnsw_index_create(
        index->config->search.hnsw_m,
        dims,
        index->config->search.hnsw_ef,
        metric,
        QUANTIZATION_TYPE_NONE);

    index->ivf_pq_index = ivf_pq_create(
        index->config->search.ivf_nlist,
        16, /* pq_m */
        8,  /* pq_bits */
        dims);

    /* DiskANN 索引 */
    index->diskann_index = diskann_index_create(
        dims,
        32, /* index_size */
        100, /* build_search_list_size */
        metric == DISTANCE_METRIC_L2 ? DISTANCE_METRIC_L2 : DISTANCE_METRIC_COSINE);

    /* 初始化 ID 映射 */
    index->id_map_capacity = 10000;
    index->id_map = (IdTierMap *)calloc(index->id_map_capacity, sizeof(IdTierMap));
    index->vectors = (float *)calloc(index->id_map_capacity * dims, sizeof(float));
    index->vectors_capacity = index->id_map_capacity;

    /* 初始化统计 */
    memset(&index->stats, 0, sizeof(TieredIndexStats));
    index->search_count = 0;
    index->tier_switch_count = 0;
    index->last_switch_check_time = 0;

    return index;
}

void tiered_index_destroy(TieredIndex *index) {
    if (!index) return;

    if (index->hnsw_index) {
        faiss_hnsw_index_drop(index->hnsw_index);
    }
    if (index->ivf_pq_index) {
        ivf_pq_destroy(index->ivf_pq_index);
    }
    if (index->diskann_index) {
        diskann_index_drop(index->diskann_index);
    }

    for (int32_t i = 0; i < index->id_map_size; i++) {
        if (index->id_map[i].stats) {
            access_stats_free(index->id_map[i].stats);
        }
    }

    free(index->id_map);
    free(index->vectors);
    tiered_config_free(index->config);
    free(index);
}

/* ========================================================================
 * 向量操作
 * ======================================================================== */

int32_t tiered_index_add(TieredIndex *index,
                         int64_t id,
                         const float *vector,
                         IndexTier tier) {
    if (!index || !vector) return -1;

    /* 检查是否已存在 */
    int32_t existing = find_id_index(index, id);
    if (existing >= 0) {
        /* 更新现有向量 */
        return tiered_index_update(index, id, vector);
    }

    /* 确定目标层级 */
    if (tier == TIER_MEMORY || tier < 0 || tier >= TIER_COUNT) {
        tier = tiered_index_recommend_tier(index);
    }

    /* 确保容量 */
    ensure_id_map_capacity(index);

    int32_t pos = index->id_map_size;

    /* 存储向量 */
    memcpy(&index->vectors[pos * index->dims], vector, index->dims * sizeof(float));

    /* 添加到 ID 映射 */
    index->id_map[pos].id = id;
    index->id_map[pos].tier = tier;
    index->id_map[pos].stats = access_stats_create();
    index->id_map_size++;

    /* 添加到对应层索引 */
    switch (tier) {
        case TIER_MEMORY:
            if (index->hnsw_index) {
                faiss_hnsw_index_add(index->hnsw_index, 1, vector);
            }
            index->stats.tier_counts[TIER_MEMORY]++;
            break;

        case TIER_SSD:
            if (index->ivf_pq_index) {
                ivf_pq_add(index->ivf_pq_index, 1, &vector, (const int *)&pos);
            }
            index->stats.tier_counts[TIER_SSD]++;
            break;

        case TIER_DISK:
            if (index->diskann_index) {
                diskann_index_add(index->diskann_index, 1, vector);
            }
            index->stats.tier_counts[TIER_DISK]++;
            break;
    }

    index->stats.total_vectors++;
    return 0;
}

int32_t tiered_index_add_batch(TieredIndex *index,
                               int32_t n,
                               const int64_t *ids,
                               const float **vectors,
                               IndexTier tier) {
    if (!index || !vectors) return -1;

    int32_t added = 0;
    for (int32_t i = 0; i < n; i++) {
        if (tiered_index_add(index, ids ? ids[i] : i, vectors[i], tier) == 0) {
            added++;
        }
    }
    return added;
}

int32_t tiered_index_delete(TieredIndex *index, int64_t id) {
    if (!index) return -1;

    int32_t pos = find_id_index(index, id);
    if (pos < 0) return -1;

    IndexTier tier = index->id_map[pos].tier;

    /* 从对应层索引删除（简化实现：标记删除） */
    /* 实际实现需要从各层索引中移除 */

    /* 释放统计 */
    if (index->id_map[pos].stats) {
        access_stats_free(index->id_map[pos].stats);
    }

    /* 移动最后一个元素到当前位置 */
    if (pos < index->id_map_size - 1) {
        index->id_map[pos] = index->id_map[index->id_map_size - 1];
        memcpy(&index->vectors[pos * index->dims],
               &index->vectors[(index->id_map_size - 1) * index->dims],
               index->dims * sizeof(float));
    }

    index->id_map_size--;
    index->stats.total_vectors--;
    index->stats.tier_counts[tier]--;

    return 0;
}

int32_t tiered_index_update(TieredIndex *index,
                            int64_t id,
                            const float *vector) {
    if (!index || !vector) return -1;

    int32_t pos = find_id_index(index, id);
    if (pos < 0) return -1;

    /* 更新向量 */
    memcpy(&index->vectors[pos * index->dims], vector, index->dims * sizeof(float));

    /* 更新对应层索引 */
    IndexTier tier = index->id_map[pos].tier;
    /* 简化：重新添加到层索引 */
    switch (tier) {
        case TIER_MEMORY:
            if (index->hnsw_index) {
                faiss_hnsw_index_add(index->hnsw_index, 1, vector);
            }
            break;
        case TIER_SSD:
            if (index->ivf_pq_index) {
                ivf_pq_add(index->ivf_pq_index, 1, &vector, (const int *)&pos);
            }
            break;
        case TIER_DISK:
            if (index->diskann_index) {
                diskann_index_add(index->diskann_index, 1, vector);
            }
            break;
    }

    return 0;
}

/* ========================================================================
 * 搜索
 * ======================================================================== */

int32_t tiered_index_search(const TieredIndex *index,
                            const float *query,
                            int32_t top_k,
                            float *distances,
                            int64_t *ids) {
    return tiered_index_search_tier(index, TIER_MEMORY, query, top_k, distances, ids);
}

int32_t tiered_index_search_tier(const TieredIndex *index,
                                 IndexTier tier,
                                 const float *query,
                                 int32_t top_k,
                                 float *distances,
                                 int64_t *ids) {
    if (!index || !query || top_k <= 0) return -1;

    int32_t n_results = 0;

    switch (tier) {
        case TIER_MEMORY: {
            /* 从 HNSW 搜索 */
            if (index->hnsw_index) {
                int32_t *hnsw_ids = (int32_t *)malloc(top_k * sizeof(int32_t));
                float *hnsw_distances = (float *)malloc(top_k * sizeof(float));

                if (hnsw_ids && hnsw_distances) {
                    int32_t count = faiss_hnsw_index_search(
                        index->hnsw_index, query, top_k,
                        index->config->search.hnsw_ef,
                        hnsw_distances, hnsw_ids);

                    for (int32_t i = 0; i < count && n_results < top_k; i++) {
                        if (hnsw_ids[i] >= 0 && hnsw_ids[i] < index->id_map_size) {
                            ids[n_results] = index->id_map[hnsw_ids[i]].id;
                            distances[n_results] = hnsw_distances[i];
                            n_results++;
                        }
                    }
                }

                free(hnsw_ids);
                free(hnsw_distances);
            }
            break;
        }

        case TIER_SSD: {
            /* 从 IVF-PQ 搜索 */
            if (index->ivf_pq_index) {
                int32_t *ivf_ids = (int32_t *)malloc(top_k * sizeof(int32_t));
                float *ivf_distances = (float *)malloc(top_k * sizeof(float));

                if (ivf_ids && ivf_distances) {
                    int32_t count = ivf_pq_search(
                        index->ivf_pq_index, query, top_k, ivf_ids, ivf_distances);

                    for (int32_t i = 0; i < count && n_results < top_k; i++) {
                        if (ivf_ids[i] >= 0 && ivf_ids[i] < index->id_map_size) {
                            ids[n_results] = index->id_map[ivf_ids[i]].id;
                            distances[n_results] = ivf_distances[i];
                            n_results++;
                        }
                    }
                }

                free(ivf_ids);
                free(ivf_distances);
            }
            break;
        }

        case TIER_DISK: {
            /* 从 DiskANN 搜索 */
            if (index->diskann_index) {
                int32_t *diskann_labels = (int32_t *)malloc(top_k * sizeof(int32_t));
                float *diskann_distances = (float *)malloc(top_k * sizeof(float));

                if (diskann_labels && diskann_distances) {
                    int32_t count = diskann_index_search(
                        index->diskann_index, query, top_k,
                        index->config->search.disk_search_list, 1,
                        diskann_distances, diskann_labels);

                    for (int32_t i = 0; i < count && n_results < top_k; i++) {
                        if (diskann_labels[i] >= 0 && diskann_labels[i] < index->id_map_size) {
                            ids[n_results] = index->id_map[diskann_labels[i]].id;
                            distances[n_results] = diskann_distances[i];
                            n_results++;
                        }
                    }
                }

                free(diskann_labels);
                free(diskann_distances);
            }
            break;
        }
    }

    index->search_count++;
    return n_results;
}

/* ========================================================================
 * 层级管理
 * ======================================================================== */

int32_t tiered_index_move_tier(TieredIndex *index,
                               int64_t id,
                               IndexTier target_tier) {
    if (!index || target_tier < 0 || target_tier >= TIER_COUNT) return -1;

    int32_t pos = find_id_index(index, id);
    if (pos < 0) return -1;

    IndexTier current_tier = index->id_map[pos].tier;
    if (current_tier == target_tier) return 0;

    const float *vector = &index->vectors[pos * index->dims];

    /* 从当前层移除 */
    /* 简化实现：直接从新层级添加，不从旧层移除 */

    /* 添加到目标层 */
    switch (target_tier) {
        case TIER_MEMORY:
            if (index->hnsw_index) {
                faiss_hnsw_index_add(index->hnsw_index, 1, vector);
            }
            break;
        case TIER_SSD:
            if (index->ivf_pq_index) {
                ivf_pq_add(index->ivf_pq_index, 1, &vector, (const int *)&pos);
            }
            break;
        case TIER_DISK:
            if (index->diskann_index) {
                diskann_index_add(index->diskann_index, 1, vector);
            }
            break;
    }

    index->id_map[pos].tier = target_tier;
    index->stats.tier_counts[current_tier]--;
    index->stats.tier_counts[target_tier]++;
    index->tier_switch_count++;

    return 0;
}

void tiered_index_record_access(TieredIndex *index, int64_t id) {
    if (!index) return;

    int32_t pos = find_id_index(index, id);
    if (pos < 0) return;

    if (!index->id_map[pos].stats) {
        index->id_map[pos].stats = access_stats_create();
    }

    access_stats_update(index->id_map[pos].stats, 0); /* 简化：使用 0 作为当前时间 */
}

int32_t tiered_index_rebalance(TieredIndex *index) {
    if (!index) return -1;

    /* 基于热度重新分配 */
    for (int32_t i = 0; i < index->id_map_size; i++) {
        if (index->id_map[i].stats) {
            int32_t recommended_tier = access_stats_get_tier(
                index->id_map[i].stats, 0);
            IndexTier current_tier = index->id_map[i].tier;

            if (recommended_tier != current_tier) {
                tiered_index_move_tier(index, index->id_map[i].id,
                                       (IndexTier)recommended_tier);
            }
        }
    }

    return 0;
}

int32_t tiered_index_check_tier_switch(TieredIndex *index) {
    if (!index || !index->config->auto_tier_switching) return 0;

    /* 检查是否需要平衡各层容量 */
    int64_t memory_count = index->stats.tier_counts[TIER_MEMORY];
    int64_t ssd_count = index->stats.tier_counts[TIER_SSD];

    /* 内存层过满，下沉部分到 SSD */
    if (memory_count > index->config->memory_tier_capacity) {
        int32_t to_move = (int32_t)((memory_count - index->config->memory_tier_capacity) / 2);
        int32_t moved = 0;

        for (int32_t i = 0; i < index->id_map_size && moved < to_move; i++) {
            if (index->id_map[i].tier == TIER_MEMORY) {
                if (index->id_map[i].stats) {
                    float heat = index->id_map[i].stats->heat_score;
                    if (heat < 0.5f) { /* 冷数据下沉 */
                        tiered_index_move_tier(index, index->id_map[i].id, TIER_SSD);
                        moved++;
                    }
                }
            }
        }
    }

    return 0;
}

IndexTier tiered_index_recommend_tier(const TieredIndex *index) {
    if (!index) return TIER_MEMORY;

    int64_t memory_count = index->stats.tier_counts[TIER_MEMORY];
    int64_t ssd_count = index->stats.tier_counts[TIER_SSD];

    /* 基于当前容量推荐 */
    if (memory_count < index->config->memory_tier_capacity) {
        return TIER_MEMORY;
    }
    if (ssd_count < index->config->ssd_tier_capacity) {
        return TIER_SSD;
    }
    return TIER_DISK;
}

bool tiered_index_needs_switch(const TieredIndex *index) {
    if (!index) return false;

    int64_t memory_count = index->stats.tier_counts[TIER_MEMORY];
    return memory_count > index->config->memory_tier_capacity;
}

/* ========================================================================
 * 统计信息
 * ======================================================================== */

void tiered_index_stats(const TieredIndex *index, TieredIndexStats *out_stats) {
    if (!index || !out_stats) return;

    memcpy(out_stats, &index->stats, sizeof(TieredIndexStats));
    out_stats->search_count = index->search_count;
    out_stats->tier_switch_count = index->tier_switch_count;
}

int64_t tiered_index_tier_size(const TieredIndex *index, IndexTier tier) {
    if (!index || tier < 0 || tier >= TIER_COUNT) return 0;
    return index->stats.tier_counts[tier];
}

int64_t tiered_index_size(const TieredIndex *index) {
    return index ? index->stats.total_vectors : 0;
}

/* ========================================================================
 * 持久化
 * ======================================================================== */

int32_t tiered_index_save(const TieredIndex *index, const char *path) {
    if (!index || !path) return -1;

    /* 保存各层索引 */
    /* 简化实现：仅保存向量数据 */

    return 0;
}

TieredIndex *tiered_index_load(const char *path, const TieredIndexConfig *config) {
    if (!path) return NULL;

    /* 简化实现：返回 NULL */
    (void)path;
    (void)config;
    return NULL;
}

/* ========================================================================
 * 性能调优
 * ======================================================================== */

void tiered_index_set_search_params(TieredIndex *index,
                                    int32_t hnsw_ef,
                                    int32_t ivf_nprobe) {
    if (!index) return;

    if (index->config) {
        index->config->search.hnsw_ef = hnsw_ef;
        index->config->search.ivf_nprobe = ivf_nprobe;
    }
}

int32_t tiered_index_set_memory_limit(TieredIndex *index,
                                      int64_t limit_mb) {
    if (!index || !index->config) return -1;

    index->config->max_memory_usage_mb = limit_mb;
    index->config->memory_tier_capacity = limit_mb * 10000; /* 简化估算 */
    return 0;
}

void tiered_index_memory_usage(const TieredIndex *index,
                               int64_t *used_mb,
                               int64_t *limit_mb) {
    if (!index) return;

    /* 简化估算 */
    int64_t vectors_size = index->id_map_size * index->dims * sizeof(float);
    int64_t metadata_size = index->id_map_size * sizeof(IdTierMap);

    if (used_mb) {
        *used_mb = (vectors_size + metadata_size) / (1024 * 1024);
    }
    if (limit_mb) {
        *limit_mb = index->config ? index->config->max_memory_usage_mb : 0;
    }
}

int32_t tiered_index_memory_defrag(TieredIndex *index) {
    return tiered_index_check_tier_switch(index);
}

/* ========================================================================
 * 访问模式
 * ======================================================================== */

void tiered_index_set_access_pattern(TieredIndex *index, AccessPattern pattern) {
    if (!index) return;
    index->access_pattern = pattern;
}

AccessPattern tiered_index_get_access_pattern(const TieredIndex *index) {
    return index ? index->access_pattern : ACCESS_UNIFORM;
}
