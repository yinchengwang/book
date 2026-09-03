// faiss_hnsw.h
// 内存版 HNSW 索引（参考 FAISS HNSW.cpp 重写）
// 提供基于 Hierarchical Navigable Small World 图的近似最近邻搜索

#ifndef FAISS_HNSW_H
#define FAISS_HNSW_H

#include <stdint.h>
#include <stdbool.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

// 不透明的 HNSW 索引结构体类型
// 实际定义在 faiss_hnsw.c 中，对外仅暴露指针
typedef struct faiss_hnsw faiss_hnsw_t;

/**
 * 创建 HNSW 索引实例
 *
 * @param M               每个节点在每层的邻居数（典型值 16-32）
 * @param dims            向量维度
 * @param ef_construction 构建时的搜索宽度（典型值 64-200，越大越精确但越慢）
 * @param metric          距离度量类型（L2 欧氏距离或 IP 内积）
 * @param quant_type      量化类型（FP32 / SQ8 / PQ 等）
 * @return                索引指针，失败返回 NULL
 */
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims, int32_t ef_construction,
                                       distance_metric_t metric, quantization_type_t quant_type);

/**
 * 预分配索引容量（建议在批量插入前调用，避免逐向量 realloc）
 *
 * 该函数一次性扩容 vectors / levels / delete_bitmap / offsets / neighbors，
 * 把每次 add 的 realloc 开销从 O(N) 降到 O(1) 总开销。
 * 百万级场景可将构建时间从不可接受降至分钟级。
 *
 * @param index 索引指针
 * @param n     预期总向量数（>= 当前 n_total）
 * @return      0 成功，-1 失败（参数无效或内存不足）
 */
int32_t faiss_hnsw_index_reserve(faiss_hnsw_t *index, int32_t n);

/**
 * 向索引中添加向量
 *
 * @param index   索引指针
 * @param n       向量数量
 * @param vectors 向量数据，行主序布局，长度 n*dims
 * @return        实际添加的向量数（成功等于 n），失败返回负数
 */
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors);

/**
 * 在索引中搜索最近邻
 *
 * @param index      索引指针
 * @param query      查询向量，长度 dims
 * @param k          返回最近邻的数量
 * @param ef_search  搜索时的搜索宽度（越大越精确但越慢）
 * @param distances  输出距离数组，长度 k
 * @param ids        输出 id 数组，长度 k
 * @return           实际返回的结果数（成功等于 k），失败返回负数
 */
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k,
                                 int32_t ef_search, float *distances, int32_t *ids);

/**
 * 销毁索引并释放所有资源
 *
 * @param index 索引指针
 */
void faiss_hnsw_index_drop(faiss_hnsw_t *index);

/**
 * 获取索引当前已插入的向量数量
 *
 * @param index 索引指针
 * @return      已插入向量数；参数为 NULL 时返回 0
 */
int32_t faiss_hnsw_index_size(const faiss_hnsw_t *index);

/**
 * 获取索引当前的最高层号（0-indexed，空索引时为 -1）
 *
 * @param index 索引指针
 * @return      最大层号；参数为 NULL 时返回 -1
 */
int32_t faiss_hnsw_index_max_level(const faiss_hnsw_t *index);

/**
 * 获取索引当前的入口节点 ID（空索引时为 -1）
 *
 * @param index 索引指针
 * @return      入口节点 ID；参数为 NULL 时返回 -1
 */
int32_t faiss_hnsw_index_entry_point(const faiss_hnsw_t *index);

/**
 * Filter 谓词回调：判断给定的 HNSW 内部 vec_id 是否通过过滤。
 *
 * 由调用方实现（典型场景：从 SQLite metadata 表查询该 vec_id 对应的
 * metadata，判断是否满足 filter_json 条件）。
 *
 * @param vec_id     HNSW 内部向量 ID（0-indexed）
 * @param user_data  调用方自定义数据指针
 * @return           非 0 表示通过（保留），0 表示被过滤
 */
typedef int (*faiss_hnsw_filter_fn)(int32_t vec_id, void *user_data);

/**
 * 带 filter 的 HNSW 搜索（P4-T4.5 新增，末尾 append，ABI 零破坏）。
 *
 * 与 faiss_hnsw_index_search() 等价的搜索语义，但允许对候选 ID
 * 应用自定义谓词过滤：
 *   1. 调 faiss_hnsw_search_layer() 在 level 0 取 K*5+50 个候选
 *   2. 对每个候选调用 filter 回调（filter 非 NULL 时）；
 *      回调返回 0 的候选被丢弃
 *   3. 重读 idx->vectors 中的原始向量，重算 L2 平方距离，取 top-K
 *
 * filter_json 为空时（filter == NULL）行为等同 faiss_hnsw_index_search()。
 *
 * @param idx         HNSW 索引
 * @param query       查询向量（长度 dims）
 * @param k           返回最近邻数量
 * @param filter      filter 回调；NULL 表示不过滤
 * @param user_data   透传给 filter 回调的自定义数据
 * @param out_ids     输出 id 数组（容量 k）
 * @param out_distances 输出距离数组（容量 k）
 * @return            实际命中数（≥ 0）；失败返回 -1
 */
int32_t faiss_hnsw_search_filtered(
    faiss_hnsw_t *idx,
    const float *query,
    int32_t k,
    faiss_hnsw_filter_fn filter,
    void *user_data,
    int32_t *out_ids,
    float *out_distances);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_H