/**
 * @file graphrag_entity.c
 * @brief GraphRAG 实体提取实现
 *
 * 从文本中提取命名实体（NER）并生成向量嵌入
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

/** 内部实体节点（用于构建） */
typedef struct entity_node {
    char name[GRAPHRAG_MAX_ENTITY_NAME_LEN];
    graphrag_entity_type_t type;
    int start_pos;
    int end_pos;
    float confidence;
} entity_node_t;

/** 简单正则匹配结果 */
typedef struct regex_match {
    int start;
    int end;
    char text[256];
} regex_match_t;

/* ============================================================
 * 内部函数声明
 * ============================================================ */

static int extract_person_names(const char *text, int text_len,
                                 entity_node_t *entities, int *count, int max_count);
static int extract_organization_names(const char *text, int text_len,
                                      entity_node_t *entities, int *count, int max_count);
static int extract_location_names(const char *text, int text_len,
                                   entity_node_t *entities, int *count, int max_count);
static int extract_concept_names(const char *text, int text_len,
                                  entity_node_t *entities, int *count, int max_count);
static void deduplicate_entities(entity_node_t *entities, int *count);
static void generate_entity_embedding(const char *name, float *embedding, int dim);
static char *generate_entity_id(const char *name);

/* ============================================================
 * 实体提取核心实现
 * ============================================================ */

/**
 * @brief 从文本中提取所有类型的实体
 */
static int extract_all_entities(const char *text, int text_len,
                                 entity_node_t *entities, int *count, int max_count) {
    if (!text || !entities || !count) return -1;

    *count = 0;

    /* 提取各种类型的实体 */
    extract_person_names(text, text_len, entities, count, max_count);
    extract_organization_names(text, text_len, entities, count, max_count);
    extract_location_names(text, text_len, entities, count, max_count);
    extract_concept_names(text, text_len, entities, count, max_count);

    /* 去重 */
    deduplicate_entities(entities, count);

    return 0;
}

/**
 * @brief 提取人名（简单的启发式规则）
 *
 * 规则：
 * 1. 大写字母开头 + 小写字母组成的词（可能是名字）
 * 2. 两个连续大写字母开头的词（可能是全名）
 * 3. 常见称呼：Mr./Mrs./Dr. + 姓氏
 */
static int extract_person_names(const char *text, int text_len,
                                 entity_node_t *entities, int *count, int max_count) {
    if (!text) return 0;

    int len = text_len > 0 ? text_len : (int)strlen(text);
    char word[128] = {0};
    int word_start = -1;
    int i = 0;

    while (i < len && *count < max_count) {
        char c = text[i];

        /* 收集以大写字母开头的词 */
        if (isupper(c) && (i == 0 || !isalnum(text[i-1]))) {
            word_start = i;
            int j = 0;
            while (i < len && j < (int)sizeof(word) - 1) {
                c = text[i];
                if (isalnum(c) || c == '\'' || c == '-' || c == '.') {
                    word[j++] = c;
                    i++;
                } else {
                    break;
                }
            }
            word[j] = '\0';

            /* 过滤太短的词 */
            if (j >= 2) {
                /* 检查是否是常见人名模式 */
                int is_person = 0;

                /* 模式1：常见人名后缀 */
                if (j >= 3 && (strcmp(word + j - 3, "son") == 0 ||
                               strcmp(word + j - 3, "sen") == 0 ||
                               strcmp(word + j - 2, "ez") == 0)) {
                    is_person = 1;
                }
                /* 模式2：常见称呼 */
                else if (strncmp(word, "Mr.", 3) == 0 ||
                         strncmp(word, "Mrs.", 4) == 0 ||
                         strncmp(word, "Dr.", 3) == 0 ||
                         strncmp(word, "Ms.", 3) == 0) {
                    is_person = 1;
                }
                /* 模式3：单个大写字母 + 小写字母（可能是名字） */
                else if (j >= 2 && j <= 15 && islower(word[1])) {
                    /* 检查下一个词是否可能是姓氏 */
                    int next_start = i;
                    while (next_start < len && !isalpha(text[next_start])) next_start++;
                    if (next_start < len && isupper(text[next_start])) {
                        int k = 0;
                        while (next_start + k < len && k < (int)sizeof(word) - 1) {
                            c = text[next_start + k];
                            if (isalpha(c)) {
                                word[k++] = c;
                                next_start++;
                            } else {
                                break;
                            }
                        }
                        if (k >= 2 && k <= 20) {
                            is_person = 1;
                        }
                    }
                }

                if (is_person && *count < max_count) {
                    entity_node_t *e = &entities[*count];
                    strncpy(e->name, word, GRAPHRAG_MAX_ENTITY_NAME_LEN - 1);
                    e->name[GRAPHRAG_MAX_ENTITY_NAME_LEN - 1] = '\0';
                    e->type = GRAPHRAG_ENTITY_PERSON;
                    e->start_pos = word_start;
                    e->end_pos = i;
                    e->confidence = 0.7f;
                    (*count)++;
                }
            }
        } else {
            i++;
        }
    }

    return 0;
}

/**
 * @brief 提取组织名称（公司、机构等）
 */
static int extract_organization_names(const char *text, int text_len,
                                       entity_node_t *entities, int *count, int max_count) {
    if (!text) return 0;

    int len = text_len > 0 ? text_len : (int)strlen(text);
    const char *org_suffixes[] = {
        "Inc.", "Corp.", "LLC", "Ltd.", "Co.", "Company", "Group",
        "University", "Institute", "Foundation", "Association",
        "Organization", "Bank", "Hospital", "Museum", NULL
    };

    int i = 0;
    while (i < len && *count < max_count) {
        /* 查找组织后缀 */
        for (int j = 0; org_suffixes[j] != NULL && *count < max_count; j++) {
            const char *suffix = org_suffixes[j];
            int suffix_len = (int)strlen(suffix);

            if (i + suffix_len <= len &&
                strncmp(text + i, suffix, suffix_len) == 0) {
                /* 检查前面是否有空格 */
                if (i > 0 && isspace(text[i-1])) {
                    /* 向前收集组织名称 */
                    int start = i;
                    int k = i + suffix_len;
                    while (k < len && (isalnum(text[k]) || isspace(text[k]) ||
                                       text[k] == '.' || text[k] == ',')) {
                        k++;
                    }

                    char org_name[256] = {0};
                    int org_len = k - start;
                    if (org_len > 0 && org_len < (int)sizeof(org_name)) {
                        strncpy(org_name, text + start, org_len);
                        org_name[org_len] = '\0';

                        /* 添加实体 */
                        entity_node_t *e = &entities[*count];
                        strncpy(e->name, org_name, GRAPHRAG_MAX_ENTITY_NAME_LEN - 1);
                        e->name[GRAPHRAG_MAX_ENTITY_NAME_LEN - 1] = '\0';
                        e->type = GRAPHRAG_ENTITY_ORGANIZATION;
                        e->start_pos = start;
                        e->end_pos = k;
                        e->confidence = 0.8f;
                        (*count)++;
                    }
                }
            }
        }
        i++;
    }

    return 0;
}

/**
 * @brief 提取地名
 */
static int extract_location_names(const char *text, int text_len,
                                  entity_node_t *entities, int *count, int max_count) {
    if (!text) return 0;

    int len = text_len > 0 ? text_len : (int)strlen(text);
    const char *location_suffixes[] = {
        "City", "Town", "County", "State", "Country", "River", "Mountain",
        "Lake", "Ocean", "Sea", "Street", "Avenue", "Park", "Building",
        "Region", "Province", "District", "Island", "Valley", NULL
    };

    int i = 0;
    while (i < len && *count < max_count) {
        /* 检查是否是大写开头的词 */
        if (i > 0 && isupper(text[i])) {
            int start = i;
            int j = 0;
            char word[64] = {0};

            while (i < len && j < (int)sizeof(word) - 1) {
                char c = text[i];
                if (isalnum(c) || c == '-' || c == '\'') {
                    word[j++] = c;
                    i++;
                } else {
                    break;
                }
            }

            /* 检查是否以常见地名后缀结尾 */
            for (int k = 0; location_suffixes[k] != NULL; k++) {
                int suffix_len = strlen(location_suffixes[k]);
                if (j > suffix_len &&
                    strcmp(word + j - suffix_len, location_suffixes[k]) == 0) {
                    /* 添加实体 */
                    entity_node_t *e = &entities[*count];
                    strncpy(e->name, word, GRAPHRAG_MAX_ENTITY_NAME_LEN - 1);
                    e->name[GRAPHRAG_MAX_ENTITY_NAME_LEN - 1] = '\0';
                    e->type = GRAPHRAG_ENTITY_LOCATION;
                    e->start_pos = start;
                    e->end_pos = i;
                    e->confidence = 0.75f;
                    (*count)++;
                    break;
                }
            }
        } else {
            i++;
        }
    }

    return 0;
}

/**
 * @brief 提取概念名称（技术术语等）
 */
static int extract_concept_names(const char *text, int text_len,
                                  entity_node_t *entities, int *count, int max_count) {
    if (!text) return 0;

    int len = text_len > 0 ? text_len : (int)strlen(text);
    const char *concept_prefixes[] = {
        "the", "a", "an", "our", "your", "their", "its",
        "new", "old", "big", "small", "large", "local", "global", NULL
    };

    const char *concept_words[] = {
        "technology", "system", "platform", "framework", "architecture",
        "algorithm", "database", "network", "protocol", "standard",
        "interface", "application", "service", "process", "method",
        "model", "data", "information", "knowledge", "intelligence",
        "machine", "learning", "deep", "neural", "network", "artificial",
        "computing", "cloud", "edge", "mobile", "security", NULL
    };

    int i = 0;
    while (i < len && *count < max_count) {
        /* 查找概念词 */
        for (int k = 0; concept_words[k] != NULL && *count < max_count; k++) {
            const char *word = concept_words[k];
            int word_len = (int)strlen(word);

            if (i + word_len <= len &&
                strncasecmp(text + i, word, word_len) == 0) {
                int end = i + word_len;

                /* 检查边界（前一个字符不是字母，后一个字符不是字母） */
                if ((i == 0 || !isalpha(text[i-1])) &&
                    (end >= len || !isalpha(text[end]))) {

                    /* 向前查找修饰词 */
                    int start = i;
                    while (start > 0) {
                        int found = 0;
                        for (int p = 0; concept_prefixes[p] != NULL; p++) {
                            int prefix_len = (int)strlen(concept_prefixes[p]);
                            if (start >= prefix_len + 1 &&
                                text[start - prefix_len - 1] == ' ' &&
                                strncasecmp(text + start - prefix_len,
                                           concept_prefixes[p], prefix_len) == 0) {
                                start -= prefix_len + 1;
                                found = 1;
                                break;
                            }
                        }
                        if (!found) break;
                    }

                    char concept_name[256] = {0};
                    int concept_len = end - start;
                    if (concept_len > 0 && concept_len < (int)sizeof(concept_name)) {
                        strncpy(concept_name, text + start, concept_len);
                        concept_name[concept_len] = '\0';

                        /* 添加实体 */
                        entity_node_t *e = &entities[*count];
                        strncpy(e->name, concept_name, GRAPHRAG_MAX_ENTITY_NAME_LEN - 1);
                        e->name[GRAPHRAG_MAX_ENTITY_NAME_LEN - 1] = '\0';
                        e->type = GRAPHRAG_ENTITY_CONCEPT;
                        e->start_pos = start;
                        e->end_pos = end;
                        e->confidence = 0.6f;
                        (*count)++;
                    }
                }
            }
        }
        i++;
    }

    return 0;
}

/**
 * @brief 实体去重（按名称）
 */
static void deduplicate_entities(entity_node_t *entities, int *count) {
    if (!entities || *count <= 1) return;

    for (int i = 0; i < *count - 1; i++) {
        for (int j = i + 1; j < *count; ) {
            if (strcasecmp(entities[i].name, entities[j].name) == 0) {
                /* 保留置信度更高的 */
                if (entities[j].confidence > entities[i].confidence) {
                    entities[i] = entities[j];
                }
                /* 移除重复项 */
                for (int k = j; k < *count - 1; k++) {
                    entities[k] = entities[k + 1];
                }
                (*count)--;
            } else {
                j++;
            }
        }
    }
}

/* ============================================================
 * 向量嵌入生成（简化实现）
 * ============================================================ */

/**
 * @brief 生成实体的向量嵌入
 *
 * 简化实现：基于实体名称的确定性伪随机向量
 * 真实实现应调用外部 Embedding 服务（如 Ollama/OpenAI）
 */
static void generate_entity_embedding(const char *name, float *embedding, int dim) {
    if (!name || !embedding || dim <= 0) return;

    /* 使用简单的哈希生成确定性向量 */
    unsigned int seed = 2166136261U;
    const char *p = name;
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

/**
 * @brief 生成实体 ID
 */
static char *generate_entity_id(const char *name) {
    if (!name) return NULL;

    /* 使用简单哈希作为 ID */
    unsigned int hash = 2166136261U;
    const char *p = name;
    while (*p) {
        hash ^= (unsigned char)*p;
        hash *= 16777619U;
        p++;
    }

    char *id = (char *)malloc(64);
    if (id) {
        snprintf(id, 64, "entity-%08x", hash);
    }
    return id;
}

/* ============================================================
 * 公开 API 实现
 * ============================================================ */

int graphrag_extract_entities(graphrag_context_st_t *ctx,
                              const char *text,
                              int text_len,
                              const char *chunk_id,
                              graphrag_entity_t **out_entities,
                              int *out_count) {
    if (!ctx || !text || !out_entities || !out_count) return -1;

    int max_entities = 100;
    entity_node_t *nodes = (entity_node_t *)calloc(max_entities, sizeof(entity_node_t));
    if (!nodes) return -1;

    /* 提取实体 */
    int node_count = 0;
    extract_all_entities(text, text_len, nodes, &node_count, max_entities);

    if (node_count == 0) {
        free(nodes);
        *out_entities = NULL;
        *out_count = 0;
        return 0;
    }

    /* 分配输出数组 */
    graphrag_entity_t *entities = (graphrag_entity_t *)calloc(node_count, sizeof(graphrag_entity_t));
    if (!entities) {
        free(nodes);
        return -1;
    }

    int dim = ctx->config.vector_dimension > 0 ? ctx->config.vector_dimension : GRAPHRAG_DEFAULT_DIMENSION;

    /* 转换为 GraphRAG 实体 */
    for (int i = 0; i < node_count; i++) {
        graphrag_entity_t *e = &entities[i];

        /* 生成 ID */
        char *id = generate_entity_id(nodes[i].name);
        if (id) {
            strncpy(e->id, id, sizeof(e->id) - 1);
            free(id);
        }

        /* 复制名称 */
        strncpy(e->name, nodes[i].name, GRAPHRAG_MAX_ENTITY_NAME_LEN - 1);
        e->name[GRAPHRAG_MAX_ENTITY_NAME_LEN - 1] = '\0';

        /* 设置类型 */
        e->type = nodes[i].type;

        /* 分配描述 */
        e->description = (char *)malloc(512);
        if (e->description) {
            snprintf(e->description, 512, "Entity '%s' of type %d", e->name, e->type);
        }

        /* 分配并生成向量嵌入 */
        e->embedding = (float *)calloc(dim, sizeof(float));
        if (e->embedding) {
            generate_entity_embedding(e->name, e->embedding, dim);
            e->embedding_dim = dim;
        }

        /* 设置其他字段 */
        e->confidence = nodes[i].confidence;
        e->graph_vertex_id = GRAPH_INVALID_ID;

        if (chunk_id) {
            strncpy(e->source_chunk_id, chunk_id, sizeof(e->source_chunk_id) - 1);
        }
    }

    free(nodes);
    *out_entities = entities;
    *out_count = node_count;

    return 0;
}

void graphrag_entities_free(graphrag_entity_t *entities, int count) {
    if (!entities) return;

    for (int i = 0; i < count; i++) {
        if (entities[i].description) {
            free(entities[i].description);
        }
        if (entities[i].embedding) {
            free(entities[i].embedding);
        }
    }
    free(entities);
}

float *graphrag_entity_get_embedding(graphrag_entity_t *entity) {
    return entity ? entity->embedding : NULL;
}

int graphrag_entity_get_embedding_dim(const graphrag_entity_t *entity) {
    return entity ? entity->embedding_dim : 0;
}
