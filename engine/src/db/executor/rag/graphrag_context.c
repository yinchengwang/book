/**
 * @file graphrag_context.c
 * @brief GraphRAG 上下文组装实现
 *
 * 将检索结果组装为 LLM 上下文
 */

#include "db/rag/graphrag.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

#define RAG_DEFAULT_CHUNK_SIZE     512
#define RAG_DEFAULT_CHUNK_OVERLAP  64

static int assemble_context_items(graphrag_search_results_t *search_results,
                                 int max_items,
                                 graphrag_context_item_t *items,
                                 int *out_count);
static char *build_entity_text(graphrag_entity_t *entity);
static char *build_relation_text(graphrag_relation_t *relation);
static char *build_graph_summary(graphrag_context_st_t *ctx,
                                graphrag_context_item_t *items,
                                int num_items);
static char *build_text_chunks(graphrag_context_st_t *ctx,
                               graphrag_search_results_t *search_results);

/* ============================================================
 * 上下文组装实现
 * ============================================================ */

/**
 * @brief 从搜索结果组装上下文项
 */
static int assemble_context_items(graphrag_search_results_t *search_results,
                                 int max_items,
                                 graphrag_context_item_t *items,
                                 int *out_count) {
    if (!search_results || !items || !out_count) return -1;

    *out_count = 0;

    if (!search_results->results || search_results->nresults == 0) {
        return 0;
    }

    /* 按分数排序（已经在 RRF 融合时排序） */
    int count = search_results->nresults;
    if (count > max_items) count = max_items;

    for (int i = 0; i < count; i++) {
        graphrag_search_result_t *result = &search_results->results[i];
        graphrag_context_item_t *item = &items[i];

        item->type = 'E';  /* 实体 */
        item->data.entity = result->entity;
        item->score = result->fused_score;

        (*out_count)++;
    }

    return 0;
}

/**
 * @brief 构建实体文本描述
 */
static char *build_entity_text(graphrag_entity_t *entity) {
    if (!entity) return NULL;

    /* 实体类型名称 */
    const char *type_names[] = {
        "Person", "Organization", "Location", "Concept", "Event", "Other"
    };
    const char *type_name = type_names[entity->type];

    /* 构建描述 */
    char *text = (char *)malloc(1024);
    if (!text) return NULL;

    if (entity->description && strlen(entity->description) > 0) {
        snprintf(text, 1024, "[Entity] %s (%s): %s (confidence: %.2f)",
                entity->name, type_name, entity->description, entity->confidence);
    } else {
        snprintf(text, 1024, "[Entity] %s (%s) (confidence: %.2f)",
                entity->name, type_name, entity->confidence);
    }

    return text;
}

/**
 * @brief 构建关系文本描述
 */
static char *build_relation_text(graphrag_relation_t *relation) {
    if (!relation) return NULL;

    char *text = (char *)malloc(1024);
    if (!text) return NULL;

    if (relation->description && strlen(relation->description) > 0) {
        snprintf(text, 1024, "[Relation] %s --%s--> %s: %s (confidence: %.2f)",
                relation->src ? relation->src->name : "?",
                relation->rel_type,
                relation->dst ? relation->dst->name : "?",
                relation->description,
                relation->confidence);
    } else {
        snprintf(text, 1024, "[Relation] %s --%s--> %s (confidence: %.2f)",
                relation->src ? relation->src->name : "?",
                relation->rel_type,
                relation->dst ? relation->dst->name : "?",
                relation->confidence);
    }

    return text;
}

/**
 * @brief 构建图结构摘要
 */
static char *build_graph_summary(graphrag_context_st_t *ctx,
                                graphrag_context_item_t *items,
                                int num_items) {
    if (!ctx || num_items == 0) return NULL;

    /* 计算统计信息 */
    int person_count = 0, org_count = 0, loc_count = 0, concept_count = 0;

    for (int i = 0; i < num_items; i++) {
        if (items[i].type == 'E' && items[i].data.entity) {
            switch (items[i].data.entity->type) {
                case GRAPHRAG_ENTITY_PERSON: person_count++; break;
                case GRAPHRAG_ENTITY_ORGANIZATION: org_count++; break;
                case GRAPHRAG_ENTITY_LOCATION: loc_count++; break;
                case GRAPHRAG_ENTITY_CONCEPT: concept_count++; break;
                default: break;
            }
        }
    }

    /* 构建摘要 */
    char *summary = (char *)malloc(2048);
    if (!summary) return NULL;

    snprintf(summary, 2048,
        "# Graph Knowledge Summary\n\n"
        "This section contains %d extracted entities from the knowledge graph:\n\n"
        "- **People**: %d\n"
        "- **Organizations**: %d\n"
        "- **Locations**: %d\n"
        "- **Concepts**: %d\n\n"
        "The entities are ranked by relevance to your query.\n",
        num_items, person_count, org_count, loc_count, concept_count);

    return summary;
}

/**
 * @brief 构建文本块内容
 */
static char *build_text_chunks(graphrag_context_st_t *ctx,
                               graphrag_search_results_t *search_results) {
    if (!search_results || !search_results->results || search_results->nresults == 0) {
        return NULL;
    }

    /* 估算所需空间 */
    int estimated_size = search_results->nresults * 512 + 256;
    char *chunks = (char *)malloc(estimated_size);
    if (!chunks) return NULL;

    strcpy(chunks, "# Related Context\n\n");

    for (int i = 0; i < search_results->nresults; i++) {
        graphrag_search_result_t *result = &search_results->results[i];
        if (!result->entity) continue;

        char entity_text[1024];
        const char *type_names[] = {
            "Person", "Organization", "Location", "Concept", "Event", "Other"
        };
        const char *type_name = type_names[result->entity->type];

        if (result->entity->description) {
            snprintf(entity_text, sizeof(entity_text),
                "**%d. %s** (%s, score: %.3f)\n"
                "   %s\n",
                i + 1,
                result->entity->name,
                type_name,
                result->fused_score,
                result->entity->description);
        } else {
            snprintf(entity_text, sizeof(entity_text),
                "**%d. %s** (%s, score: %.3f)\n",
                i + 1,
                result->entity->name,
                type_name,
                result->fused_score);
        }

        strcat(chunks, entity_text);
    }

    return chunks;
}

/* ============================================================
 * 公开 API 实现
 * ============================================================ */

/**
 * @brief 组装检索结果为 GraphRAG 上下文
 */
graphrag_context_t *graphrag_context_assemble(graphrag_context_st_t *ctx,
                                               graphrag_search_results_t *search_results,
                                               int max_items) {
    if (!ctx || !search_results) return NULL;

    graphrag_context_t *context = (graphrag_context_t *)calloc(1, sizeof(graphrag_context_t));
    if (!context) return NULL;

    context->capacity = (max_items > 0) ? max_items : 50;
    context->items = (graphrag_context_item_t *)calloc(context->capacity,
                                                      sizeof(graphrag_context_item_t));
    if (!context->items) {
        free(context);
        return NULL;
    }

    /* 组装上下文项 */
    if (assemble_context_items(search_results, context->capacity,
                              context->items, &context->nitems) != 0) {
        free(context->items);
        free(context);
        return NULL;
    }

    /* 构建图摘要 */
    if (ctx->config.include_graph_summary) {
        context->graph_summary = build_graph_summary(ctx, context->items, context->nitems);
    }

    /* 构建文本块 */
    if (ctx->config.include_text_chunks) {
        context->text_chunks = build_text_chunks(ctx, search_results);
    }

    return context;
}

/**
 * @brief 释放 GraphRAG 上下文
 */
void graphrag_context_free(graphrag_context_t *context) {
    if (!context) return;

    if (context->items) {
        /* 不释放内部指针，因为实体/关系由调用方管理 */
        free(context->items);
    }

    if (context->graph_summary) {
        free(context->graph_summary);
    }

    if (context->text_chunks) {
        free(context->text_chunks);
    }

    free(context);
}

/**
 * @brief 构建 Prompt
 */
int graphrag_context_build_prompt(const graphrag_context_t *context,
                                  const char *query,
                                  char *output,
                                  int output_size) {
    if (!context || !query || !output || output_size <= 0) return -1;

    int offset = 0;
    int remaining = output_size;

    /* 系统提示 */
    const char *system_prompt =
        "You are a helpful AI assistant with access to a knowledge graph.\n"
        "Use the provided context from the knowledge graph to answer questions.\n"
        "If the context doesn't contain relevant information, say so.\n\n";

    int len = snprintf(output + offset, remaining, "%s", system_prompt);
    if (len < 0 || len >= remaining) {
        output[output_size - 1] = '\0';
        return -1;
    }
    offset += len;
    remaining -= len;

    /* 图摘要 */
    if (context->graph_summary) {
        len = snprintf(output + offset, remaining, "%s\n\n", context->graph_summary);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;
    }

    /* 实体上下文 */
    if (context->nitems > 0 && context->items) {
        const char *entities_header = "## Extracted Entities\n\n";
        len = snprintf(output + offset, remaining, "%s", entities_header);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;

        for (int i = 0; i < context->nitems && remaining > 100; i++) {
            graphrag_context_item_t *item = &context->items[i];
            if (item->type == 'E' && item->data.entity) {
                graphrag_entity_t *entity = item->data.entity;
                const char *type_names[] = {
                    "Person", "Organization", "Location", "Concept", "Event", "Other"
                };

                if (entity->description) {
                    len = snprintf(output + offset, remaining, "- **%s** (%s): %s [relevance: %.2f]\n",
                                  entity->name, type_names[entity->type],
                                  entity->description, item->score);
                } else {
                    len = snprintf(output + offset, remaining, "- **%s** (%s) [relevance: %.2f]\n",
                                  entity->name, type_names[entity->type], item->score);
                }

                if (len < 0 || len >= remaining) break;
                offset += len;
                remaining -= len;
            }
        }

        len = snprintf(output + offset, remaining, "\n");
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;
    }

    /* 文本块 */
    if (context->text_chunks) {
        len = snprintf(output + offset, remaining, "%s\n\n", context->text_chunks);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;
    }

    /* 用户问题 */
    const char *question_header = "---\n\n## Question\n\n";
    len = snprintf(output + offset, remaining, "%s%s\n\n", question_header, query);
    if (len < 0 || len >= remaining) {
        output[output_size - 1] = '\0';
        return -1;
    }
    offset += len;
    remaining -= len;

    /* 回答指示 */
    const char *answer_prompt = "## Answer\n\nBased on the above context, please answer the question:\n";
    len = snprintf(output + offset, remaining, "%s", answer_prompt);
    if (len < 0 || len >= remaining) {
        output[output_size - 1] = '\0';
        return -1;
    }

    return 0;
}

/**
 * @brief 转换为字符串
 */
int graphrag_context_to_string(const graphrag_context_t *context,
                               char *output,
                               int output_size) {
    if (!context || !output || output_size <= 0) return -1;

    int offset = 0;
    int remaining = output_size;

    /* 图摘要 */
    if (context->graph_summary) {
        int len = snprintf(output + offset, remaining, "%s\n", context->graph_summary);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;
    }

    /* 实体列表 */
    if (context->nitems > 0 && context->items) {
        const char *header = "## Entities\n\n";
        int len = snprintf(output + offset, remaining, "%s", header);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;

        for (int i = 0; i < context->nitems && remaining > 50; i++) {
            graphrag_context_item_t *item = &context->items[i];
            if (item->type == 'E' && item->data.entity) {
                graphrag_entity_t *entity = item->data.entity;
                const char *type_names[] = {
                    "Person", "Organization", "Location", "Concept", "Event", "Other"
                };

                len = snprintf(output + offset, remaining, "- %s (%s)\n",
                                  entity->name, type_names[entity->type]);
                if (len < 0 || len >= remaining) break;
                offset += len;
                remaining -= len;
            }
        }

        len = snprintf(output + offset, remaining, "\n");
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
        offset += len;
        remaining -= len;
    }

    /* 文本块 */
    if (context->text_chunks) {
        int len = snprintf(output + offset, remaining, "%s\n", context->text_chunks);
        if (len < 0 || len >= remaining) {
            output[output_size - 1] = '\0';
            return -1;
        }
    }

    return 0;
}

/* ============================================================
 * GraphRAG 主模块实现
 * ============================================================ */

/** 默认配置 */
static void graphrag_config_default(graphrag_config_t *config) {
    if (!config) return;

    memset(config, 0, sizeof(*config));

    strcpy(config->vector_collection, "graphrag_entities");
    config->vector_dimension = GRAPHRAG_DEFAULT_DIMENSION;
    config->metric = 1; /* METRIC_COSINE */

    strcpy(config->graph_name, "graphrag_graph");
    strcpy(config->entity_label, "Entity");
    strcpy(config->relation_type, "RELATED_TO");

    config->top_k = GRAPHRAG_DEFAULT_TOP_K;
    config->entity_top_k = GRAPHRAG_DEFAULT_ENTITY_TOP_K;
    config->relation_depth = GRAPHRAG_DEFAULT_REL_DEPTH;
    config->rrf_k = GRAPHRAG_DEFAULT_RRF_K;
    config->hybrid_weight = 0.5f;

    config->enable_ner = 1;
    config->enable_re = 1;

    config->max_context_items = 50;
    config->include_graph_summary = 1;
    config->include_text_chunks = 1;
}

/**
 * @brief 创建 GraphRAG 上下文
 */
graphrag_context_st_t *graphrag_create(const graphrag_config_t *config) {
    graphrag_context_st_t *ctx = (graphrag_context_st_t *)calloc(1, sizeof(graphrag_context_st_t));
    if (!ctx) return NULL;

    /* 初始化配置 */
    if (config) {
        ctx->config = *config;
    } else {
        graphrag_config_default(&ctx->config);
    }

    /* 初始化状态 */
    ctx->initialized = 0;
    ctx->graph = NULL;
    ctx->vector_engine = NULL;
    ctx->data_dir = NULL;

    return ctx;
}

/**
 * @brief 初始化 GraphRAG 上下文
 */
int graphrag_init(graphrag_context_st_t *ctx) {
    if (!ctx) return -1;

    if (ctx->initialized) return 0;

    /* 预留：初始化图引擎 */
    /* if (ctx->config.graph_name[0]) {
     *     ctx->graph = graph_open(ctx->data_dir);
     * }
     */

    /* 预留：初始化向量引擎 */
    /* if (ctx->config.vector_collection[0]) {
     *     ctx->vector_engine = vector_engine_open(ctx->config.vector_collection);
     * }
     */

    ctx->initialized = 1;
    return 0;
}

/**
 * @brief 销毁 GraphRAG 上下文
 */
void graphrag_destroy(graphrag_context_st_t *ctx) {
    if (!ctx) return;

    /* 关闭图引擎（预留） */
    if (ctx->graph) {
        /* graph_close(ctx->graph); */
        ctx->graph = NULL;
    }

    /* 关闭向量引擎（预留） */
    if (ctx->vector_engine) {
        /* vector_engine_close(ctx->vector_engine); */
        ctx->vector_engine = NULL;
    }

    /* 释放数据目录 */
    if (ctx->data_dir) {
        free(ctx->data_dir);
    }

    free(ctx);
}

/**
 * @brief 设置默认配置
 */
void graphrag_config_init_defaults(graphrag_config_t *config) {
    graphrag_config_default(config);
}

/**
 * @brief 获取错误信息
 */
const char *graphrag_errmsg(const graphrag_context_st_t *ctx) {
    return ctx ? "GraphRAG operation failed" : "Null context";
}

/* ============================================================
 * 存储 API
 * ============================================================ */

/**
 * @brief 存储实体
 */
int graphrag_entities_store(graphrag_context_st_t *ctx,
                           graphrag_entity_t *entities,
                           int count) {
    if (!ctx || !entities || count <= 0) return -1;

    ctx->total_entities += count;

    /* 预留：存储到向量引擎 */
    /* for (int i = 0; i < count; i++) {
     *     if (entities[i].embedding) {
     *         vector_engine_insert(ctx->vector_engine,
     *                              entities[i].embedding,
     *                              entities[i].embedding_dim * sizeof(float));
     *     }
     * }
     */

    /* 预留：存储到图引擎 */
    /* for (int i = 0; i < count; i++) {
     *     graph_vertex_create(ctx->graph, ctx->config.entity_label, ...);
     * }
     */

    return 0;
}

/**
 * @brief 存储关系
 */
int graphrag_relations_store(graphrag_context_st_t *ctx,
                             graphrag_relation_t *relations,
                             int count) {
    if (!ctx || !relations || count <= 0) return -1;

    ctx->total_relations += count;

    /* 预留：存储到图引擎 */
    /* for (int i = 0; i < count; i++) {
     *     graph_edge_create(ctx->graph,
     *                       relations[i].src->graph_vertex_id,
     *                       relations[i].dst->graph_vertex_id,
     *                       relations[i].rel_type, ...);
     * }
     */

    return 0;
}

/**
 * @brief 获取实体
 */
graphrag_entity_t *graphrag_entity_get(graphrag_context_st_t *ctx,
                                       const char *entity_id) {
    if (!ctx || !entity_id) return NULL;

    /* 预留：从存储中查找实体 */
    /* 这里应该从向量引擎或内存索引中查找 */

    return NULL;
}

/**
 * @brief 获取邻居实体
 */
int graphrag_entity_get_neighbors(graphrag_context_st_t *ctx,
                                  graphrag_entity_t *entity,
                                  int depth,
                                  graphrag_entity_t **out_entities,
                                  int *out_count) {
    if (!ctx || !entity || !out_entities || !out_count) return -1;

    *out_entities = NULL;
    *out_count = 0;

    if (entity->graph_vertex_id == GRAPH_INVALID_ID || !ctx->graph) {
        return 0;
    }

    /* 预留：图遍历获取邻居 */
    /* 实现应该递归扩展 depth 层 */

    return 0;
}

/* ============================================================
 * 统计 API
 * ============================================================ */

/**
 * @brief 获取统计信息
 */
void graphrag_get_stats(const graphrag_context_st_t *ctx,
                       graphrag_stats_t *stats) {
    if (!ctx || !stats) return;

    memset(stats, 0, sizeof(*stats));
    stats->total_queries = ctx->total_queries;
    stats->total_entities = ctx->total_entities;
    stats->total_relations = ctx->total_relations;
}

/* ============================================================
 * 便捷 API
 * ============================================================ */

/**
 * @brief 执行端到端 GraphRAG 查询
 */
graphrag_context_t *graphrag_query(graphrag_context_st_t *ctx,
                                  const char *query_text,
                                  int top_k) {
    if (!ctx || !query_text) return NULL;

    ctx->total_queries++;

    /* 初始化（如果尚未初始化） */
    if (!ctx->initialized) {
        if (graphrag_init(ctx) != 0) {
            return NULL;
        }
    }

    int effective_top_k = (top_k > 0) ? top_k : ctx->config.top_k;
    if (effective_top_k <= 0) effective_top_k = GRAPHRAG_DEFAULT_TOP_K;

    /* 执行混合检索 */
    graphrag_search_results_t *search_results = NULL;
    if (graphrag_hybrid_search(ctx, query_text, effective_top_k,
                              &search_results) != 0) {
        return NULL;
    }

    if (!search_results || search_results->nresults == 0) {
        if (search_results) graphrag_search_results_free(search_results);
        return NULL;
    }

    /* 组装上下文 */
    graphrag_context_t *context = graphrag_context_assemble(
        ctx, search_results,
        ctx->config.max_context_items > 0 ? ctx->config.max_context_items : 50);

    graphrag_search_results_free(search_results);

    return context;
}

/**
 * @brief 索引文档
 */
int graphrag_index_document(graphrag_context_st_t *ctx,
                           const char *text,
                           const char *doc_id,
                           int chunk_size,
                           int chunk_overlap) {
    if (!ctx || !text || !doc_id) return -1;

    /* 初始化（如果尚未初始化） */
    if (!ctx->initialized) {
        if (graphrag_init(ctx) != 0) {
            return -1;
        }
    }

    /* 默认分块参数 */
    if (chunk_size <= 0) chunk_size = RAG_DEFAULT_CHUNK_SIZE;
    if (chunk_overlap < 0) chunk_overlap = RAG_DEFAULT_CHUNK_OVERLAP;

    /* 简单分块（按固定大小） */
    int text_len = (int)strlen(text);
    int num_chunks = (text_len + chunk_size - chunk_overlap - 1) / (chunk_size - chunk_overlap);
    if (num_chunks <= 0) num_chunks = 1;

    int chunk_idx = 0;
    int pos = 0;

    while (pos < text_len && chunk_idx < 100) {
        int this_chunk_size = chunk_size;
        if (pos + this_chunk_size > text_len) {
            this_chunk_size = text_len - pos;
        }

        /* 创建 Chunk ID */
        char chunk_id[128];
        snprintf(chunk_id, sizeof(chunk_id), "%s-chunk-%d", doc_id, chunk_idx);

        /* 提取实体 */
        graphrag_entity_t *entities = NULL;
        int num_entities = 0;

        if (graphrag_extract_entities(ctx, text + pos, this_chunk_size,
                                     chunk_id, &entities, &num_entities) == 0) {
            if (num_entities > 0) {
                /* 存储实体 */
                graphrag_entities_store(ctx, entities, num_entities);

                /* 提取关系 */
                graphrag_relation_t *relations = NULL;
                int num_relations = 0;

                if (graphrag_extract_relations(ctx, entities, num_entities,
                                             text + pos, &relations,
                                             &num_relations) == 0) {
                    if (num_relations > 0) {
                        graphrag_relations_store(ctx, relations, num_relations);
                    }
                    graphrag_relations_free(relations, num_relations);
                }

                graphrag_entities_free(entities, num_entities);
            }
        }

        pos += (chunk_size - chunk_overlap);
        chunk_idx++;
    }

    return 0;
}
