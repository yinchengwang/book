// faiss_hnsw_segment.h
// COW segment + collection 公开接口（C5.1-C5.3）
//
// faiss_hnsw_collection_t 是面向增量写入的 HNSW 集合容器：
//   - 向量先写入内存缓冲，超过 buf_threshold 后 compact 成独立 segment
//   - 搜索时遍历所有 segment + 缓冲，各自线性扫描后合并 top-k
//
// segment 与 faiss_hnsw_t 的生命周期绑定：
//   - faiss_hnsw_seal_segment() / faiss_hnsw_get_active_segment()
//     提供 COW 语义的 segment 转换（当前实现为占位，直接返回原指针）
//   - 完整 COW（不可变 buffer + 原子指针切换）在后续变更中展开

#ifndef FAISS_HNSW_SEGMENT_H
#define FAISS_HNSW_SEGMENT_H

#include <stdint.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

// 单个 segment（一个不可变的 faiss_hnsw_t 图 + 向量 ID 范围）
typedef struct {
    void *index;         // faiss_hnsw_t*（不透明指针，仅 COW 层访问）
    int32_t start_id;    // 该 segment 内向量的起始全局 ID
    int32_t end_id;      // 该 segment 内向量的末尾全局 ID
    int32_t n_at_compact; // compact 时记录的向量数（调试/统计用）
} faiss_hnsw_segment_t;

// COW 集合容器
typedef struct {
    faiss_hnsw_segment_t *segs;   // 已 compact 的 segment 数组
    int32_t n_segs;               // 当前 segment 数量
    int32_t cap_segs;             // segs 数组容量
    int32_t M;                    // HNSW M 参数
    int32_t dims;                 // 向量维度
    distance_metric_t metric;     // 距离度量

    int32_t n_total;              // 已写入全向量数（含缓冲）
    int32_t next_id;              // 下一个分配的全局 ID

    int32_t buf_threshold;        // 触发 compact 的缓冲向量数
    int32_t buf_cap;              // buf_vectors / buf_ids 容量
    int32_t buf_count;            // 当前缓冲中的向量数
    float *buf_vectors;           // 缓冲中的向量数据（行主序）
    int32_t *buf_ids;             // 缓冲中向量对应的全局 ID
} faiss_hnsw_collection_t;

/**
 * 创建 COW 集合
 *
 * @param M                  HNSW 每层最大邻居数
 * @param dims               向量维度
 * @param ef_construction    构建时的搜索宽度
 * @param metric             距离度量（DISTANCE_METRIC_L2_SQUARED / COSINE / IP）
 * @param segment_threshold  触发 compact 的缓冲向量数（<=0 时默认 1024）
 * @return                   集合指针，失败返回 NULL
 */
faiss_hnsw_collection_t *faiss_hnsw_collection_create(
    int32_t M, int32_t dims, int32_t ef_construction,
    distance_metric_t metric, int32_t segment_threshold);

/**
 * 销毁集合并释放所有资源
 */
void faiss_hnsw_collection_destroy(faiss_hnsw_collection_t *col);

/**
 * 向集合追加向量（先缓冲，超阈值后自动 compact）
 *
 * @param n       向量数量
 * @param vectors 向量数据，行主序，长度 n*dims
 * @return        0 成功，-1 失败
 */
int faiss_hnsw_collection_add(faiss_hnsw_collection_t *col,
                              int32_t n, const float *vectors);

/**
 * 手动触发 compact（将当前缓冲写入新 segment）
 *
 * @return 0 成功，-1 失败（缓冲为空或内存不足）
 */
int faiss_hnsw_collection_compact(faiss_hnsw_collection_t *col);

/**
 * 在集合中搜索 top-k 最近邻
 *
 * @param k          返回数量
 * @param query      查询向量（长度 dims）
 * @param distances  输出距离（容量 k）
 * @param ids        输出全局 ID（容量 k）
 * @return           实际返回数量（<= k）
 */
int32_t faiss_hnsw_collection_search(faiss_hnsw_collection_t *col,
                                     const float *query, int32_t k,
                                     float *distances, int32_t *ids);

/**
 * 获取集合中所有向量数（含缓冲中未 compact 的）
 */
int32_t faiss_hnsw_collection_ntotal(const faiss_hnsw_collection_t *col);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_SEGMENT_H
