/**
 * @file doc_vector.h
 * @brief 文档向量检索头文件
 *
 * 支持文档向量字段、嵌入存储、语义搜索、BM25+向量混合检索、分数融合
 */
#ifndef DB_DOC_VECTOR_H
#define DB_DOC_VECTOR_H

#include "db/storage/doc/doc_engine.h"
#include "db/index/vector_index/hnsw.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 文档向量字段
 * ======================================================================== */

/**
 * @brief 向量字段配置
 */
typedef struct DocVectorFieldConfig_s {
    char field_name[64];      /**< 字段名 */
    int vector_dim;           /**< 向量维度 */
    bool is_indexed;          /**< 是否建立向量索引 */
    int max_elements;         /**< 最大元素数（HNSW） */
    int m_construction;       /**< 构建时的连接数 */
    int ef_construction;      /**< 构建时的搜索宽度 */
    float normalize;          /**< 是否归一化（0/1） */
} DocVectorFieldConfig;

/* ========================================================================
 * 文档嵌入存储
 * ======================================================================== */

/** 文档向量嵌入 */
typedef struct DocEmbedding_s {
    char doc_id[128];         /**< 文档 ID */
    char field_name[64];      /**< 字段名 */
    float *vector;            /**< 向量数据 */
    int dimension;            /**< 向量维度 */
    uint32_t doc_size;        /**< 文档大小（用于 BM25） */
    double bm25_score;        /**< BM25 分数 */
    double vector_score;      /**< 向量相似度分数 */
    double combined_score;    /**< 综合分数 */
} DocEmbedding;

/** 嵌入存储 */
typedef struct DocEmbeddingStore_s {
    DocEmbedding *embeddings; /**< 嵌入数组 */
    size_t num_embeddings;    /**< 嵌入数量 */
    size_t capacity;          /**< 容量 */
    void *mem_pool;           /**< 内存池 */
} DocEmbeddingStore;

/**
 * @brief 创建嵌入存储
 */
DocEmbeddingStore *doc_embedding_store_create(void *mem_pool);

/**
 * @brief 销毁嵌入存储
 */
void doc_embedding_store_destroy(DocEmbeddingStore *store);

/**
 * @brief 添加嵌入
 */
int doc_embedding_store_add(DocEmbeddingStore *store,
                           const char *doc_id,
                           const char *field_name,
                           const float *vector,
                           int dimension,
                           uint32_t doc_size);

/**
 * @brief 获取嵌入
 */
DocEmbedding *doc_embedding_store_get(DocEmbeddingStore *store,
                                      const char *doc_id,
                                      const char *field_name);

/**
 * @brief 删除嵌入
 */
int doc_embedding_store_remove(DocEmbeddingStore *store,
                               const char *doc_id,
                               const char *field_name);

/**
 * @brief 获取嵌入数量
 */
size_t doc_embedding_store_count(DocEmbeddingStore *store);

/* ========================================================================
 * 语义搜索
 * ======================================================================== */

/** 搜索结果 */
typedef struct DocVectorSearchResult_s {
    char doc_id[128];         /**< 文档 ID */
    char *snippet;            /**< 文档片段 */
    double vector_score;      /**< 向量相似度 */
    double bm25_score;        /**< BM25 分数 */
    double combined_score;    /**< 综合分数 */
    double rank;              /**< 排名 */
} DocVectorSearchResult;

/** 搜索选项 */
typedef struct DocVectorSearchOptions_s {
    int top_k;                /**< 返回 Top-K 结果 */
    float min_score;          /**< 最小分数阈值 */
    bool include_scores;      /**< 包含各分数 */
    bool include_snippets;    /**< 包含片段 */
    int snippet_length;       /**< 片段长度 */
    bool use_rerank;          /**< 是否重排序 */
} DocVectorSearchOptions;

/**
 * @brief 语义搜索结果
 */
typedef struct DocSemanticSearchResult_s {
    DocVectorSearchResult *results; /**< 结果数组 */
    size_t num_results;      /**< 结果数量 */
    size_t total_hits;       /**< 总命中数 */
    double query_time_ms;    /**< 查询耗时（毫秒） */
} DocSemanticSearchResult;

/**
 * @brief 执行语义搜索
 * @param store 嵌入存储
 * @param query_vector 查询向量
 * @param query_dim 向量维度
 * @param options 搜索选项
 * @return 搜索结果
 */
DocSemanticSearchResult *doc_semantic_search(DocEmbeddingStore *store,
                                            const float *query_vector,
                                            int query_dim,
                                            const DocVectorSearchOptions *options);

/**
 * @brief 释放语义搜索结果
 */
void doc_semantic_search_free(DocSemanticSearchResult *result);

/* ========================================================================
 * BM25 + 向量混合检索
 * ======================================================================== */

/** 混合检索模式 */
typedef enum DocHybridMode_e {
    DOC_HYBRID_RRF = 0,      /**< Reciprocal Rank Fusion */
    DOC_HYBRID_WEIGHTED,     /**< 加权分数融合 */
    DOC_HYBRID_RERANK,       /**< 重排序 */
} DocHybridMode;

/** 混合检索配置 */
typedef struct DocHybridConfig_s {
    DocHybridMode mode;      /**< 混合模式 */
    double vector_weight;    /**< 向量权重 */
    double bm25_weight;      /**< BM25 权重 */
    int rrf_k;               /**< RRF 参数 k */
    bool use_adaptive;       /**< 自适应权重 */
} DocHybridConfig;

/**
 * @brief 混合检索上下文
 */
typedef struct DocHybridSearcher_s {
    DocEmbeddingStore *embeddings; /**< 嵌入存储 */
    DocHybridConfig config;        /**< 配置 */
    void *hnsw_index;              /**< HNSW 索引 */
    void *bm25_index;              /**< BM25 索引 */
} DocHybridSearcher;

/**
 * @brief 创建混合检索器
 * @param store 嵌入存储
 * @param config 配置（NULL 使用默认）
 * @return 检索器句柄
 */
DocHybridSearcher *doc_hybrid_searcher_create(DocEmbeddingStore *store,
                                             const DocHybridConfig *config);

/**
 * @brief 销毁混合检索器
 */
void doc_hybrid_searcher_destroy(DocHybridSearcher *searcher);

/**
 * @brief 设置 BM25 索引
 */
int doc_hybrid_set_bm25_index(DocHybridSearcher *searcher, void *bm25_index);

/**
 * @brief 执行混合搜索
 * @param searcher 检索器
 * @param query_vector 查询向量（可为 NULL）
 * @param query_dim 向量维度
 * @param query_text 文本查询（可为 NULL）
 * @param options 搜索选项
 * @return 搜索结果
 */
DocSemanticSearchResult *doc_hybrid_search(DocHybridSearcher *searcher,
                                          const float *query_vector,
                                          int query_dim,
                                          const char *query_text,
                                          const DocVectorSearchOptions *options);

/* ========================================================================
 * 分数融合
 * ======================================================================== */

/**
 * @brief RRF 融合
 * @param doc_scores 文档分数数组（每行一个列表的排名）
 * @param num_lists 列表数量
 * @param k RRF 参数
 * @param out_scores 输出分数
 * @param num_docs 文档数量
 */
void doc_score_fusion_rrf(const double **doc_scores,
                         size_t num_lists,
                         int k,
                         double *out_scores,
                         size_t num_docs);

/**
 * @brief 加权融合
 * @param doc_scores 文档分数数组
 * @param weights 权重数组
 * @param num_lists 列表数量
 * @param out_scores 输出分数
 * @param num_docs 文档数量
 */
void doc_score_fusion_weighted(const double **doc_scores,
                              const double *weights,
                              size_t num_lists,
                              double *out_scores,
                              size_t num_docs);

/**
 * @brief 归一化分数
 * @param scores 分数数组
 * @param num_docs 文档数量
 */
void doc_normalize_scores(double *scores, size_t num_docs);

/**
 * @brief 综合排序
 * @param results 结果数组
 * @param num_results 结果数量
 * @param config 配置
 */
void doc_rank_results(DocVectorSearchResult *results,
                     size_t num_results,
                     const DocHybridConfig *config);

/* ========================================================================
 * SQL 函数扩展
 * ======================================================================== */

/**
 * @brief DOCUMENT_SEARCH 函数
 *
 * DOCUMENT_SEARCH(collection, query_vector, query_text, top_k)
 *
 * 示例:
 *   SELECT doc_id, score
 *   FROM DOCUMENT_SEARCH('articles', '[0.1, 0.2, ...]', 'database', 10)
 *
 * @param collection 集合名
 * @param query_vector 查询向量（JSON 格式）
 * @param query_text 文本查询
 * @param top_k 返回数量
 * @param results 输出结果
 * @return 结果数量
 */
size_t doc_sql_search(const char *collection,
                     const char *query_vector,
                     const char *query_text,
                     int top_k,
                     DocVectorSearchResult **results);

/**
 * @brief DOCUMENT_EMBED 函数 - 生成文档嵌入
 *
 * DOCUMENT_EMBED(document, field_name, model)
 *
 * @param document 文档 JSON
 * @param field_name 字段名
 * @param model 模型名
 * @param out_vector 输出向量
 * @param out_dim 输出维度
 * @return 0 成功，-1 失败
 */
int doc_sql_embed(const char *document,
                 const char *field_name,
                 const char *model,
                 float **out_vector,
                 int *out_dim);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_VECTOR_H */
