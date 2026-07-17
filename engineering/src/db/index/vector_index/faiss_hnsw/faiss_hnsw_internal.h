// faiss_hnsw_internal.h
// 内部结构体定义：faiss_hnsw_t 的实际字段
// 对外只暴露 faiss_hnsw_t 不透明指针（声明在 faiss_hnsw.h）
// 实际字段在本文件中定义，由 .c 文件实现内部访问
//
// 参考 FAISS HNSW.cpp / HNSW.h，重写为 C 版本

#ifndef FAISS_HNSW_INTERNAL_H
#define FAISS_HNSW_INTERNAL_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#include <db/index/vector_index/faiss_hnsw/faiss_hnsw.h>
#include <db/index/vector_index/faiss_hnsw/faiss_hnsw_heap.h>
#include <db/index/vector_index/faiss_hnsw/faiss_hnsw_visited_table.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mersenne Twister RNG 状态（与 FAISS RandomGenerator 行为一致）
typedef struct {
    uint32_t state[624];
    int index;
} faiss_hnsw_mt_state_t;

// MT19937 核心：从 RNG 状态中提取一个 32 位无符号整数
// 实现与 std::mt19937 的 gen() 等价，FAISS RandomGenerator 也基于此
uint32_t faiss_hnsw_mt_extract(faiss_hnsw_mt_state_t *mt);

// 从 MT19937 状态中提取 [0, 1) 范围的 float
// 实现方式：取 mt_extract() 结果的高 24 位作为尾数
// 即 uniform [0, 1) = extract() >> 8 * (1/16777216)
// 与 FAISS HNSW.cpp 中 distrib(rng) 等价（精度为 24 位尾数）
float faiss_hnsw_mt_uniform_01(faiss_hnsw_mt_state_t *mt);

/**
 * 为新插入的向量分配一个随机层号（0-indexed）
 * 参考 FAISS HNSW::random_level()：
 *   - 从 level 0 开始，累积概率 p = assign_probas[0] = 1 - ml
 *   - 生成 [0,1) 均匀随机数，如果 < p 则升到 level 1，更新 p = assign_probas[1]
 *   - 重复直到随机数 >= p
 *
 * @param idx    索引实例（含 RNG 状态和 assign_probas 表）
 * @param vec_id 向量 ID（当前未使用，保留接口与 FAISS 对齐）
 * @return       分配的层号（0 表示仅在底层）
 */
int32_t faiss_hnsw_random_level(faiss_hnsw_t *idx, int32_t vec_id);

// faiss_hnsw 主结构体（不透明对象的具体定义）
// 字段含义与 FAISS HNSW.h 中 HNSW 类成员一一对应
struct faiss_hnsw {
    int32_t dims;              // 向量维度
    int32_t M;                 // 每个节点在 1+ 层的最大邻居数（典型值 16-32）
    int32_t ef_construction;   // 建图时的搜索宽度
    int32_t ef_search;         // 搜索时的搜索宽度（默认等于 ef_construction）

    distance_metric_t metric;            // 距离度量类型（L2/IP 等）
    quantization_type_t quantization_type;  // 量化类型（FP32/SQ8/PQ 等）

    int32_t n_total;           // 已插入向量数
    int32_t max_level;         // 当前最大层号（0-indexed，初始为 -1）
    int32_t entry_point;       // 入口节点 ID（初始为 -1）

    // 向量存储（内存连续 float 数组，长度 = n_total * dims）
    float *vectors;

    // 量化编码（可选，仅当 quantization_type != NONE 时使用）
    uint8_t *codes;
    int32_t code_size;

    // HNSW 图结构
    int32_t *levels;           // 每个向量所在的层号（0-indexed）
    int32_t *offsets;          // 每个向量在 neighbors 数组中的偏移
    int32_t *neighbors;        // 邻居数组（扁平化存储）
    float *assign_probas;      // 层分配概率表（与 FAISS set_default_probas 一致）
    int32_t assign_probas_size;
    int32_t *cum_nneighbor;    // 每层累计邻居数（cum_nneighbor_per_level）
    int32_t cum_nneighbor_size;

    // 随机数生成器（Mersenne Twister，seed = 12345 与 FAISS 默认一致）
    faiss_hnsw_mt_state_t rng_state;

    // 量化器（统一通过 algo-prod/quantization/quantization.h 的 quantizer_t 接口）
    quantizer_t *quantizer;

    // 删除位图（标记向量是否已删除）
    uint8_t *delete_bitmap;
};

/**
 * HNSW 核心搜索层算法
 *
 * 从 entry_point 开始，在指定 level 层进行贪婪搜索：
 *   1. 将 entry_point 压入 MinimaxHeap
 *   2. 循环弹出当前最小距离的候选，扩展其邻居
 *   3. 直到堆为空
 * 收集堆中所有候选作为结果（最多 result_capacity 个）
 *
 * 与 FAISS HNSW::search_from_candidates 语义一致。
 *
 * @param idx             索引实例
 * @param level           搜索的目标层
 * @param query           查询向量
 * @param ef              搜索宽度（堆容量）
 * @param result_ids      输出 id 数组（容量 result_capacity）
 * @param result_dist     输出距离数组（容量 result_capacity）
 * @param result_capacity 输出数组容量
 * @return                实际返回的结果数（<= result_capacity）
 */
int32_t faiss_hnsw_search_layer(const faiss_hnsw_t *idx, int32_t level, const float *query,
                                 int32_t ef, int32_t *result_ids, float *result_dist,
                                 int32_t result_capacity);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_INTERNAL_H
