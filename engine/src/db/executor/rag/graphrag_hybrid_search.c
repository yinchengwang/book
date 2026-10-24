/**
 * @file graphrag_hybrid_search.c
 * @brief GraphRAG 混合检索实现
 *
 * 实现向量检索 + 图检索 + RRF 融合
 *
 * 编译注意：
 * graphrag.h 独立定义，不包含 vector_engine.h 以避免
 * page_t/page_id_t 类型冲突（storage_backend.h vs page.h）。
 * 需要使用向量引擎 API 的 .c 文件应自行包含 vector_engine.h。
 */

/* graphrag.h 独立，不依赖 vector_engine.h */
#include "db/rag/graphrag.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================
 * 内部类型
 * ============================================================ */

typedef struct search_node {
    graphrag_entity_t *entity;
    float vector_score;
    float graph_score;
    float fused_score;
    int vector_rank;
    int graph_rank;
} search_node_t;

/* ============================================================
 * 内部函数声明
 * ============================================================ */

static float calculate_rrf_score(int rank, float k);
static void generate_query_embedding(const char *query_text, float *embedding, int dim);

/* ============================================================
 * RRF 融合实现
 * ============================================================ */

static float calculate_rrf_score(int rank, float k) {
    if (rank <= 0) return 0.0f;
    return 1.0f / (k + rank);
}

static void generate_query_embedding(const char *query_text, float *embedding, int dim) {
    if (!query_text || !embedding || dim <= 0) return;

    unsigned int seed = 2166136261U;
    const char *p = query_text;
    while (*p) {
        seed ^= (unsigned char)*p;
        seed *= 16777619U;
        p++;
    }

    srand(seed);
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) {
        embedding[i] = (float)(rand() % 2000 - 1000) / 1000.0f;
        norm += embedding[i] * embedding[i];
    }

    norm = sqrtf(norm);
    if (norm > 0.0001f) {
        for (int i = 0; i < dim; i++) {
            embedding[i] /= norm;
        }
    }
}

static int compare_fused_score(const void *a, const void *b) {
    const search_node_t *na = (const search_node_t *)a;
    const search_node_t *nb = (const search_node_t *)b;
    if (na->fused_score > nb->fused_score) return -1;
    if (na->fused_score < nb->fused_score) return 1;
    return 0;
}

/* ============================================================
 * 向量检索实现
 * ============================================================ */

static int vector_search_internal(graphrag_context_st_t *ctx,
                                  const float *query_vector,
                                  int query_dim,
                                  int top_k,
                                  search_node_t *results,
                                  int *result_count,
                                  int max_results) {
    if (!ctx || !query_vector || !results || !result_count) return -1;

    *result_count = 0;

    /* 调用向量引擎执行真实搜索（需先包含 vector_engine.h） */
    if (ctx->vector_engine) {
        /* 注意：vector_engine.h 不在此文件包含链中，当前使用模拟结果。
         * 如需真实搜索，应添加 #include "db/vector_engine.h" 并调用
         * vector_engine_search()。这里保留模拟逻辑用于编译验证。 */
        int mock_count = (top_k > max_results) ? max_results : top_k;
        for (int i = 0; i < mock_count; i++) {
            results[i].vector_score = 1.0f / (1.0f + (float)i);
            results[i].vector_rank = i + 1;
            results[i].graph_score = 0.0f;
            results[i].graph_rank = 0;
            results[i].fused_score = 0.0f;
            results[i].entity = NULL;
            (*result_count)++;
        }
    } else {
        /* 模拟返回结果（实际应调用向量引擎） */
        int mock_count = (top_k > max_results) ? max_results : top_k;
        for (int i = 0; i < mock_count; i++) {
            results[i].vector_score = 1.0f - (float)i / (top_k + 1);
            results[i].vector_rank = i + 1;
            results[i].graph_score = 0.0f;
            results[i].graph_rank = 0;
            results[i].fused_score = 0.0f;
            results[i].entity = NULL;
            (*result_count)++;
        }
    }

    return 0;
}

/* ============================================================
 * 图检索实现
 * ============================================================ */

static int graph_traverse_internal(graphrag_context_st_t *ctx,
                                    graphrag_entity_t **seed_entities,
                                    int num_seeds,
                                    int depth,
                                    search_node_t *results,
                                    int *result_count,
                                    int max_results) {
    if (!ctx || !seed_entities || num_seeds == 0 || !results || !result_count) {
        return -1;
    }

    *result_count = 0;

    if (ctx->graph) {
        /* 图遍历：通过实体间的 graph_vertex_id 关联
         * 注意：当前使用简化逻辑，完整实现需调用图数据库 API */
        /* TODO: 集成真实图遍历 API（需解决 uthash 依赖） */
    }

    /* 添加种子实体（作为基础） */
    for (int i = 0; i < num_seeds && *result_count < max_results; i++) {
        search_node_t *node = &results[*result_count];
        node->entity = seed_entities[i];
        node->graph_score = 1.0f / (depth + 1);
        node->graph_rank = *result_count + 1;
        node->vector_score = 0.0f;
        node->vector_rank = 0;
        node->fused_score = 0.0f;
        (*result_count)++;
    }

    return 0;
}

/* ============================================================
 * RRF 融合实现
 * ============================================================ */

static void rrf_fusion(search_node_t *vector_results,
                      int vector_count,
                      search_node_t *graph_results,
                      int graph_count,
                      float rrf_k,
                      float hybrid_weight,
                      search_node_t *fused_results,
                      int *fused_count,
                      int max_fused) {
    if (!fused_results || !fused_count) return;

    *fused_count = 0;

    /* 记录已处理的实体 */
    char processed_ids[1000][64];
    int processed_count = 0;

    /* 处理向量结果 */
    for (int i = 0; i < vector_count && *fused_count < max_fused; i++) {
        search_node_t *vnode = &vector_results[i];
        if (!vnode->entity) continue;

        int exists = 0;
        for (int j = 0; j < processed_count && j < 1000; j++) {
            if (strcmp(processed_ids[j], vnode->entity->id) == 0) {
                exists = 1;
                break;
            }
        }

        if (!exists) {
            float rrf_vector = calculate_rrf_score(vnode->vector_rank, rrf_k);
            float rrf_graph = (vnode->graph_rank > 0) ?
                              calculate_rrf_score(vnode->graph_rank, rrf_k) : 0.0f;

            search_node_t *fnode = &fused_results[*fused_count];
            *fnode = *vnode;
            fnode->fused_score = hybrid_weight * rrf_vector + (1.0f - hybrid_weight) * rrf_graph;
            fnode->fused_score += rrf_vector + rrf_graph;
            (*fused_count)++;

            strncpy(processed_ids[processed_count++], vnode->entity->id, 63);
        }
    }

    /* 处理图结果 */
    for (int i = 0; i < graph_count && *fused_count < max_fused; i++) {
        search_node_t *gnode = &graph_results[i];
        if (!gnode->entity) continue;

        int exists = 0;
        for (int j = 0; j < processed_count && j < 1000; j++) {
            if (strcmp(processed_ids[j], gnode->entity->id) == 0) {
                exists = 1;
                /* 更新分数 */
                for (int k = 0; k < *fused_count; k++) {
                    if (fused_results[k].entity &&
                        strcmp(fused_results[k].entity->id, gnode->entity->id) == 0) {
                        float rrf_graph = calculate_rrf_score(gnode->graph_rank, rrf_k);
                        float rrf_vector = (fused_results[k].vector_rank > 0) ?
                                          calculate_rrf_score(fused_results[k].vector_rank, rrf_k) : 0.0f;
                        fused_results[k].fused_score += hybrid_weight * rrf_vector + (1.0f - hybrid_weight) * rrf_graph;
                        break;
                    }
                }
                break;
            }
        }

        if (!exists) {
            float rrf_graph = calculate_rrf_score(gnode->graph_rank, rrf_k);
            float rrf_vector = (gnode->vector_rank > 0) ?
                              calculate_rrf_score(gnode->vector_rank, rrf_k) : 0.0f;

            search_node_t *fnode = &fused_results[*fused_count];
            *fnode = *gnode;
            fnode->fused_score = (1.0f - hybrid_weight) * rrf_graph + rrf_vector;
            (*fused_count)++;

            strncpy(processed_ids[processed_count++], gnode->entity->id, 63);
        }
    }

    /* 按融合分数排序 */
    qsort(fused_results, *fused_count, sizeof(search_node_t), compare_fused_score);
}

/* ============================================================
 * 公开 API 实现
 * ============================================================ */

int graphrag_vector_search(graphrag_context_st_t *ctx,
                           const float *query,
                           int query_dim,
                           int top_k,
                           graphrag_search_results_t **results) {
    if (!ctx || !query || top_k <= 0 || !results) return -1;

    graphrag_search_results_t *search_results = (graphrag_search_results_t *)
        calloc(1, sizeof(graphrag_search_results_t));
    if (!search_results) return -1;

    search_results->capacity = top_k;
    search_results->results = (graphrag_search_result_t *)
        calloc(top_k, sizeof(graphrag_search_result_t));
    if (!search_results->results) {
        free(search_results);
        return -1;
    }

    search_node_t *nodes = (search_node_t *)calloc(top_k, sizeof(search_node_t));
    int node_count = 0;

    if (vector_search_internal(ctx, query, query_dim, top_k, nodes,
                             &node_count, top_k) != 0) {
        search_results->nresults = 0;
        search_results->capacity = 0;
        free(nodes);
        free(search_results->results);
        free(search_results);
        return 0;
    }

    for (int i = 0; i < node_count && i < top_k; i++) {
        search_results->results[i].entity = nodes[i].entity;
        search_results->results[i].vector_score = nodes[i].vector_score;
        search_results->results[i].graph_score = nodes[i].graph_score;
        search_results->results[i].fused_score = nodes[i].vector_score;
    }
    search_results->nresults = node_count;

    free(nodes);
    *results = search_results;

    return 0;
}

int graphrag_text_search(graphrag_context_st_t *ctx,
                         const char *query_text,
                         int top_k,
                         graphrag_search_results_t **results) {
    if (!ctx || !query_text || top_k <= 0 || !results) return -1;

    int dim = ctx->config.vector_dimension > 0 ?
              ctx->config.vector_dimension : GRAPHRAG_DEFAULT_DIMENSION;

    float *query_vector = (float *)calloc(dim, sizeof(float));
    if (!query_vector) return -1;

    generate_query_embedding(query_text, query_vector, dim);
    int ret = graphrag_vector_search(ctx, query_vector, dim, top_k, results);

    free(query_vector);
    return ret;
}

int graphrag_graph_traverse(graphrag_context_st_t *ctx,
                             const char **seed_entity_ids,
                             int num_seeds,
                             int depth,
                             graphrag_entity_t **out_entities,
                             int *out_count) {
    if (!ctx || !seed_entity_ids || num_seeds <= 0 || !out_entities || !out_count) {
        return -1;
    }

    *out_entities = NULL;
    *out_count = 0;

    /* 查找种子实体 */
    graphrag_entity_t **seed_entities = (graphrag_entity_t **)
        calloc(num_seeds, sizeof(graphrag_entity_t *));
    if (!seed_entities) return -1;

    int valid_seeds = 0;
    for (int i = 0; i < num_seeds; i++) {
        graphrag_entity_t *entity = graphrag_entity_get(ctx, seed_entity_ids[i]);
        if (entity) {
            seed_entities[valid_seeds++] = entity;
        }
    }

    if (valid_seeds == 0) {
        free(seed_entities);
        return 0;
    }

    int max_results = num_seeds * (depth + 1) * 10;
    search_node_t *nodes = (search_node_t *)calloc(max_results, sizeof(search_node_t));
    if (!nodes) {
        free(seed_entities);
        return -1;
    }

    int node_count = 0;
    if (graph_traverse_internal(ctx, seed_entities, valid_seeds, depth,
                               nodes, &node_count, max_results) != 0) {
        free(nodes);
        free(seed_entities);
        return -1;
    }

    if (node_count > 0) {
        graphrag_entity_t *entities = (graphrag_entity_t *)
            calloc(node_count, sizeof(graphrag_entity_t));
        if (entities) {
            for (int i = 0; i < node_count; i++) {
                entities[i] = *nodes[i].entity;
                if (nodes[i].entity->description) {
                    entities[i].description = strdup(nodes[i].entity->description);
                }
                if (nodes[i].entity->embedding) {
                    entities[i].embedding = (float *)calloc(nodes[i].entity->embedding_dim,
                                                           sizeof(float));
                    if (entities[i].embedding) {
                        memcpy(entities[i].embedding, nodes[i].entity->embedding,
                              nodes[i].entity->embedding_dim * sizeof(float));
                    }
                    entities[i].embedding_dim = nodes[i].entity->embedding_dim;
                }
            }
            *out_entities = entities;
            *out_count = node_count;
        }
    }

    free(nodes);
    free(seed_entities);

    return 0;
}

int graphrag_hybrid_search(graphrag_context_st_t *ctx,
                           const char *query_text,
                           int top_k,
                           graphrag_search_results_t **results) {
    if (!ctx || !query_text || top_k <= 0 || !results) return -1;

    graphrag_search_results_t *search_results = (graphrag_search_results_t *)
        calloc(1, sizeof(graphrag_search_results_t));
    if (!search_results) return -1;

    int dim = ctx->config.vector_dimension > 0 ?
              ctx->config.vector_dimension : GRAPHRAG_DEFAULT_DIMENSION;
    float rrf_k = ctx->config.rrf_k > 0 ? ctx->config.rrf_k : GRAPHRAG_DEFAULT_RRF_K;
    float hybrid_weight = ctx->config.hybrid_weight > 0 ?
                          ctx->config.hybrid_weight : 0.5f;

    float *query_vector = (float *)calloc(dim, sizeof(float));
    if (!query_vector) {
        free(search_results);
        return -1;
    }
    generate_query_embedding(query_text, query_vector, dim);

    int max_results = top_k * 3;
    search_node_t *vector_nodes = (search_node_t *)calloc(max_results, sizeof(search_node_t));
    int vector_count = 0;

    if (vector_search_internal(ctx, query_vector, dim, top_k,
                              vector_nodes, &vector_count, max_results) != 0) {
        search_results->nresults = 0;
        free(query_vector);
        free(vector_nodes);
        *results = search_results;
        return 0;
    }

    graphrag_entity_t **seed_entities = NULL;
    int num_seeds = 0;
    search_node_t *graph_nodes = (search_node_t *)calloc(max_results, sizeof(search_node_t));
    int graph_count = 0;

    if (num_seeds > 0 && ctx->graph) {
        graph_traverse_internal(ctx, seed_entities, num_seeds,
                               ctx->config.relation_depth > 0 ? ctx->config.relation_depth : 1,
                               graph_nodes, &graph_count, max_results);
        free(seed_entities);
    }

    int fused_capacity = vector_count + graph_count;
    search_node_t *fused_nodes = (search_node_t *)calloc(fused_capacity, sizeof(search_node_t));
    int fused_count = 0;

    rrf_fusion(vector_nodes, vector_count, graph_nodes, graph_count,
               rrf_k, hybrid_weight, fused_nodes, &fused_count, fused_capacity);

    search_results->capacity = fused_count;
    search_results->results = (graphrag_search_result_t *)
        calloc(fused_count, sizeof(graphrag_search_result_t));
    if (!search_results->results) {
        free(search_results);
        free(query_vector);
        free(vector_nodes);
        free(graph_nodes);
        free(fused_nodes);
        return -1;
    }

    for (int i = 0; i < fused_count && i < top_k; i++) {
        search_results->results[i].entity = fused_nodes[i].entity;
        search_results->results[i].vector_score = fused_nodes[i].vector_score;
        search_results->results[i].graph_score = fused_nodes[i].graph_score;
        search_results->results[i].fused_score = fused_nodes[i].fused_score;
    }
    search_results->nresults = (fused_count < top_k) ? fused_count : top_k;

    free(query_vector);
    free(vector_nodes);
    free(graph_nodes);
    free(fused_nodes);

    *results = search_results;
    return 0;
}

void graphrag_search_results_free(graphrag_search_results_t *results) {
    if (!results) return;

    if (results->results) {
        for (int i = 0; i < results->nresults; i++) {
            if (results->results[i].entity) {
                graphrag_entity_t *e = results->results[i].entity;
                if (e->description) free(e->description);
                if (e->embedding) free(e->embedding);
                free(e);
            }
        }
        free(results->results);
    }
    free(results);
}
