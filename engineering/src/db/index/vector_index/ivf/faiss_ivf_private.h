/*
 * faiss_ivf_private.h
 *
 * IVF 索引内部结构定义与内部 API 声明。
 *
 * === 架构概述 ===
 * IVF (Inverted File) 是一种基于聚类的近似最近邻索引:
 *   1. 训练阶段: 对向量空间做两级 K-Means 聚类，产生 nlist × nlist2 个桶
 *   2. 添加阶段: 每个向量路由到最近的二级桶中心，存入对应倒排列表
 *   3. 搜索阶段: 查询向量先找最近的 nprobe 个一级簇，展开其下所有二级桶，
 *      扫描桶内向量，用大顶堆维护 top-k 最近邻
 *
 * === 两级聚类的设计意图 ===
 * 单级 IVF 在 nlist 较大时，每个桶内仍有很多向量，扫描开销大。
 * 两级设计将每个一级簇再细分为 nlist2 个二级桶，在相近的索引大小下
 * 获得更细粒度的空间划分，查询时只需扫描 nprobe × nlist2 个桶，
 * 比单级大 nlist 的索引有更好的搜索精度/速度折中。
 *
 * === PQ 量化模式 ===
 * 当 quantization_type != NONE 时，向量不直接存储，而是:
 *   1. 计算向量与其所属二级桶中心的残差
 *   2. 用 PQ 对残差编码，存储紧凑的 code
 * 搜索时通过 ADC (Asymmetric Distance Computation) 用距离表查表计算距离，
 * 避免解压完整向量，大幅减少内存占用。
 *
 * === DirectMap 的作用 ===
 * DirectMap 维护 向量 ID → (桶号, 桶内偏移) 的 O(1) 映射，
 * 用于支持删除和重建操作。使用数组直接索引，id 即数组下标。
 */

#ifndef FAISS_IVF_PRIVATE_H
#define FAISS_IVF_PRIVATE_H

#include <db/index/vector_index/ivf/IndexIVF.h>
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

/* 搜索结果中的候选元素，用于排序桶和簇的优先级 */
typedef struct faiss_ivf_scored_index {
    float distance;  /* 到查询向量的距离，越小越优先 */
    int32_t id;      /* 簇编号或桶编号 */
} faiss_ivf_scored_index_t;

/*
 * faiss_ivf 内部结构。
 *
 * 内存布局要点:
 * - primary_centroids:    [nlist × dims] 一级聚类中心
 * - secondary_centroids:  [nlist × nlist2 × dims] 二级聚类中心，
 *   按 bucket_id = primary * nlist2 + secondary 线性排列
 * - secondary_counts:     [nlist] 每个一级簇下实际的二级中心数（可能 < nlist2）
 * - vectors:              [n_total × dims] 原始向量（非量化模式）
 * - codes:                [n_total × code_size] PQ 编码（量化模式）
 */
struct faiss_ivf {
    float *vectors;                                 /* 原始向量数据（非量化模式使用） */
    uint8_t *codes;                                 /* PQ 编码数据（量化模式使用） */
    int32_t n_total;                                /* 已添加向量总数 */
    int32_t dims;                                   /* 向量维度 */
    int32_t nlist;                                  /* 一级聚类中心数（粗粒度空间划分） */
    int32_t nlist2;                                 /* 每个一级簇下的二级中心数（细粒度划分） */
    int32_t nprobe;                                 /* 查询时探测的一级簇数，越大精度越高速度越慢 */
    int32_t code_size;                              /* 单向量 PQ 编码字节数，0 表示无量化 */
    int32_t total_bucket_count;                     /* 总桶数 = nlist × nlist2 */
    distance_metric_t metric;                  /* 距离度量类型（L2/IP 等） */
    quantization_type_t quantization_type;     /* 量化模式（NONE/PQ 等） */
    bool trained;                                   /* 是否已完成训练 */
    faiss_ivf_training_params_t training_params;    /* K-Means 训练超参数 */
    faiss_ivf_quantization_params_t quantization_params; /* PQ 量化超参数 */
    quantizer_config_t quantizer_config;       /* 传递给量化器工厂的配置 */
    quantizer_t *quantizer;                    /* 已训练的量化器实例（NULL 表示未启用量化） */
    faiss_direct_map_t direct_map;             /* ID → (桶号, 偏移) O(1) 映射表 */
    float *primary_centroids;                       /* 一级中心数组 [nlist × dims] */
    float *secondary_centroids;                     /* 二级中心数组 [total_bucket_count × dims] */
    int32_t *secondary_counts;                      /* 每个一级簇实际二级中心数 [nlist] */
    faiss_inverted_lists_t *invlists;               /* 倒排桶抽象层，管理所有桶的 ID 和编码 */

    /* Phase 1 基础设施：存储后端与 Heap 向量存储 */
    storage_backend_t *storage;                 /**< 存储后端 */
    heap_vector_store_t *heap_store;            /**< 向量主存储 */
    vector_ref_t *vector_refs;                  /**< 向量引用数组 */
    uint32_t vector_refs_size;                  /**< 引用数组大小 */
    persist_control_t *persist_ctrl;            /**< 持久化控制 */
};

/* ── 内部工具函数 ── */

/* 设置训练参数为合理默认值 */
void faiss_ivf_set_default_training_params(faiss_ivf_training_params_t *params);
int faiss_ivf_training_params_are_valid(const faiss_ivf_training_params_t *params);
int faiss_ivf_quantization_params_are_valid(const faiss_ivf_t *index, const faiss_ivf_quantization_params_t *params);

/* 量化配置变更后重建底层 quantizer 实例（先销毁旧的再创建新的） */
int faiss_ivf_rebuild_quantizer(faiss_ivf_t *index);

/* qsort 比较器: 按距离升序，距离相同时按 id 升序（保证确定性） */
int faiss_ivf_compare_scored_indices(const void *left, const void *right);

/* 将 (一级簇, 二级簇) 映射为扁平的桶编号: bucket_id = primary * nlist2 + secondary */
int32_t faiss_ivf_get_bucket_id(const faiss_ivf_t *index, int32_t primary_cluster, int32_t secondary_cluster);

/* 返回某桶对应二级中心向量的只读指针 */
const float *faiss_ivf_get_bucket_centroid_ptr(const faiss_ivf_t *index, int32_t primary_cluster, int32_t secondary_cluster);

/* 计算残差: residual[i] = vector[i] - centroid[i]（用于 PQ 编码） */
void faiss_ivf_compute_residual(float *residual, const float *vector, const float *centroid, int32_t dims);

/* 执行 K-Means 训练，float 输入自动转为 double 以提升数值稳定性。
 * 成功返回 1，失败返回 0。centroids_out 和 labels_out 由调用方预分配。 */
int faiss_ivf_run_kmeans_training(const faiss_ivf_t *index,
                                  int32_t n,
                                  const float *vectors,
                                  int32_t n_clusters,
                                  float *centroids_out,
                                  int32_t *labels_out);

/* 遍历所有一级中心，返回距离向量最近的那个的编号 */
int32_t faiss_ivf_find_nearest_primary_centroid(const faiss_ivf_t *index, const float *vector);

/* 在指定一级簇内遍历其所有二级中心，返回最近的编号。
 * 若该簇只有 ≤1 个二级中心，直接返回 0。 */
int32_t faiss_ivf_find_nearest_secondary_centroid(const faiss_ivf_t *index,
                                                  int32_t primary_cluster,
                                                  const float *vector);

/* 计算所有训练样本到其最近二级中心的残差，用这些残差训练 PQ 量化器。
 * 训练完成后更新 index->code_size。无量化器时直接返回 0。 */
int faiss_ivf_train_quantizer_residuals(faiss_ivf_t *index,
                                        int32_t n,
                                        const float *vectors,
                                        const int32_t *primary_labels);

#endif
