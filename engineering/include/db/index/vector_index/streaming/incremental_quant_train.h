/**
 * @file incremental_quant_train.h
 * @brief 增量量化训练 - 避免全量重训
 *
 * 设计原则：
 * 1. 增量更新：当新增向量数量达到阈值时，更新码书而非全量重训
 * 2. 在线学习：使用流式 k-means 或在线聚类更新码书
 * 3. 延迟重建：当增量更新效果下降时，触发全量重建
 */

#ifndef INCREMENTAL_QUANT_TRAIN_H
#define INCREMENTAL_QUANT_TRAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 增量训练配置
 * ======================================================================== */

/**
 * @brief 增量训练配置
 */
typedef struct incremental_quant_config {
    int32_t incremental_threshold;  /* 增量更新阈值（向量数） */
    int32_t max_incremental_updates; /* 最大增量更新次数后强制全量重建 */
    float drift_threshold;          /* 码书漂移阈值，触发重建 */
    int32_t online_iters;           /* 在线 k-means 迭代次数 */
    bool auto_retrain;              /* 是否自动触发全量重建 */
} incremental_quant_config_t;

/**
 * @brief 默认配置
 */
#define INCREMENTAL_QUANT_DEFAULT_THRESHOLD 1000
#define INCREMENTAL_QUANT_DEFAULT_MAX_UPDATES 10
#define INCREMENTAL_QUANT_DEFAULT_DRIFT_THRESHOLD 0.1f
#define INCREMENTAL_QUANT_DEFAULT_ONLINE_ITERS 5

/**
 * @brief 创建默认配置
 */
incremental_quant_config_t incremental_quant_config_default(void);

/* ========================================================================
 * 增量训练器
 * ======================================================================== */

typedef struct incremental_quant_trainer incremental_quant_trainer_t;

/**
 * @brief 创建增量训练器
 * @param base_quantizer 基础量化器（已训练）
 * @param config 配置（NULL 使用默认配置）
 * @return 训练器句柄，失败返回 NULL
 */
incremental_quant_trainer_t *incremental_quant_trainer_create(
    quantizer_t *base_quantizer,
    const incremental_quant_config_t *config);

/**
 * @brief 销毁增量训练器
 * @param trainer 训练器句柄
 */
void incremental_quant_trainer_destroy(incremental_quant_trainer_t *trainer);

/**
 * @brief 增量更新码书
 *
 * 使用在线 k-means 或在线聚类算法增量更新码书。
 * 不需要重训全部数据。
 *
 * @param trainer 训练器句柄
 * @param n 新增向量数量
 * @param vectors 新增向量数据
 * @return 0 成功，-1 失败，1 需要全量重建
 */
int32_t incremental_quant_update(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors);

/**
 * @brief 检查是否需要全量重建
 * @param trainer 训练器句柄
 * @return true 需要全量重建
 */
bool incremental_quant_need_full_retrain(const incremental_quant_trainer_t *trainer);

/**
 * @brief 触发全量重建
 *
 * 使用全部数据重训练量化器。
 *
 * @param trainer 训练器句柄
 * @param total_n 总向量数量
 * @param all_vectors 全部向量数据
 * @return 0 成功，-1 失败
 */
int32_t incremental_quant_full_retrain(
    incremental_quant_trainer_t *trainer,
    int32_t total_n,
    const float *all_vectors);

/**
 * @brief 获取增量更新次数
 * @param trainer 训练器句柄
 * @return 更新次数
 */
int32_t incremental_quant_update_count(const incremental_quant_trainer_t *trainer);

/**
 * @brief 重置增量训练器
 * @param trainer 训练器句柄
 */
void incremental_quant_reset(incremental_quant_trainer_t *trainer);

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 计算两个码本之间的漂移距离
 *
 * 用于判断增量更新是否导致码书大幅变化。
 *
 * @param old_codebook 旧码本
 * @param new_codebook 新码本
 * @param size 码本大小
 * @return 漂移距离
 */
float incremental_quant_codebook_drift(
    const float *old_codebook,
    const float *new_codebook,
    int32_t size);

/**
 * @brief 批量编码新向量（使用更新的量化器）
 *
 * @param trainer 训练器句柄
 * @param n 向量数量
 * @param vectors 向量数据
 * @param codes 输出编码
 * @return 0 成功，-1 失败
 */
int32_t incremental_quant_encode(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors,
    uint8_t *codes);

/* ========================================================================
 * 状态查询
 * ======================================================================== */

/**
 * @brief 增量训练器状态
 */
typedef struct incremental_quant_stats {
    int32_t update_count;        /* 增量更新次数 */
    int32_t incremental_threshold; /* 增量阈值 */
    int32_t max_updates;         /* 最大更新次数 */
    float last_drift;            /* 上次漂移距离 */
    bool need_full_retrain;      /* 是否需要全量重建 */
    int64_t total_vectors_processed; /* 累计处理向量数 */
} incremental_quant_stats_t;

/**
 * @brief 获取统计信息
 * @param trainer 训练器句柄
 * @param stats 输出统计信息
 */
void incremental_quant_get_stats(
    const incremental_quant_trainer_t *trainer,
    incremental_quant_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* INCREMENTAL_QUANT_TRAIN_H */
