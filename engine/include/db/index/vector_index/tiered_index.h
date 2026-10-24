/**
 * @file tiered_index.h
 * @brief 分层索引管理头文件
 *
 * 实现多层索引架构：
 * 1. L0: 内存索引（HNSW）- 热数据
 * 2. L1: SSD 索引（IVF-PQ）- 温数据
 * 3. L2: 对象存储索引（DiskANN）- 冷数据
 *
 * 支持自动根据数据规模和访问频率切换索引层。
 */
#ifndef DB_TIERED_INDEX_H
#define DB_TIERED_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#include <algo-prod/distance/distance.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 索引层级
 * ======================================================================== */

/** 索引层级 */
typedef enum IndexTier_e {
    TIER_MEMORY = 0,   /**< 内存层：HNSW */
    TIER_SSD = 1,      /**< SSD 层：IVF-PQ */
    TIER_DISK = 2,     /**< 磁盘层：DiskANN */
    TIER_COUNT = 3     /**< 层级数量 */
} IndexTier;

/** 索引层级名称 */
extern const char *tier_names[];

/**
 * @brief 获取层级名称
 */
const char *tier_get_name(IndexTier tier);

/**
 * @brief 获取层级优先级（数值越小优先级越高）
 */
int tier_get_priority(IndexTier tier);

/* ========================================================================
 * 数据访问统计
 * ======================================================================== */

/** 数据访问统计 */
typedef struct AccessStats_s {
    int64_t total_accesses;    /**< 总访问次数 */
    int64_t recent_accesses;   /**< 最近窗口内访问次数 */
    int64_t last_access_time;  /**< 上次访问时间戳 */
    float heat_score;          /**< 热度分数 [0, 1] */
} AccessStats;

/**
 * @brief 创建访问统计
 */
AccessStats *access_stats_create(void);

/**
 * @brief 更新访问统计
 */
void access_stats_update(AccessStats *stats, int64_t current_time);

/**
 * @brief 获取热度等级
 */
int access_stats_get_tier(const AccessStats *stats, int64_t current_time);

/**
 * @brief 释放访问统计
 */
void access_stats_free(AccessStats *stats);

/* ========================================================================
 * 分层索引配置
 * ======================================================================== */

/** 分层索引配置 */
typedef struct TieredIndexConfig_s {
    /* 各层容量阈值 */
    int64_t memory_tier_capacity;    /**< 内存层最大容量 */
    int64_t ssd_tier_capacity;       /**< SSD 层最大容量 */

    /* 自动切换配置 */
    bool auto_tier_switching;        /**< 是否启用自动层级切换 */
    int64_t hot_threshold;           /**< 热数据阈值（访问次数） */
    int64_t cold_threshold;          /**< 冷数据阈值 */
    int32_t tier_switch_interval;    /**< 层级切换检查间隔（秒） */

    /* 内存管理 */
    int64_t max_memory_usage_mb;     /**< 最大内存使用 (MB) */
    int64_t memory_warning_threshold; /**< 内存警告阈值 */

    /* 性能配置 */
    int32_t prefetch_count;          /**< 预取数量 */
    bool enable_prometheus;          /**< 是否启用性能指标 */

    /* 搜索配置 */
    struct {
        int32_t hnsw_ef;             /**< HNSW ef 参数 */
        int32_t hnsw_m;              /**< HNSW M 参数 */
        int32_t ivf_nlist;           /**< IVF nlist 参数 */
        int32_t ivf_nprobe;          /**< IVF nprobe 参数 */
        int32_t disk_search_list;    /**< DiskANN 搜索列表大小 */
    } search;
} TieredIndexConfig;

/**
 * @brief 创建默认分层索引配置
 */
TieredIndexConfig *tiered_config_default(void);

/**
 * @brief 创建分层索引配置
 */
TieredIndexConfig *tiered_config_create(int64_t memory_limit_mb,
                                        int64_t ssd_limit_mb,
                                        bool auto_switch);

/**
 * @brief 释放分层索引配置
 */
void tiered_config_free(TieredIndexConfig *config);

/**
 * @brief 克隆分层索引配置
 */
TieredIndexConfig *tiered_config_clone(const TieredIndexConfig *config);

/* ========================================================================
 * 分层索引
 * ======================================================================== */

/** 分层索引（不透明类型） */
typedef struct TieredIndex_s TieredIndex;

/**
 * @brief 创建分层索引
 *
 * @param dims 向量维度
 * @param metric 距离度量
 * @param config 配置（可为 NULL）
 * @return 索引句柄，失败返回 NULL
 */
TieredIndex *tiered_index_create(int32_t dims,
                                 distance_metric_t metric,
                                 const TieredIndexConfig *config);

/**
 * @brief 添加向量
 *
 * @param index 分层索引
 * @param id 向量 ID
 * @param vector 向量数据
 * @param tier 目标层级（TIER_MEMORY 表示自动选择）
 * @return 0 成功，-1 失败
 */
int32_t tiered_index_add(TieredIndex *index,
                         int64_t id,
                         const float *vector,
                         IndexTier tier);

/**
 * @brief 批量添加向量
 */
int32_t tiered_index_add_batch(TieredIndex *index,
                               int32_t n,
                               const int64_t *ids,
                               const float **vectors,
                               IndexTier tier);

/**
 * @brief 搜索
 *
 * @param index 分层索引
 * @param query 查询向量
 * @param top_k 返回数量
 * @param distances 输出距离数组
 * @param ids 输出 ID 数组
 * @return 实际返回数量，-1 失败
 */
int32_t tiered_index_search(const TieredIndex *index,
                            const float *query,
                            int32_t top_k,
                            float *distances,
                            int64_t *ids);

/**
 * @brief 指定层级搜索
 */
int32_t tiered_index_search_tier(const TieredIndex *index,
                                 IndexTier tier,
                                 const float *query,
                                 int32_t top_k,
                                 float *distances,
                                 int64_t *ids);

/**
 * @brief 删除向量
 */
int32_t tiered_index_delete(TieredIndex *index, int64_t id);

/**
 * @brief 移动向量到不同层级
 */
int32_t tiered_index_move_tier(TieredIndex *index,
                               int64_t id,
                               IndexTier target_tier);

/**
 * @brief 更新向量
 */
int32_t tiered_index_update(TieredIndex *index,
                            int64_t id,
                            const float *vector);

/**
 * @brief 记录向量访问
 *
 * 用于热度统计和自动层级切换
 */
void tiered_index_record_access(TieredIndex *index, int64_t id);

/**
 * @brief 触发层级平衡
 *
 * 根据当前数据分布和访问模式，将数据在不同层级间移动
 */
int32_t tiered_index_rebalance(TieredIndex *index);

/**
 * @brief 手动触发层级切换检查
 */
int32_t tiered_index_check_tier_switch(TieredIndex *index);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** 索引统计信息 */
typedef struct TieredIndexStats_s {
    int64_t total_vectors;         /**< 总向量数 */
    int64_t tier_counts[TIER_COUNT]; /**< 各层向量数量 */
    int64_t tier_sizes[TIER_COUNT];  /**< 各层存储大小 */
    int64_t memory_usage_bytes;    /**< 当前内存使用 */
    int64_t search_count;          /**< 搜索次数 */
    int64_t tier_switch_count;     /**< 层级切换次数 */
    double avg_search_latency_ms;  /**< 平均搜索延迟 */
    double tier_hit_rates[TIER_COUNT]; /**< 各层命中率 */
} TieredIndexStats;

/**
 * @brief 获取索引统计
 */
void tiered_index_stats(const TieredIndex *index, TieredIndexStats *out_stats);

/**
 * @brief 获取层级大小
 */
int64_t tiered_index_tier_size(const TieredIndex *index, IndexTier tier);

/**
 * @brief 获取总向量数
 */
int64_t tiered_index_size(const TieredIndex *index);

/**
 * @brief 检查层级是否需要切换
 */
bool tiered_index_needs_switch(const TieredIndex *index);

/**
 * @brief 获取推荐插入层级
 */
IndexTier tiered_index_recommend_tier(const TieredIndex *index);

/* ========================================================================
 * 持久化
 * ======================================================================== */

/**
 * @brief 保存分层索引
 *
 * @param index 分层索引
 * @param path 基础路径
 * @return 0 成功，-1 失败
 */
int32_t tiered_index_save(const TieredIndex *index, const char *path);

/**
 * @brief 加载分层索引
 *
 * @param path 基础路径
 * @param config 配置（可为 NULL）
 * @return 索引句柄，失败返回 NULL
 */
TieredIndex *tiered_index_load(const char *path, const TieredIndexConfig *config);

/**
 * @brief 销毁分层索引
 */
void tiered_index_destroy(TieredIndex *index);

/* ========================================================================
 * 性能调优
 * ======================================================================== */

/**
 * @brief 设置搜索参数
 */
void tiered_index_set_search_params(TieredIndex *index,
                                    int32_t hnsw_ef,
                                    int32_t ivf_nprobe);

/**
 * @brief 设置内存限制
 */
int32_t tiered_index_set_memory_limit(TieredIndex *index,
                                      int64_t limit_mb);

/**
 * @brief 获取内存使用情况
 */
void tiered_index_memory_usage(const TieredIndex *index,
                               int64_t *used_mb,
                               int64_t *limit_mb);

/**
 * @brief 执行内存整理
 *
 * 将冷数据从内存层下沉到 SSD 层
 */
int32_t tiered_index_memory_defrag(TieredIndex *index);

/* ========================================================================
 * 访问模式配置
 * ======================================================================== */

/** 访问模式 */
typedef enum AccessPattern_e {
    ACCESS_UNIFORM = 0,    /**< 均匀访问 */
    ACCESS_HOT_COLD = 1,   /**< 热冷分离 */
    ACCESS_STREAMING = 2,  /**< 流式访问 */
    ACCESS_TEMPORAL = 3    /**< 时间序列访问 */
} AccessPattern;

/**
 * @brief 设置访问模式
 *
 * 影响自动层级切换策略
 */
void tiered_index_set_access_pattern(TieredIndex *index, AccessPattern pattern);

/**
 * @brief 获取当前访问模式
 */
AccessPattern tiered_index_get_access_pattern(const TieredIndex *index);

#ifdef __cplusplus
}
#endif

#endif /* DB_TIERED_INDEX_H */
