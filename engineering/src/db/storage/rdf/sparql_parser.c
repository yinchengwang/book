/**
 * @file sparql_parser.c
 * @brief SPARQL 查询解析器
 *
 * 实现基础的 SPARQL SELECT 解析。
 */
#include "sparql_parser.h"
#include <string.h>
#include <ctype.h>

/* ========================================================================
 * 词法分析
 * ======================================================================== */

typedef enum {
    TOKEN_SELECT,
    TOKEN_WHERE,
    TOKEN_OPTIONAL,
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
    TOKEN_IDENT,
    TOKEN_EOF,
    TOKEN_UNKNOWN,
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

    /* 标识符/关键字/变量 */
    if (isalpha(c) || c == '_' || c == '?') {
        size_t start = lexer->pos;
        while (lexer->pos < lexer->len &&
               (isalnum(lexer->input[lexer->pos]) || lexer->input[lexer->pos] == '_' || lexer->input[lexer->pos] == '-' || lexer->input[lexer->pos] == ':')) {
            lexer->pos++;
            lexer->col++;
        }
        size_t len = lexer->pos - start;
        if (len < sizeof(token.value)) {
            memcpy(token.value, lexer->input + start, len);
            token.value[len] = '\0';
        }

        /* 判断类型 */
        if (c == '?') {
            token.type = TOKEN_VAR;
        } else if (strcasecmp(token.value, "SELECT") == 0) token.type = TOKEN_SELECT;
        else if (strcasecmp(token.value, "WHERE") == 0) token.type = TOKEN_WHERE;
        else if (strcasecmp(token.value, "OPTIONAL") == 0) token.type = TOKEN_OPTIONAL;
        else if (strcasecmp(token.value, "LIMIT") == 0) token.type = TOKEN_LIMIT;
        else if (strcasecmp(token.value, "OFFSET") == 0) token.type = TOKEN_OFFSET;
        else if (strcasecmp(token.value, "PREFIX") == 0) token.type = TOKEN_PREFIX;
        else if (strcasecmp(token.value, "BASE") == 0) token.type = TOKEN_BASE;
        else if (strcasecmp(token.value, "a") == 0) token.type = TOKEN_A;
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
    if (parser->current.type == TOKEN_IDENT || parser->current.type == TOKEN_IDENT) {
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
    query->type = SPARQL_SELECT;

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

/** 解析 WHERE 子句 */
static int parse_where(parser_t *parser) {
    advance_token(parser);  /* 跳过 WHERE */

    /* 期望 { */
    if (!expect_token(parser, TOKEN_LBRACE)) {
        return -1;
    }
    advance_token(parser);

    /* 解析三元组模式 */
    sparql_query_t *query = parser->query;

    while (!expect_token(parser, TOKEN_RBRACE) && !expect_token(parser, TOKEN_EOF)) {
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
        }

        /* 解析谓语 */
        if (parser->current.type == TOKEN_A) {
            /* 特殊处理 rdf:type */
            strcpy(pattern->predicate_uri, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
            advance_token(parser);
        } else if (parser->current.type == TOKEN_URI_OPEN) {
            strncpy(pattern->predicate_uri, parser->current.value, 255);
            advance_token(parser);
        } else if (parser->current.type == TOKEN_IDENT) {
            /* 简写：直接写本地名 */
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

        query->triple_pattern_count++;
    }

    /* 跳过 } */
    if (expect_token(parser, TOKEN_RBRACE)) {
        advance_token(parser);
    }

    return 0;
}

/** 解析 LIMIT 子句 */
static int parse_limit(parser_t *parser) {
    advance_token(parser);  /* 跳过 LIMIT */

    if (parser->current.type == TOKEN_IDENT || parser->current.type == TOKEN_LITERAL) {
        parser->query->limit = atoi(parser->current.value);
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
            case TOKEN_LIMIT:
                parse_limit(&parser);
                break;
            default:
                advance_token(&parser);
                break;
        }
    }
}

void sparql_free(sparql_query_t *query) {
    /* 内存已内联在结构体中，无需释放 */
    (void)query;
}
