/**
 * @file graph_cypher.c
 * @brief Cypher 查询语言解析器实现
 *
 * 实现词法分析器和递归下降解析器
 */

#include "db/graph/graph_cypher.h"
#include "db/graph/graph_engine.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ========================================================================
 * 关键字表
 * ======================================================================== */

static struct {
    const char *keyword;
    CypherTokenType token;
} keywords[] = {
    {"MATCH", CYPHER_TOKEN_MATCH},
    {"RETURN", CYPHER_TOKEN_RETURN},
    {"WHERE", CYPHER_TOKEN_WHERE},
    {"CREATE", CYPHER_TOKEN_CREATE},
    {"DELETE", CYPHER_TOKEN_DELETE},
    {"SET", CYPHER_TOKEN_SET},
    {"REMOVE", CYPHER_TOKEN_REMOVE},
    {"WITH", CYPHER_TOKEN_WITH},
    {"OPTIONAL", CYPHER_TOKEN_OPTIONAL},
    {"UNWIND", CYPHER_TOKEN_UNWIND},
    {"AS", CYPHER_TOKEN_AS},
    {"DISTINCT", CYPHER_TOKEN_DISTINCT},
    {"ORDER", CYPHER_TOKEN_ORDER},
    {"BY", CYPHER_TOKEN_BY},
    {"SKIP", CYPHER_TOKEN_SKIP},
    {"LIMIT", CYPHER_TOKEN_LIMIT},
    {"AND", CYPHER_TOKEN_AND},
    {"OR", CYPHER_TOKEN_OR},
    {"NOT", CYPHER_TOKEN_NOT},
    {"XOR", CYPHER_TOKEN_XOR},
    {"NULL", CYPHER_TOKEN_NULL},
    {"TRUE", CYPHER_TOKEN_TRUE},
    {"FALSE", CYPHER_TOKEN_FALSE},
    {"count", CYPHER_TOKEN_AGG_FUNC},
    {"sum", CYPHER_TOKEN_AGG_FUNC},
    {"avg", CYPHER_TOKEN_AGG_FUNC},
    {"min", CYPHER_TOKEN_AGG_FUNC},
    {"max", CYPHER_TOKEN_AGG_FUNC},
    {"collect", CYPHER_TOKEN_AGG_FUNC},
    {"properties", CYPHER_TOKEN_FUNC_CALL},
    {"type", CYPHER_TOKEN_FUNC_CALL},
    {"labels", CYPHER_TOKEN_FUNC_CALL},
    {"id", CYPHER_TOKEN_FUNC_CALL},
    {"startNode", CYPHER_TOKEN_FUNC_CALL},
    {"endNode", CYPHER_TOKEN_FUNC_CALL},
    {"nodes", CYPHER_TOKEN_FUNC_CALL},
    {"relationships", CYPHER_TOKEN_FUNC_CALL},
    {"HEAD", CYPHER_TOKEN_FUNC_CALL},
    {"TAIL", CYPHER_TOKEN_FUNC_CALL},
    {"SIZE", CYPHER_TOKEN_FUNC_CALL},
    {"EXISTS", CYPHER_TOKEN_FUNC_CALL},
    {"COALESCE", CYPHER_TOKEN_FUNC_CALL},
    {NULL, 0}
};

/* ========================================================================
 * 词法分析器
 * ======================================================================== */

static CypherTokenType lookup_identifier(const char *start, size_t len)
{
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (strncmp(start, keywords[i].keyword, len) == 0 &&
            keywords[i].keyword[len] == '\0') {
            return keywords[i].token;
        }
    }
    return CYPHER_TOKEN_IDENTIFIER;
}

static void cypher_lexer_init(CypherParser *parser, const char *input, size_t len)
{
    parser->input = input;
    parser->input_len = len;
    parser->pos = 0;
    parser->line = 1;
    parser->column = 1;
    parser->had_error = false;
    parser->error_msg[0] = '\0';
}

static char cypher_peek(CypherParser *parser)
{
    if (parser->pos >= parser->input_len) return '\0';
    return parser->input[parser->pos];
}

static char cypher_peek_next(CypherParser *parser)
{
    if (parser->pos + 1 >= parser->input_len) return '\0';
    return parser->input[parser->pos + 1];
}

static char cypher_advance(CypherParser *parser)
{
    char c = parser->input[parser->pos++];
    if (c == '\n') {
        parser->line++;
        parser->column = 1;
    } else {
        parser->column++;
    }
    return c;
}

static bool cypher_is_at_end(CypherParser *parser)
{
    return parser->pos >= parser->input_len;
}

static CypherToken cypher_make_token(CypherParser *parser, CypherTokenType type,
                                     const char *start, size_t len)
{
    CypherToken token;
    token.type = type;
    token.line = parser->line;
    token.column = parser->column - len;
    token.lexeme = (char *)malloc(len + 1);
    memcpy(token.lexeme, start, len);
    token.lexeme[len] = '\0';
    token.raw_value = NULL;
    return token;
}

static void cypher_skip_whitespace(CypherParser *parser)
{
    while (!cypher_is_at_end(parser)) {
        char c = cypher_peek(parser);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            cypher_advance(parser);
        } else if (c == '/') {
            if (cypher_peek_next(parser) == '/') {
                while (!cypher_is_at_end(parser) && cypher_peek(parser) != '\n') {
                    cypher_advance(parser);
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static CypherToken cypher_scan_token(CypherParser *parser)
{
    cypher_skip_whitespace(parser);

    if (cypher_is_at_end(parser)) {
        return cypher_make_token(parser, CYPHER_TOKEN_EOF,
                                 parser->input + parser->pos, 0);
    }

    const char *start = parser->input + parser->pos;
    char c = cypher_advance(parser);

    /* 标识符或关键字 */
    if (isalpha(c) || c == '_') {
        size_t len = 1;
        while (!cypher_is_at_end(parser) &&
               (isalnum(cypher_peek(parser)) || cypher_peek(parser) == '_')) {
            cypher_advance(parser);
            len++;
        }
        return cypher_make_token(parser, lookup_identifier(start, len), start, len);
    }

    /* 数字 */
    if (isdigit(c) || (c == '-' && isdigit(cypher_peek(parser)))) {
        size_t len = 1;
        bool is_float = false;

        if (c == '-') {
            c = cypher_advance(parser);
        }

        while (!cypher_is_at_end(parser) && isdigit(cypher_peek(parser))) {
            cypher_advance(parser);
            len++;
        }

        if (cypher_peek(parser) == '.' && isdigit(cypher_peek_next(parser))) {
            is_float = true;
            cypher_advance(parser);
            len++;
            while (!cypher_is_at_end(parser) && isdigit(cypher_peek(parser))) {
                cypher_advance(parser);
                len++;
            }
        }

        return cypher_make_token(parser,
                                is_float ? CYPHER_TOKEN_FLOAT : CYPHER_TOKEN_INTEGER,
                                start, len);
    }

    /* 字符串 */
    if (c == '\'' || c == '"') {
        char quote = c;
        size_t len = 0;
        while (!cypher_is_at_end(parser) && cypher_peek(parser) != quote) {
            if (cypher_peek(parser) == '\\' && cypher_peek_next(parser) == quote) {
                cypher_advance(parser);
            }
            cypher_advance(parser);
            len++;
        }

        if (cypher_is_at_end(parser)) {
            return cypher_make_token(parser, CYPHER_TOKEN_ERROR, start, len + 1);
        }

        cypher_advance(parser); /* 吃掉结束引号 */
        return cypher_make_token(parser, CYPHER_TOKEN_STRING, start + 1, len);
    }

    /* 操作符 */
    switch (c) {
        case '(': return cypher_make_token(parser, CYPHER_TOKEN_LPAREN, start, 1);
        case ')': return cypher_make_token(parser, CYPHER_TOKEN_RPAREN, start, 1);
        case '[': return cypher_make_token(parser, CYPHER_TOKEN_LBRACKET, start, 1);
        case ']': return cypher_make_token(parser, CYPHER_TOKEN_RBRACKET, start, 1);
        case '{': return cypher_make_token(parser, CYPHER_TOKEN_LBRACE, start, 1);
        case '}': return cypher_make_token(parser, CYPHER_TOKEN_RBRACE, start, 1);
        case ',': return cypher_make_token(parser, CYPHER_TOKEN_COMMA, start, 1);
        case ';': return cypher_make_token(parser, CYPHER_TOKEN_SEMICOLON, start, 1);
        case ':': return cypher_make_token(parser, CYPHER_TOKEN_COLON, start, 1);
        case '|': return cypher_make_token(parser, CYPHER_TOKEN_PIPE, start, 1);
        case '@': return cypher_make_token(parser, CYPHER_TOKEN_ATSIGN, start, 1);
        case '.': return cypher_make_token(parser, CYPHER_TOKEN_DOT, start, 1);
        case '*': return cypher_make_token(parser, CYPHER_TOKEN_MULTIPLY, start, 1);
        case '+': return cypher_make_token(parser, CYPHER_TOKEN_PLUS, start, 1);
        case '/': return cypher_make_token(parser, CYPHER_TOKEN_DIVIDE, start, 1);
        case '%': return cypher_make_token(parser, CYPHER_TOKEN_MOD, start, 1);
        case '^': return cypher_make_token(parser, CYPHER_TOKEN_XOR, start, 1);
        case '=': {
            if (cypher_peek(parser) == '=') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_EQUAL, start, 2);
            }
            return cypher_make_token(parser, CYPHER_TOKEN_EQUAL, start, 1);
        }
        case '!': {
            if (cypher_peek(parser) == '=') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_NOT_EQUAL, start, 2);
            }
            return cypher_make_token(parser, CYPHER_TOKEN_NOT, start, 1);
        }
        case '<': {
            if (cypher_peek(parser) == '=') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_LESS_EQUAL, start, 2);
            }
            if (cypher_peek(parser) == '-') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_ARROW, start, 2);
            }
            return cypher_make_token(parser, CYPHER_TOKEN_LESS, start, 1);
        }
        case '>': {
            if (cypher_peek(parser) == '=') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_GREATER_EQUAL, start, 2);
            }
            return cypher_make_token(parser, CYPHER_TOKEN_GREATER, start, 1);
        }
        case '-': {
            if (cypher_peek(parser) == '>') {
                cypher_advance(parser);
                return cypher_make_token(parser, CYPHER_TOKEN_ARROW, start, 2);
            }
            return cypher_make_token(parser, CYPHER_TOKEN_MINUS, start, 1);
        }
        case '$': {
            size_t len = 1;
            while (!cypher_is_at_end(parser) &&
                   (isalnum(cypher_peek(parser)) || cypher_peek(parser) == '_')) {
                cypher_advance(parser);
                len++;
            }
            return cypher_make_token(parser, CYPHER_TOKEN_PARAMETER, start, len);
        }
        default:
            return cypher_make_token(parser, CYPHER_TOKEN_ERROR, start, 1);
    }
}

static void cypher_advance_token(CypherParser *parser)
{
    parser->previous_token = parser->current_token;
    parser->current_token = cypher_scan_token(parser);
}

static bool cypher_check(CypherParser *parser, CypherTokenType type)
{
    return parser->current_token.type == type;
}

static bool cypher_match(CypherParser *parser, CypherTokenType type)
{
    if (cypher_check(parser, type)) {
        cypher_advance_token(parser);
        return true;
    }
    return false;
}

static void cypher_error_at_current(CypherParser *parser, const char *message)
{
    parser->had_error = true;
    snprintf(parser->error_msg, sizeof(parser->error_msg),
             "Line %d, Col %d: %s",
             parser->current_token.line,
             parser->current_token.column,
             message);
}

static void cypher_error_at(CypherParser *parser, const CypherToken *token,
                            const char *message)
{
    parser->had_error = true;
    snprintf(parser->error_msg, sizeof(parser->error_msg),
             "Line %d, Col %d: %s",
             token->line, token->column, message);
}

/* ========================================================================
 * 解析器实现
 * ======================================================================== */

static CypherNodePattern *cypher_parse_node_pattern(CypherParser *parser);
static CypherRelPattern *cypher_parse_rel_pattern(CypherParser *parser);
static CypherPatternChain *cypher_parse_pattern_chain(CypherParser *parser);
static CypherASTNode *cypher_parse_expression(CypherParser *parser);
static CypherReturnItem *cypher_parse_return_item(CypherParser *parser);
static CypherReturn *cypher_parse_return(CypherParser *parser);
static CypherWhere *cypher_parse_where(CypherParser *parser);
static CypherCreate *cypher_parse_create(CypherParser *parser);

static CypherASTNode *cypher_alloc_node(CypherASTType type)
{
    CypherASTNode *node = (CypherASTNode *)calloc(1, sizeof(CypherASTNode));
    node->type = type;
    return node;
}

static char *cypher_copy_string(const char *s)
{
    if (!s) return NULL;
    return strdup(s);
}

/* 解析属性 */
static CypherProp *cypher_parse_props(CypherParser *parser)
{
    CypherProp *props = NULL;
    CypherProp *last = NULL;

    if (cypher_match(parser, CYPHER_TOKEN_LBRACE)) {
        if (!cypher_check(parser, CYPHER_TOKEN_RBRACE)) {
            do {
                if (!cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
                    cypher_error_at_current(parser, "期望属性名");
                    return props;
                }

                CypherProp *prop = (CypherProp *)malloc(sizeof(CypherProp));
                prop->name = cypher_copy_string(parser->current_token.lexeme);
                prop->next = NULL;
                cypher_advance_token(parser);

                if (!cypher_match(parser, CYPHER_TOKEN_COLON)) {
                    cypher_error_at_current(parser, "期望 ':'");
                    free(prop->name);
                    free(prop);
                    return props;
                }

                prop->value = cypher_parse_expression(parser);

                if (!last) {
                    props = last = prop;
                } else {
                    last->next = prop;
                    last = prop;
                }
            } while (cypher_match(parser, CYPHER_TOKEN_COMMA));
        }

        if (!cypher_match(parser, CYPHER_TOKEN_RBRACE)) {
            cypher_error_at_current(parser, "期望 '}'");
        }
    }

    return props;
}

/* 解析标签 */
static char **cypher_parse_labels(CypherParser *parser, size_t *num_labels)
{
    char **labels = NULL;
    *num_labels = 0;

    while (cypher_match(parser, CYPHER_TOKEN_COLON)) {
        if (!cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
            cypher_error_at_current(parser, "期望标签名");
            break;
        }

        labels = (char **)realloc(labels, sizeof(char *) * (*num_labels + 1));
        labels[*num_labels] = cypher_copy_string(parser->current_token.lexeme);
        (*num_labels)++;
        cypher_advance_token(parser);
    }

    return labels;
}

/* 解析节点模式: (variable:Label {props}) */
static CypherNodePattern *cypher_parse_node_pattern(CypherParser *parser)
{
    CypherNodePattern *node = (CypherNodePattern *)calloc(1, sizeof(CypherNodePattern));

    if (cypher_match(parser, CYPHER_TOKEN_LPAREN)) {
        /* 检查是否有变量名 */
        if (cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
            /* 可能是变量名，后面跟着 : 或 { */
            const char *token_start = parser->input + parser->pos;

            /* 向前看：检查下一个非空白字符 */
            size_t saved_pos = parser->pos;
            int saved_line = parser->line;
            int saved_col = parser->column;

            cypher_advance_token(parser);
            bool has_colon_or_brace = cypher_check(parser, CYPHER_TOKEN_COLON) ||
                                      cypher_check(parser, CYPHER_TOKEN_LBRACE);

            if (!has_colon_or_brace) {
                node->variable = cypher_copy_string(parser->previous_token.lexeme);
            } else {
                /* 回退并只解析标签 */
                parser->pos = saved_pos;
                parser->line = saved_line;
                parser->column = saved_col;
            }
        }

        /* 解析标签 */
        node->labels = cypher_parse_labels(parser, &node->num_labels);

        /* 解析属性 */
        node->props = cypher_parse_props(parser);

        if (!cypher_match(parser, CYPHER_TOKEN_RPAREN)) {
            cypher_error_at_current(parser, "期望 ')'");
        }
    } else {
        cypher_error_at_current(parser, "期望 '('");
    }

    return node;
}

/* 解析变量长度模式: *1..3 或 *1.. 或 *..3 或 * */
static CypherVarLen *cypher_parse_var_len(CypherParser *parser)
{
    CypherVarLen *vl = NULL;

    if (cypher_match(parser, CYPHER_TOKEN_MULTIPLY)) {
        vl = (CypherVarLen *)calloc(1, sizeof(CypherVarLen));
        vl->min = 1;
        vl->max = -1;

        if (cypher_check(parser, CYPHER_TOKEN_INTEGER)) {
            vl->min = atoi(parser->current_token.lexeme);
            cypher_advance_token(parser);

            if (cypher_match(parser, CYPHER_TOKEN_DOT)) {
                if (cypher_match(parser, CYPHER_TOKEN_DOT)) {
                    if (cypher_check(parser, CYPHER_TOKEN_INTEGER)) {
                        vl->max = atoi(parser->current_token.lexeme);
                        cypher_advance_token(parser);
                    } else {
                        vl->max = -1; /* .. 表示无上限 */
                    }
                } else {
                    vl->max = vl->min; /* . 表示精确跳数 */
                    vl->min = vl->max;
                }
            }
        } else if (cypher_match(parser, CYPHER_TOKEN_DOT)) {
            if (cypher_match(parser, CYPHER_TOKEN_DOT)) {
                vl->min = 1;
                if (cypher_check(parser, CYPHER_TOKEN_INTEGER)) {
                    vl->max = atoi(parser->current_token.lexeme);
                    cypher_advance_token(parser);
                } else {
                    vl->max = -1;
                }
            }
        }
    }

    return vl;
}

/* 解析关系模式: -[:TYPE *1..3 {props}]-> */
static CypherRelPattern *cypher_parse_rel_pattern(CypherParser *parser)
{
    CypherRelPattern *rel = (CypherRelPattern *)calloc(1, sizeof(CypherRelPattern));
    rel->dir = CYPHER_REL_UNDIRECTED;

    /* 处理方向 */
    bool left_arrow = false;
    bool right_arrow = false;

    if (cypher_match(parser, CYPHER_TOKEN_LESS)) {
        if (cypher_match(parser, CYPHER_TOKEN_MINUS)) {
            left_arrow = true;
        }
    }

    if (cypher_match(parser, CYPHER_TOKEN_MINUS)) {
        right_arrow = true;
    }

    if (left_arrow && right_arrow) {
        rel->dir = CYPHER_REL_UNDIRECTED;
    } else if (left_arrow) {
        rel->dir = CYPHER_REL_INCOMING;
    } else if (right_arrow) {
        rel->dir = CYPHER_REL_OUTGOING;
    }

    /* 检查是否有关系模式（不是简单的 --> */
    if (!cypher_check(parser, CYPHER_TOKEN_GREATER) && !cypher_check(parser, CYPHER_TOKEN_RPAREN)) {
        /* 可能是变量 */
        if (cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
            const char *token_start = parser->input + parser->pos;
            size_t saved_pos = parser->pos;

            cypher_advance_token(parser);
            bool has_content = cypher_check(parser, CYPHER_TOKEN_COLON) ||
                              cypher_check(parser, CYPHER_TOKEN_LBRACKET) ||
                              cypher_check(parser, CYPHER_TOKEN_LBRACE);

            if (!has_content) {
                rel->variable = cypher_copy_string(parser->previous_token.lexeme);
            } else {
                parser->pos = saved_pos;
            }
        }

        /* 解析关系类型 */
        if (cypher_match(parser, CYPHER_TOKEN_COLON)) {
            if (cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
                rel->num_rel_types = 1;
                rel->rel_types = (char **)malloc(sizeof(char *));
                rel->rel_types[0] = cypher_copy_string(parser->current_token.lexeme);
                cypher_advance_token(parser);
            }
        }

        /* 解析变量长度 */
        rel->var_len = cypher_parse_var_len(parser);

        /* 解析属性 */
        rel->props = cypher_parse_props(parser);
    }

    /* 跳过右箭头 */
    if (cypher_match(parser, CYPHER_TOKEN_MINUS)) {
        if (!cypher_match(parser, CYPHER_TOKEN_GREATER)) {
            if (!cypher_check(parser, CYPHER_TOKEN_GREATER)) {
                cypher_error_at_current(parser, "期望 '>'");
            }
        }
    }

    return rel;
}

/* 解析模式链 */
static CypherPatternChain *cypher_parse_pattern_chain(CypherParser *parser)
{
    CypherPatternChain *chain = (CypherPatternChain *)calloc(1, sizeof(CypherPatternChain));

    chain->node = cypher_parse_node_pattern(parser);

    if (cypher_check(parser, CYPHER_TOKEN_ARROW) ||
        cypher_check(parser, CYPHER_TOKEN_MINUS) ||
        cypher_check(parser, CYPHER_TOKEN_LESS)) {
        chain->rel = cypher_parse_rel_pattern(parser);
    }

    if (cypher_check(parser, CYPHER_TOKEN_LPAREN) ||
        cypher_check(parser, CYPHER_TOKEN_PIPE)) {
        chain->next = cypher_parse_pattern_chain(parser);
    }

    return chain;
}

/* 解析表达式 */
static CypherASTNode *cypher_parse_expression(CypherParser *parser)
{
    (void)parser;
    /* 简化实现：解析标识符或字面量 */
    CypherASTNode *node = NULL;

    if (cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
        node = cypher_alloc_node(CYPHER_AST_IDENTIFIER);
        node->u.identifier = cypher_copy_string(parser->current_token.lexeme);
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_STRING)) {
        node = cypher_alloc_node(CYPHER_AST_LITERAL);
        node->u.literal.literal_type = CYPHER_TOKEN_STRING;
        node->u.literal.value = cypher_copy_string(parser->current_token.lexeme);
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_INTEGER)) {
        node = cypher_alloc_node(CYPHER_AST_LITERAL);
        node->u.literal.literal_type = CYPHER_TOKEN_INTEGER;
        node->u.literal.value = cypher_copy_string(parser->current_token.lexeme);
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_FLOAT)) {
        node = cypher_alloc_node(CYPHER_AST_LITERAL);
        node->u.literal.literal_type = CYPHER_TOKEN_FLOAT;
        node->u.literal.value = cypher_copy_string(parser->current_token.lexeme);
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_TRUE) ||
               cypher_check(parser, CYPHER_TOKEN_FALSE)) {
        node = cypher_alloc_node(CYPHER_AST_LITERAL);
        node->u.literal.literal_type = parser->current_token.type;
        node->u.literal.value = NULL;
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_NULL)) {
        node = cypher_alloc_node(CYPHER_AST_LITERAL);
        node->u.literal.literal_type = CYPHER_TOKEN_NULL;
        node->u.literal.value = NULL;
        cypher_advance_token(parser);
    } else if (cypher_check(parser, CYPHER_TOKEN_PARAMETER)) {
        node = cypher_alloc_node(CYPHER_AST_PARAMETER);
        node->u.parameter = cypher_copy_string(parser->current_token.lexeme + 1);
        cypher_advance_token(parser);
    }

    return node;
}

/* 解析 RETURN 项 */
static CypherReturnItem *cypher_parse_return_item(CypherParser *parser)
{
    CypherReturnItem *item = (CypherReturnItem *)calloc(1, sizeof(CypherReturnItem));

    item->expr = cypher_parse_expression(parser);

    if (cypher_match(parser, CYPHER_TOKEN_AS)) {
        if (cypher_check(parser, CYPHER_TOKEN_IDENTIFIER)) {
            item->alias = cypher_copy_string(parser->current_token.lexeme);
            cypher_advance_token(parser);
        }
    }

    return item;
}

/* 解析 RETURN 子句 */
static CypherReturn *cypher_parse_return(CypherParser *parser)
{
    CypherReturn *ret = (CypherReturn *)calloc(1, sizeof(CypherReturn));
    ret->distinct = cypher_match(parser, CYPHER_TOKEN_DISTINCT);

    if (!cypher_check(parser, CYPHER_TOKEN_ORDER) &&
        !cypher_check(parser, CYPHER_TOKEN_SKIP) &&
        !cypher_check(parser, CYPHER_TOKEN_LIMIT) &&
        !cypher_check(parser, CYPHER_TOKEN_EOF) &&
        !cypher_check(parser, CYPHER_TOKEN_SEMICOLON)) {

        do {
            ret->items = (CypherReturnItem **)realloc(ret->items,
                sizeof(CypherReturnItem *) * (ret->num_items + 1));
            ret->items[ret->num_items] = cypher_parse_return_item(parser);
            ret->num_items++;
        } while (cypher_match(parser, CYPHER_TOKEN_COMMA));
    }

    return ret;
}

/* 解析 WHERE 子句 */
static CypherWhere *cypher_parse_where(CypherParser *parser)
{
    CypherWhere *where = (CypherWhere *)calloc(1, sizeof(CypherWhere));
    where->expr = cypher_parse_expression(parser);
    return where;
}

/* 解析 CREATE 子句 */
static CypherCreate *cypher_parse_create(CypherParser *parser)
{
    CypherCreate *create = (CypherCreate *)calloc(1, sizeof(CypherCreate));

    do {
        create->patterns = (CypherPatternChain **)realloc(create->patterns,
            sizeof(CypherPatternChain *) * (create->num_patterns + 1));
        create->patterns[create->num_patterns] = cypher_parse_pattern_chain(parser);
        create->num_patterns++;
    } while (cypher_check(parser, CYPHER_TOKEN_LPAREN));

    return create;
}

/* 解析单查询 */
static CypherSingleQuery *cypher_parse_single_query(CypherParser *parser)
{
    CypherSingleQuery *query = (CypherSingleQuery *)calloc(1, sizeof(CypherSingleQuery));

    /* 解析 OPTIONAL MATCH 或 MATCH */
    if (cypher_match(parser, CYPHER_TOKEN_OPTIONAL)) {
        if (!cypher_match(parser, CYPHER_TOKEN_MATCH)) {
            cypher_error_at_current(parser, "期望 MATCH");
        }
    }

    if (cypher_match(parser, CYPHER_TOKEN_MATCH)) {
        do {
            query->patterns = (CypherPatternChain **)realloc(query->patterns,
                sizeof(CypherPatternChain *) * (query->num_patterns + 1));
            query->patterns[query->num_patterns] = cypher_parse_pattern_chain(parser);
            query->num_patterns++;
        } while (cypher_check(parser, CYPHER_TOKEN_LPAREN));

        /* 可选的 WHERE */
        if (cypher_match(parser, CYPHER_TOKEN_WHERE)) {
            query->where = cypher_parse_where(parser);
        }
    }

    /* 解析 WITH */
    if (cypher_match(parser, CYPHER_TOKEN_WITH)) {
        query->with = cypher_parse_expression(parser);
    }

    /* 解析 CREATE */
    if (cypher_match(parser, CYPHER_TOKEN_CREATE)) {
        query->create = cypher_parse_create(parser);
    }

    /* 解析 SET */
    if (cypher_match(parser, CYPHER_TOKEN_SET)) {
        /* SET 实现 */
    }

    /* 解析 DELETE */
    if (cypher_match(parser, CYPHER_TOKEN_DELETE)) {
        /* DELETE 实现 */
    }

    /* 解析 RETURN */
    if (cypher_match(parser, CYPHER_TOKEN_RETURN)) {
        query->ret = cypher_parse_return(parser);
    }

    return query;
}

/* 解析查询 */
static CypherQuery *cypher_parse_query_internal(CypherParser *parser)
{
    CypherQuery *query = (CypherQuery *)calloc(1, sizeof(CypherQuery));

    cypher_advance_token(parser);

    query->query = cypher_parse_single_query(parser);

    return query;
}

/* ========================================================================
 * 公共 API
 * ======================================================================== */

CypherParser *cypher_parser_create(const char *query, size_t len)
{
    CypherParser *parser = (CypherParser *)calloc(1, sizeof(CypherParser));
    cypher_lexer_init(parser, query, len);
    return parser;
}

void cypher_parser_destroy(CypherParser *parser)
{
    if (parser) {
        free(parser);
    }
}

CypherQuery *cypher_parse(const char *query)
{
    return cypher_parse_query(query, NULL);
}

CypherQuery *cypher_parse_query(const char *query, char *out_error)
{
    CypherParser parser;
    cypher_lexer_init(&parser, query, strlen(query));

    CypherQuery *result = cypher_parse_query_internal(&parser);

    if (parser.had_error) {
        if (out_error) {
            strncpy(out_error, parser.error_msg, 511);
            out_error[511] = '\0';
        }
        cypher_query_free(result);
        return NULL;
    }

    return result;
}

const char *cypher_get_error(const CypherParser *parser)
{
    return parser->error_msg;
}

void cypher_prop_free(CypherProp *prop)
{
    if (!prop) return;

    CypherProp *next;
    while (prop) {
        next = prop->next;
        free(prop->name);
        if (prop->value) cypher_ast_free(prop->value);
        free(prop);
        prop = next;
    }
}

void cypher_node_pattern_free(CypherNodePattern *node)
{
    if (!node) return;

    free(node->variable);
    for (size_t i = 0; i < node->num_labels; i++) {
        free(node->labels[i]);
    }
    free(node->labels);
    cypher_prop_free(node->props);
    free(node);
}

void cypher_rel_pattern_free(CypherRelPattern *rel)
{
    if (!rel) return;

    free(rel->variable);
    for (size_t i = 0; i < rel->num_rel_types; i++) {
        free(rel->rel_types[i]);
    }
    free(rel->rel_types);
    cypher_prop_free(rel->props);
    free(rel->var_len);
    free(rel);
}

void cypher_pattern_chain_free(CypherPatternChain *chain)
{
    if (!chain) return;

    cypher_node_pattern_free(chain->node);
    cypher_rel_pattern_free(chain->rel);
    if (chain->next) {
        cypher_pattern_chain_free(chain->next);
    }
    free(chain);
}

void cypher_ast_free(CypherASTNode *node)
{
    if (!node) return;

    switch (node->type) {
        case CYPHER_AST_QUERY:
            cypher_query_free(&node->u.query);
            break;
        case CYPHER_AST_SINGLE_QUERY:
            {
                CypherSingleQuery *q = &node->u.single_query;
                for (size_t i = 0; i < q->num_patterns; i++) {
                    cypher_pattern_chain_free(q->patterns[i]);
                }
                free(q->patterns);
                if (q->where) { free(q->where); }
                if (q->ret) { free(q->ret); }
            }
            break;
        case CYPHER_AST_NODE_PATTERN:
            cypher_node_pattern_free(&node->u.node_pattern);
            break;
        case CYPHER_AST_REL_PATTERN:
            cypher_rel_pattern_free(&node->u.rel_pattern);
            break;
        case CYPHER_AST_IDENTIFIER:
            free(node->u.identifier);
            break;
        case CYPHER_AST_LITERAL:
            free(node->u.literal.value);
            break;
        case CYPHER_AST_PARAMETER:
            free(node->u.parameter);
            break;
        case CYPHER_AST_BINARY_OP:
            cypher_ast_free(node->u.binary_op.left);
            cypher_ast_free(node->u.binary_op.right);
            break;
        case CYPHER_AST_UNARY_OP:
            cypher_ast_free(node->u.unary_op.operand);
            break;
        case CYPHER_AST_FUNC_CALL:
            free(node->u.func_call.name);
            for (size_t i = 0; i < node->u.func_call.num_args; i++) {
                cypher_ast_free(node->u.func_call.args[i]);
            }
            free(node->u.func_call.args);
            break;
        default:
            break;
    }

    free(node);
}

void cypher_query_free(CypherQuery *query)
{
    if (!query) return;

    if (query->query) {
        CypherSingleQuery *sq = query->query;
        for (size_t i = 0; i < sq->num_patterns; i++) {
            cypher_pattern_chain_free(sq->patterns[i]);
        }
        free(sq->patterns);
        if (sq->where) free(sq->where);
        if (sq->ret) free(sq->ret);
        if (sq->create) free(sq->create);
        free(sq);
    }
    free(query);
}

void cypher_result_free(CypherResult *result)
{
    if (!result) return;

    for (size_t i = 0; i < result->num_columns; i++) {
        free(result->column_names[i]);
    }
    free(result->column_names);

    for (size_t i = 0; i < result->num_rows; i++) {
        void **row = (void **)result->rows[i];
        for (size_t j = 0; j < result->num_columns; j++) {
            free(row[j]);
        }
        free(row);
    }
    free(result->rows);

    free(result);
}

CypherResult *cypher_execute(void *graph, const char *query, void *params)
{
    (void)graph; (void)query; (void)params;

    CypherResult *result = (CypherResult *)calloc(1, sizeof(CypherResult));
    result->num_columns = 0;
    result->num_rows = 0;
    result->rows = NULL;

    return result;
}

void cypher_ast_print(const CypherASTNode *node, int indent)
{
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");

    switch (node->type) {
        case CYPHER_AST_QUERY:
            printf("Query\n");
            if (node->u.query.query) {
                cypher_ast_print((CypherASTNode *)node->u.query.query, indent + 1);
            }
            break;
        case CYPHER_AST_SINGLE_QUERY:
            printf("SingleQuery\n");
            break;
        default:
            printf("AST(%d)\n", node->type);
    }
}

char *cypher_ast_to_string(const CypherASTNode *node)
{
    (void)node;
    return strdup("Cypher AST");
}
