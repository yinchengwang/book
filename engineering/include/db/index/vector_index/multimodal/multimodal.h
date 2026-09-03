/*
 * multimodal.h
 *
 * 多模态向量索引
 *
 * 功能：
 * - 支持文本、图像、音频等多模态数据的联合检索
 * - 跨模态搜索
 * - 稀疏+密集混合搜索（BM25 + HNSW）
 *
 * 使用示例：
 * @code
 *   multimodal_index_t *idx = multimodal_create();
 *
 *   // 注册模态
 *   multimodal_register_modality(idx, "text", INDEX_HNSW);
 *   multimodal_register_modality(idx, "image", INDEX_HNSW);
 *
 *   // 添加向量
 *   multimodal_add(idx, 1, "text", text_vector);
 *   multimodal_add(idx, 1, "image", image_vector);
 *
 *   // 跨模态搜索（用文本搜索图像）
 *   int results[10];
 *   float scores[10];
 *   multimodal_cross_search(idx, "text", text_query, 10, results, scores);
 *
 *   // 混合搜索
 *   multimodal_hybrid_search(idx, "向量 数据库", dense_query, 0.3, 10, results, scores);
 *
 *   multimodal_destroy(idx);
 * @endcode
 */

#ifndef DB_INDEX_VECTOR_MULTIMODAL_H
#define DB_INDEX_VECTOR_MULTIMODAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct multimodal_index multimodal_index_t;

/* ── 索引类型 ── */
typedef enum {
    INDEX_NONE = 0,
    INDEX_HNSW = 1,
    INDEX_FLAT = 2,
    INDEX_BM25 = 3,
} index_type_t;

/* ── 融合类型 ── */
typedef enum {
    FUSION_RRF = 0,           /* Reciprocal Rank Fusion */
    FUSION_WEIGHTED_SUM = 1,  /* 加权和 */
} fusion_type_t;

/* ── 核心 API ── */

/**
 * 创建多模态索引
 * @return 多模态索引指针，失败返回 NULL
 */
multimodal_index_t *multimodal_create(void);

/**
 * 注册模态
 * @param idx 多模态索引
 * @param name 模态名称
 * @param index_type 索引类型
 * @param dims 向量维度
 * @return 0 成功，-1 失败
 */
int multimodal_register_modality(multimodal_index_t *idx, const char *name,
                               index_type_t index_type, int dims);

/**
 * 添加向量
 * @param idx 多模态索引
 * @param id 实体 ID
 * @param modality 模态名称
 * @param vector 向量
 * @param text 文本（可选，用于 BM25）
 * @return 0 成功，-1 失败
 */
int multimodal_add(multimodal_index_t *idx, int id, const char *modality,
                 const float *vector, const char *text);

/**
 * 跨模态搜索
 * @param idx 多模态索引
 * @param query_modality 查询模态
 * @param query_vec 查询向量
 * @param search_modality 搜索模态
 * @param k 返回数量
 * @param ids 输出：匹配的实体 ID
 * @param scores 输出：分数
 * @return 实际返回数量
 */
int multimodal_cross_search(const multimodal_index_t *idx,
                          const char *query_modality, const float *query_vec,
                          const char *search_modality, int k, int *ids, float *scores);

/**
 * 多模态融合搜索
 * @param idx 多模态索引
 * @param modalities 模态数组
 * @param query_vecs 查询向量数组
 * @param n_modalities 模态数量
 * @param k 返回数量
 * @param ids 输出：匹配的实体 ID
 * @param scores 输出：融合分数
 * @return 实际返回数量
 */
int multimodal_fusion_search(multimodal_index_t *idx,
                           const char **modalities, const float **query_vecs,
                           int n_modalities, int k, int *ids, float *scores);

/**
 * 混合搜索（稀疏 + 密集）
 * @param idx 多模态索引
 * @param text_query 文本查询
 * @param vector_query 向量查询
 * @param alpha 文本权重 (0.0-1.0)
 * @param k 返回数量
 * @param ids 输出：匹配的实体 ID
 * @param scores 输出：综合分数
 * @return 实际返回数量
 */
int multimodal_hybrid_search(multimodal_index_t *idx,
                           const char *text_query, const float *vector_query,
                           float alpha, int k, int *ids, float *scores);

/**
 * 设置融合类型
 * @param idx 多模态索引
 * @param type 融合类型
 */
void multimodal_set_fusion(multimodal_index_t *idx, fusion_type_t type);

/**
 * 获取索引统计信息
 * @param idx 多模态索引
 * @param out_n_modalities 输出：模态数量
 * @param out_total_vectors 输出：向量总数
 */
void multimodal_stats(const multimodal_index_t *idx, int *out_n_modalities, int *out_total_vectors);

/**
 * 销毁多模态索引
 * @param idx 多模态索引
 */
void multimodal_destroy(multimodal_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_VECTOR_MULTIMODAL_H */
