/**
 * @file gql_parser.c
 * @brief GQL 语法分析器
 *
 * 实现递归下降语法分析器
 */
#include "db/parser/graph/gql.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 解析器结构
 * ============================================================ */

struct gql_parser_s {
    const char  *input;          /**< 输入字符串 */
    size_t      len;             /**< 输入长度 */
    size_t      pos;             /**< 当前读取位置 */
    size_t      line;            /**< 当前行号 */
    size_t      column;          /**< 当前列号 */
    gql_token_t *current;        /**< 当前 token */
    gql_token_t *next_tok;       /**< 下一个 token */
    char        error_msg[256];  /**< 错误信息 */
    size_t      error_line;      /**< 错误行号 */
    size_t      error_column;    /**< 错误列号 */
};

/* ============================================================
 * 前向声明
 * ============================================================ */

gql_token_t *gql_lexer_next(gql_parser_t *p);

static gql_node_t *parse_statement(gql_parser_t *p);
static gql_node_t *parse_create(gql_parser_t *p);
static gql_node_t *parse_match(gql_parser_t *p);
static gql_node_t *parse_delete(gql_parser_t *p);
static gql_node_t *parse_pattern(gql_parser_t *p);
static gql_node_t *parse_node_pattern(gql_parser_t *p);
static gql_node_t *parse_rel_pattern(gql_parser_t *p);
static gql_node_t *parse_return(gql_parser_t *p);
static gql_node_t *parse_expression(gql_parser_t *p);
static gql_node_t *parse_or_expr(gql_parser_t *p);
static gql_node_t *parse_and_expr(gql_parser_t *p);
static gql_node_t *parse_not_expr(gql_parser_t *p);
static gql_node_t *parse_comparison(gql_parser_t *p);
static gql_node_t *parse_additive(gql_parser_t *p);
static gql_node_t *parse_multiplicative(gql_parser_t *p);
static gql_node_t *parse_primary(gql_parser_t *p);
static gql_node_t *parse_property_map(gql_parser_t *p);

/* ============================================================
 * 工具函数
 * ============================================================ */

static void set_error(gql_parser_t *p, const char *msg) {
    snprintf(p->error_msg, sizeof(p->error_msg), "%s", msg);
    p->error_line = p->line;
    p->error_column = p->column;
}

static bool is_eof(gql_parser_t *p) {
    return !p->current || p->current->type == GQL_TOKEN_EOF;
}

static gql_token_type_t tok_type(gql_parser_t *p) {
    return p->current ? p->current->type : GQL_TOKEN_EOF;
}

static char *tok_lexeme(gql_parser_t *p) {
    return p->current ? p->current->lexeme : NULL;
}

static void advance(gql_parser_t *p) {
    if (p->current) {
        free(p->current->lexeme);
        free(p->current);
    }
    p->current = p->next_tok;
    p->next_tok = gql_lexer_next(p);
}

static bool check(gql_parser_t *p, gql_token_type_t type) {
    return tok_type(p) == type;
}

static bool match(gql_parser_t *p, gql_token_type_t type) {
    if (check(p, type)) {
        advance(p);
        return true;
    }
    return false;
}

static gql_node_t *alloc_node(gql_parser_t *p, gql_node_type_t type) {
    gql_node_t *node = calloc(1, sizeof(gql_node_t));
    if (!node) {
        set_error(p, "Out of memory");
        return NULL;
    }
    node->type = type;
    return node;
}

/* ============================================================
 * 解析器创建/销毁
 * ============================================================ */

gql_parser_t *gql_parser_create(const char *input) {
    gql_parser_t *p = calloc(1, sizeof(gql_parser_t));
    if (!p) return NULL;

    p->input = input;
    p->len = input ? strlen(input) : 0;
    p->pos = 0;
    p->line = 1;
    p->column = 0;

    p->current = gql_lexer_next(p);
    p->next_tok = gql_lexer_next(p);

    return p;
}

void gql_parser_destroy(gql_parser_t *parser) {
    if (!parser) return;
    if (parser->current) {
        free(parser->current->lexeme);
        free(parser->current);
    }
    if (parser->next_tok) {
        free(parser->next_tok->lexeme);
        free(parser->next_tok);
    }
    free(parser);
}

const char *gql_parser_errmsg(const gql_parser_t *parser) {
    return parser ? parser->error_msg : "";
}

void gql_parser_error_pos(const gql_parser_t *parser, size_t *out_line, size_t *out_col) {
    if (!parser) return;
    if (out_line) *out_line = parser->error_line;
    if (out_col) *out_col = parser->error_column;
}

/* ============================================================
 * 解析入口
 * ============================================================ */

gql_node_t *gql_parse_one(gql_parser_t *p) {
    if (!p || is_eof(p)) return NULL;
    return parse_statement(p);
}

gql_node_t **gql_parse(gql_parser_t *p, size_t *out_count) {
    if (!p || !out_count) return NULL;

    *out_count = 0;
    gql_node_t **nodes = NULL;

    while (!is_eof(p)) {
        gql_node_t *node = parse_statement(p);
        if (!node) break;

        nodes = realloc(nodes, sizeof(gql_node_t*) * (*out_count + 1));
        nodes[*out_count++] = node;

        /* 消费分号 */
        match(p, GQL_TOKEN_SEMICOLON);
    }

    return nodes;
}

/* ============================================================
 * 语句解析
 * ============================================================ */

static gql_node_t *parse_statement(gql_parser_t *p) {
    if (!p) return NULL;

    gql_token_type_t type = tok_type(p);

    if (type == GQL_TOKEN_CREATE) {
        return parse_create(p);
    } else if (type == GQL_TOKEN_MATCH) {
        return parse_match(p);
    } else if (type == GQL_TOKEN_DELETE) {
        return parse_delete(p);
    }

    set_error(p, "Expected CREATE, MATCH, or DELETE");
    return NULL;
}

/* ============================================================
 * CREATE 语句解析
 * ============================================================ */

static gql_node_t *parse_create(gql_parser_t *p) {
    if (!match(p, GQL_TOKEN_CREATE)) return NULL;

    gql_node_t *node = alloc_node(p, GQL_NODE_CREATE);
    if (!node) return NULL;

    node->u.create.patterns = parse_pattern(p);

    return node;
}

/* ============================================================
 * MATCH 语句解析
 * ============================================================ */

static gql_node_t *parse_match(gql_parser_t *p) {
    if (!match(p, GQL_TOKEN_MATCH)) return NULL;

    gql_node_t *node = alloc_node(p, GQL_NODE_QUERY);
    if (!node) return NULL;

    /* 解析模式 */
    node->u.query.patterns = parse_pattern(p);

    /* 解析 WHERE */
    if (match(p, GQL_TOKEN_WHERE)) {
        node->u.query.where = parse_expression(p);
    }

    /* 解析 RETURN */
    if (check(p, GQL_TOKEN_RETURN)) {
        node->u.query.ret = parse_return(p);
    }

    return node;
}

/* ============================================================
 * DELETE 语句解析
 * ============================================================ */

static gql_node_t *parse_delete(gql_parser_t *p) {
    if (!match(p, GQL_TOKEN_DELETE)) return NULL;

    gql_node_t *node = alloc_node(p, GQL_NODE_DELETE);
    if (!node) return NULL;

    node->u.del.detach_delete = false;  /* TODO: 支持 DETACH DELETE */

    /* 解析删除目标 */
    node->u.del.targets = parse_expression(p);

    return node;
}

/* ============================================================
 * 图模式解析
 * ============================================================ */

static gql_node_t *parse_pattern(gql_parser_t *p) {
    gql_node_t *pattern = alloc_node(p, GQL_NODE_PATTERN_LIST);
    if (!pattern) return NULL;

    gql_node_t **patterns = NULL;
    size_t count = 0;

    /* 解析第一个节点模式 */
    gql_node_t *first = parse_node_pattern(p);
    if (!first) {
        gql_node_free(pattern);
        return NULL;
    }

    patterns = realloc(patterns, sizeof(gql_node_t*) * (count + 1));
    patterns[count++] = first;

    /* 解析后续的 (节点)-(关系)-(节点) 模式 */
    while (check(p, GQL_TOKEN_LPAREN) || check(p, GQL_TOKEN_LBRACKET)) {
        /* 可选：关系模式 */
        if (check(p, GQL_TOKEN_LBRACKET)) {
            gql_node_t *rel = parse_rel_pattern(p);
            if (rel) {
                patterns = realloc(patterns, sizeof(gql_node_t*) * (count + 1));
                patterns[count++] = rel;
            }
        }

        /* 节点模式 */
        gql_node_t *node = parse_node_pattern(p);
        if (!node) break;

        patterns = realloc(patterns, sizeof(gql_node_t*) * (count + 1));
        patterns[count++] = node;
    }

    pattern->u.pattern_list.patterns = patterns;
    pattern->u.pattern_list.count = count;

    return pattern;
}

static gql_node_t *parse_node_pattern(gql_parser_t *p) {
    gql_node_t *node = alloc_node(p, GQL_NODE_NODE_PATTERN);
    if (!node) return NULL;

    /* 消费 '(' */
    if (!match(p, GQL_TOKEN_LPAREN)) {
        gql_node_free(node);
        return NULL;
    }

    node->u.node_pattern.anonymous = true;

    /* 检查变量或标签 */
    if (check(p, GQL_TOKEN_IDENT)) {
        node->u.node_pattern.anonymous = false;
        node->u.node_pattern.variable = strdup(tok_lexeme(p));
        advance(p);
    }

    /* 检查标签 */
    if (match(p, GQL_TOKEN_COLON)) {
        if (check(p, GQL_TOKEN_IDENT)) {
            node->u.node_pattern.label = strdup(tok_lexeme(p));
            advance(p);
        }
    }

    /* 检查属性 */
    if (check(p, GQL_TOKEN_LBRACE)) {
        advance(p);  /* 消费 '{' */
        node->u.node_pattern.props = parse_property_map(p);
        if (!match(p, GQL_TOKEN_RBRACE)) {
            set_error(p, "Expected '}'");
        }
    }

    /* 消费 ')' */
    if (!match(p, GQL_TOKEN_RPAREN)) {
        set_error(p, "Expected ')'");
    }

    return node;
}

static gql_node_t *parse_rel_pattern(gql_parser_t *p) {
    gql_node_t *rel = alloc_node(p, GQL_NODE_REL_PATTERN);
    if (!rel) return NULL;

    /* 消费 '[' */
    if (!match(p, GQL_TOKEN_LBRACKET)) {
        gql_node_free(rel);
        return NULL;
    }

    rel->u.rel_pattern.anonymous = true;
    rel->u.rel_pattern.direction = 0;  /* 双向 */

    /* 检查变量或类型 */
    if (check(p, GQL_TOKEN_IDENT)) {
        rel->u.rel_pattern.anonymous = false;
        rel->u.rel_pattern.variable = strdup(tok_lexeme(p));
        advance(p);
    }

    /* 检查类型 */
    if (match(p, GQL_TOKEN_COLON)) {
        if (check(p, GQL_TOKEN_IDENT)) {
            rel->u.rel_pattern.rel_type = strdup(tok_lexeme(p));
            advance(p);
        }
    }

    /* 检查属性 */
    if (check(p, GQL_TOKEN_LBRACE)) {
        advance(p);
        rel->u.rel_pattern.props = parse_property_map(p);
        if (!match(p, GQL_TOKEN_RBRACE)) {
            set_error(p, "Expected '}'");
        }
    }

    /* 检查长度范围 */
    if (match(p, GQL_TOKEN_STAR)) {
        gql_node_t *range = alloc_node(p, GQL_NODE_RANGE);
        range->u.range.min = 1;
        range->u.range.max = -1;  /* 无限制 */

        if (check(p, GQL_TOKEN_INT)) {
            range->u.range.min = (int)p->current->value.int_val;
            advance(p);
        }

        if (match(p, GQL_TOKEN_DOT)) {
            if (match(p, GQL_TOKEN_DOT)) {
                if (check(p, GQL_TOKEN_INT)) {
                    range->u.range.max = (int)p->current->value.int_val;
                    advance(p);
                } else {
                    range->u.range.max = -1;
                }
            }
        }

        rel->u.rel_pattern.length = range;
    }

    /* 消费 ']' */
    if (!match(p, GQL_TOKEN_RBRACKET)) {
        set_error(p, "Expected ']'");
    }

    /* 检查方向 */
    if (match(p, GQL_TOKEN_ARROW)) {
        rel->u.rel_pattern.direction = 1;  /* 正向 */
    } else if (match(p, GQL_TOKEN_LARROW)) {
        rel->u.rel_pattern.direction = 2;  /* 反向 */
    }

    return rel;
}

/* ============================================================
 * RETURN 子句解析
 * ============================================================ */

static gql_node_t *parse_return(gql_parser_t *p) {
    if (!match(p, GQL_TOKEN_RETURN)) return NULL;

    gql_node_t *ret = alloc_node(p, GQL_NODE_RETURN);
    if (!ret) return NULL;

    gql_node_t **items = NULL;
    size_t count = 0;

    /* 支持 RETURN * 或 RETURN expr, ... */
    if (match(p, GQL_TOKEN_STAR)) {
        /* RETURN * 表示返回所有变量 */
        gql_node_t *star = alloc_node(p, GQL_NODE_VARIABLE);
        star->u.variable.name = strdup("*");
        items = realloc(items, sizeof(gql_node_t*) * (count + 1));
        items[count++] = star;
    } else {
        do {
            gql_node_t *item = parse_expression(p);
            if (!item) break;

            /* 检查 AS 别名 */
            if (match(p, GQL_TOKEN_AS)) {
                gql_node_t *alias = alloc_node(p, GQL_NODE_ALIAS);
                alias->u.alias.expr = item;
                if (check(p, GQL_TOKEN_IDENT)) {
                    alias->u.alias.alias = strdup(tok_lexeme(p));
                    advance(p);
                }
                item = alias;
            }

            items = realloc(items, sizeof(gql_node_t*) * (count + 1));
            items[count++] = item;
        } while (match(p, GQL_TOKEN_COMMA));
    }

    ret->u.pattern_list.patterns = items;
    ret->u.pattern_list.count = count;

    return ret;
}

/* ============================================================
 * 属性映射解析
 * ============================================================ */

static gql_node_t *parse_property_map(gql_parser_t *p) {
    gql_node_t *map = alloc_node(p, GQL_NODE_PROPERTY_MAP);
    if (!map) return NULL;

    char **keys = NULL;
    gql_node_t **values = NULL;
    size_t count = 0;

    while (check(p, GQL_TOKEN_IDENT) && !check(p, GQL_TOKEN_RBRACE)) {
        char *key = strdup(tok_lexeme(p));
        advance(p);

        gql_node_t *value = NULL;
        if (match(p, GQL_TOKEN_COLON)) {
            value = parse_expression(p);
        }

        keys = realloc(keys, sizeof(char*) * (count + 1));
        values = realloc(values, sizeof(gql_node_t*) * (count + 1));
        keys[count] = key;
        values[count] = value;
        count++;

        if (!match(p, GQL_TOKEN_COMMA)) break;
    }

    map->u.property_map.keys = keys;
    map->u.property_map.values = values;
    map->u.property_map.count = count;

    return map;
}

/* ============================================================
 * 表达式解析
 * ============================================================ */

static gql_node_t *parse_expression(gql_parser_t *p) {
    return parse_or_expr(p);
}

static gql_node_t *parse_or_expr(gql_parser_t *p) {
    gql_node_t *left = parse_and_expr(p);
    if (!left) return NULL;

    while (match(p, GQL_TOKEN_OR)) {
        gql_node_t *right = parse_and_expr(p);
        if (!right) {
            gql_node_free(left);
            return NULL;
        }

        gql_node_t *node = alloc_node(p, GQL_NODE_BINARY_OP);
        node->u.binary_op.op = GQL_NODE_BINARY_OP;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        left = node;
    }

    return left;
}

static gql_node_t *parse_and_expr(gql_parser_t *p) {
    gql_node_t *left = parse_not_expr(p);
    if (!left) return NULL;

    while (match(p, GQL_TOKEN_AND)) {
        gql_node_t *right = parse_not_expr(p);
        if (!right) {
            gql_node_free(left);
            return NULL;
        }

        gql_node_t *node = alloc_node(p, GQL_NODE_BINARY_OP);
        node->u.binary_op.op = GQL_NODE_BINARY_OP;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        left = node;
    }

    return left;
}

static gql_node_t *parse_not_expr(gql_parser_t *p) {
    if (match(p, GQL_TOKEN_NOT)) {
        gql_node_t *expr = parse_not_expr(p);
        if (!expr) return NULL;

        gql_node_t *node = alloc_node(p, GQL_NODE_UNARY_OP);
        node->u.binary_op.left = expr;
        return node;
    }

    return parse_comparison(p);
}

static gql_node_t *parse_comparison(gql_parser_t *p) {
    gql_node_t *left = parse_additive(p);
    if (!left) return NULL;

    gql_token_type_t op = tok_type(p);
    if (op == GQL_TOKEN_EQ || op == GQL_TOKEN_NE ||
        op == GQL_TOKEN_LT || op == GQL_TOKEN_LE ||
        op == GQL_TOKEN_GT || op == GQL_TOKEN_GE) {

        advance(p);
        gql_node_t *right = parse_additive(p);
        if (!right) {
            gql_node_free(left);
            return NULL;
        }

        gql_node_t *node = alloc_node(p, GQL_NODE_BINARY_OP);
        node->u.binary_op.op = (gql_node_type_t)op;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        return node;
    }

    return left;
}

static gql_node_t *parse_additive(gql_parser_t *p) {
    gql_node_t *left = parse_multiplicative(p);
    if (!left) return NULL;

    gql_token_type_t op = tok_type(p);
    while (op == GQL_TOKEN_PLUS || op == GQL_TOKEN_MINUS) {
        advance(p);
        gql_node_t *right = parse_multiplicative(p);
        if (!right) {
            gql_node_free(left);
            return NULL;
        }

        gql_node_t *node = alloc_node(p, GQL_NODE_BINARY_OP);
        node->u.binary_op.op = (gql_node_type_t)op;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        left = node;

        op = tok_type(p);
    }

    return left;
}

static gql_node_t *parse_multiplicative(gql_parser_t *p) {
    gql_node_t *left = parse_primary(p);
    if (!left) return NULL;

    gql_token_type_t op = tok_type(p);
    while (op == GQL_TOKEN_STAR || op == GQL_TOKEN_SLASH || op == GQL_TOKEN_PERCENT) {
        advance(p);
        gql_node_t *right = parse_primary(p);
        if (!right) {
            gql_node_free(left);
            return NULL;
        }

        gql_node_t *node = alloc_node(p, GQL_NODE_BINARY_OP);
        node->u.binary_op.op = (gql_node_type_t)op;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        left = node;

        op = tok_type(p);
    }

    return left;
}

static gql_node_t *parse_primary(gql_parser_t *p) {
    gql_token_type_t type = tok_type(p);

    /* 整数 */
    if (type == GQL_TOKEN_INT) {
        gql_node_t *node = alloc_node(p, GQL_NODE_CONSTANT);
        node->u.constant.value_type = GRAPH_INT;
        node->u.constant.value.u.int_val = p->current->value.int_val;
        advance(p);
        return node;
    }

    /* 浮点数 */
    if (type == GQL_TOKEN_FLOAT) {
        gql_node_t *node = alloc_node(p, GQL_NODE_CONSTANT);
        node->u.constant.value_type = GRAPH_FLOAT;
        node->u.constant.value.u.float_val = p->current->value.float_val;
        advance(p);
        return node;
    }

    /* 字符串 */
    if (type == GQL_TOKEN_STRING) {
        gql_node_t *node = alloc_node(p, GQL_NODE_CONSTANT);
        node->u.constant.value_type = GRAPH_STRING;
        node->u.constant.value.u.string_val.str = strdup(p->current->value.str_val.str);
        node->u.constant.value.u.string_val.len = p->current->value.str_val.len;
        advance(p);
        return node;
    }

    /* 标识符/变量 */
    if (type == GQL_TOKEN_IDENT) {
        gql_node_t *node = alloc_node(p, GQL_NODE_VARIABLE);
        node->u.variable.name = strdup(tok_lexeme(p));
        advance(p);
        return node;
    }

    /* 布尔值 */
    if (type == GQL_TOKEN_TRUE || type == GQL_TOKEN_FALSE) {
        gql_node_t *node = alloc_node(p, GQL_NODE_CONSTANT);
        node->u.constant.value_type = GRAPH_BOOL;
        node->u.constant.value.u.bool_val = (type == GQL_TOKEN_TRUE);
        advance(p);
        return node;
    }

    /* NULL */
    if (type == GQL_TOKEN_NULL) {
        gql_node_t *node = alloc_node(p, GQL_NODE_CONSTANT);
        node->u.constant.value_type = GRAPH_NULL;
        advance(p);
        return node;
    }

    /* '(' 表达式 ')' */
    if (match(p, GQL_TOKEN_LPAREN)) {
        gql_node_t *expr = parse_expression(p);
        if (!match(p, GQL_TOKEN_RPAREN)) {
            set_error(p, "Expected ')'");
        }
        return expr;
    }

    /* 列表 [expr, ...] */
    if (match(p, GQL_TOKEN_LBRACKET)) {
        gql_node_t *list = alloc_node(p, GQL_NODE_LIST_EXPR);
        gql_node_t **items = NULL;
        size_t count = 0;

        if (!check(p, GQL_TOKEN_RBRACKET)) {
            do {
                gql_node_t *item = parse_expression(p);
                if (!item) break;
                items = realloc(items, sizeof(gql_node_t*) * (count + 1));
                items[count++] = item;
            } while (match(p, GQL_TOKEN_COMMA));
        }

        if (!match(p, GQL_TOKEN_RBRACKET)) {
            set_error(p, "Expected ']'");
        }

        list->u.pattern_list.patterns = items;
        list->u.pattern_list.count = count;
        return list;
    }

    return NULL;
}
