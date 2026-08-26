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

/** SPARQL 查询结构 */
typedef struct sparql_query_s {
    sparql_query_type_e type;

    /* SELECT 变量 */
    char variables[32][64];
    int variable_count;

    /* 三元组模式 */
    sparql_triple_pattern_t triple_patterns[32];
    int triple_pattern_count;

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

/** 解析 SPARQL 查询 */
void sparql_parse(const char *query, sparql_query_t *result);

/** 释放查询结构 */
void sparql_free(sparql_query_t *query);

#endif /* SPARQL_PARSER_H */
