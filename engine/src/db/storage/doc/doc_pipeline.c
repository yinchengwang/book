/**
 * @file doc_pipeline.c
 * @brief 文档聚合管道实现
 *
 * 实现 MongoDB 风格的聚合管道，支持 $match/$group/$sort/$limit/$skip 等操作符。
 */

#include "db/storage/doc/doc_pipeline.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ========================================================================
 * 内存分配宏
 * ======================================================================== */

#define DOC_PIPELINE_ALLOC(type, count) ((type *)calloc(count, sizeof(type)))
#define DOC_PIPELINE_FREE(ptr) do { if (ptr) { free(ptr); (ptr) = NULL; } } while(0)

/* ========================================================================
 * 表达式实现
 * ======================================================================== */

/**
 * @brief 创建字段表达式
 */
DocExpr *doc_expr_create_field(const char *field_name) {
    if (!field_name) return NULL;

    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = DOC_EXPR_FIELD;
    expr->type = DOC_EXPR_TYPE_STRING;
    expr->field_name = strdup(field_name);
    expr->num_args = 0;
    expr->args = NULL;

    return expr;
}

/**
 * @brief 创建常量表达式
 */
DocExpr *doc_expr_create_const_int(int64_t value) {
    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = DOC_EXPR_CONST;
    expr->type = DOC_EXPR_TYPE_INT;
    expr->int_value = value;
    expr->num_args = 0;
    expr->args = NULL;

    return expr;
}

DocExpr *doc_expr_create_const_double(double value) {
    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = DOC_EXPR_CONST;
    expr->type = DOC_EXPR_TYPE_DOUBLE;
    expr->num_value = value;
    expr->num_args = 0;
    expr->args = NULL;

    return expr;
}

DocExpr *doc_expr_create_const_string(const char *value) {
    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = DOC_EXPR_CONST;
    expr->type = DOC_EXPR_TYPE_STRING;
    expr->str_value = strdup(value ? value : "");
    expr->num_args = 0;
    expr->args = NULL;

    return expr;
}

DocExpr *doc_expr_create_const_bool(bool value) {
    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = DOC_EXPR_CONST;
    expr->type = DOC_EXPR_TYPE_BOOL;
    expr->bool_value = value;
    expr->num_args = 0;
    expr->args = NULL;

    return expr;
}

/**
 * @brief 创建二元表达式
 */
DocExpr *doc_expr_create_binary(DocExprOp op, DocExpr *left, DocExpr *right) {
    if (!left || !right) return NULL;

    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = op;
    expr->num_args = 2;
    expr->args = DOC_PIPELINE_ALLOC(DocExpr *, 2);
    if (!expr->args) {
        free(expr);
        return NULL;
    }
    expr->args[0] = (DocExpr *)left;
    expr->args[1] = (DocExpr *)right;

    return expr;
}

/**
 * @brief 创建一元表达式
 */
DocExpr *doc_expr_create_unary(DocExprOp op, DocExpr *arg) {
    if (!arg) return NULL;

    DocExpr *expr = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!expr) return NULL;

    expr->op = op;
    expr->num_args = 1;
    expr->args = DOC_PIPELINE_ALLOC(DocExpr *, 1);
    if (!expr->args) {
        free(expr);
        return NULL;
    }
    expr->args[0] = (DocExpr *)arg;

    return expr;
}

/**
 * @brief 释放表达式
 */
void doc_expr_free(DocExpr *expr) {
    if (!expr) return;

    if (expr->args) {
        for (size_t i = 0; i < expr->num_args; i++) {
            doc_expr_free(expr->args[i]);
        }
        free(expr->args);
    }

    if (expr->op == DOC_EXPR_FIELD && expr->field_name) {
        free(expr->field_name);
    }
    if (expr->op == DOC_EXPR_CONST && expr->type == DOC_EXPR_TYPE_STRING && expr->str_value) {
        free(expr->str_value);
    }

    free(expr);
}

/**
 * @brief 复制表达式
 */
DocExpr *doc_expr_clone(const DocExpr *expr) {
    if (!expr) return NULL;

    DocExpr *clone = DOC_PIPELINE_ALLOC(DocExpr, 1);
    if (!clone) return NULL;

    *clone = *expr;

    if (expr->field_name) {
        clone->field_name = strdup(expr->field_name);
    }
    if (expr->str_value) {
        clone->str_value = strdup(expr->str_value);
    }

    if (expr->args && expr->num_args > 0) {
        clone->args = DOC_PIPELINE_ALLOC(DocExpr *, expr->num_args);
        if (clone->args) {
            for (size_t i = 0; i < expr->num_args; i++) {
                clone->args[i] = doc_expr_clone(expr->args[i]);
            }
        }
    }

    return clone;
}

/**
 * @brief 跳过空白字符
 */
static const char *skip_whitespace(const char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

/**
 * @brief 解析 JSON 字符串值
 */
static char *parse_json_string(const char **p) {
    if (!p || !*p || **p != '"') return NULL;

    const char *start = *p + 1;
    const char *end = start;
    while (*end && *end != '"') {
        if (*end == '\\' && end[1]) end++;
        end++;
    }

    size_t len = end - start;
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    memcpy(result, start, len);
    result[len] = '\0';
    *p = (*end == '"') ? end + 1 : end;
    return result;
}

/**
 * @brief 获取 JSON 对象中字段值
 *
 * 简化实现：支持简单的字段访问，如 {"field": value}
 */
static char *json_get_field(const char *json, const char *field) {
    if (!json || !field) return NULL;

    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\"", field);

    const char *p = strstr(json, search_key);
    if (!p) return NULL;

    p += strlen(search_key);
    p = skip_whitespace(p);
    if (!p || *p != ':') return NULL;
    p++;
    p = skip_whitespace(p);

    if (!p || !*p) return NULL;

    /* 处理不同类型 */
    if (*p == '"') {
        return parse_json_string(&p);
    } else if (*p == '{' || *p == '[') {
        /* 嵌套对象/数组 - 简化处理 */
        int depth = 1;
        const char *start = p;
        p++;
        while (*p && depth > 0) {
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') depth--;
            p++;
        }
        size_t len = p - start;
        char *result = (char *)malloc(len + 1);
        if (result) {
            memcpy(result, start, len);
            result[len] = '\0';
        }
        return result;
    } else {
        /* 数字或布尔值 */
        const char *start = p;
        while (*p && !isspace((unsigned char)*p) && *p != ',' && *p != '}') p++;
        size_t len = p - start;
        char *result = (char *)malloc(len + 1);
        if (result) {
            memcpy(result, start, len);
            result[len] = '\0';
        }
        return result;
    }
}

/**
 * @brief 获取字段的数值
 */
static int json_get_field_as_double(const char *json, const char *field, double *out_value) {
    char *str = json_get_field(json, field);
    if (!str) return -1;

    char *endptr;
    double val = strtod(str, &endptr);
    int success = (endptr != str && *endptr == '\0');
    if (success && out_value) {
        *out_value = val;
    }
    free(str);
    return success ? 0 : -1;
}

/**
 * @brief 获取字段的整数值
 */
static int json_get_field_as_int(const char *json, const char *field, int64_t *out_value) {
    char *str = json_get_field(json, field);
    if (!str) return -1;

    char *endptr;
    int64_t val = strtoll(str, &endptr, 10);
    int success = (endptr != str && *endptr == '\0');
    if (success && out_value) {
        *out_value = val;
    }
    free(str);
    return success ? 0 : -1;
}

/**
 * @brief 比较两个值
 */
static int compare_values(const char *v1, const char *v2) {
    if (!v1 && !v2) return 0;
    if (!v1) return -1;
    if (!v2) return 1;

    /* 尝试作为数字比较 */
    char *e1, *e2;
    double d1 = strtod(v1, &e1);
    double d2 = strtod(v2, &e2);

    if (e1 != v1 && e2 != v2) {
        if (d1 < d2) return -1;
        if (d1 > d2) return 1;
        return 0;
    }

    /* 作为字符串比较 */
    return strcmp(v1, v2);
}

/**
 * @brief 求值表达式（简化实现）
 *
 * TODO: 完善 JSONPath 支持和完整表达式求值
 */
int doc_expr_evaluate(const DocExpr *expr, const DocExprContext *ctx, void *result) {
    if (!expr || !ctx || !result) return -1;

    DocExprContext *res = (DocExprContext *)result;
    res->doc_json = ctx->doc_json;
    res->doc_len = ctx->doc_len;
    res->json_parser = NULL;

    switch (expr->op) {
        case DOC_EXPR_FIELD: {
            /* 从 JSON 文档中获取字段值 */
            if (expr->field_name) {
                char *value = json_get_field(ctx->doc_json, expr->field_name);
                if (value) {
                    /* 返回值存储在 result 中 */
                    ((DocExprContext *)result)->doc_json = value; /* 简化处理 */
                    return 0;
                }
            }
            return -1;
        }

        case DOC_EXPR_CONST: {
            /* 常量值已存储在表达式中 */
            return 0;
        }

        case DOC_EXPR_EQ:
        case DOC_EXPR_NE:
        case DOC_EXPR_LT:
        case DOC_EXPR_LE:
        case DOC_EXPR_GT:
        case DOC_EXPR_GE: {
            /* 二元比较运算 */
            if (expr->num_args != 2) return -1;

            char *left_val = NULL;
            char *right_val = NULL;

            if (expr->args[0]->op == DOC_EXPR_FIELD) {
                left_val = json_get_field(ctx->doc_json, expr->args[0]->field_name);
            } else if (expr->args[0]->op == DOC_EXPR_CONST) {
                if (expr->args[0]->type == DOC_EXPR_TYPE_STRING) {
                    left_val = strdup(expr->args[0]->str_value);
                } else if (expr->args[0]->type == DOC_EXPR_TYPE_INT) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lld", (long long)expr->args[0]->int_value);
                    left_val = strdup(buf);
                } else if (expr->args[0]->type == DOC_EXPR_TYPE_DOUBLE) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%g", expr->args[0]->num_value);
                    left_val = strdup(buf);
                }
            }

            if (expr->args[1]->op == DOC_EXPR_FIELD) {
                right_val = json_get_field(ctx->doc_json, expr->args[1]->field_name);
            } else if (expr->args[1]->op == DOC_EXPR_CONST) {
                if (expr->args[1]->type == DOC_EXPR_TYPE_STRING) {
                    right_val = strdup(expr->args[1]->str_value);
                } else if (expr->args[1]->type == DOC_EXPR_TYPE_INT) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%lld", (long long)expr->args[1]->int_value);
                    right_val = strdup(buf);
                } else if (expr->args[1]->type == DOC_EXPR_TYPE_DOUBLE) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%g", expr->args[1]->num_value);
                    right_val = strdup(buf);
                }
            }

            int cmp = compare_values(left_val, right_val);
            int bool_result = 0;

            switch (expr->op) {
                case DOC_EXPR_EQ: bool_result = (cmp == 0); break;
                case DOC_EXPR_NE: bool_result = (cmp != 0); break;
                case DOC_EXPR_LT: bool_result = (cmp < 0); break;
                case DOC_EXPR_LE: bool_result = (cmp <= 0); break;
                case DOC_EXPR_GT: bool_result = (cmp > 0); break;
                case DOC_EXPR_GE: bool_result = (cmp >= 0); break;
                default: break;
            }

            free(left_val);
            free(right_val);

            return bool_result ? 0 : -1;
        }

        case DOC_EXPR_AND:
        case DOC_EXPR_OR: {
            if (expr->num_args != 2) return -1;

            int left_ok = doc_expr_evaluate(expr->args[0], ctx, result);
            int right_ok = doc_expr_evaluate(expr->args[1], ctx, result);

            if (expr->op == DOC_EXPR_AND) {
                return (left_ok == 0 && right_ok == 0) ? 0 : -1;
            } else {
                return (left_ok == 0 || right_ok == 0) ? 0 : -1;
            }
        }

        case DOC_EXPR_NOT: {
            if (expr->num_args != 1) return -1;
            int ok = doc_expr_evaluate(expr->args[0], ctx, result);
            return (ok == 0) ? -1 : 0;
        }

        default:
            return -1;
    }
}

/* ========================================================================
 * $match 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $match 阶段
 */
DocMatchStage *doc_match_stage_create(const DocExpr *filter) {
    DocMatchStage *stage = DOC_PIPELINE_ALLOC(DocMatchStage, 1);
    if (!stage) return NULL;

    stage->filter = filter ? doc_expr_clone(filter) : NULL;
    stage->use_index = false;

    return stage;
}

/**
 * @brief 释放 $match 阶段
 */
void doc_match_stage_free(DocMatchStage *stage) {
    if (!stage) return;
    doc_expr_free(stage->filter);
    free(stage);
}

/* ========================================================================
 * $group 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $group 阶段
 */
DocGroupStage *doc_group_stage_create(const DocExpr *group_id, const char *group_id_field) {
    DocGroupStage *stage = DOC_PIPELINE_ALLOC(DocGroupStage, 1);
    if (!stage) return NULL;

    stage->group_id = group_id ? doc_expr_clone(group_id) : NULL;
    if (group_id_field) {
        strncpy(stage->group_id_field, group_id_field, sizeof(stage->group_id_field) - 1);
    }
    stage->accumulators = NULL;
    stage->num_accumulators = 0;

    return stage;
}

/**
 * @brief 添加累加器
 */
int doc_group_stage_add_accumulator(DocGroupStage *stage,
                                    const char *name,
                                    DocGroupAccumulator type,
                                    const DocExpr *expr) {
    if (!stage || !name) return -1;

    size_t new_size = (stage->num_accumulators + 1) * sizeof(DocGroupAccumulatorDef);
    DocGroupAccumulatorDef *new_acc = (DocGroupAccumulatorDef *)realloc(
        stage->accumulators, new_size);

    if (!new_acc) return -1;

    stage->accumulators = new_acc;
    DocGroupAccumulatorDef *acc = &stage->accumulators[stage->num_accumulators];

    /* 初始化新元素 */
    memset(acc, 0, sizeof(DocGroupAccumulatorDef));

    strncpy(acc->name, name, sizeof(acc->name) - 1);
    acc->type = type;
    acc->expr = expr ? doc_expr_clone(expr) : NULL;

    stage->num_accumulators++;
    return 0;
}

/**
 * @brief 释放 $group 阶段
 */
void doc_group_stage_free(DocGroupStage *stage) {
    if (!stage) return;

    doc_expr_free(stage->group_id);

    if (stage->accumulators) {
        for (size_t i = 0; i < stage->num_accumulators; i++) {
            doc_expr_free(stage->accumulators[i].expr);
        }
        free(stage->accumulators);
    }

    free(stage);
}

/* ========================================================================
 * $sort 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $sort 阶段
 */
DocSortStage *doc_sort_stage_create(const DocSortField *fields, size_t num_fields) {
    DocSortStage *stage = DOC_PIPELINE_ALLOC(DocSortStage, 1);
    if (!stage) return NULL;

    if (fields && num_fields > 0) {
        stage->fields = DOC_PIPELINE_ALLOC(DocSortField, num_fields);
        if (!stage->fields) {
            free(stage);
            return NULL;
        }
        memcpy(stage->fields, fields, num_fields * sizeof(DocSortField));
        stage->num_fields = num_fields;
    } else {
        stage->fields = NULL;
        stage->num_fields = 0;
    }

    return stage;
}

/**
 * @brief 添加排序字段
 */
int doc_sort_stage_add_field(DocSortStage *stage, const char *field, int direction) {
    if (!stage || !field) return -1;

    DocSortField *new_fields = (DocSortField *)realloc(
        stage->fields,
        (stage->num_fields + 1) * sizeof(DocSortField));

    if (!new_fields) return -1;

    stage->fields = new_fields;
    strncpy(stage->fields[stage->num_fields].field, field,
            sizeof(stage->fields[stage->num_fields].field) - 1);
    stage->fields[stage->num_fields].direction = (direction >= 0) ? 1 : -1;
    stage->num_fields++;

    return 0;
}

/**
 * @brief 释放 $sort 阶段
 */
void doc_sort_stage_free(DocSortStage *stage) {
    if (!stage) return;
    free(stage->fields);
    free(stage);
}

/* ========================================================================
 * $limit 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $limit 阶段
 */
DocLimitStage *doc_limit_stage_create(uint64_t limit) {
    DocLimitStage *stage = DOC_PIPELINE_ALLOC(DocLimitStage, 1);
    if (!stage) return NULL;

    stage->limit = limit;
    return stage;
}

/**
 * @brief 释放 $limit 阶段
 */
void doc_limit_stage_free(DocLimitStage *stage) {
    free(stage);
}

/* ========================================================================
 * $skip 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $skip 阶段
 */
DocSkipStage *doc_skip_stage_create(uint64_t skip) {
    DocSkipStage *stage = DOC_PIPELINE_ALLOC(DocSkipStage, 1);
    if (!stage) return NULL;

    stage->skip = skip;
    return stage;
}

/**
 * @brief 释放 $skip 阶段
 */
void doc_skip_stage_free(DocSkipStage *stage) {
    free(stage);
}

/* ========================================================================
 * $project 阶段实现
 * ======================================================================== */

/**
 * @brief 创建 $project 阶段
 */
DocProjectStage *doc_project_stage_create(const DocProjectField *fields, size_t num_fields) {
    DocProjectStage *stage = DOC_PIPELINE_ALLOC(DocProjectStage, 1);
    if (!stage) return NULL;

    if (fields && num_fields > 0) {
        stage->fields = DOC_PIPELINE_ALLOC(DocProjectField, num_fields);
        if (!stage->fields) {
            free(stage);
            return NULL;
        }
        memcpy(stage->fields, fields, num_fields * sizeof(DocProjectField));
        stage->num_fields = num_fields;
    } else {
        stage->fields = NULL;
        stage->num_fields = 0;
    }

    stage->exclude_id = false;

    return stage;
}

/**
 * @brief 释放 $project 阶段
 */
void doc_project_stage_free(DocProjectStage *stage) {
    if (!stage) return;

    if (stage->fields) {
        for (size_t i = 0; i < stage->num_fields; i++) {
            doc_expr_free(stage->fields[i].expr);
        }
        free(stage->fields);
    }

    free(stage);
}

/* ========================================================================
 * 管道阶段创建/释放
 * ======================================================================== */

/**
 * @brief 创建管道阶段
 */
DocPipelineStage *doc_pipeline_stage_create(DocPipelineStageType type) {
    DocPipelineStage *stage = DOC_PIPELINE_ALLOC(DocPipelineStage, 1);
    if (!stage) return NULL;

    stage->type = type;
    stage->next = NULL;

    switch (type) {
        case DOC_STAGE_MATCH:
            memset(&stage->match, 0, sizeof(stage->match));
            strncpy(stage->name, "$match", sizeof(stage->name) - 1);
            break;
        case DOC_STAGE_GROUP:
            memset(&stage->group, 0, sizeof(stage->group));
            strncpy(stage->name, "$group", sizeof(stage->name) - 1);
            break;
        case DOC_STAGE_SORT:
            memset(&stage->sort, 0, sizeof(stage->sort));
            strncpy(stage->name, "$sort", sizeof(stage->name) - 1);
            break;
        case DOC_STAGE_LIMIT:
            memset(&stage->limit, 0, sizeof(stage->limit));
            strncpy(stage->name, "$limit", sizeof(stage->name) - 1);
            break;
        case DOC_STAGE_SKIP:
            memset(&stage->skip, 0, sizeof(stage->skip));
            strncpy(stage->name, "$skip", sizeof(stage->name) - 1);
            break;
        case DOC_STAGE_PROJECT:
            memset(&stage->project, 0, sizeof(stage->project));
            strncpy(stage->name, "$project", sizeof(stage->name) - 1);
            break;
        default:
            strncpy(stage->name, "unknown", sizeof(stage->name) - 1);
            break;
    }

    return stage;
}

/**
 * @brief 释放管道阶段
 */
void doc_pipeline_stage_free(DocPipelineStage *stage) {
    if (!stage) return;

    switch (stage->type) {
        case DOC_STAGE_MATCH:
            doc_match_stage_free(&stage->match);
            break;
        case DOC_STAGE_GROUP:
            doc_group_stage_free(&stage->group);
            break;
        case DOC_STAGE_SORT:
            doc_sort_stage_free(&stage->sort);
            break;
        case DOC_STAGE_LIMIT:
            doc_limit_stage_free(&stage->limit);
            break;
        case DOC_STAGE_SKIP:
            doc_skip_stage_free(&stage->skip);
            break;
        case DOC_STAGE_PROJECT:
            doc_project_stage_free(&stage->project);
            break;
        default:
            break;
    }

    free(stage);
}

/* ========================================================================
 * 聚合管道
 * ======================================================================== */

/**
 * @brief 创建聚合管道
 */
DocPipeline *doc_pipeline_create(const DocPipelineConfig *config) {
    (void)config; /* TODO: 使用配置 */

    DocPipeline *pipeline = DOC_PIPELINE_ALLOC(DocPipeline, 1);
    if (!pipeline) return NULL;

    pipeline->head = NULL;
    pipeline->tail = NULL;
    pipeline->num_stages = 0;
    pipeline->mem_pool = NULL;

    return pipeline;
}

/**
 * @brief 添加管道阶段
 */
int doc_pipeline_add_stage(DocPipeline *pipeline, DocPipelineStage *stage) {
    if (!pipeline || !stage) return -1;

    stage->next = NULL;

    if (!pipeline->head) {
        pipeline->head = stage;
        pipeline->tail = stage;
    } else {
        pipeline->tail->next = stage;
        pipeline->tail = stage;
    }

    pipeline->num_stages++;
    return 0;
}

/**
 * @brief 添加 $match 阶段
 */
int doc_pipeline_add_match(DocPipeline *pipeline, const DocExpr *filter) {
    DocMatchStage *match = doc_match_stage_create(filter);
    if (!match) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_MATCH);
    if (!stage) {
        doc_match_stage_free(match);
        return -1;
    }

    memcpy(&stage->match, match, sizeof(DocMatchStage));
    free(match);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 添加 $group 阶段
 */
int doc_pipeline_add_group(DocPipeline *pipeline,
                           const DocExpr *group_id,
                           const char *group_id_field) {
    DocGroupStage *group = doc_group_stage_create(group_id, group_id_field);
    if (!group) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_GROUP);
    if (!stage) {
        doc_group_stage_free(group);
        return -1;
    }

    memcpy(&stage->group, group, sizeof(DocGroupStage));
    free(group);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 添加 $sort 阶段
 */
int doc_pipeline_add_sort(DocPipeline *pipeline,
                          const DocSortField *fields,
                          size_t num_fields) {
    DocSortStage *sort = doc_sort_stage_create(fields, num_fields);
    if (!sort) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_SORT);
    if (!stage) {
        doc_sort_stage_free(sort);
        return -1;
    }

    memcpy(&stage->sort, sort, sizeof(DocSortStage));
    free(sort);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 添加 $limit 阶段
 */
int doc_pipeline_add_limit(DocPipeline *pipeline, uint64_t limit) {
    DocLimitStage *limit_stage = doc_limit_stage_create(limit);
    if (!limit_stage) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_LIMIT);
    if (!stage) {
        doc_limit_stage_free(limit_stage);
        return -1;
    }

    memcpy(&stage->limit, limit_stage, sizeof(DocLimitStage));
    free(limit_stage);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 添加 $skip 阶段
 */
int doc_pipeline_add_skip(DocPipeline *pipeline, uint64_t skip) {
    DocSkipStage *skip_stage = doc_skip_stage_create(skip);
    if (!skip_stage) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_SKIP);
    if (!stage) {
        doc_skip_stage_free(skip_stage);
        return -1;
    }

    memcpy(&stage->skip, skip_stage, sizeof(DocSkipStage));
    free(skip_stage);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 添加 $project 阶段
 */
int doc_pipeline_add_project(DocPipeline *pipeline,
                             const DocProjectField *fields,
                             size_t num_fields) {
    DocProjectStage *project = doc_project_stage_create(fields, num_fields);
    if (!project) return -1;

    DocPipelineStage *stage = doc_pipeline_stage_create(DOC_STAGE_PROJECT);
    if (!stage) {
        doc_project_stage_free(project);
        return -1;
    }

    memcpy(&stage->project, project, sizeof(DocProjectStage));
    free(project);

    return doc_pipeline_add_stage(pipeline, stage);
}

/**
 * @brief 释放聚合管道
 */
void doc_pipeline_free(DocPipeline *pipeline) {
    if (!pipeline) return;

    DocPipelineStage *current = pipeline->head;
    while (current) {
        DocPipelineStage *next = current->next;
        doc_pipeline_stage_free(current);
        current = next;
    }

    free(pipeline);
}

/* ========================================================================
 * 管道执行器
 * ======================================================================== */

/**
 * @brief 创建管道执行器
 */
DocPipelineExecutor *doc_pipeline_executor_create(DocPipeline *pipeline, void *mem_pool) {
    if (!pipeline) return NULL;

    DocPipelineExecutor *exec = DOC_PIPELINE_ALLOC(DocPipelineExecutor, 1);
    if (!exec) return NULL;

    exec->pipeline = pipeline;
    exec->input_docs = NULL;
    exec->num_input_docs = 0;
    exec->output_docs = NULL;
    exec->num_output_docs = 0;
    exec->output_capacity = 0;
    exec->mem_pool = mem_pool;

    return exec;
}

/**
 * @brief 执行 $match 阶段
 */
static int execute_match(const DocMatchStage *stage,
                         const char **input_docs,
                         size_t num_input,
                         const char ***output_docs,
                         size_t *num_output) {
    if (!stage || !input_docs || !output_docs || !num_output) return -1;

    /* 简化实现：所有文档都通过匹配 */
    /* TODO: 实际执行 filter 表达式 */
    *output_docs = input_docs;
    *num_output = num_input;

    return 0;
}

/**
 * @brief 执行 $group 阶段（简化实现）
 */
static int execute_group(const DocGroupStage *stage,
                         const char **input_docs,
                         size_t num_input,
                         const char ***output_docs,
                         size_t *num_output) {
    if (!stage || !input_docs || !output_docs || !num_output) return -1;

    /* 简化实现：创建一个分组结果文档 */
    char *result = (char *)malloc(512);
    if (!result) return -1;

    snprintf(result, 512,
            "{\"_id\":null,\"count\":%zu,\"docs\":[",
            num_input);

    for (size_t i = 0; i < num_input; i++) {
        if (i > 0) strcat(result, ",");
        strncat(result, input_docs[i], 400);
    }

    strcat(result, "]}");

    char **results = DOC_PIPELINE_ALLOC(char *, 1);
    if (!results) {
        free(result);
        return -1;
    }

    results[0] = result;
    *output_docs = (const char **)results;
    *num_output = 1;

    return 0;
}

/**
 * @brief 排序比较函数
 * @note Windows qsort_s 要求: int compare(void *ctx, const void *a, const void *b)
 *       POSIX qsort_r 要求: int compare(const void *a, const void *b, void *ctx)
 */
#ifdef _WIN32
static int __cdecl sort_compare(void *ctx, const void *a, const void *b) {
#else
static int sort_compare(const void *a, const void *b, void *ctx) {
#endif
    const char **doc_a = (const char **)a;
    const char **doc_b = (const char **)b;
    const DocSortField *fields = (const DocSortField *)ctx;

    if (!fields) return 0;

    /* 简化实现：按第一个排序字段比较 */
    const DocSortField *field = &fields[0];
    char *val_a = json_get_field(*doc_a, field->field);
    char *val_b = json_get_field(*doc_b, field->field);

    int cmp = 0;
    if (val_a && val_b) {
        cmp = strcmp(val_a, val_b);
    }

    free(val_a);
    free(val_b);

    return (field->direction >= 0) ? cmp : -cmp;
}

/**
 * @brief 执行 $sort 阶段
 */
static int execute_sort(const DocSortStage *stage,
                        const char **input_docs,
                        size_t num_input,
                        const char ***output_docs,
                        size_t *num_output) {
    if (!stage || !input_docs || !output_docs || !num_output) return -1;

    if (num_input == 0) {
        *output_docs = NULL;
        *num_output = 0;
        return 0;
    }

    /* 复制输入以便排序 */
    char **sorted = DOC_PIPELINE_ALLOC(char *, num_input);
    if (!sorted) return -1;

    for (size_t i = 0; i < num_input; i++) {
        sorted[i] = (char *)input_docs[i];
    }

    /* 执行排序 */
    if (stage->num_fields > 0) {
#ifdef _WIN32
        /* Windows: 使用 qsort_s */
        qsort_s(sorted, num_input, sizeof(char *), sort_compare, (void *)stage->fields);
#else
        /* POSIX: 使用 qsort_r */
        qsort_r(sorted, num_input, sizeof(char *), sort_compare, (void *)stage->fields);
#endif
    }

    *output_docs = (const char **)sorted;
    *num_output = num_input;

    return 0;
}

/**
 * @brief 执行 $limit 阶段
 */
static int execute_limit(const DocLimitStage *stage,
                         const char **input_docs,
                         size_t num_input,
                         const char ***output_docs,
                         size_t *num_output) {
    if (!stage || !output_docs || !num_output) return -1;

    size_t limit = (stage->limit < num_input) ? stage->limit : num_input;

    /* 简化：直接返回指针，不复制 */
    *output_docs = input_docs;
    *num_output = limit;

    return 0;
}

/**
 * @brief 执行 $skip 阶段
 */
static int execute_skip(const DocSkipStage *stage,
                        const char **input_docs,
                        size_t num_input,
                        const char ***output_docs,
                        size_t *num_output) {
    if (!stage || !output_docs || !num_output) return -1;

    size_t skip = (stage->skip < num_input) ? stage->skip : num_input;

    /* 简化：直接返回指针，不复制 */
    *output_docs = input_docs + skip;
    *num_output = num_input - skip;

    return 0;
}

/**
 * @brief 执行 $project 阶段（简化实现）
 */
static int execute_project(const DocProjectStage *stage,
                           const char **input_docs,
                           size_t num_input,
                           const char ***output_docs,
                           size_t *num_output) {
    if (!stage || !output_docs || !num_output) return -1;

    /* 简化：直接返回原文档 */
    *output_docs = input_docs;
    *num_output = num_input;

    return 0;
}

/**
 * @brief 执行管道
 */
int doc_pipeline_execute(DocPipelineExecutor *exec,
                         const char **docs,
                         size_t num_docs,
                         char ***results) {
    if (!exec || !results) return -1;

    /* 初始化 */
    const char **current_docs = docs;
    size_t current_count = num_docs;
    const char **temp_docs = NULL;
    size_t temp_count = 0;

    /* 遍历管道阶段 */
    DocPipelineStage *stage = exec->pipeline->head;
    while (stage) {
        switch (stage->type) {
            case DOC_STAGE_MATCH:
                if (execute_match(&stage->match, current_docs, current_count,
                                  &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            case DOC_STAGE_GROUP:
                if (execute_group(&stage->group, current_docs, current_count,
                                  &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            case DOC_STAGE_SORT:
                if (execute_sort(&stage->sort, current_docs, current_count,
                                 &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            case DOC_STAGE_LIMIT:
                if (execute_limit(&stage->limit, current_docs, current_count,
                                  &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            case DOC_STAGE_SKIP:
                if (execute_skip(&stage->skip, current_docs, current_count,
                                 &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            case DOC_STAGE_PROJECT:
                if (execute_project(&stage->project, current_docs, current_count,
                                    &temp_docs, &temp_count) != 0) {
                    return -1;
                }
                break;

            default:
                /* 未知阶段，跳过 */
                break;
        }

        /* 更新当前文档集 */
        if (temp_docs != current_docs && current_docs != docs) {
            free(current_docs);
        }
        current_docs = temp_docs;
        current_count = temp_count;

        stage = stage->next;
    }

    /* 复制结果 */
    char **output = DOC_PIPELINE_ALLOC(char *, current_count);
    if (!output && current_count > 0) {
        return -1;
    }

    for (size_t i = 0; i < current_count; i++) {
        if (current_docs[i]) {
            output[i] = strdup(current_docs[i]);
        } else {
            output[i] = NULL;
        }
    }

    *results = output;
    exec->num_output_docs = current_count;

    /* 清理临时内存 */
    if (current_docs != docs) {
        free(current_docs);
    }

    return (int)current_count;
}

/**
 * @brief 释放管道执行器
 */
void doc_pipeline_executor_free(DocPipelineExecutor *exec) {
    if (!exec) return;
    free(exec);
}

/* ========================================================================
 * 便捷函数
 * ======================================================================== */

/**
 * @brief 管道转 JSON 字符串（简化实现）
 */
char *doc_pipeline_to_json(const DocPipeline *pipeline) {
    if (!pipeline) return strdup("[]");

    char *json = (char *)malloc(4096);
    if (!json) return NULL;

    char *p = json;
    p += sprintf(p, "[");

    DocPipelineStage *stage = pipeline->head;
    bool first = true;

    while (stage) {
        if (!first) {
            p += sprintf(p, ",");
        }
        first = false;

        p += sprintf(p, "{\"%s\":{}}", stage->name);
        stage = stage->next;
    }

    p += sprintf(p, "]");

    return json;
}

/**
 * @brief 解析管道 JSON（简化实现，仅支持基本功能）
 *
 * TODO: 完善 JSON 解析器以支持完整的管道语法
 */
DocPipeline *doc_pipeline_parse(const char *pipeline_json) {
    if (!pipeline_json) return NULL;

    DocPipeline *pipeline = doc_pipeline_create(NULL);
    if (!pipeline) return NULL;

    /* 简化解析：识别基本操作符 */
    const char *p = pipeline_json;
    while (*p) {
        /* 跳过空白 */
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '[') p++;

        /* 检查 $match */
        if (strncmp(p, "\"$match\"", 8) == 0) {
            doc_pipeline_add_match(pipeline, NULL);
            p += 8;
        }
        /* 检查 $group */
        else if (strncmp(p, "\"$group\"", 8) == 0) {
            doc_pipeline_add_group(pipeline, NULL, "_id");
            p += 8;
        }
        /* 检查 $sort */
        else if (strncmp(p, "\"$sort\"", 7) == 0) {
            DocSortField field = {"_id", 1};
            doc_pipeline_add_sort(pipeline, &field, 1);
            p += 7;
        }
        /* 检查 $limit */
        else if (strncmp(p, "\"$limit\"", 8) == 0) {
            doc_pipeline_add_limit(pipeline, 0);
            p += 8;
        }
        /* 检查 $skip */
        else if (strncmp(p, "\"$skip\"", 7) == 0) {
            doc_pipeline_add_skip(pipeline, 0);
            p += 7;
        }
        else if (*p == '{') {
            /* 跳过未知对象 */
            int depth = 1;
            p++;
            while (*p && depth > 0) {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            }
        }
        else if (*p == '}') {
            p++;
        }
        else if (*p == ',' || *p == ']') {
            p++;
        }
        else {
            p++;
        }
    }

    return pipeline;
}
