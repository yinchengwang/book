/*
 * faiss_ivf_hnsw_private.h
 *
 * HNSW-IVF 内部结构定义与工具函数声明。
 */

#ifndef FAISS_IVF_HNSW_PRIVATE_H
#define FAISS_IVF_HNSW_PRIVATE_H

#include <db/index/vector_index/ivf_hnsw/IndexIVFHNSW.h>
#include <db/index/vector_index/ivf/inverted_lists.h>
#include <db/index/vector_index/ivf/direct_map.h>

#include <algo-prod/Kmeans/kmeans.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────
 * 内部常量
 * ───────────────────────────────────────────────────────────────── */

#define FAISS_IVF_HNSW_RAND_LEVEL_SEED 12345
#define FAISS_IVF_HNSW_SWAP_POSITIONS_SEED 789
#define FAISS_IVF_HNSW_MAX_CANDIDATES_ON_STACK 256
#define FAISS_IVF_HNSW_DEFAULT_EF_SEARCH 50
#define FAISS_IVF_HNSW_DEFAULT_M 16
#define FAISS_IVF_HNSW_DEFAULT_EF_CONSTRUCTION 100
#define FAISS_IVF_HNSW_DEFAULT_MAX_ASSIGNMENTS 1

/* ─────────────────────────────────────────────────────────────────
 * 分配条目结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * Multi-assignment 分配条目。
 * 记录向量被分配到的桶及其在桶内的偏移。
 */
typedef struct faiss_ivf_hnsw_assign_entry {
    int32_t bucket_id;   /* 桶编号 */
    int32_t offset;       /* 桶内偏移 */
} faiss_ivf_hnsw_assign_entry_t;

/**
 * 搜索结果中的候选元素（用于排序）。
 */
typedef struct faiss_ivf_hnsw_scored_index {
    float distance;   /* 到查询向量的距离 */
    int32_t id;       /* 簇编号或桶编号 */
} faiss_ivf_hnsw_scored_index_t;

/* ─────────────────────────────────────────────────────────────────
 * HNSW-IVF 内部结构
 * ───────────────────────────────────────────────────────────────── */

/**
 * HNSW-IVF 索引内部结构。
 *
 * 内存布局要点:
 * - primary_centroids:    [nlist × dims] 一级聚类中心
 * - secondary_centroids:  [nlist × nlist2 × dims] 二级聚类中心
 * - secondary_counts:     [nlist] 每个一级簇下的实际二级中心数
 * - vectors:              [n_total × dims] 原始向量（非量化模式）
 * - codes:                [n_total × code_size] PQ 编码（量化模式）
 *
 * HNSW 图结构（索引一级中心）:
 * - hnsw_levels:         [nlist] 每个一级中心的层高
 * - hnsw_offsets:        [nlist] 邻接表偏移
 * - hnsw_nbs:            [...] 扁平化邻接表
 *
 * Multi-assignment:
 * - assignments:          [n_total × max_assignments] 分配表
 */
struct faiss_ivf_hnsw {
    /* ── 数据存储 ── */
    float *vectors;                         /* 原始向量数据 */
    uint8_t *codes;                         /* PQ 编码数据 */
    int32_t n_total;                        /* 已添加向量总数 */

    /* ── IVF 结构 ── */
    int32_t dims;                           /* 向量维度 */
    int32_t nlist;                          /* 一级聚类中心数 */
    int32_t nlist2;                         /* 每簇内二级聚类数 */
    int32_t nprobe;                         /* 搜索时探测的一级簇数 */
    int32_t code_size;                      /* 单向量 PQ 编码字节数 */
    int32_t total_bucket_count;             /* 总桶数 = nlist × nlist2 */

    distance_metric_t metric;               /* 距离度量类型 */
    quantization_type_t quantization_type;  /* 量化模式 */

    /* ── 聚类中心 ── */
    float *primary_centroids;               /* 一级中心 [nlist × dims] */
    float *secondary_centroids;             /* 二级中心 [total_bucket_count × dims] */
    int32_t *secondary_counts;              /* 每簇实际二级中心数 [nlist] */

    /* ── 量化 ── */
    bool trained;                           /* 是否已训练 */
    faiss_ivf_hnsw_training_params_t training_params;
    faiss_ivf_hnsw_quantization_params_t quantization_params;
    quantizer_config_t quantizer_config;
    quantizer_t *quantizer;                 /* 量化器实例 */

    /* ── HNSW 图结构（索引一级中心）─ */
    int32_t *hnsw_levels;                  /* 层高 [nlist] */
    uint32_t hnsw_levels_size;
    int32_t *hnsw_offsets;                 /* 邻接表偏移 [nlist] */
    uint32_t hnsw_offsets_size;
    int32_t *hnsw_nbs;                     /* 扁平化邻接表 [...] */
    uint32_t hnsw_nbs_size;
    int32_t hnsw_entry_point;              /* HNSW 入口点 */
    int32_t hnsw_max_level;                /* 最大层号 */
    float *hnsw_assign_probas;             /* 层高概率分布 */
    uint32_t hnsw_assign_probas_size;
    int32_t *hnsw_cum_nneighbor_per_level;  /* 每层累计邻居数 */
    uint32_t hnsw_cum_nneighbor_per_level_size;
    int32_t M;                              /* HNSW 邻居数 */
    int32_t ef_construction;                /* HNSW 构建宽度 */
    int32_t ef_search;                      /* HNSW 搜索宽度 */

    /* ── 倒排桶 ── */
    faiss_inverted_lists_t *invlists;
    faiss_direct_map_t direct_map;

    /* ── Multi-assignment ── */
    int32_t max_assignments;               /* 每向量最大分配桶数 */
    faiss_ivf_hnsw_assign_entry_t *assignments;  /* 分配表 [n_total × max_assignments] */
};

/* ─────────────────────────────────────────────────────────────────
 * 内部工具函数
 * ───────────────────────────────────────────────────────────────── */

/* ── HNSW 图操作 ── */

/**
 * 计算第 layer 层开始的邻居槽偏移。
 *
 * 内存布局：每个节点在每层有固定数量的邻居槽
 * - 层 0（基础层）: M*2 个槽
 * - 层 1+: 每层 M 个槽
 *
 * 节点 i 在层 L 的邻居范围是 [offset[i] + base[L], offset[i] + base[L+1])
 * 其中 base[L] = sum_{k=0}^{L-1} neighbors_per_layer[k]
 */
static inline size_t hnsw_neighbor_offset(const faiss_ivf_hnsw_t *index, int32_t node_id, int32_t layer) {
    if (node_id < 0 || layer < 0) return 0;
    /* 基础层 M*2，上层各 M */
    return (size_t)(index->hnsw_offsets[node_id] +
           (layer == 0 ? index->M * 2 : index->M * (1 + layer)));
}

/**
 * 获取节点 no 在某层的邻居范围。
 *
 * 节点 no 在层 l（l <= node_level）的邻居存储位置：
 * - 层 0: [offset[no], offset[no] + M*2)
 * - 层 1: [offset[no] + M*2, offset[no] + M*2 + M)
 * - 层 2: [offset[no] + M*2 + M, offset[no] + M*2 + 2*M)
 * - 层 l: [offset[no] + M*2 + (l-1)*M, offset[no] + M*2 + l*M)
 *
 * 如果 layer_no > node_level，则该节点在该层没有邻居，返回空范围。
 */
static inline void hnsw_neighbor_range(const faiss_ivf_hnsw_t *index,
                                        int32_t no,
                                        int32_t layer_no,
                                        size_t *begin,
                                        size_t *end) {
    if (no < 0 || layer_no < 0) {
        *begin = *end = 0;
        return;
    }

    /* 检查节点是否在该层有邻居 */
    if (layer_no > index->hnsw_levels[no]) {
        /* 该节点在该层没有邻居 */
        *begin = *end = 0;
        return;
    }

    size_t base_offset = (size_t)index->hnsw_offsets[no];

    if (layer_no == 0) {
        /* 基础层 */
        *begin = base_offset;
        *end = base_offset + (size_t)(index->M * 2);
    } else {
        /* 上层: 基础层 + (layer-1) 个完整上层 + 当前层 */
        *begin = base_offset + (size_t)(index->M * 2) + (size_t)(index->M * (layer_no - 1));
        *end = *begin + (size_t)index->M;
    }
}

/**
 * 初始化 HNSW 层的概率分布。
 */
int32_t hnsw_set_default_probas(faiss_ivf_hnsw_t *index);

/**
 * 随机采样层高。
 */
int32_t hnsw_random_level(faiss_ivf_hnsw_t *index);

/**
 * 在指定层执行贪心下降。
 */
int32_t hnsw_greedy_update_nearest(const faiss_ivf_hnsw_t *index,
                                    const float *query,
                                    int32_t level,
                                    int32_t *nearest,
                                    float *d_nearest);

/**
 * 准备新节点的层信息。
 */
int32_t hnsw_prepare_level_tab(faiss_ivf_hnsw_t *index, int32_t n, int32_t *max_level);

/**
 * 连接两个节点。
 */
void hnsw_connect_points(faiss_ivf_hnsw_t *index,
                          int32_t pt_id,
                          int32_t neighbor,
                          int32_t layer_no);

/**
 * 插入节点到 HNSW 图。
 */
int32_t hnsw_insert_vector(faiss_ivf_hnsw_t *index,
                             int32_t pt_id,
                             int32_t pt_level,
                             faiss_ivf_hnsw_t *visited_marker);

/**
 * 搜索层内候选。
 * @param query 查询向量（或 NULL 表示自身）
 */
int32_t hnsw_search_layer_candidates(const faiss_ivf_hnsw_t *index,
                                       const float *query,
                                       int32_t layer_no,
                                       int32_t entry,
                                       int32_t ef,
                                       int32_t *result_ids,
                                       float *result_dist,
                                       int32_t *result_count);

/* ── HNSW-IVF 工具函数 ── */

/**
 * 设置默认训练参数。
 */
void faiss_ivf_hnsw_set_default_training_params(faiss_ivf_hnsw_training_params_t *params);

/**
 * 验证训练参数是否合法。
 */
int faiss_ivf_hnsw_training_params_are_valid(const faiss_ivf_hnsw_training_params_t *params);

/**
 * 验证量化参数是否合法。
 */
int faiss_ivf_hnsw_quantization_params_are_valid(const faiss_ivf_hnsw_t *index,
                                                   const faiss_ivf_hnsw_quantization_params_t *params);

/**
 * 重建量化器。
 */
int faiss_ivf_hnsw_rebuild_quantizer(faiss_ivf_hnsw_t *index);

/**
 * qsort 比较器：按距离升序排序。
 */
int faiss_ivf_hnsw_compare_scored_indices(const void *left, const void *right);

/**
 * 获取桶编号。
 */
int32_t faiss_ivf_hnsw_get_bucket_id(const faiss_ivf_hnsw_t *index,
                                       int32_t primary_cluster,
                                       int32_t secondary_cluster);

/**
 * 获取桶中心指针。
 */
const float *faiss_ivf_hnsw_get_bucket_centroid_ptr(const faiss_ivf_hnsw_t *index,
                                                      int32_t primary_cluster,
                                                      int32_t secondary_cluster);

/**
 * 计算残差向量。
 */
void faiss_ivf_hnsw_compute_residual(float *residual,
                                       const float *vector,
                                       const float *centroid,
                                       int32_t dims);

/**
 * 运行 K-Means 训练。
 */
int faiss_ivf_hnsw_run_kmeans_training(const faiss_ivf_hnsw_t *index,
                                         int32_t n,
                                         const float *vectors,
                                         int32_t n_clusters,
                                         float *centroids_out,
                                         int32_t *labels_out);

/**
 * 找最近的一级中心。
 */
int32_t faiss_ivf_hnsw_find_nearest_primary_centroid(const faiss_ivf_hnsw_t *index,
                                                       const float *vector);

/**
 * 找最近的二级中心。
 */
int32_t faiss_ivf_hnsw_find_nearest_secondary_centroid(const faiss_ivf_hnsw_t *index,
                                                         int32_t primary_cluster,
                                                         const float *vector);

/**
 * 训练量化器（残差）。
 */
int faiss_ivf_hnsw_train_quantizer_residuals(faiss_ivf_hnsw_t *index,
                                               int32_t n,
                                               const float *vectors,
                                               const int32_t *primary_labels);

/**
 * HNSW 粗排：找 nprobe 个最近的一级簇。
 */
int32_t faiss_ivf_hnsw_coarse_search(const faiss_ivf_hnsw_t *index,
                                      const float *query,
                                      int32_t nprobe,
                                      faiss_ivf_hnsw_scored_index_t *primary_clusters);

/**
 * 分配向量到 k 个最近桶。
 */
void faiss_ivf_hnsw_assign_to_buckets(faiss_ivf_hnsw_t *index,
                                        const float *vector,
                                        int32_t vector_id,
                                        int32_t k);

/**
 * 扩展分配表容量。
 */
int32_t faiss_ivf_hnsw_expand_assignments(faiss_ivf_hnsw_t *index, int32_t new_total);

/* ── 聚簇平衡优化 ── */

/**
 * 计算簇大小的统计信息。
 */
void faiss_ivf_hnsw_compute_cluster_stats(const int32_t *counts,
                                          int32_t nlist,
                                          float *out_avg,
                                          float *out_std,
                                          int32_t *out_max,
                                          int32_t *out_min);

/**
 * 检查簇大小是否平衡。
 */
int faiss_ivf_hnsw_check_cluster_balance(const int32_t *counts,
                                          int32_t nlist,
                                          float max_imbalance);

/**
 * 重分配边界向量到最近的相邻簇（增强版）。
 */
int32_t faiss_ivf_hnsw_reassign_boundary_vectors_enhanced(
    faiss_ivf_hnsw_t *index,
    const float *vectors,
    int32_t n,
    int32_t *labels,
    float reassign_threshold);

#endif /* FAISS_IVF_HNSW_PRIVATE_H */
