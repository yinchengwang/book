/**
 * @file sparql_parser.c
 * @brief SPARQL 查询解析器
 *
 * 实现基础的 SPARQL SELECT 解析，支持 FILTER、OPTIONAL、GROUP BY。
 */
#include "sparql_parser.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

/* ========================================================================
 * 词法分析
 * ======================================================================== */

typedef enum {
    TOKEN_SELECT,
    TOKEN_WHERE,
    TOKEN_OPTIONAL,
    TOKEN_FILTER,
    TOKEN_GROUP,
    TOKEN_BY,
    TOKEN_LIMIT,
    TOKEN_OFFSET,
    TOKEN_PREFIX,
    TOKEN_BASE,
    TOKEN_A,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_DOT,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_URI_OPEN,
    TOKEN_URI_CLOSE,
    TOKEN_VAR,
    TOKEN_LITERAL,
    TOKEN_NUMBER,
    TOKEN_IDENT,
    TOKEN_EOF,
    TOKEN_UNKNOWN,
    /* 比较操作符 */
    TOKEN_GT,
    TOKEN_LT,
    TOKEN_EQ,
    TOKEN_GE,
    TOKEN_LE,
    TOKEN_NE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_BOUND,
    TOKEN_STR,
    TOKEN_COUNT,
    TOKEN_SUM,
    TOKEN_AVG,
    TOKEN_MIN,
    TOKEN_MAX,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
} token_type_t;

typedef struct {
    token_type_t type;
    char value[256];
    int line;
    int col;
} token_t;

typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    int line;
    int col;
} lexer_t;

static void lexer_init(lexer_t *lexer, const char *input) {
    lexer->input = input;
    lexer->pos = 0;
    lexer->len = strlen(input);
    lexer->line = 1;
    lexer->col = 1;
}

static void skip_whitespace(lexer_t *lexer) {
    while (lexer->pos < lexer->len) {
        char c = lexer->input[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            lexer->pos++;
            if (c == '\n') {
                lexer->line++;
                lexer->col = 1;
            } else {
                lexer->col++;
            }
        } else if (c == '#') {
            /* 跳过注释 */
            while (lexer->pos < lexer->len && lexer->input[lexer->pos] != '\n') {
                lexer->pos++;
            }
        } else {
            break;
        }
    }
}

static token_t next_token(lexer_t *lexer) {
    token_t token = {TOKEN_UNKNOWN, {0}, lexer->line, lexer->col};
    skip_whitespace(lexer);

    if (lexer->pos >= lexer->len) {
        token.type = TOKEN_EOF;
        return token;
    }

    char c = lexer->input[lexer->pos];

    /* 单字符 token */
    switch (c) {
        case '{': token.type = TOKEN_LBRACE; lexer->pos++; lexer->col++; return token;
        case '}': token.type = TOKEN_RBRACE; lexer->pos++; lexer->col++; return token;
        case '.': token.type = TOKEN_DOT; lexer->pos++; lexer->col++; return token;
        case ',': token.type = TOKEN_COMMA; lexer->pos++; lexer->col++; return token;
        case ';': token.type = TOKEN_SEMICOLON; lexer->pos++; lexer->col++; return token;
        case '(': token.type = TOKEN_LPAREN; lexer->pos++; lexer->col++; return token;
        case ')': token.type = TOKEN_RPAREN; lexer->pos++; lexer->col++; return token;
    }

    /* URI <...> */
    if (c == '<') {
        lexer->pos++;
        lexer->col++;
        size_t start = lexer->pos;
        while (lexer->pos < lexer->len && lexer->input[lexer->pos] != '>') {
            lexer->pos++;
            lexer->col++;
        }
        if (lexer->pos < lexer->len) {
            size_t len = lexer->pos - start;
            if (len < sizeof(token.value)) {
                memcpy(token.value, lexer->input + start, len);
                token.value[len] = '\0';
            }
            token.type = TOKEN_URI_OPEN;
            lexer->pos++;
            lexer->col++;
            return token;
        }
    }

    /* 字符串字面量 "..." */
    if (c == '"') {
        lexer->pos++;
        lexer->col++;
        size_t start = lexer->pos;
        while (lexer->pos < lexer->len && lexer->input[lexer->pos] != '"') {
            lexer->pos++;
            lexer->col++;
        }
        size_t len = lexer->pos - start;
        if (len < sizeof(token.value)) {
            memcpy(token.value, lexer->input + start, len);
            token.value[len] = '\0';
        }
        token.type = TOKEN_LITERAL;
        if (lexer->pos < lexer->len) {
            lexer->pos++;
            lexer->col++;
        }
        return token;
    }

    /* 数字字面量 */
    if (isdigit(c) || (c == '.' && lexer->pos + 1 < lexer->len && isdigit(lexer->input[lexer->pos + 1]))) {
        size_t start = lexer->pos;
        bool has_dot = false;
        while (lexer->pos < lexer->len && (isdigit(lexer->input[lexer->pos]) || lexer->input[lexer->pos] == '.')) {
            if (lexer->input[lexer->pos] == '.') {
                if (has_dot) break;
                has_dot = true;
            }
            lexer->pos++;
            lexer->col++;
        }
        size_t len = lexer->pos - start;
        if (len < sizeof(token.value)) {
            memcpy(token.value, lexer->input + start, len);
            token.value[len] = '\0';
        }
        token.type = TOKEN_NUMBER;
        return token;
    }

    /* 比较操作符: >, <, =, >=, <=, != */
    if (c == '>') {
        token.type = TOKEN_GT;
        lexer->pos++; lexer->col++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '=') {
            token.type = TOKEN_GE;
            lexer->pos++; lexer->col++;
        }
        return token;
    }
    if (c == '<') {
        token.type = TOKEN_LT;
        lexer->pos++; lexer->col++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '=') {
            token.type = TOKEN_LE;
            lexer->pos++; lexer->col++;
        }
        return token;
    }
    if (c == '=') {
        token.type = TOKEN_EQ;
        lexer->pos++; lexer->col++;
        return token;
    }
    if (c == '!') {
        lexer->pos++; lexer->col++;
        if (lexer->pos < lexer->len && lexer->input[lexer->pos] == '=') {
            token.type = TOKEN_NE;
            lexer->pos++; lexer->col++;
        } else {
            token.type = TOKEN_UNKNOWN;
        }
        return token;
    }

    /* 标识符/关键字/变量 */
    if (isalpha(c) || c == '_' || c == '?') {
        size_t start = lexer->pos;
        while (lexer->pos < lexer->len &&
               (isalnum(lexer->input[lexer->pos]) || lexer->input[lexer->pos] == '_' || lexer->input[lexer->pos] == '-' || lexer->input[lexer->pos] == ':' || lexer->input[lexer->pos] == '?')) {
            lexer->pos++;
            lexer->col++;
        }
        size_t len = lexer->pos - start;
        if (len < sizeof(token.value)) {
            memcpy(token.value, lexer->input + start, len);
            token.value[len] = '\0';
        }

        /* 判断类型 */
        if (c == '?' || token.value[0] == '?') {
            token.type = TOKEN_VAR;
        } else if (strcasecmp(token.value, "SELECT") == 0) token.type = TOKEN_SELECT;
        else if (strcasecmp(token.value, "WHERE") == 0) token.type = TOKEN_WHERE;
        else if (strcasecmp(token.value, "OPTIONAL") == 0) token.type = TOKEN_OPTIONAL;
        else if (strcasecmp(token.value, "FILTER") == 0) token.type = TOKEN_FILTER;
        else if (strcasecmp(token.value, "GROUP") == 0) token.type = TOKEN_GROUP;
        else if (strcasecmp(token.value, "BY") == 0) token.type = TOKEN_BY;
        else if (strcasecmp(token.value, "LIMIT") == 0) token.type = TOKEN_LIMIT;
        else if (strcasecmp(token.value, "OFFSET") == 0) token.type = TOKEN_OFFSET;
        else if (strcasecmp(token.value, "PREFIX") == 0) token.type = TOKEN_PREFIX;
        else if (strcasecmp(token.value, "BASE") == 0) token.type = TOKEN_BASE;
        else if (strcasecmp(token.value, "a") == 0) token.type = TOKEN_A;
        else if (strcasecmp(token.value, "BOUND") == 0) token.type = TOKEN_BOUND;
        else if (strcasecmp(token.value, "STR") == 0) token.type = TOKEN_STR;
        else if (strcasecmp(token.value, "COUNT") == 0) token.type = TOKEN_COUNT;
        else if (strcasecmp(token.value, "SUM") == 0) token.type = TOKEN_SUM;
        else if (strcasecmp(token.value, "AVG") == 0) token.type = TOKEN_AVG;
        else if (strcasecmp(token.value, "MIN") == 0) token.type = TOKEN_MIN;
        else if (strcasecmp(token.value, "MAX") == 0) token.type = TOKEN_MAX;
        else if (strcasecmp(token.value, "AND") == 0) token.type = TOKEN_AND;
        else if (strcasecmp(token.value, "OR") == 0) token.type = TOKEN_OR;
        else if (strcasecmp(token.value, "NOT") == 0) token.type = TOKEN_NOT;
        else token.type = TOKEN_IDENT;

        return token;
    }

    /* 未知字符 */
    token.value[0] = c;
    token.type = TOKEN_UNKNOWN;
    lexer->pos++;
    lexer->col++;
    return token;
}

/* ========================================================================
 * 解析器
 * ======================================================================== */

/** 前缀定义 */
typedef struct {
    char prefix[64];
    char uri[256];
} prefix_def_t;

/** SPARQL 解析上下文 */
typedef struct {
    lexer_t lexer;
    token_t current;
    prefix_def_t prefixes[16];
    int prefix_count;
    sparql_query_t *query;
} parser_t;

static void advance_token(parser_t *parser) {
    parser->current = next_token(&parser->lexer);
}

static bool expect_token(parser_t *parser, token_type_t type) {
    return parser->current.type == type;
}

/** 解析 PREFIX 子句 */
static int parse_prefix(parser_t *parser) {
    advance_token(parser);  /* 跳过 PREFIX */

    /* 解析前缀名 */
    if (parser->current.type == TOKEN_IDENT) {
        if (parser->prefix_count < 16) {
            strncpy(parser->prefixes[parser->prefix_count].prefix,
                   parser->current.value, 63);
        }
        advance_token(parser);
    }

    /* 跳过 : */
    if (parser->current.type == TOKEN_UNKNOWN && parser->current.value[0] == ':') {
        advance_token(parser);
    }

    /* 解析 URI */
    if (parser->current.type == TOKEN_URI_OPEN) {
        if (parser->prefix_count < 16) {
            strncpy(parser->prefixes[parser->prefix_count].uri,
                   parser->current.value, 255);
            parser->prefix_count++;
        }
        advance_token(parser);
    }

    return 0;
}

/** 解析 SELECT 子句 */
static int parse_select(parser_t *parser) {
    advance_token(parser);  /* 跳过 SELECT */

    sparql_query_t *query = parser->query;
    query->type = SPARQL_QUERY_SELECT;

    /* 解析变量 */
    while (parser->current.type == TOKEN_VAR) {
        if (query->variable_count < 32) {
            strncpy(query->variables[query->variable_count],
                   parser->current.value + 1, 63);  /* 去掉 ? */
            query->variable_count++;
        }
        advance_token(parser);
    }

    return 0;
}

/** 解析一个三元组模式 */
static int parse_triple_pattern(parser_t *parser, sparql_triple_pattern_t *pattern) {
    memset(pattern, 0, sizeof(*pattern));

    /* 解析主体 */
    if (parser->current.type == TOKEN_VAR) {
        pattern->is_subject_var = true;
        strncpy(pattern->subject_var, parser->current.value + 1, 63);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_URI_OPEN) {
        strncpy(pattern->subject_uri, parser->current.value, 255);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_A) {
        strcpy(pattern->subject_uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
        advance_token(parser);
    }

    /* 解析谓语 */
    if (parser->current.type == TOKEN_A) {
        strcpy(pattern->predicate_uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
        advance_token(parser);
    } else if (parser->current.type == TOKEN_URI_OPEN) {
        strncpy(pattern->predicate_uri, parser->current.value, 255);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_IDENT) {
        strcpy(pattern->predicate_uri, "http://example.org/");
        strncat(pattern->predicate_uri, parser->current.value, 255 - strlen(pattern->predicate_uri));
        advance_token(parser);
    }

    /* 解析宾语 */
    if (parser->current.type == TOKEN_VAR) {
        pattern->is_object_var = true;
        strncpy(pattern->object_var, parser->current.value + 1, 63);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_URI_OPEN) {
        strncpy(pattern->object_uri, parser->current.value, 255);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_LITERAL) {
        strncpy(pattern->object_literal, parser->current.value, 255);
        pattern->object_is_literal = true;
        advance_token(parser);
    }

    return 0;
}

/** 解析简单的比较表达式 */
static int parse_comparison(parser_t *parser, sparql_filter_t *filter) {
    memset(filter, 0, sizeof(*filter));
    filter->type = SPARQL_EXPR_COMPARE;

    /* 左边: 变量 */
    if (parser->current.type == TOKEN_VAR) {
        strncpy(filter->var_left, parser->current.value + 1, 63);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_IDENT) {
        strncpy(filter->var_left, parser->current.value, 63);
        advance_token(parser);
    }

    /* 操作符 */
    if (parser->current.type == TOKEN_GT) {
        filter->op = SPARQL_OP_GT;
        advance_token(parser);
    } else if (parser->current.type == TOKEN_LT) {
        filter->op = SPARQL_OP_LT;
        advance_token(parser);
    } else if (parser->current.type == TOKEN_EQ) {
        filter->op = SPARQL_OP_EQ;
        advance_token(parser);
    } else if (parser->current.type == TOKEN_GE) {
        filter->op = SPARQL_OP_GE;
        advance_token(parser);
    } else if (parser->current.type == TOKEN_LE) {
        filter->op = SPARQL_OP_LE;
        advance_token(parser);
    } else if (parser->current.type == TOKEN_NE) {
        filter->op = SPARQL_OP_NE;
        advance_token(parser);
    }

    /* 右边: 变量或数字 */
    if (parser->current.type == TOKEN_VAR) {
        strncpy(filter->var_right, parser->current.value + 1, 63);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_IDENT) {
        strncpy(filter->var_right, parser->current.value, 63);
        advance_token(parser);
    } else if (parser->current.type == TOKEN_NUMBER || parser->current.type == TOKEN_LITERAL) {
        /* 尝试解析为数字 */
        char *end;
        int64_t ival = strtoll(parser->current.value, &end, 10);
        if (*end == '\0') {
            filter->value.int_val = ival;
            filter->value_type = 0;
        } else {
            double dval = strtod(parser->current.value, &end);
            if (*end == '\0') {
                filter->value.double_val = dval;
                filter->value_type = 1;
            } else {
                filter->value.str_val = strdup(parser->current.value);
                filter->value_type = 2;
            }
        }
        advance_token(parser);
    }

    return 0;
}

/** 解析 FILTER 表达式 (内部) */
static int parse_filter_expr(parser_t *parser, sparql_filter_t *filter) {
    memset(filter, 0, sizeof(*filter));

    /* 检查是否为 BOUND 函数 */
    if (parser->current.type == TOKEN_BOUND) {
        filter->type = SPARQL_EXPR_FUNCTION;
        advance_token(parser);
        if (parser->current.type == TOKEN_LPAREN) advance_token(parser);
        if (parser->current.type == TOKEN_VAR) {
            strncpy(filter->var_left, parser->current.value + 1, 63);
            advance_token(parser);
        }
        if (parser->current.type == TOKEN_RPAREN) advance_token(parser);
        return 0;
    }

    /* 检查是否为 NOT BOUND */
    if (parser->current.type == TOKEN_NOT) {
        filter->type = SPARQL_EXPR_FUNCTION;
        filter->op = SPARQL_OP_NE;
        advance_token(parser);
        if (parser->current.type == TOKEN_BOUND) advance_token(parser);
        if (parser->current.type == TOKEN_LPAREN) advance_token(parser);
        if (parser->current.type == TOKEN_VAR) {
            strncpy(filter->var_left, parser->current.value + 1, 63);
            advance_token(parser);
        }
        if (parser->current.type == TOKEN_RPAREN) advance_token(parser);
        return 0;
    }

    /* 检查是否为括号表达式 */
    if (parser->current.type == TOKEN_LPAREN) {
        advance_token(parser);
        parse_filter_expr(parser, filter);
        if (parser->current.type == TOKEN_RPAREN) advance_token(parser);
        return 0;
    }

    /* 否则是普通比较表达式 */
    return parse_comparison(parser, filter);
}

/** 解析 FILTER */
static int parse_filter(parser_t *parser, sparql_filter_t **out_filter) {
    advance_token(parser);  /* 跳过 FILTER */

    sparql_filter_t *filter = (sparql_filter_t *)calloc(1, sizeof(sparql_filter_t));
    if (!filter) return -1;

    /* 期待 ( */
    if (parser->current.type == TOKEN_LPAREN) {
        advance_token(parser);
    }

    /* 解析表达式 */
    if (parse_filter_expr(parser, filter) != 0) {
        free(filter);
        return -1;
    }

    /* 期待 ) */
    if (parser->current.type == TOKEN_RPAREN) {
        advance_token(parser);
    }

    *out_filter = filter;
    return 0;
}

/** 解析 OPTIONAL 块 */
static int parse_optional(parser_t *parser, sparql_query_t *query) {
    advance_token(parser);  /* 跳过 OPTIONAL */

    /* 期待 { */
    if (!expect_token(parser, TOKEN_LBRACE)) {
        return -1;
    }
    advance_token(parser);

    /* 分配新的 optional 块 */
    sparql_optional_t *opt = (sparql_optional_t *)calloc(1, sizeof(sparql_optional_t));
    if (!opt) return -1;
    opt->next = query->optionals;
    query->optionals = opt;

    /* 解析可选三元组 */
    while (!expect_token(parser, TOKEN_RBRACE) && !expect_token(parser, TOKEN_EOF)) {
        if (opt->pattern_count >= 32) break;

        sparql_triple_pattern_t *pattern = &opt->patterns[opt->pattern_count];
        parse_triple_pattern(parser, pattern);

        if (pattern->subject_var[0] != 0 || pattern->subject_uri[0] != 0) {
            opt->pattern_count++;
        }

        /* 跳过 . */
        if (expect_token(parser, TOKEN_DOT)) {
            advance_token(parser);
        }
    }

    /* 跳过 } */
    if (expect_token(parser, TOKEN_RBRACE)) {
        advance_token(parser);
    }

    return 0;
}

/** 解析 WHERE 子句 */
static int parse_where(parser_t *parser) {
    advance_token(parser);  /* 跳过 WHERE */

    /* 期望 { */
    if (!expect_token(parser, TOKEN_LBRACE)) {
        return -1;
    }
    advance_token(parser);

    /* 解析三元组模式和 FILTER */
    sparql_query_t *query = parser->query;

    while (!expect_token(parser, TOKEN_RBRACE) && !expect_token(parser, TOKEN_EOF)) {
        /* 检查 OPTIONAL */
        if (expect_token(parser, TOKEN_OPTIONAL)) {
            parse_optional(parser, query);
            continue;
        }

        /* 检查 FILTER */
        if (expect_token(parser, TOKEN_FILTER)) {
            sparql_filter_t *filter = NULL;
            if (parse_filter(parser, &filter) == 0 && filter) {
                filter->next = query->filters;
                query->filters = filter;
                query->filter_count++;
            }
            continue;
        }

        if (query->triple_pattern_count >= 32) {
            break;
        }

        sparql_triple_pattern_t *pattern = &query->triple_patterns[query->triple_pattern_count];

        /* 解析主体 */
        if (parser->current.type == TOKEN_VAR) {
            pattern->is_subject_var = true;
            strncpy(pattern->subject_var, parser->current.value + 1, 63);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_URI_OPEN) {
            strncpy(pattern->subject_uri, parser->current.value, 255);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_A) {
            strcpy(pattern->predicate_uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
            advance_token(parser);
        }

        /* 解析谓语 */
        if (parser->current.type == TOKEN_A) {
            strcpy(pattern->predicate_uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
            advance_token(parser);
        } else if (parser->current.type == TOKEN_URI_OPEN) {
            strncpy(pattern->predicate_uri, parser->current.value, 255);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_IDENT) {
            strcpy(pattern->predicate_uri, "http://example.org/");
            strncat(pattern->predicate_uri, parser->current.value, 255 - strlen(pattern->predicate_uri));
            advance_token(parser);
        }

        /* 解析宾语 */
        if (parser->current.type == TOKEN_VAR) {
            pattern->is_object_var = true;
            strncpy(pattern->object_var, parser->current.value + 1, 63);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_URI_OPEN) {
            strncpy(pattern->object_uri, parser->current.value, 255);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_LITERAL) {
            strncpy(pattern->object_literal, parser->current.value, 255);
            pattern->object_is_literal = true;
            advance_token(parser);
        }

        /* 跳过 . */
        if (expect_token(parser, TOKEN_DOT)) {
            advance_token(parser);
        }

        if (pattern->subject_var[0] != 0 || pattern->subject_uri[0] != 0) {
            query->triple_pattern_count++;
        }
    }

    /* 跳过 } */
    if (expect_token(parser, TOKEN_RBRACE)) {
        advance_token(parser);
    }

    return 0;
}

/** 解析 GROUP BY 子句 */
static int parse_group_by(parser_t *parser) {
    advance_token(parser);  /* 跳过 GROUP */
    advance_token(parser);  /* 跳过 BY */

    sparql_query_t *query = parser->query;

    /* 解析分组变量 */
    while (parser->current.type == TOKEN_VAR) {
        if (query->group_by_count < 32) {
            strncpy(query->group_by_vars[query->group_by_count],
                   parser->current.value + 1, 63);
            query->group_by_count++;
        }
        advance_token(parser);
    }

    return 0;
}

/** 解析 LIMIT 子句 */
static int parse_limit(parser_t *parser) {
    advance_token(parser);  /* 跳过 LIMIT */

    if (parser->current.type == TOKEN_NUMBER) {
        parser->query->limit = atoi(parser->current.value);
        advance_token(parser);
    }

    return 0;
}

/** 解析 OFFSET 子句 */
static int parse_offset(parser_t *parser) {
    advance_token(parser);  /* 跳过 OFFSET */

    if (parser->current.type == TOKEN_NUMBER) {
        parser->query->offset = atoi(parser->current.value);
        advance_token(parser);
    }

    return 0;
}

/* ========================================================================
 * 公开 API
 * ======================================================================== */

void sparql_parse(const char *query_str, sparql_query_t *result) {
    memset(result, 0, sizeof(sparql_query_t));

    parser_t parser;
    lexer_init(&parser.lexer, query_str);
    parser.query = result;
    parser.prefix_count = 0;

    advance_token(&parser);

    while (!expect_token(&parser, TOKEN_EOF)) {
        switch (parser.current.type) {
            case TOKEN_PREFIX:
                parse_prefix(&parser);
                break;
            case TOKEN_SELECT:
                parse_select(&parser);
                break;
            case TOKEN_WHERE:
                parse_where(&parser);
                break;
            case TOKEN_OPTIONAL:
                parse_optional(&parser, result);
                break;
            case TOKEN_FILTER:
                {
                    sparql_filter_t *filter = NULL;
                    if (parse_filter(&parser, &filter) == 0 && filter) {
                        filter->next = result->filters;
                        result->filters = filter;
                        result->filter_count++;
                    }
                }
                break;
            case TOKEN_GROUP:
                parse_group_by(&parser);
                break;
            case TOKEN_LIMIT:
                parse_limit(&parser);
                break;
            case TOKEN_OFFSET:
                parse_offset(&parser);
                break;
            default:
                advance_token(&parser);
                break;
        }
    }

    /* 复制前缀到查询结构 */
    for (int i = 0; i < parser.prefix_count && i < 16; i++) {
        strncpy(result->prefixes[i].prefix, parser.prefixes[i].prefix, 63);
        strncpy(result->prefixes[i].uri, parser.prefixes[i].uri, 255);
        result->prefix_count++;
    }
}

void sparql_free(sparql_query_t *query) {
    /* 释放 FILTER 链表 */
    sparql_filter_t *f = query->filters;
    while (f) {
        sparql_filter_t *next = f->next;
        if (f->value.str_val) {
            free(f->value.str_val);
        }
        free(f);
        f = next;
    }
    query->filters = NULL;

    /* 释放 OPTIONAL 链表 */
    sparql_optional_t *opt = query->optionals;
    while (opt) {
        sparql_optional_t *next = opt->next;
        free(opt);
        opt = next;
    }
    query->optionals = NULL;

    (void)query;
}

/* ========================================================================
 * FILTER API
 * ======================================================================== */

int sparql_filter_parse(const char *expr, sparql_filter_t *filter) {
    if (!expr || !filter) return -1;

    lexer_t lexer;
    lexer_init(&lexer, expr);

    parser_t parser;
    parser.lexer = lexer;
    parser.query = NULL;
    parser.prefix_count = 0;

    advance_token(&parser);
    return parse_filter_expr(&parser, filter);
}

int sparql_filter_evaluate(const sparql_filter_t *filter, const sparql_binding_t *bindings, int binding_count) {
    if (!filter) return 1;  /* 无 filter 则通过 */

    const char *left_val = NULL;
    const char *right_val = NULL;
    int left_bound = 0, right_bound = 0;

    /* 查找左值 */
    if (filter->var_left[0] != 0) {
        for (int i = 0; i < binding_count; i++) {
            if (strcmp(bindings[i].var_name, filter->var_left) == 0) {
                left_val = bindings[i].value;
                left_bound = bindings[i].is_bound;
                break;
            }
        }
    }

    /* 查找右值 */
    if (filter->var_right[0] != 0) {
        for (int i = 0; i < binding_count; i++) {
            if (strcmp(bindings[i].var_name, filter->var_right) == 0) {
                right_val = bindings[i].value;
                right_bound = bindings[i].is_bound;
                break;
            }
        }
    }

    /* BOUND 检查 */
    if (filter->type == SPARQL_EXPR_FUNCTION) {
        if (filter->op == SPARQL_OP_NE) {  /* NOT BOUND */
            return !left_bound;
        }
        return left_bound ? 1 : 0;
    }

    /* 如果左值或右值未绑定，表达式无效 */
    if (filter->var_left[0] != 0 && !left_bound) return 0;
    if (filter->var_right[0] != 0 && !right_bound) return 0;

    /* 比较操作 */
    if (filter->type == SPARQL_EXPR_COMPARE) {
        double left_num = 0, right_num = 0;
        int left_is_num = 0, right_is_num = 0;

        /* 解析左值为数字 */
        if (left_val) {
            char *end;
            left_num = strtod(left_val, &end);
            left_is_num = (*end == '\0');
        } else if (filter->value_type == 0) {
            left_num = filter->value.int_val;
            left_is_num = 1;
        } else if (filter->value_type == 1) {
            left_num = filter->value.double_val;
            left_is_num = 1;
        }

        /* 解析右值为数字 */
        if (right_val) {
            char *end;
            right_num = strtod(right_val, &end);
            right_is_num = (*end == '\0');
        } else if (filter->value_type == 0) {
            right_num = filter->value.int_val;
            right_is_num = 1;
        } else if (filter->value_type == 1) {
            right_num = filter->value.double_val;
            right_is_num = 1;
        }

        /* 数值比较 */
        if (left_is_num && right_is_num) {
            switch (filter->op) {
                case SPARQL_OP_GT: return left_num > right_num ? 1 : 0;
                case SPARQL_OP_LT: return left_num < right_num ? 1 : 0;
                case SPARQL_OP_EQ: return left_num == right_num ? 1 : 0;
                case SPARQL_OP_GE: return left_num >= right_num ? 1 : 0;
                case SPARQL_OP_LE: return left_num <= right_num ? 1 : 0;
                case SPARQL_OP_NE: return left_num != right_num ? 1 : 0;
                default: return 0;
            }
        }

        /* 字符串比较 */
        const char *lstr = left_val ? left_val : (filter->value.str_val ? filter->value.str_val : "");
        const char *rstr = right_val ? right_val : (filter->value.str_val ? filter->value.str_val : "");

        int cmp = strcmp(lstr, rstr);
        switch (filter->op) {
            case SPARQL_OP_GT: return cmp > 0 ? 1 : 0;
            case SPARQL_OP_LT: return cmp < 0 ? 1 : 0;
            case SPARQL_OP_EQ: return cmp == 0 ? 1 : 0;
            case SPARQL_OP_GE: return cmp >= 0 ? 1 : 0;
            case SPARQL_OP_LE: return cmp <= 0 ? 1 : 0;
            case SPARQL_OP_NE: return cmp != 0 ? 1 : 0;
            default: return 0;
        }
    }

    return 0;
}

void sparql_filter_free(sparql_filter_t *filter) {
    if (filter) {
        if (filter->value.str_val) {
            free(filter->value.str_val);
        }
        if (filter->next) {
            sparql_filter_free(filter->next);
        }
        free(filter);
    }
}
