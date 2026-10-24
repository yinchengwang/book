/**
 * @file sparql_parser.h
 * @brief SPARQL 解析器接口
 */
#ifndef SPARQL_PARSER_H
#define SPARQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

/** SPARQL 查询类型 */
typedef enum {
    SPARQL_QUERY_SELECT = 0,
    SPARQL_QUERY_CONSTRUCT,
    SPARQL_QUERY_ASK,
    SPARQL_QUERY_DESCRIBE,
} sparql_query_type_e;

/** 三元组模式 */
typedef struct sparql_triple_pattern_s {
    /* 主语 */
    char subject_uri[256];
    char subject_var[64];
    bool is_subject_var;

    /* 谓语 */
    char predicate_uri[256];

    /* 宾语 */
    char object_uri[256];
    char object_var[64];
    char object_literal[256];
    bool is_object_var;
    bool object_is_literal;
} sparql_triple_pattern_t;

/** OPTIONAL 模式链表 */
typedef struct sparql_optional_s {
    sparql_triple_pattern_t patterns[32];
    int pattern_count;
    struct sparql_optional_s *next;
} sparql_optional_t;

/** 聚合函数 */
typedef struct sparql_aggregate_s {
    char func_name[16];      /* COUNT, SUM, AVG, MIN, MAX */
    char var_name[64];       /* 输入变量 */
    char alias[64];          /* 输出别名 */
} sparql_aggregate_t;

/** SPARQL 查询结构 */
typedef struct sparql_query_s {
    sparql_query_type_e type;

    /* SELECT 变量 */
    char variables[32][64];
    int variable_count;

    /* 三元组模式 */
    sparql_triple_pattern_t triple_patterns[32];
    int triple_pattern_count;

    /* FILTER 表达式 */
    struct sparql_filter_s *filters;
    int filter_count;

    /* OPTIONAL 模式 */
    sparql_optional_t *optionals;

    /* GROUP BY */
    char group_by_vars[32][64];
    int group_by_count;

    /* 聚合函数 */
    sparql_aggregate_t aggregates[16];
    int aggregate_count;

    /* 限制 */
    int limit;
    int offset;

    /* 前缀 */
    struct {
        char prefix[64];
        char uri[256];
    } prefixes[16];
    int prefix_count;
} sparql_query_t;

/* ========================================================================
 * FILTER 表达式
 * ======================================================================== */

/** FILTER 表达式类型 */
typedef enum {
    SPARQL_EXPR_COMPARE,   /* 比较: >, <, =, >=, <= */
    SPARQL_EXPR_LOGICAL,   /* 逻辑: AND, OR, NOT */
    SPARQL_EXPR_FUNCTION,  /* 函数: BOUND, STR, LANG */
} sparql_expr_type_t;

/** 比较操作符 */
typedef enum {
    SPARQL_OP_GT,   /* > */
    SPARQL_OP_LT,   /* < */
    SPARQL_OP_EQ,   /* = */
    SPARQL_OP_GE,   /* >= */
    SPARQL_OP_LE,   /* <= */
    SPARQL_OP_NE,   /* != */
} sparql_expr_op_t;

/** FILTER 表达式值联合体 */
typedef union {
    int64_t int_val;
    double double_val;
    char *str_val;
} sparql_expr_value_t;

/** FILTER 表达式节点 */
typedef struct sparql_filter_s {
    sparql_expr_type_t type;
    sparql_expr_op_t op;
    char var_left[64];
    char var_right[64];
    sparql_expr_value_t value;
    int value_type;  /* 0=int, 1=double, 2=string */
    struct sparql_filter_s *next;  /* AND/OR 链 */
} sparql_filter_t;

/** 变量绑定 */
typedef struct {
    char var_name[64];
    char value[256];
    int is_bound;
} sparql_binding_t;

/** C/C++ 交叉链接保护 */
#ifdef __cplusplus
extern "C" {
#endif

/** 解析 FILTER 表达式 */
int sparql_filter_parse(const char *expr, sparql_filter_t *filter);

/** 评估 FILTER 表达式 */
int sparql_filter_evaluate(const sparql_filter_t *filter, const sparql_binding_t *bindings, int binding_count);

/** 释放 FILTER 表达式 */
void sparql_filter_free(sparql_filter_t *filter);

/** 解析 SPARQL 查询 */
void sparql_parse(const char *query, sparql_query_t *result);

/** 释放查询结构 */
void sparql_free(sparql_query_t *query);

#ifdef __cplusplus
}
#endif

#endif /* SPARQL_PARSER_H */
