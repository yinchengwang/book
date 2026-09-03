/**
 * @file hybrid_search.c
 * @brief 混合检索实现
 *
 * 实现向量搜索与结构化过滤条件的混合检索。
 */
#include "hybrid_search.h"
#include "hnsw/faiss_hnsw.h"
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

/* ========================================================================
 * 辅助宏
 * ======================================================================== */

#define HYBRID_MAX(x, y) ((x) > (y) ? (x) : (y))
#define HYBRID_MIN(x, y) ((x) < (y) ? (x) : (y))

/* ========================================================================
 * 过滤表达式实现
 * ======================================================================== */

/**
 * @brief 创建过滤表达式
 */
FilterExpr *filter_expr_create_leaf(const char *field, FilterOperator op,
                                    FilterValueType value_type, FilterValue value) {
    if (!field) return NULL;

    FilterExpr *expr = (FilterExpr *)calloc(1, sizeof(FilterExpr));
    if (!expr) return NULL;

    strncpy(expr->cond.field, field, sizeof(expr->cond.field) - 1);
    expr->cond.op = op;
    expr->cond.value_type = value_type;
    expr->cond.value = value;
    expr->is_leaf = true;

    return expr;
}

/**
 * @brief 创建复合过滤表达式
 */
FilterExpr *filter_expr_create_compound(FilterExpr *left, FilterExpr *right,
                                        FilterOperator op) {
    if (!left && !right) return NULL;
    if (op != FILTER_OP_AND && op != FILTER_OP_OR && op != FILTER_OP_NOT) {
        return NULL;
    }

    FilterExpr *expr = (FilterExpr *)calloc(1, sizeof(FilterExpr));
    if (!expr) return NULL;

    expr->left = left;
    expr->right = right;
    expr->op = op;
    expr->is_leaf = false;

    return expr;
}

/**
 * @brief 递归释放过滤表达式
 */
static void filter_expr_free_impl(FilterExpr *expr) {
    if (!expr) return;
    if (!expr->is_leaf) {
        filter_expr_free_impl(expr->left);
        filter_expr_free_impl(expr->right);
    }
    free(expr);
}

/**
 * @brief 释放过滤表达式
 */
void filter_expr_free(FilterExpr *expr) {
    filter_expr_free_impl(expr);
}

/**
 * @brief 递归复制过滤表达式
 */
static FilterExpr *filter_expr_copy_impl(const FilterExpr *expr) {
    if (!expr) return NULL;

    FilterExpr *copy = (FilterExpr *)calloc(1, sizeof(FilterExpr));
    if (!copy) return NULL;

    memcpy(copy, expr, sizeof(FilterExpr));

    if (!expr->is_leaf) {
        copy->left = filter_expr_copy_impl(expr->left);
        copy->right = filter_expr_copy_impl(expr->right);
    }

    return copy;
}

/**
 * @brief 复制过滤表达式
 */
FilterExpr *filter_expr_copy(const FilterExpr *expr) {
    return filter_expr_copy_impl(expr);
}

/* ========================================================================
 * JSON 解析
 * ======================================================================== */

/**
 * @brief 从 JSON 创建过滤表达式（简化实现）
 *
 * 支持格式：
 * {"field": "name", "op": "=", "value": "xxx"}
 * {"op": "AND", "conditions": [...]}
 */
FilterExpr *filter_expr_from_json(const char *json) {
    if (!json) return NULL;

    /* 简化解析：检测是否为简单条件还是复合条件 */
    const char *op_pos = strstr(json, "\"op\"");
    const char *field_pos = strstr(json, "\"field\"");
    const char *conditions_pos = strstr(json, "\"conditions\"");

    if (conditions_pos && op_pos && conditions_pos < op_pos) {
        /* 复合条件 */
        /* 简单实现：解析 AND/OR，然后递归处理 conditions */
        FilterExpr *result = NULL;
        FilterExpr *current = NULL;

        /* 查找操作符 */
        if (strstr(json, "\"AND\"") || strstr(json, "\"and\"")) {
            /* AND 复合条件 */
            /* 简化：递归解析第一个和第二个条件 */
            FilterExpr *left = NULL, *right = NULL;

            /* 查找第一个 { 开始和对应的 } 结束 */
            const char *first_start = strchr(json, '{');
            if (first_start) {
                int depth = 0;
                const char *first_end = first_start;
                do {
                    if (*first_end == '{') depth++;
                    else if (*first_end == '}') depth--;
                    first_end++;
                } while (depth > 0 && *first_end);

                /* 解析第一个条件 */
                char *first_json = (char *)malloc(first_end - first_start + 1);
                if (first_json) {
                    memcpy(first_json, first_start, first_end - first_start);
                    first_json[first_end - first_start] = '\0';
                    left = filter_expr_from_json(first_json);
                    free(first_json);
                }

                /* 查找第二个条件 */
                const char *second_start = first_end;
                while (*second_start && (*second_start == ',' || *second_start == ' ')) {
                    second_start++;
                }
                if (*second_start == '[') {
                    second_start = strchr(second_start, '{');
                }
                if (*second_start == '{') {
                    const char *second_end = second_start + 1;
                    depth = 1;
                    while (depth > 0 && *second_end) {
                        if (*second_end == '{') depth++;
                        else if (*second_end == '}') depth--;
                        second_end++;
                    }
                    char *second_json = (char *)malloc(second_end - second_start + 1);
                    if (second_json) {
                        memcpy(second_json, second_start, second_end - second_start);
                        second_json[second_end - second_start] = '\0';
                        right = filter_expr_from_json(second_json);
                        free(second_json);
                    }
                }

                result = filter_expr_create_compound(left, right, FILTER_OP_AND);
            }
        } else if (strstr(json, "\"OR\"") || strstr(json, "\"or\"")) {
            /* OR 复合条件，类似处理 */
            result = NULL; /* 简化实现 */
        }

        (void)current; /* 消除未使用警告 */
        return result;
    } else if (field_pos) {
        /* 简单条件 */
        char field[64] = {0};
        FilterOperator op = FILTER_OP_EQ;
        FilterValue value = {0};
        FilterValueType value_type = FILTER_VALUE_STRING;

        /* 解析 field */
        const char *field_start = strchr(field_pos, ':');
        if (field_start) {
            field_start++;
            while (*field_start == ' ' || *field_start == '\"') field_start++;
            const char *field_end = field_start;
            while (*field_end && *field_end != '\"' && *field_end != ',') field_end++;
            size_t len = HYBRID_MIN(field_end - field_start, sizeof(field) - 1);
            memcpy(field, field_start, len);
        }

        /* 解析 op */
        if (op_pos) {
            const char *op_start = strchr(op_pos, ':');
            if (op_start) {
                op_start++;
                while (*op_start == ' ' || *op_start == '\"') op_start++;
                if (strncmp(op_start, "=", 1) == 0) op = FILTER_OP_EQ;
                else if (strncmp(op_start, "!=", 2) == 0) op = FILTER_OP_NE;
                else if (strncmp(op_start, ">", 1) == 0) op = FILTER_OP_GT;
                else if (strncmp(op_start, ">=", 2) == 0) op = FILTER_OP_GE;
                else if (strncmp(op_start, "<", 1) == 0) op = FILTER_OP_LT;
                else if (strncmp(op_start, "<=", 2) == 0) op = FILTER_OP_LE;
            }
        }

        /* 解析 value（简化实现） */
        const char *value_pos = strstr(json, "\"value\"");
        if (value_pos) {
            const char *value_start = strchr(value_pos, ':');
            if (value_start) {
                value_start++;
                while (*value_start == ' ' || *value_start == '\"') value_start++;

                if (*value_start == '"') {
                    /* 字符串值 */
                    value_start++;
                    const char *value_end = strchr(value_start, '"');
                    if (!value_end) value_end = value_start + strlen(value_start);
                    value.string_val.str = (char *)malloc(value_end - value_start + 1);
                    if (value.string_val.str) {
                        memcpy(value.string_val.str, value_start, value_end - value_start);
                        value.string_val.str[value_end - value_start] = '\0';
                        value.string_val.len = value_end - value_start;
                        value_type = FILTER_VALUE_STRING;
                    }
                } else if (*value_start == '-' || (*value_start >= '0' && *value_start <= '9')) {
                    /* 数值 */
                    if (strchr(value_start, '.')) {
                        value.double_val = atof(value_start);
                        value_type = FILTER_VALUE_DOUBLE;
                    } else {
                        value.bigint_val = atoll(value_start);
                        value_type = FILTER_VALUE_BIGINT;
                    }
                } else if (strncmp(value_start, "true", 4) == 0 ||
                           strncmp(value_start, "false", 5) == 0) {
                    value.bool_val = (strncmp(value_start, "true", 4) == 0);
                    value_type = FILTER_VALUE_BOOL;
                }
            }
        }

        return filter_expr_create_leaf(field, op, value_type, value);
    }

    return NULL;
}

/**
 * @brief 将过滤表达式转换为 JSON（简化实现）
 */
char *filter_expr_to_json(const FilterExpr *expr) {
    if (!expr) {
        char *result = (char *)malloc(5);
        if (result) strcpy(result, "{}");
        return result;
    }

    if (expr->is_leaf) {
        /* 简单条件 */
        char field_buf[128] = {0};
        char value_buf[256] = {0};

        /* 转义字段名 */
        snprintf(field_buf, sizeof(field_buf), "\"%s\"", expr->cond.field);

        /* 格式化值 */
        switch (expr->cond.value_type) {
            case FILTER_VALUE_STRING:
                snprintf(value_buf, sizeof(value_buf), "\"%s\"", expr->cond.value.string_val.str);
                break;
            case FILTER_VALUE_INT:
                snprintf(value_buf, sizeof(value_buf), "%d", expr->cond.value.int_val);
                break;
            case FILTER_VALUE_BIGINT:
                snprintf(value_buf, sizeof(value_buf), "%lld", (long long)expr->cond.value.bigint_val);
                break;
            case FILTER_VALUE_FLOAT:
                snprintf(value_buf, sizeof(value_buf), "%f", expr->cond.value.float_val);
                break;
            case FILTER_VALUE_DOUBLE:
                snprintf(value_buf, sizeof(value_buf), "%f", expr->cond.value.double_val);
                break;
            case FILTER_VALUE_BOOL:
                strcpy(value_buf, expr->cond.value.bool_val ? "true" : "false");
                break;
            default:
                strcpy(value_buf, "null");
        }

        /* 操作符映射 */
        const char *op_str = "=";
        switch (expr->cond.op) {
            case FILTER_OP_EQ: op_str = "="; break;
            case FILTER_OP_NE: op_str = "!="; break;
            case FILTER_OP_LT: op_str = "<"; break;
            case FILTER_OP_LE: op_str = "<="; break;
            case FILTER_OP_GT: op_str = ">"; break;
            case FILTER_OP_GE: op_str = ">="; break;
            default: op_str = "=";
        }

        char *result = (char *)malloc(256);
        if (result) {
            snprintf(result, 256, "{\"field\":%s,\"op\":\"%s\",\"value\":%s}",
                    field_buf, op_str, value_buf);
        }
        return result;
    } else {
        /* 复合条件 */
        const char *op_str = expr->op == FILTER_OP_AND ? "AND" :
                             expr->op == FILTER_OP_OR ? "OR" : "NOT";

        char *left_json = filter_expr_to_json(expr->left);
        char *right_json = filter_expr_to_json(expr->right);

        char *result = (char *)malloc(512);
        if (result) {
            if (expr->op == FILTER_OP_NOT) {
                snprintf(result, 512, "{\"op\":\"NOT\",\"condition\":%s}", left_json);
            } else {
                snprintf(result, 512, "{\"op\":\"%s\",\"left\":%s,\"right\":%s}",
                        op_str, left_json ? left_json : "{}",
                        right_json ? right_json : "{}");
            }
        }

        free(left_json);
        free(right_json);
        return result;
    }
}

/* ========================================================================
 * 混合检索配置实现
 * ======================================================================== */

/**
 * @brief 创建默认混合检索配置
 */
HybridSearchConfig *hybrid_search_config_default(void) {
    return hybrid_search_config_create(HYBRID_FILTER_AUTO);
}

/**
 * @brief 创建混合检索配置
 */
HybridSearchConfig *hybrid_search_config_create(HybridFilterStrategy strategy) {
    HybridSearchConfig *config = (HybridSearchConfig *)calloc(1, sizeof(HybridSearchConfig));
    if (!config) return NULL;

    config->strategy = strategy;
    config->adaptive_mode = HYBRID_ADAPTIVE_FILTER_RATE;

    /* Pre-filter 默认配置 */
    config->pre_filter.max_filter_batch = 10000;
    config->pre_filter.use_index_for_filter = true;

    /* Post-filter 默认配置 */
    config->post_filter.oversample_factor = 4;
    config->post_filter.min_candidates = 100;

    /* 混合策略默认配置 */
    config->hybrid.batch_size = 50000;
    config->hybrid.batch_filter_rate = 0.5f;

    /* 自适应策略默认配置 */
    config->adaptive.pre_filter_threshold = 0.5f;
    config->adaptive.post_filter_threshold = 0.3f;
    config->adaptive.min_data_size = 10000;

    /* 性能优化默认配置 */
    config->optimization.parallel_filter = false;
    config->optimization.num_threads = 4;
    config->optimization.cache_filter_results = true;

    return config;
}

/**
 * @brief 释放混合检索配置
 */
void hybrid_search_config_free(HybridSearchConfig *config) {
    free(config);
}

/**
 * @brief 克隆混合检索配置
 */
HybridSearchConfig *hybrid_search_config_clone(const HybridSearchConfig *config) {
    if (!config) return NULL;
    HybridSearchConfig *copy = (HybridSearchConfig *)malloc(sizeof(HybridSearchConfig));
    if (copy) {
        memcpy(copy, config, sizeof(HybridSearchConfig));
    }
    return copy;
}

/* ========================================================================
 * 索引后端实现
 * ======================================================================== */

struct HybridIndex_s {
    int32_t dims;
    distance_metric_t metric;
    HybridSearchConfig *config;

    /* 向量存储 */
    float *vectors;       /**< [capacity × dims] 原始向量 */
    int64_t *ids;         /**< [capacity] ID 映射 */
    uint8_t *metadata;    /**< [capacity × metadata_size] 元数据 */
    int32_t metadata_size;
    int32_t size;
    int32_t capacity;

    /* 索引后端 */
    HybridBackend *backend;

    /* 过滤缓存 */
    void *filter_cache;

    /* 统计信息 */
    int64_t search_count;
    int64_t total_search_time_us;
};

/**
 * @brief 创建混合检索索引
 */
HybridIndex *hybrid_index_create(int32_t vector_dim,
                                 distance_metric_t metric,
                                 const HybridSearchConfig *config) {
    if (vector_dim <= 0) return NULL;

    HybridIndex *index = (HybridIndex *)calloc(1, sizeof(HybridIndex));
    if (!index) return NULL;

    index->dims = vector_dim;
    index->metric = metric;
    index->config = config ? hybrid_search_config_clone(config) : hybrid_search_config_default();
    if (!index->config) {
        free(index);
        return NULL;
    }

    /* 初始容量 */
    index->capacity = 10000;
    index->vectors = (float *)calloc(index->capacity * index->dims, sizeof(float));
    index->ids = (int64_t *)calloc(index->capacity, sizeof(int64_t));
    index->metadata_size = 256; /* 默认元数据大小 */
    index->metadata = (uint8_t *)calloc(index->capacity, index->metadata_size);

    if (!index->vectors || !index->ids || !index->metadata) {
        free(index->vectors);
        free(index->ids);
        free(index->metadata);
        free(index->config);
        free(index);
        return NULL;
    }

    /* 创建 HNSW 后端 */
    index->backend = hybrid_backend_create(HYBRID_BACKEND_HNSW, vector_dim);

    return index;
}

/**
 * @brief 销毁混合检索索引
 */
void hybrid_index_destroy(HybridIndex *index) {
    if (!index) return;

    free(index->vectors);
    free(index->ids);
    free(index->metadata);
    free(index->config);
    hybrid_backend_destroy(index->backend);
    free(index);
}

/**
 * @brief 添加向量
 */
int32_t hybrid_index_add(HybridIndex *index,
                         int64_t id,
                         const float *vector,
                         const void *metadata,
                         int32_t metadata_size) {
    if (!index || !vector) return -1;

    /* 扩容检查 */
    if (index->size >= index->capacity) {
        int32_t new_capacity = index->capacity * 2;
        float *new_vectors = (float *)realloc(index->vectors, new_capacity * index->dims * sizeof(float));
        int64_t *new_ids = (int64_t *)realloc(index->ids, new_capacity * sizeof(int64_t));
        uint8_t *new_metadata = (uint8_t *)realloc(index->metadata, new_capacity * index->metadata_size);

        if (!new_vectors || !new_ids || !new_metadata) {
            free(new_vectors);
            free(new_ids);
            free(new_metadata);
            return -1;
        }

        index->vectors = new_vectors;
        index->ids = new_ids;
        index->metadata = new_metadata;
        index->capacity = new_capacity;
    }

    int32_t pos = index->size;
    memcpy(&index->vectors[pos * index->dims], vector, index->dims * sizeof(float));
    index->ids[pos] = id;

    if (metadata && metadata_size > 0) {
        int32_t copy_size = HYBRID_MIN(metadata_size, index->metadata_size);
        memcpy(&index->metadata[pos * index->metadata_size], metadata, copy_size);
    }

    index->size++;

    /* 更新后端索引 */
    if (index->backend && index->backend->ops.add) {
        index->backend->ops.add(index->backend->impl, pos, vector);
    }

    return 0;
}

/**
 * @brief 批量添加向量
 */
int32_t hybrid_index_add_batch(HybridIndex *index,
                               int32_t n,
                               const int64_t *ids,
                               const float **vectors,
                               const void **metadata,
                               const int32_t *metadata_sizes) {
    if (!index || !vectors) return -1;

    int32_t added = 0;
    for (int32_t i = 0; i < n; i++) {
        int32_t ret = hybrid_index_add(index, ids ? ids[i] : i,
                                       vectors[i],
                                       metadata ? metadata[i] : NULL,
                                       metadata_sizes ? metadata_sizes[i] : 0);
        if (ret == 0) added++;
    }

    return added;
}

/* ========================================================================
 * 过滤评估
 * ======================================================================== */

/**
 * @brief 评估过滤条件（简化实现）
 *
 * 实际应用中应该解析元数据字段并与条件比较
 */
static bool evaluate_filter(const FilterExpr *filter, const void *metadata, int32_t metadata_size) {
    if (!filter) return true; /* 无过滤条件，全部通过 */
    if (!metadata || metadata_size == 0) return false;

    if (filter->is_leaf) {
        /* 简化实现：假设元数据是简单的 key=value 格式 */
        const char *meta_str = (const char *)metadata;
        char field_pattern[128];
        snprintf(field_pattern, sizeof(field_pattern), "%s=", filter->cond.field);

        const char *field_pos = strstr(meta_str, field_pattern);
        if (!field_pos) return false;

        /* 获取字段值 */
        const char *value_start = field_pos + strlen(field_pattern);
        const char *value_end = value_start;
        while (*value_end && *value_end != '&' && *value_end != '\0') {
            value_end++;
        }

        size_t value_len = value_end - value_start;

        switch (filter->cond.op) {
            case FILTER_OP_EQ: {
                if (filter->cond.value_type == FILTER_VALUE_STRING) {
                    return strncmp(value_start, filter->cond.value.string_val.str,
                                   HYBRID_MIN(value_len, filter->cond.value.string_val.len)) == 0;
                } else if (filter->cond.value_type == FILTER_VALUE_BIGINT) {
                    char num_buf[32] = {0};
                    strncpy(num_buf, value_start, HYBRID_MIN(value_len, sizeof(num_buf) - 1));
                    return atoll(num_buf) == filter->cond.value.bigint_val;
                }
                break;
            }
            case FILTER_OP_NE: {
                /* 类似 EQ 但取反 */
                break;
            }
            case FILTER_OP_GT:
            case FILTER_OP_GE:
            case FILTER_OP_LT:
            case FILTER_OP_LE: {
                if (filter->cond.value_type == FILTER_VALUE_BIGINT ||
                    filter->cond.value_type == FILTER_VALUE_INT) {
                    char num_buf[32] = {0};
                    strncpy(num_buf, value_start, HYBRID_MIN(value_len, sizeof(num_buf) - 1));
                    int64_t val = atoll(num_buf);
                    int64_t cond_val = filter->cond.value.bigint_val;

                    switch (filter->cond.op) {
                        case FILTER_OP_GT: return val > cond_val;
                        case FILTER_OP_GE: return val >= cond_val;
                        case FILTER_OP_LT: return val < cond_val;
                        case FILTER_OP_LE: return val <= cond_val;
                        default: break;
                    }
                }
                break;
            }
            default:
                break;
        }
        return false;
    } else {
        /* 复合条件 */
        bool left_result = evaluate_filter(filter->left, metadata, metadata_size);
        bool right_result = evaluate_filter(filter->right, metadata, metadata_size);

        switch (filter->op) {
            case FILTER_OP_AND: return left_result && right_result;
            case FILTER_OP_OR: return left_result || right_result;
            case FILTER_OP_NOT: return !left_result;
            default: return false;
        }
    }
}

/* ========================================================================
 * 搜索实现
 * ======================================================================== */

/**
 * @brief Pre-filter 策略搜索
 *
 * 先过滤后搜索
 */
HybridSearchResults *hybrid_index_search_pre(HybridIndex *index,
                                             const float *query,
                                             const FilterExpr *filter,
                                             int32_t top_k) {
    if (!index) return NULL;

    HybridSearchResults *results = (HybridSearchResults *)calloc(1, sizeof(HybridSearchResults));
    if (!results) return NULL;

    int64_t start_time = 0; /* 简化：实际应该用 clock_gettime 或 QueryPerformanceCounter */

    /* 1. 先过滤：收集符合条件的 ID */
    int64_t *filtered_ids = (int64_t *)malloc(index->size * sizeof(int64_t));
    int32_t filtered_count = 0;

    if (filtered_ids) {
        for (int32_t i = 0; i < index->size; i++) {
            const void *meta = &index->metadata[i * index->metadata_size];
            if (evaluate_filter(filter, meta, index->metadata_size)) {
                filtered_ids[filtered_count++] = i;
            }
        }
    }

    /* 2. 在过滤后的子集上搜索 */
    if (filtered_count > 0 && top_k > 0) {
        results->results = (HybridSearchResult *)malloc(sizeof(HybridSearchResult) * top_k);
        results->capacity = top_k;

        if (results->results) {
            /* 简化的暴力搜索实现 */
            /* 实际应用中应该使用索引后端 */
            typedef struct {
                int32_t idx;
                float dist;
            } Candidate;

            Candidate *candidates = (Candidate *)malloc(sizeof(Candidate) * filtered_count);
            if (candidates) {
                /* 计算所有候选的距离 */
                for (int32_t i = 0; i < filtered_count; i++) {
                    int32_t vec_idx = (int32_t)filtered_ids[i];
                    float dist = 0.0f;

                    /* L2 距离 */
                    for (int32_t d = 0; d < index->dims; d++) {
                        float diff = query[d] - index->vectors[vec_idx * index->dims + d];
                        dist += diff * diff;
                    }

                    candidates[i].idx = vec_idx;
                    candidates[i].dist = sqrtf(dist);
                }

                /* 部分排序获取 Top-K */
                for (int32_t i = 0; i < filtered_count && i < top_k; i++) {
                    for (int32_t j = i + 1; j < filtered_count; j++) {
                        if (candidates[j].dist < candidates[i].dist) {
                            Candidate tmp = candidates[i];
                            candidates[i] = candidates[j];
                            candidates[j] = tmp;
                        }
                    }
                }

                int32_t result_count = HYBRID_MIN(top_k, filtered_count);
                for (int32_t i = 0; i < result_count; i++) {
                    results->results[i].id = index->ids[candidates[i].idx];
                    results->results[i].distance = candidates[i].dist;
                    results->results[i].score = 1.0f / (1.0f + candidates[i].dist);
                    results->results[i].filtered = true;
                }
                results->count = result_count;

                free(candidates);
            }
        }
    }

    results->total_candidates = index->size;
    results->filtered_count = filtered_count;

    free(filtered_ids);
    return results;
}

/**
 * @brief Post-filter 策略搜索
 *
 * 先搜索后过滤
 */
HybridSearchResults *hybrid_index_search_post(HybridIndex *index,
                                              const float *query,
                                              const FilterExpr *filter,
                                              int32_t top_k) {
    if (!index) return NULL;

    HybridSearchResults *results = (HybridSearchResults *)calloc(1, sizeof(HybridSearchResults));
    if (!results) return NULL;

    /* 过采样因子 */
    int32_t oversample = index->config ? index->config->post_filter.oversample_factor : 4;
    int32_t search_k = HYBRID_MIN(top_k * oversample, index->size);

    /* 1. 先搜索获取候选 */
    typedef struct {
        int32_t idx;
        float dist;
    } Candidate;

    Candidate *candidates = (Candidate *)malloc(sizeof(Candidate) * search_k);
    if (!candidates) {
        free(results);
        return NULL;
    }

    /* 简化的暴力搜索 */
    for (int32_t i = 0; i < search_k; i++) {
        candidates[i].idx = -1;
        candidates[i].dist = FLT_MAX;
    }

    for (int32_t i = 0; i < index->size; i++) {
        float dist = 0.0f;
        for (int32_t d = 0; d < index->dims; d++) {
            float diff = query[d] - index->vectors[i * index->dims + d];
            dist += diff * diff;
        }

        /* 插入排序 */
        if (dist < candidates[search_k - 1].dist) {
            int32_t pos = search_k - 1;
            while (pos > 0 && candidates[pos - 1].dist > dist) {
                candidates[pos] = candidates[pos - 1];
                pos--;
            }
            candidates[pos].idx = i;
            candidates[pos].dist = sqrtf(dist);
        }
    }

    /* 2. 应用过滤 */
    int32_t result_capacity = HYBRID_MIN(top_k, search_k);
    results->results = (HybridSearchResult *)malloc(sizeof(HybridSearchResult) * result_capacity);
    results->capacity = result_capacity;

    if (results->results) {
        for (int32_t i = 0; i < search_k && results->count < result_capacity; i++) {
            if (candidates[i].idx < 0) break;

            const void *meta = &index->metadata[candidates[i].idx * index->metadata_size];
            bool passed = evaluate_filter(filter, meta, index->metadata_size);

            if (passed) {
                int32_t pos = results->count;
                results->results[pos].id = index->ids[candidates[i].idx];
                results->results[pos].distance = candidates[i].dist;
                results->results[pos].score = 1.0f / (1.0f + candidates[i].dist);
                results->results[pos].filtered = true;
                results->count++;
            }
        }
    }

    results->total_candidates = index->size;
    results->filtered_count = results->count;

    free(candidates);
    return results;
}

/**
 * @brief 混合策略搜索
 *
 * 分批搜索 + 结果合并
 */
HybridSearchResults *hybrid_index_search_hybrid(HybridIndex *index,
                                                const float *query,
                                                const FilterExpr *filter,
                                                int32_t top_k) {
    if (!index) return NULL;

    HybridSearchResults *results = (HybridSearchResults *)calloc(1, sizeof(HybridSearchResults));
    if (!results) return NULL;

    int32_t batch_size = index->config ? index->config->hybrid.batch_size : 50000;
    int32_t num_batches = (index->size + batch_size - 1) / batch_size;
    int32_t oversample = index->config ? index->config->post_filter.oversample_factor : 4;

    /* 收集所有批次的结果 */
    typedef struct {
        int32_t idx;
        float dist;
    } Candidate;

    int32_t max_candidates = HYBRID_MIN(top_k * oversample * num_batches, index->size);
    Candidate *all_candidates = (Candidate *)malloc(sizeof(Candidate) * max_candidates);
    int32_t total_candidates = 0;

    if (all_candidates) {
        for (int32_t batch = 0; batch < num_batches && total_candidates < max_candidates; batch++) {
            int32_t start = batch * batch_size;
            int32_t end = HYBRID_MIN(start + batch_size, index->size);

            /* 搜索当前批次 */
            for (int32_t i = start; i < end && total_candidates < max_candidates; i++) {
                float dist = 0.0f;
                for (int32_t d = 0; d < index->dims; d++) {
                    float diff = query[d] - index->vectors[i * index->dims + d];
                    dist += diff * diff;
                }

                all_candidates[total_candidates].idx = i;
                all_candidates[total_candidates].dist = sqrtf(dist);
                total_candidates++;
            }
        }

        /* 排序并取 Top-K */
        for (int32_t i = 0; i < total_candidates; i++) {
            for (int32_t j = i + 1; j < total_candidates; j++) {
                if (all_candidates[j].dist < all_candidates[i].dist) {
                    Candidate tmp = all_candidates[i];
                    all_candidates[i] = all_candidates[j];
                    all_candidates[j] = tmp;
                }
            }
        }

        /* 应用过滤并构建结果 */
        results->results = (HybridSearchResult *)malloc(sizeof(HybridSearchResult) * top_k);
        results->capacity = top_k;

        if (results->results) {
            for (int32_t i = 0; i < total_candidates && results->count < top_k; i++) {
                const void *meta = &index->metadata[all_candidates[i].idx * index->metadata_size];
                bool passed = evaluate_filter(filter, meta, index->metadata_size);

                if (passed) {
                    int32_t pos = results->count;
                    results->results[pos].id = index->ids[all_candidates[i].idx];
                    results->results[pos].distance = all_candidates[i].dist;
                    results->results[pos].score = 1.0f / (1.0f + all_candidates[i].dist);
                    results->results[pos].filtered = true;
                    results->count++;
                }
            }
        }

        free(all_candidates);
    }

    results->total_candidates = index->size;
    results->filtered_count = results->count;

    return results;
}

/**
 * @brief 混合检索搜索（自动选择策略）
 */
HybridSearchResults *hybrid_index_search(HybridIndex *index,
                                         const float *query,
                                         const FilterExpr *filter,
                                         int32_t top_k) {
    if (!index || !query || top_k <= 0) return NULL;

    /* 无过滤条件，直接搜索 */
    if (!filter) {
        HybridSearchResults *results = (HybridSearchResults *)calloc(1, sizeof(HybridSearchResults));
        if (!results) return NULL;

        results->results = (HybridSearchResult *)malloc(sizeof(HybridSearchResult) * top_k);
        results->capacity = top_k;

        if (results->results) {
            typedef struct {
                int32_t idx;
                float dist;
            } Candidate;

            Candidate *candidates = (Candidate *)malloc(sizeof(Candidate) * index->size);
            if (candidates) {
                for (int32_t i = 0; i < index->size; i++) {
                    float dist = 0.0f;
                    for (int32_t d = 0; d < index->dims; d++) {
                        float diff = query[d] - index->vectors[i * index->dims + d];
                        dist += diff * diff;
                    }
                    candidates[i].idx = i;
                    candidates[i].dist = sqrtf(dist);
                }

                /* 部分排序 */
                for (int32_t i = 0; i < index->size && i < top_k; i++) {
                    for (int32_t j = i + 1; j < index->size; j++) {
                        if (candidates[j].dist < candidates[i].dist) {
                            Candidate tmp = candidates[i];
                            candidates[i] = candidates[j];
                            candidates[j] = tmp;
                        }
                    }
                }

                for (int32_t i = 0; i < top_k && i < index->size; i++) {
                    results->results[i].id = index->ids[candidates[i].idx];
                    results->results[i].distance = candidates[i].dist;
                    results->results[i].score = 1.0f / (1.0f + candidates[i].dist);
                    results->results[i].filtered = true;
                    results->count++;
                }

                free(candidates);
            }
        }

        results->total_candidates = index->size;
        results->filtered_count = results->count;
        return results;
    }

    /* 根据策略选择搜索方法 */
    HybridFilterStrategy strategy = index->config ? index->config->strategy : HYBRID_FILTER_AUTO;

    if (strategy == HYBRID_FILTER_AUTO) {
        /* 自适应策略：基于数据规模选择 */
        if (index->size < index->config->adaptive.min_data_size) {
            strategy = HYBRID_FILTER_POST;
        } else {
            strategy = HYBRID_FILTER_PRE;
        }
    }

    switch (strategy) {
        case HYBRID_FILTER_PRE:
            return hybrid_index_search_pre(index, query, filter, top_k);
        case HYBRID_FILTER_POST:
            return hybrid_index_search_post(index, query, filter, top_k);
        case HYBRID_FILTER_HYBRID:
            return hybrid_index_search_hybrid(index, query, filter, top_k);
        default:
            return hybrid_index_search_pre(index, query, filter, top_k);
    }
}

/* ========================================================================
 * 其他接口实现
 * ======================================================================== */

/**
 * @brief 删除向量
 */
int32_t hybrid_index_delete(HybridIndex *index, int64_t id) {
    if (!index) return -1;

    /* 查找 ID */
    for (int32_t i = 0; i < index->size; i++) {
        if (index->ids[i] == id) {
            /* 标记删除（简化实现） */
            index->ids[i] = -1;
            return 0;
        }
    }

    return -1;
}

/**
 * @brief 更新向量元数据
 */
int32_t hybrid_index_update_metadata(HybridIndex *index,
                                     int64_t id,
                                     const void *metadata,
                                     int32_t metadata_size) {
    if (!index) return -1;

    /* 查找 ID */
    for (int32_t i = 0; i < index->size; i++) {
        if (index->ids[i] == id) {
            int32_t copy_size = HYBRID_MIN(metadata_size, index->metadata_size);
            memcpy(&index->metadata[i * index->metadata_size], metadata, copy_size);
            return 0;
        }
    }

    return -1;
}

/**
 * @brief 获取索引大小
 */
int32_t hybrid_index_size(const HybridIndex *index) {
    return index ? index->size : 0;
}

/**
 * @brief 获取索引统计信息
 */
void hybrid_index_stats(const HybridIndex *index,
                        int32_t *out_size,
                        int32_t *out_dims,
                        HybridFilterStrategy *out_strategy) {
    if (!index) return;

    if (out_size) *out_size = index->size;
    if (out_dims) *out_dims = index->dims;
    if (out_strategy) *out_strategy = index->config ? index->config->strategy : HYBRID_FILTER_AUTO;
}

/**
 * @brief 设置过滤策略
 */
void hybrid_index_set_strategy(HybridIndex *index, HybridFilterStrategy strategy) {
    if (!index || !index->config) return;
    index->config->strategy = strategy;
}

/**
 * @brief 获取当前过滤策略
 */
HybridFilterStrategy hybrid_index_get_strategy(const HybridIndex *index) {
    if (!index || !index->config) return HYBRID_FILTER_AUTO;
    return index->config->strategy;
}

/**
 * @brief 估算过滤率（简化实现）
 */
float hybrid_index_estimate_filter_rate(const HybridIndex *index,
                                        const FilterExpr *filter) {
    if (!index || !filter) return 0.0f;

    /* 简化：根据过滤条件类型估算 */
    if (filter->is_leaf) {
        /* 基于操作符估算 */
        switch (filter->cond.op) {
            case FILTER_OP_EQ:
                return 0.1f; /* 等值查询通常过滤掉大部分 */
            case FILTER_OP_NE:
                return 0.9f;
            case FILTER_OP_GT:
            case FILTER_OP_GE:
            case FILTER_OP_LT:
            case FILTER_OP_LE:
                return 0.5f; /* 范围查询 */
            default:
                return 0.5f;
        }
    } else {
        /* 复合条件 */
        float left_rate = hybrid_index_estimate_filter_rate(index, filter->left);
        float right_rate = hybrid_index_estimate_filter_rate(index, filter->right);

        if (filter->op == FILTER_OP_AND) {
            return left_rate * right_rate;
        } else if (filter->op == FILTER_OP_OR) {
            return 1.0f - (1.0f - left_rate) * (1.0f - right_rate);
        } else {
            return 1.0f - left_rate;
        }
    }
}

/**
 * @brief 释放搜索结果
 */
void hybrid_search_results_free(HybridSearchResults *results) {
    if (!results) return;
    free(results->results);
    free(results);
}

/* ========================================================================
 * 索引后端实现
 * ======================================================================== */

/**
 * @brief 创建索引后端
 */
HybridBackend *hybrid_backend_create(HybridBackendType type, int32_t dims) {
    HybridBackend *backend = (HybridBackend *)calloc(1, sizeof(HybridBackend));
    if (!backend) return NULL;

    backend->type = type;
    backend->dims = dims;
    backend->size = 0;

    /* 根据类型创建后端实现 */
    switch (type) {
        case HYBRID_BACKEND_HNSW: {
            /* HNSW 后端（简化：实际应该使用 faiss_hnsw） */
            /* 后端实现待集成 */
            break;
        }
        case HYBRID_BACKEND_DISKANN: {
            /* DiskANN 后端 */
            break;
        }
        case HYBRID_BACKEND_IVF:
        case HYBRID_BACKEND_IVF_PQ: {
            /* IVF 后端 */
            break;
        }
        default:
            /* 暴力搜索后端 */
            break;
    }

    return backend;
}

/**
 * @brief 销毁索引后端
 */
void hybrid_backend_destroy(HybridBackend *backend) {
    if (!backend) return;

    /* 清理后端资源 */
    if (backend->impl) {
        /* 根据类型清理 */
    }

    free(backend);
}

/* ========================================================================
 * SQL 函数集成
 * ======================================================================== */

/**
 * @brief 解析 VECTOR_SEARCH 函数参数（简化实现）
 */
VectorSearchArgs *vector_search_parse_args(const char *sql_func) {
    if (!sql_func) return NULL;

    VectorSearchArgs *args = (VectorSearchArgs *)calloc(1, sizeof(VectorSearchArgs));
    if (!args) return NULL;

    /* 简化解析：从 SQL 中提取参数 */
    /* 格式：VECTOR_SEARCH('collection', [1,2,3,...], 10, '{"field":"category","op":"=","value":"a"}') */

    /* 查找参数分隔 */
    const char *start = strchr(sql_func, '(');
    if (start) start++;
    else start = sql_func;

    /* 解析集合名 */
    const char *quote1 = strchr(start, '\'');
    const char *quote2 = quote1 ? strchr(quote1 + 1, '\'') : NULL;
    if (quote1 && quote2) {
        size_t len = quote2 - quote1 - 1;
        args->collection = (char *)malloc(len + 1);
        if (args->collection) {
            memcpy((char *)args->collection, quote1 + 1, len);
            ((char *)args->collection)[len] = '\0';
        }
    }

    return args;
}

/**
 * @brief 释放 VECTOR_SEARCH 函数参数
 */
void vector_search_free_args(VectorSearchArgs *args) {
    if (!args) return;
    free((void *)args->collection);
    free(args);
}

/**
 * @brief 执行 VECTOR_SEARCH（简化实现）
 */
int32_t vector_search_execute(const VectorSearchArgs *args,
                              HybridSearchResults *results) {
    if (!args || !results) return -1;

    /* 简化实现：返回空结果 */
    results->results = NULL;
    results->count = 0;
    results->capacity = 0;

    return 0;
}
