/**
 * @file graphrag_relation.c
 * @brief GraphRAG 关系提取实现
 *
 * 从实体对中提取关系并生成向量嵌入
 */

#include "db/rag/graphrag.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdio.h>
#include <math.h>

/* ============================================================
 * 内部类型
 * ============================================================ */

/** 关系类型定义 */
typedef struct relation_pattern {
    const char *pattern;       /**< 关系模式（关键词） */
    const char *rel_type;      /**< 关系类型名 */
    float confidence_boost;     /**< 置信度提升 */
} relation_pattern_t;

/** 内部关系节点 */
typedef struct relation_node {
    graphrag_entity_t *src;
    graphrag_entity_t *dst;
    char rel_type[GRAPHRAG_MAX_REL_TYPE_LEN];
    float confidence;
    int src_pos;   /**< 源实体结束位置 */
    int dst_pos;   /**< 目标实体开始位置 */
} relation_node_t;

/* ============================================================
 * 关系模式定义
 * ============================================================ */

/** 预定义关系模式 */
static const relation_pattern_t g_relation_patterns[] = {
    /* 组织-人关系 */
    {"CEO of", "leads", 0.3f},
    {"founder of", "founded", 0.3f},
    {"founder and CEO", "leads", 0.3f},
    {"works at", "employed_by", 0.2f},
    {"employed by", "employed_by", 0.2f},
    {"employee of", "employed_by", 0.2f},
    {"member of", "member_of", 0.2f},
    {"chairman of", "leads", 0.3f},
    {"director of", "leads", 0.3f},
    {"president of", "leads", 0.3f},
    {"partner of", "partnered_with", 0.2f},

    /* 位置关系 */
    {"located in", "located_in", 0.3f},
    {"headquartered in", "headquartered_in", 0.3f},
    {"based in", "based_in", 0.3f},
    {"born in", "born_in", 0.3f},
    {"lives in", "lives_in", 0.2f},
    {"moved to", "moved_to", 0.2f},
    {"works in", "works_in", 0.2f},
    {"in", "related_to", 0.1f},

    /* 概念关系 */
    {"is a", "is_a", 0.1f},
    {"is an", "is_a", 0.1f},
    {"part of", "part_of", 0.2f},
    {"related to", "related_to", 0.1f},
    {"similar to", "similar_to", 0.2f},
    {"uses", "uses", 0.2f},
    {"implements", "implements", 0.2f},
    {"provides", "provides", 0.2f},
    {"enables", "enables", 0.2f},
    {"based on", "based_on", 0.2f},
    {"developed by", "developed_by", 0.3f},
    {"created by", "created_by", 0.3f},
    {"designed by", "designed_by", 0.3f},

    /* 时间关系 */
    {"founded in", "founded_in", 0.3f},
    {"established in", "established_in", 0.3f},
    {"released in", "released_in", 0.3f},
    {"launched in", "launched_in", 0.3f},
    {"started in", "started_in", 0.2f},
    {"occurred in", "occurred_in", 0.2f},

    /* 事件关系 */
    {"announced", "announced", 0.2f},
    {"signed", "signed", 0.2f},
    {"acquired", "acquired", 0.3f},
    {"merged with", "merged_with", 0.3f},
    {"partnered with", "partnered_with", 0.3f},
    {"competed with", "competed_with", 0.2f},
    {"invested in", "invested_in", 0.3f},
    {"acquired by", "acquired_by", 0.3f},

    /* 默认 */
    {"", "related_to", 0.1f}
};

#define NUM_RELATION_PATTERNS (sizeof(g_relation_patterns) / sizeof(g_relation_patterns[0]) - 1)

/* ============================================================
 * 内部函数声明
 * ============================================================ */

static void extract_relations_from_text(const char *text,
                                        graphrag_entity_t *entities,
                                        int num_entities,
                                        relation_node_t *relations,
                                        int *num_relations,
                                        int max_relations);
static const char *match_relation_pattern(const char *text, int src_end, int dst_start);
static void generate_relation_embedding(const char *src_name, const char *rel_type,
                                         const char *dst_name, float *embedding, int dim);
static void deduplicate_relations(relation_node_t *relations, int *num_relations);
static float calculate_relation_confidence(graphrag_entity_type_t src_type,
                                           graphrag_entity_type_t dst_type,
                                           const char *rel_type,
                                           float base_confidence);

/* ============================================================
 * 关系提取核心实现
 * ============================================================ */

/**
 * @brief 从文本中提取实体间的关系
 */
static void extract_relations_from_text(const char *text,
                                        graphrag_entity_t *entities,
                                        int num_entities,
                                        relation_node_t *relations,
                                        int *num_relations,
                                        int max_relations) {
    if (!text || !entities || num_entities < 2 || !relations || !num_relations) return;

    *num_relations = 0;

    int text_len = (int)strlen(text);

    /* 遍历所有实体对 */
    for (int i = 0; i < num_entities && *num_relations < max_relations; i++) {
        graphrag_entity_t *src = &entities[i];

        /* 查找文本中源实体后面的目标实体 */
        for (int j = i + 1; j < num_entities && *num_relations < max_relations; j++) {
            graphrag_entity_t *dst = &entities[j];

            /* 计算实体在文本中的位置 */
            int src_end = -1;
            int dst_start = -1;

            /* 简单的位置估算 */
            for (int pos = 0; pos < text_len; pos++) {
                int match_src = 1;
                int match_dst = 1;

                /* 检查是否匹配源实体 */
                for (int k = 0; k < (int)strlen(src->name) && pos + k < text_len; k++) {
                    if (tolower(text[pos + k]) != tolower(src->name[k])) {
                        match_src = 0;
                        break;
                    }
                }

                /* 检查是否匹配目标实体 */
                for (int k = 0; k < (int)strlen(dst->name) && pos + k < text_len; k++) {
                    if (tolower(text[pos + k]) != tolower(dst->name[k])) {
                        match_dst = 0;
                        break;
                    }
                }

                if (match_src) {
                    src_end = pos + (int)strlen(src->name);
                }
                if (match_dst) {
                    dst_start = pos;
                }
            }

            /* 如果找到两个实体 */
            if (src_end > 0 && dst_start > src_end && dst_start - src_end < 200) {
                /* 匹配关系模式 */
                const char *matched_rel = match_relation_pattern(text, src_end, dst_start);

                if (matched_rel) {
                    relation_node_t *rel = &relations[*num_relations];
                    rel->src = src;
                    rel->dst = dst;
                    strncpy(rel->rel_type, matched_rel, GRAPHRAG_MAX_REL_TYPE_LEN - 1);
                    rel->rel_type[GRAPHRAG_MAX_REL_TYPE_LEN - 1] = '\0';
                    rel->src_pos = src_end;
                    rel->dst_pos = dst_start;

                    /* 计算置信度 */
                    rel->confidence = calculate_relation_confidence(
                        src->type, dst->type, matched_rel, 0.5f);

                    (*num_relations)++;
                }
            }
        }
    }

    /* 去重 */
    deduplicate_relations(relations, num_relations);
}

/**
 * @brief 匹配关系模式
 */
static const char *match_relation_pattern(const char *text, int src_end, int dst_start) {
    if (!text || src_end < 0 || dst_start <= src_end) return NULL;

    int pattern_len = dst_start - src_end;
    if (pattern_len <= 0 || pattern_len > 150) return "related_to";

    /* 提取关系文本 */
    char relation_text[256] = {0};
    strncpy(relation_text, text + src_end, pattern_len);
    relation_text[pattern_len] = '\0';

    /* 小写化 */
    for (int i = 0; relation_text[i]; i++) {
        relation_text[i] = tolower(relation_text[i]);
    }

    /* 匹配预定义模式 */
    for (size_t i = 0; i < NUM_RELATION_PATTERNS; i++) {
        const relation_pattern_t *pat = &g_relation_patterns[i];
        if (pat->pattern[0] == '\0') {
            /* 默认关系 */
            return "related_to";
        }

        if (strstr(relation_text, pat->pattern) != NULL) {
            return pat->rel_type;
        }
    }

    /* 检查是否有连接词 */
    const char *connectors[] = {" and ", " with ", " to ", " for ", " by ", NULL};
    for (int i = 0; connectors[i] != NULL; i++) {
        if (strstr(relation_text, connectors[i]) != NULL) {
            return "related_to";
        }
    }

    return "related_to";
}

/**
 * @brief 去重关系
 */
static void deduplicate_relations(relation_node_t *relations, int *num_relations) {
    if (!relations || *num_relations <= 1) return;

    for (int i = 0; i < *num_relations - 1; i++) {
        for (int j = i + 1; j < *num_relations; ) {
            int same = (relations[i].src == relations[j].src) &&
                       (relations[i].dst == relations[j].dst) &&
                       (strcmp(relations[i].rel_type, relations[j].rel_type) == 0);

            if (same) {
                /* 保留置信度更高的 */
                if (relations[j].confidence > relations[i].confidence) {
                    relations[i] = relations[j];
                }
                /* 移除重复项 */
                for (int k = j; k < *num_relations - 1; k++) {
                    relations[k] = relations[k + 1];
                }
                (*num_relations)--;
            } else {
                j++;
            }
        }
    }
}

/**
 * @brief 计算关系置信度
 */
static float calculate_relation_confidence(graphrag_entity_type_t src_type,
                                          graphrag_entity_type_t dst_type,
                                          const char *rel_type,
                                          float base_confidence) {
    float confidence = base_confidence;

    /* 根据实体类型调整置信度 */
    if (src_type == GRAPHRAG_ENTITY_PERSON && dst_type == GRAPHRAG_ENTITY_ORGANIZATION) {
        if (strcmp(rel_type, "leads") == 0 || strcmp(rel_type, "employed_by") == 0 ||
            strcmp(rel_type, "founder") == 0 || strcmp(rel_type, "founded") == 0) {
            confidence += 0.2f;
        }
    }

    if (src_type == GRAPHRAG_ENTITY_ORGANIZATION && dst_type == GRAPHRAG_ENTITY_LOCATION) {
        if (strcmp(rel_type, "located_in") == 0 || strcmp(rel_type, "headquartered_in") == 0 ||
            strcmp(rel_type, "based_in") == 0) {
            confidence += 0.2f;
        }
    }

    if (src_type == GRAPHRAG_ENTITY_CONCEPT && dst_type == GRAPHRAG_ENTITY_CONCEPT) {
        if (strcmp(rel_type, "is_a") == 0 || strcmp(rel_type, "part_of") == 0 ||
            strcmp(rel_type, "based_on") == 0) {
            confidence += 0.15f;
        }
    }

    /* 根据关系类型调整 */
    const struct { const char *type; float boost; } type_boosts[] = {
        {"leads", 0.15f}, {"founded", 0.15f}, {"acquired", 0.2f},
        {"located_in", 0.1f}, {"related_to", -0.1f}, {"is_a", 0.1f}
    };

    for (size_t i = 0; i < sizeof(type_boosts) / sizeof(type_boosts[0]); i++) {
        if (strcmp(rel_type, type_boosts[i].type) == 0) {
            confidence += type_boosts[i].boost;
            break;
        }
    }

    /* 限制在 [0, 1] 范围内 */
    if (confidence < 0.0f) confidence = 0.0f;
    if (confidence > 1.0f) confidence = 1.0f;

    return confidence;
}

/* ============================================================
 * 向量嵌入生成
 * ============================================================ */

/**
 * @brief 生成关系的向量嵌入
 *
 * 简化实现：基于源实体名 + 关系类型 + 目标实体名的确定性向量
 */
static void generate_relation_embedding(const char *src_name, const char *rel_type,
                                         const char *dst_name, float *embedding, int dim) {
    if (!src_name || !rel_type || !dst_name || !embedding || dim <= 0) return;

    /* 组合关系文本 */
    char combined[512];
    snprintf(combined, sizeof(combined), "%s %s %s", src_name, rel_type, dst_name);

    /* 使用哈希生成确定性向量 */
    unsigned int seed = 2166136261U;
    const char *p = combined;
    while (*p) {
        seed ^= (unsigned char)*p;
        seed *= 16777619U;
        p++;
    }

    /* 生成归一化向量 */
    srand(seed);
    float norm = 0.0f;
    for (int i = 0; i < dim; i++) {
        embedding[i] = (float)(rand() % 2000 - 1000) / 1000.0f;
        norm += embedding[i] * embedding[i];
    }

    /* L2 归一化 */
    norm = sqrtf(norm);
    if (norm > 0.0001f) {
        for (int i = 0; i < dim; i++) {
            embedding[i] /= norm;
        }
    }
}

/* ============================================================
 * 公开 API 实现
 * ============================================================ */

int graphrag_extract_relations(graphrag_context_st_t *ctx,
                               graphrag_entity_t *entities,
                               int num_entities,
                               const char *text,
                               graphrag_relation_t **out_relations,
                               int *out_count) {
    if (!ctx || !entities || num_entities < 2 || !out_relations || !out_count) {
        return -1;
    }

    int max_relations = num_entities * (num_entities - 1) / 2;
    if (max_relations > 100) max_relations = 100;

    relation_node_t *nodes = (relation_node_t *)calloc(max_relations, sizeof(relation_node_t));
    if (!nodes) return -1;

    int node_count = 0;

    /* 从文本中提取关系 */
    if (text) {
        extract_relations_from_text(text, entities, num_entities,
                                    nodes, &node_count, max_relations);
    }

    /* 如果文本提取的关系太少，尝试基于实体类型推断 */
    if (node_count < num_entities / 2) {
        for (int i = 0; i < num_entities && node_count < max_relations; i++) {
            for (int j = i + 1; j < num_entities && node_count < max_relations; j++) {
                /* 检查是否已存在类似关系 */
                int exists = 0;
                for (int k = 0; k < node_count; k++) {
                    if ((nodes[k].src == &entities[i] && nodes[k].dst == &entities[j]) ||
                        (nodes[k].src == &entities[j] && nodes[k].dst == &entities[i])) {
                        exists = 1;
                        break;
                    }
                }

                if (!exists) {
                    /* 基于实体类型推断关系 */
                    graphrag_entity_type_t t1 = entities[i].type;
                    graphrag_entity_type_t t2 = entities[j].type;

                    const char *inferred_rel = NULL;
                    float base_conf = 0.3f;

                    if (t1 == GRAPHRAG_ENTITY_PERSON && t2 == GRAPHRAG_ENTITY_PERSON) {
                        inferred_rel = "knows";
                    } else if (t1 == GRAPHRAG_ENTITY_PERSON && t2 == GRAPHRAG_ENTITY_ORGANIZATION) {
                        inferred_rel = "works_for";
                    } else if (t1 == GRAPHRAG_ENTITY_ORGANIZATION && t2 == GRAPHRAG_ENTITY_ORGANIZATION) {
                        inferred_rel = "partners_with";
                    } else if (t1 == GRAPHRAG_ENTITY_ORGANIZATION && t2 == GRAPHRAG_ENTITY_LOCATION) {
                        inferred_rel = "located_in";
                    } else if (t1 == GRAPHRAG_ENTITY_CONCEPT && t2 == GRAPHRAG_ENTITY_CONCEPT) {
                        inferred_rel = "related_to";
                    }

                    if (inferred_rel) {
                        relation_node_t *rel = &nodes[node_count];
                        rel->src = &entities[i];
                        rel->dst = &entities[j];
                        strncpy(rel->rel_type, inferred_rel, GRAPHRAG_MAX_REL_TYPE_LEN - 1);
                        rel->rel_type[GRAPHRAG_MAX_REL_TYPE_LEN - 1] = '\0';
                        rel->confidence = base_conf;
                        node_count++;
                    }
                }
            }
        }
    }

    if (node_count == 0) {
        free(nodes);
        *out_relations = NULL;
        *out_count = 0;
        return 0;
    }

    /* 分配输出数组 */
    graphrag_relation_t *relations = (graphrag_relation_t *)calloc(node_count, sizeof(graphrag_relation_t));
    if (!relations) {
        free(nodes);
        return -1;
    }

    int dim = ctx->config.vector_dimension > 0 ? ctx->config.vector_dimension : GRAPHRAG_DEFAULT_DIMENSION;

    /* 转换为 GraphRAG 关系 */
    for (int i = 0; i < node_count; i++) {
        graphrag_relation_t *rel = &relations[i];

        /* 生成 ID */
        unsigned int hash = 2166136261U;
        char id_buf[256];
        snprintf(id_buf, sizeof(id_buf), "%s-%s-%s",
                 nodes[i].src->name, nodes[i].rel_type, nodes[i].dst->name);
        const char *p = id_buf;
        while (*p) {
            hash ^= (unsigned char)*p;
            hash *= 16777619U;
            p++;
        }
        snprintf(rel->id, sizeof(rel->id), "rel-%08x", hash);

        /* 设置实体指针 */
        rel->src = nodes[i].src;
        rel->dst = nodes[i].dst;

        /* 复制关系类型 */
        strncpy(rel->rel_type, nodes[i].rel_type, GRAPHRAG_MAX_REL_TYPE_LEN - 1);
        rel->rel_type[GRAPHRAG_MAX_REL_TYPE_LEN - 1] = '\0';

        /* 分配描述 */
        rel->description = (char *)malloc(512);
        if (rel->description) {
            snprintf(rel->description, 512, "%s --%s--> %s",
                     rel->src->name, rel->rel_type, rel->dst->name);
        }

        /* 分配并生成向量嵌入 */
        rel->embedding = (float *)calloc(dim, sizeof(float));
        if (rel->embedding) {
            generate_relation_embedding(rel->src->name, rel->rel_type,
                                        rel->dst->name, rel->embedding, dim);
            rel->embedding_dim = dim;
        }

        /* 设置其他字段 */
        rel->confidence = nodes[i].confidence;
        rel->graph_edge_id = GRAPH_INVALID_ID;
    }

    free(nodes);
    *out_relations = relations;
    *out_count = node_count;

    return 0;
}

void graphrag_relations_free(graphrag_relation_t *relations, int count) {
    if (!relations) return;

    for (int i = 0; i < count; i++) {
        if (relations[i].description) {
            free(relations[i].description);
        }
        if (relations[i].embedding) {
            free(relations[i].embedding);
        }
    }
    free(relations);
}

float *graphrag_relation_get_embedding(graphrag_relation_t *relation) {
    return relation ? relation->embedding : NULL;
}
