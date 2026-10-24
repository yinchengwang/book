/**
 * @file graphrag.h
 * @brief GraphRAG 接口定义
 *
 * GraphRAG = Graph + Vector RAG
 * 结合图数据库和向量数据库进行增强检索
 *
 * 核心功能：
 * 1. 实体提取（Entity Extraction）- 从文档中提取命名实体
 * 2. 关系提取（Relation Extraction）- 提取实体间关系
 * 3. 混合检索（Hybrid Search）- 向量 + 图检索融合
 * 4. 上下文组装（Context Assembly）- 生成 LLM 上下文
 */
#ifndef DB_GRAPHRAG_H
#define DB_GRAPHRAG_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 默认配置值 */
#define GRAPHRAG_DEFAULT_DIMENSION      768
#define GRAPHRAG_DEFAULT_TOP_K          10
#define GRAPHRAG_DEFAULT_ENTITY_TOP_K   5
#define GRAPHRAG_DEFAULT_REL_DEPTH      2
#define GRAPHRAG_DEFAULT_RRF_K          60
#define GRAPHRAG_MAX_ENTITY_NAME_LEN    256
#define GRAPHRAG_MAX_REL_TYPE_LEN       128

/** 无效 ID 标记（与 graph/types.h 中的 GRAPH_INVALID_ID 一致） */
#ifndef GRAPH_INVALID_ID
#define GRAPH_INVALID_ID ((uint64_t)0)
#endif

/* ============================================================
 * 度量类型
 * ============================================================ */

/* vector_metric_t 在 vector_engine.h 中定义（METRIC_L2=0, METRIC_COSINE=1, METRIC_DOT=2）。
 * graphrag.h 不包含 vector_engine.h 以避免 page_t/page_id_t 类型冲突。
 * graphrag_config_t.metric 使用 int 存储，值与 vector_metric_t 枚举值相同。
 * .c 文件如果需要显式使用 vector_metric_t 类型，应先包含 vector_engine.h。 */

/* ============================================================
 * 实体类型定义
 * ============================================================ */

/** 实体类型 */
typedef enum graphrag_entity_type {
    GRAPHRAG_ENTITY_PERSON = 0,
    GRAPHRAG_ENTITY_ORGANIZATION = 1,
    GRAPHRAG_ENTITY_LOCATION = 2,
    GRAPHRAG_ENTITY_CONCEPT = 3,
    GRAPHRAG_ENTITY_EVENT = 4,
    GRAPHRAG_ENTITY_OTHER = 5
} graphrag_entity_type_t;

/**
 * @brief GraphRAG 实体
 */
typedef struct graphrag_entity {
    char id[64];
    char name[GRAPHRAG_MAX_ENTITY_NAME_LEN];
    graphrag_entity_type_t type;
    char *description;
    float *embedding;
    int embedding_dim;
    float confidence;
    char source_chunk_id[64];
    uint64_t graph_vertex_id;
} graphrag_entity_t;

/**
 * @brief GraphRAG 关系
 */
typedef struct graphrag_relation {
    char id[64];
    graphrag_entity_t *src;
    graphrag_entity_t *dst;
    char rel_type[GRAPHRAG_MAX_REL_TYPE_LEN];
    char *description;
    float *embedding;
    int embedding_dim;
    float confidence;
    uint64_t graph_edge_id;
} graphrag_relation_t;

/* ============================================================
 * 搜索结果类型
 * ============================================================ */

typedef struct graphrag_search_result {
    graphrag_entity_t *entity;
    float vector_score;
    float graph_score;
    float fused_score;
} graphrag_search_result_t;

typedef struct graphrag_search_results {
    graphrag_search_result_t *results;
    int nresults;
    int capacity;
    int64_t processing_time_ms;
} graphrag_search_results_t;

typedef struct graphrag_context_item {
    char type;
    union {
        graphrag_entity_t *entity;
        graphrag_relation_t *relation;
    } data;
    float score;
} graphrag_context_item_t;

typedef struct graphrag_context {
    graphrag_context_item_t *items;
    int nitems;
    int capacity;
    char *graph_summary;
    char *text_chunks;
} graphrag_context_t;

/* ============================================================
 * 配置结构
 * ============================================================ */

typedef struct graphrag_config {
    char vector_collection[128];
    int vector_dimension;
    int metric; /* 值对应 vector_metric_t: METRIC_L2=0, METRIC_COSINE=1, METRIC_DOT=2 */

    char graph_name[128];
    char entity_label[64];
    char relation_type[64];

    int top_k;
    int entity_top_k;
    int relation_depth;
    float rrf_k;
    float hybrid_weight;

    int enable_ner;
    int enable_re;

    int max_context_items;
    int include_graph_summary;
    int include_text_chunks;
} graphrag_config_t;

/* ============================================================
 * GraphRAG 执行上下文
 * ============================================================ */

typedef struct graphrag_context_st {
    graphrag_config_t config;
    void *graph;
    void *vector_engine;
    char *data_dir;
    int initialized;
    int64_t total_queries;
    int64_t total_entities;
    int64_t total_relations;
} graphrag_context_st_t;

/* ============================================================
 * API 函数声明
 * ============================================================ */

graphrag_context_st_t *graphrag_create(const graphrag_config_t *config);
void graphrag_destroy(graphrag_context_st_t *ctx);
int graphrag_init(graphrag_context_st_t *ctx);
void graphrag_config_init_defaults(graphrag_config_t *config);
const char *graphrag_errmsg(const graphrag_context_st_t *ctx);

int graphrag_extract_entities(graphrag_context_st_t *ctx,
                              const char *text,
                              int text_len,
                              const char *chunk_id,
                              graphrag_entity_t **out_entities,
                              int *out_count);
void graphrag_entities_free(graphrag_entity_t *entities, int count);
float *graphrag_entity_get_embedding(graphrag_entity_t *entity);
int graphrag_entity_get_embedding_dim(const graphrag_entity_t *entity);

int graphrag_extract_relations(graphrag_context_st_t *ctx,
                               graphrag_entity_t *entities,
                               int num_entities,
                               const char *text,
                               graphrag_relation_t **out_relations,
                               int *out_count);
void graphrag_relations_free(graphrag_relation_t *relations, int count);
float *graphrag_relation_get_embedding(graphrag_relation_t *relation);

int graphrag_entities_store(graphrag_context_st_t *ctx,
                            graphrag_entity_t *entities,
                            int count);
int graphrag_relations_store(graphrag_context_st_t *ctx,
                              graphrag_relation_t *relations,
                              int count);
graphrag_entity_t *graphrag_entity_get(graphrag_context_st_t *ctx,
                                        const char *entity_id);
int graphrag_entity_get_neighbors(graphrag_context_st_t *ctx,
                                   graphrag_entity_t *entity,
                                   int depth,
                                   graphrag_entity_t **out_entities,
                                   int *out_count);

int graphrag_vector_search(graphrag_context_st_t *ctx,
                            const float *query,
                            int query_dim,
                            int top_k,
                            graphrag_search_results_t **results);
int graphrag_text_search(graphrag_context_st_t *ctx,
                         const char *query_text,
                         int top_k,
                         graphrag_search_results_t **results);
int graphrag_graph_traverse(graphrag_context_st_t *ctx,
                            const char **seed_entity_ids,
                            int num_seeds,
                            int depth,
                            graphrag_entity_t **out_entities,
                            int *out_count);
int graphrag_hybrid_search(graphrag_context_st_t *ctx,
                           const char *query_text,
                           int top_k,
                           graphrag_search_results_t **results);
void graphrag_search_results_free(graphrag_search_results_t *results);

graphrag_context_t *graphrag_context_assemble(graphrag_context_st_t *ctx,
                                               graphrag_search_results_t *search_results,
                                               int max_items);
void graphrag_context_free(graphrag_context_t *context);
int graphrag_context_build_prompt(const graphrag_context_t *context,
                                  const char *query,
                                  char *output,
                                  int output_size);
int graphrag_context_to_string(const graphrag_context_t *context,
                               char *output,
                               int output_size);

typedef struct graphrag_stats {
    int64_t total_queries;
    int64_t total_entities;
    int64_t total_relations;
    int64_t total_search_time_ms;
} graphrag_stats_t;

void graphrag_get_stats(const graphrag_context_st_t *ctx,
                        graphrag_stats_t *stats);

graphrag_context_t *graphrag_query(graphrag_context_st_t *ctx,
                                   const char *query_text,
                                   int top_k);

int graphrag_index_document(graphrag_context_st_t *ctx,
                            const char *text,
                            const char *doc_id,
                            int chunk_size,
                            int chunk_overlap);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPHRAG_H */
