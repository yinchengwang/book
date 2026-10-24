/**
 * @file incremental_quant_train.c
 * @brief 增量量化训练实现
 *
 * 实现增量更新策略：
 * 1. 在线 k-means：使用 mini-batch k-means 增量更新码书
 * 2. 漂移检测：计算新旧码本之间的距离，判断是否需要重建
 * 3. 自动重建：累计更新次数达到阈值时触发全量重建
 */

#include "incremental_quant_train.h"
#include <algo-prod/quantization/quantization_private.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/**
 * @brief 在线 k-means 聚类中心状态
 */
typedef struct cluster_state {
    float *centroid;      /* 聚类中心 */
    int32_t count;        /* 分配给该中心的样本数 */
} cluster_state_t;

/**
 * @brief 增量训练器内部结构
 */
struct incremental_quant_trainer {
    quantizer_t *base_quantizer;      /* 基础量化器 */
    incremental_quant_config_t config;

    /* 在线 k-means 状态 */
    cluster_state_t *cluster_states;  /* 聚类中心状态 */
    int32_t num_clusters;             /* 聚类数（等于码本大小） */
    int32_t subvector_dims;           /* 子向量维度 */
    int32_t subquantizers;            /* 子空间数 */

    /* 漂移检测 */
    float *old_codebook;              /* 更新前的码本（用于计算漂移） */
    int32_t codebook_size;            /* 码本大小 */

    /* 统计 */
    int32_t update_count;             /* 增量更新次数 */
    int64_t total_vectors_processed;  /* 累计处理向量数 */
};

/* ========================================================================
 * 配置
 * ======================================================================== */

incremental_quant_config_t incremental_quant_config_default(void) {
    incremental_quant_config_t config = {
        .incremental_threshold = INCREMENTAL_QUANT_DEFAULT_THRESHOLD,
        .max_incremental_updates = INCREMENTAL_QUANT_DEFAULT_MAX_UPDATES,
        .drift_threshold = INCREMENTAL_QUANT_DEFAULT_DRIFT_THRESHOLD,
        .online_iters = INCREMENTAL_QUANT_DEFAULT_ONLINE_ITERS,
        .auto_retrain = true
    };
    return config;
}

/* ========================================================================
 * 创建与销毁
 * ======================================================================== */

incremental_quant_trainer_t *incremental_quant_trainer_create(
    quantizer_t *base_quantizer,
    const incremental_quant_config_t *config) {

    if (base_quantizer == NULL || !quantizer_is_trained(base_quantizer)) {
        return NULL;
    }

    incremental_quant_trainer_t *trainer =
        (incremental_quant_trainer_t *)calloc(1, sizeof(incremental_quant_trainer_t));
    if (trainer == NULL) {
        return NULL;
    }

    /* 使用默认配置或用户配置 */
    if (config != NULL) {
        trainer->config = *config;
    } else {
        trainer->config = incremental_quant_config_default();
    }

    trainer->base_quantizer = base_quantizer;

    /* 获取码本信息 */
    trainer->subquantizers = base_quantizer->config.pq_subquantizers;
    trainer->subvector_dims = base_quantizer->config.dims / trainer->subquantizers;
    trainer->num_clusters = base_quantizer->max_codebook_size;
    trainer->codebook_size = base_quantizer->codebook_size;

    /* 分配聚类状态 */
    int32_t total_clusters = trainer->num_clusters * trainer->subquantizers;
    trainer->cluster_states = (cluster_state_t *)calloc(
        (size_t)total_clusters, sizeof(cluster_state_t));
    if (trainer->cluster_states == NULL) {
        free(trainer);
        return NULL;
    }

    /* 初始化聚类中心（从现有码本复制） */
    for (int32_t i = 0; i < total_clusters; i++) {
        trainer->cluster_states[i].centroid =
            (float *)malloc((size_t)trainer->subvector_dims * sizeof(float));
        if (trainer->cluster_states[i].centroid == NULL) {
            /* 清理并返回 */
            for (int32_t j = 0; j < i; j++) {
                free(trainer->cluster_states[j].centroid);
            }
            free(trainer->cluster_states);
            free(trainer);
            return NULL;
        }
        /* 从码本复制 */
        memcpy(trainer->cluster_states[i].centroid,
               &base_quantizer->codebooks[(size_t)i * trainer->subvector_dims],
               (size_t)trainer->subvector_dims * sizeof(float));
        trainer->cluster_states[i].count = 0;
    }

    /* 分配旧码本（用于漂移检测） */
    trainer->old_codebook = (float *)malloc(
        (size_t)total_clusters * trainer->subvector_dims * sizeof(float));
    if (trainer->old_codebook == NULL) {
        for (int32_t i = 0; i < total_clusters; i++) {
            free(trainer->cluster_states[i].centroid);
        }
        free(trainer->cluster_states);
        free(trainer);
        return NULL;
    }

    /* 初始化旧码本为当前码本 */
    memcpy(trainer->old_codebook, base_quantizer->codebooks,
           (size_t)total_clusters * trainer->subvector_dims * sizeof(float));

    trainer->update_count = 0;
    trainer->total_vectors_processed = 0;

    return trainer;
}

void incremental_quant_trainer_destroy(incremental_quant_trainer_t *trainer) {
    if (trainer == NULL) {
        return;
    }

    if (trainer->cluster_states != NULL) {
        int32_t total_clusters = trainer->num_clusters * trainer->subquantizers;
        for (int32_t i = 0; i < total_clusters; i++) {
            free(trainer->cluster_states[i].centroid);
        }
        free(trainer->cluster_states);
    }

    free(trainer->old_codebook);
    free(trainer);
}

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 计算两个向量的欧氏距离
 */
static inline float euclidean_distance(
    const float *a, const float *b, int32_t dims) {
    float dist = 0.0f;
    for (int32_t i = 0; i < dims; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * @brief 计算子向量的最近聚类中心
 */
static inline int32_t find_nearest_centroid(
    const float *vector,
    const cluster_state_t *clusters,
    int32_t num_clusters,
    int32_t dims) {
    float min_dist = euclidean_distance(vector, clusters[0].centroid, dims);
    int32_t nearest = 0;

    for (int32_t i = 1; i < num_clusters; i++) {
        float dist = euclidean_distance(vector, clusters[i].centroid, dims);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = i;
        }
    }
    return nearest;
}

/* ========================================================================
 * 在线 k-means 增量更新
 * ======================================================================== */

/**
 * @brief 在线 k-means 单次迭代
 *
 * 使用随机梯度下降风格的更新：
 * centroid = centroid * (1 - learning_rate) + sample * learning_rate
 */
static int32_t online_kmeans_iteration(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors,
    float learning_rate) {

    int32_t dims = trainer->subvector_dims;
    int32_t sub_m = trainer->subquantizers;
    int32_t k = trainer->num_clusters;

    for (int32_t i = 0; i < n; i++) {
        const float *vector = vectors + (size_t)i * dims * sub_m;

        /* 对每个子空间找到最近中心并更新 */
        for (int32_t m = 0; m < sub_m; m++) {
            const float *sub_vector = vector + (size_t)m * dims;
            int32_t cluster_idx = m * k + find_nearest_centroid(
                sub_vector,
                trainer->cluster_states + m * k,
                k, dims);

            /* 在线更新：移动聚类中心 */
            float *centroid = trainer->cluster_states[cluster_idx].centroid;
            for (int32_t d = 0; d < dims; d++) {
                centroid[d] = centroid[d] * (1.0f - learning_rate) +
                              sub_vector[d] * learning_rate;
            }
            trainer->cluster_states[cluster_idx].count++;
        }
    }

    return 0;
}

int32_t incremental_quant_update(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors) {

    if (trainer == NULL || vectors == NULL || n <= 0) {
        return -1;
    }

    /* 检查更新次数是否超过阈值 */
    if (trainer->update_count >= trainer->config.max_incremental_updates) {
        return 1;  /* 需要全量重建 */
    }

    /* 保存旧码本（用于漂移检测） */
    int32_t total_clusters = trainer->num_clusters * trainer->subquantizers;
    memcpy(trainer->old_codebook, trainer->base_quantizer->codebooks,
           (size_t)total_clusters * trainer->subvector_dims * sizeof(float));

    /* 执行在线 k-means 迭代 */
    float learning_rate = 0.1f;  /* 学习率 */
    for (int32_t iter = 0; iter < trainer->config.online_iters; iter++) {
        if (online_kmeans_iteration(trainer, n, vectors, learning_rate) != 0) {
            return -1;
        }
        learning_rate *= 0.9f;  /* 衰减学习率 */
    }

    /* 将更新后的聚类中心复制回码本 */
    for (int32_t i = 0; i < total_clusters; i++) {
        memcpy(&trainer->base_quantizer->codebooks[(size_t)i * trainer->subvector_dims],
               trainer->cluster_states[i].centroid,
               (size_t)trainer->subvector_dims * sizeof(float));
    }

    trainer->update_count++;
    trainer->total_vectors_processed += n;

    return 0;
}

bool incremental_quant_need_full_retrain(const incremental_quant_trainer_t *trainer) {
    if (trainer == NULL) {
        return true;
    }

    /* 检查更新次数阈值 */
    if (trainer->update_count >= trainer->config.max_incremental_updates) {
        return true;
    }

    /* 检查码书漂移 */
    float drift = incremental_quant_codebook_drift(
        trainer->old_codebook,
        trainer->base_quantizer->codebooks,
        trainer->num_clusters * trainer->subquantizers * trainer->subvector_dims);

    return drift > trainer->config.drift_threshold;
}

int32_t incremental_quant_full_retrain(
    incremental_quant_trainer_t *trainer,
    int32_t total_n,
    const float *all_vectors) {

    if (trainer == NULL || all_vectors == NULL || total_n <= 0) {
        return -1;
    }

    /* 重置量化器 */
    quantizer_reset(trainer->base_quantizer);

    /* 全量训练 */
    if (quantizer_train(trainer->base_quantizer, total_n, all_vectors) != 0) {
        return -1;
    }

    /* 重新初始化聚类状态 */
    int32_t total_clusters = trainer->num_clusters * trainer->subquantizers;
    for (int32_t i = 0; i < total_clusters; i++) {
        memcpy(trainer->cluster_states[i].centroid,
               &trainer->base_quantizer->codebooks[(size_t)i * trainer->subvector_dims],
               (size_t)trainer->subvector_dims * sizeof(float));
        trainer->cluster_states[i].count = 0;
    }

    /* 更新旧码本 */
    memcpy(trainer->old_codebook, trainer->base_quantizer->codebooks,
           (size_t)total_clusters * trainer->subvector_dims * sizeof(float));

    trainer->update_count = 0;

    return 0;
}

int32_t incremental_quant_update_count(const incremental_quant_trainer_t *trainer) {
    return trainer ? trainer->update_count : 0;
}

void incremental_quant_reset(incremental_quant_trainer_t *trainer) {
    if (trainer == NULL) {
        return;
    }

    trainer->update_count = 0;
    trainer->total_vectors_processed = 0;

    /* 重置聚类计数 */
    int32_t total_clusters = trainer->num_clusters * trainer->subquantizers;
    for (int32_t i = 0; i < total_clusters; i++) {
        trainer->cluster_states[i].count = 0;
    }
}

/* ========================================================================
 * 辅助函数实现
 * ======================================================================== */

float incremental_quant_codebook_drift(
    const float *old_codebook,
    const float *new_codebook,
    int32_t size) {

    if (old_codebook == NULL || new_codebook == NULL || size <= 0) {
        return 0.0f;
    }

    /* 计算均方根偏移 */
    float sum_sq = 0.0f;
    for (int32_t i = 0; i < size; i++) {
        float diff = new_codebook[i] - old_codebook[i];
        sum_sq += diff * diff;
    }

    return sqrtf(sum_sq / size);
}

int32_t incremental_quant_encode(
    incremental_quant_trainer_t *trainer,
    int32_t n,
    const float *vectors,
    uint8_t *codes) {

    if (trainer == NULL || vectors == NULL || codes == NULL || n <= 0) {
        return -1;
    }

    return quantizer_encode_batch(trainer->base_quantizer, n, vectors, codes);
}

/* ========================================================================
 * 状态查询
 * ======================================================================== */

void incremental_quant_get_stats(
    const incremental_quant_trainer_t *trainer,
    incremental_quant_stats_t *stats) {

    if (trainer == NULL || stats == NULL) {
        return;
    }

    stats->update_count = trainer->update_count;
    stats->incremental_threshold = trainer->config.incremental_threshold;
    stats->max_updates = trainer->config.max_incremental_updates;
    stats->last_drift = incremental_quant_codebook_drift(
        trainer->old_codebook,
        trainer->base_quantizer->codebooks,
        trainer->num_clusters * trainer->subquantizers * trainer->subvector_dims);
    stats->need_full_retrain = incremental_quant_need_full_retrain(trainer);
    stats->total_vectors_processed = trainer->total_vectors_processed;
}
