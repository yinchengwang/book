/**
 * @file gql_lexer.c
 * @brief GQL 词法分析器
 *
 * 实现 GQL 关键字、标识符、常量的识别
 */
#include "db/parser/graph/gql.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ============================================================
 * 关键字表
 * ============================================================ */

static struct {
    const char *name;
    gql_token_type_t type;
} keywords[] = {
    /* 语句关键字 */
    {"CREATE",      GQL_TOKEN_CREATE},
    {"MATCH",       GQL_TOKEN_MATCH},
    {"RETURN",      GQL_TOKEN_RETURN},
    {"WHERE",       GQL_TOKEN_WHERE},
    {"DELETE",      GQL_TOKEN_DELETE},
    {"SET",         GQL_TOKEN_SET},
    {"REMOVE",      GQL_TOKEN_REMOVE},
    {"WITH",        GQL_TOKEN_WITH},
    {"AS",          GQL_TOKEN_AS},
    {"DISTINCT",    GQL_TOKEN_DISTINCT},
    {"ORDER",       GQL_TOKEN_ORDER},
    {"BY",          GQL_TOKEN_BY},
    {"ASC",         GQL_TOKEN_ASC},
    {"DESC",        GQL_TOKEN_DESC},
    {"SKIP",        GQL_TOKEN_SKIP},
    {"LIMIT",       GQL_TOKEN_LIMIT},

    /* 聚合函数 */
    {"COUNT",       GQL_TOKEN_COUNT},
    {"SUM",         GQL_TOKEN_SUM},
    {"AVG",         GQL_TOKEN_AVG},
    {"MIN",         GQL_TOKEN_MIN},
    {"MAX",         GQL_TOKEN_MAX},
    {"COLLECT",     GQL_TOKEN_COLLECT},
    {"SIZE",        GQL_TOKEN_SIZE},
    {"TYPE",        GQL_TOKEN_TYPE},
    {"ID",          GQL_TOKEN_ID},
    {"PROPERTIES",  GQL_TOKEN_PROPERTIES},
    {"LABELS",      GQL_TOKEN_LABELS},
    {"EXISTS",      GQL_TOKEN_EXISTS},

    /* 字面量 */
    {"NULL",        GQL_TOKEN_NULL},
    {"TRUE",        GQL_TOKEN_TRUE},
    {"FALSE",       GQL_TOKEN_FALSE},

    /* 操作符 */
    {"AND",         GQL_TOKEN_AND},
    {"OR",          GQL_TOKEN_OR},
    {"NOT",         GQL_TOKEN_NOT},
    {"IN",          GQL_TOKEN_IN},
    {"STARTS",      GQL_TOKEN_STARTS_WITH},
    {"ENDS",        GQL_TOKEN_ENDS_WITH},
    {"CONTAINS",    GQL_TOKEN_CONTAINS},
    {"IS",          GQL_TOKEN_IS},

    {NULL,          0}
};

/* ============================================================
 * 解析器结构
 * ============================================================ */

struct gql_parser_s {
    const char  *input;          /**< 输入字符串 */
    size_t      len;             /**< 输入长度 */
    size_t      pos;             /**< 当前读取位置 */
    size_t      line;            /**< 当前行号 */
    size_t      column;          /**< 当前列号 */
    char        error_msg[256];  /**< 错误信息 */
    size_t      error_line;      /**< 错误行号 */
    size_t      error_column;    /**< 错误列号 */
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static void set_error(gql_parser_t *p, const char *msg) {
    snprintf(p->error_msg, sizeof(p->error_msg), "%s", msg);
    p->error_line = p->line;
    p->error_column = p->column;
}

static bool is_eof(gql_parser_t *p) {
    return p->pos >= p->len;
}

static char peek(gql_parser_t *p) {
    if (is_eof(p)) return '\0';
    return p->input[p->pos];
}

static char next_char(gql_parser_t *p) {
    if (is_eof(p)) return '\0';
    char c = p->input[p->pos++];
    if (c == '\n') {
        p->line++;
        p->column = 0;
    } else {
        p->column++;
    }
    return c;
}

static void skip_whitespace(gql_parser_t *p) {
    while (!is_eof(p)) {
        char c = peek(p);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            next_char(p);
        } else if (c == '/') {
            /* 跳过注释 */
            if (p->pos + 1 < p->len && p->input[p->pos + 1] == '/') {
                while (!is_eof(p) && peek(p) != '\n') {
                    next_char(p);
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

static gql_token_type_t lookup_keyword(const char *s) {
    for (int i = 0; keywords[i].name; i++) {
        if (strcmp(s, keywords[i].name) == 0) {
            return keywords[i].type;
        }
    }
    return GQL_TOKEN_IDENT;
}

/* ============================================================
 * Token 生成
 * ============================================================ */

static gql_token_t *scan_ident(gql_parser_t *p) {
    size_t start = p->pos;
    size_t start_line = p->line;
    size_t start_col = p->column;

    while (!is_eof(p) && (isalnum(peek(p)) || peek(p) == '_')) {
        next_char(p);
    }

    size_t len = p->pos - start;
    char *lexeme = malloc(len + 1);
    memcpy(lexeme, p->input + start, len);
    lexeme[len] = '\0';

    gql_token_t *tok = malloc(sizeof(gql_token_t));
    tok->type = lookup_keyword(lexeme);
    tok->lexeme = lexeme;
    tok->line = start_line;
    tok->column = start_col;

    return tok;
}

static gql_token_t *scan_number(gql_parser_t *p) {
    size_t start = p->pos;
    size_t start_line = p->line;
    size_t start_col = p->column;
    bool is_float = false;

    /* 整数部分 */
    while (!is_eof(p) && isdigit(peek(p))) {
        next_char(p);
    }

    /* 小数部分 */
    if (peek(p) == '.' && p->pos + 1 < p->len && isdigit(p->input[p->pos + 1])) {
        is_float = true;
        next_char(p);
        while (!is_eof(p) && isdigit(peek(p))) {
            next_char(p);
        }
    }

    size_t len = p->pos - start;
    char *lexeme = malloc(len + 1);
    memcpy(lexeme, p->input + start, len);
    lexeme[len] = '\0';

    gql_token_t *tok = malloc(sizeof(gql_token_t));

    if (is_float) {
        tok->type = GQL_TOKEN_FLOAT;
        tok->value.float_val = atof(lexeme);
    } else {
        tok->type = GQL_TOKEN_INT;
        tok->value.int_val = atoll(lexeme);
    }

    tok->lexeme = lexeme;
    tok->line = start_line;
    tok->column = start_col;

    return tok;
}

static gql_token_t *scan_string(gql_parser_t *p) {
    size_t start_line = p->line;
    size_t start_col = p->column;
    char quote = next_char(p);

    size_t start = p->pos;
    while (!is_eof(p) && peek(p) != quote) {
        if (peek(p) == '\\' && p->pos + 1 < p->len) {
            next_char(p);
        }
        next_char(p);
    }

    if (is_eof(p)) {
        set_error(p, "Unterminated string");
        return NULL;
    }

    size_t len = p->pos - start;
    char *str = malloc(len + 1);
    memcpy(str, p->input + start, len);
    str[len] = '\0';

    next_char(p);

    gql_token_t *tok = malloc(sizeof(gql_token_t));
    tok->type = GQL_TOKEN_STRING;
    tok->lexeme = str;
    tok->value.str_val.str = str;
    tok->value.str_val.len = len;
    tok->line = start_line;
    tok->column = start_col;

    return tok;
}

static gql_token_t *scan_operator(gql_parser_t *p) {
    size_t start_line = p->line;
    size_t start_col = p->column;
    char c = peek(p);

    gql_token_type_t type = GQL_TOKEN_ERROR;
    gql_token_t *tok = malloc(sizeof(gql_token_t));

    switch (c) {
        case '+': type = GQL_TOKEN_PLUS; break;
        case '-':
            next_char(p);
            if (peek(p) == '>') {
                next_char(p);
                tok->type = GQL_TOKEN_ARROW;
                tok->lexeme = NULL;
                tok->line = start_line;
                tok->column = start_col;
                return tok;
            }
            tok->type = GQL_TOKEN_MINUS;
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        case '*': type = GQL_TOKEN_STAR; break;
        case '/': type = GQL_TOKEN_SLASH; break;
        case '%': type = GQL_TOKEN_PERCENT; break;
        case '.': type = GQL_TOKEN_DOT; break;
        case ',': type = GQL_TOKEN_COMMA; break;
        case ':': type = GQL_TOKEN_COLON; break;
        case '|': type = GQL_TOKEN_PIPE; break;
        case '?': type = GQL_TOKEN_QUESTION; break;
        case '(': type = GQL_TOKEN_LPAREN; break;
        case ')': type = GQL_TOKEN_RPAREN; break;
        case '[': type = GQL_TOKEN_LBRACKET; break;
        case ']': type = GQL_TOKEN_RBRACKET; break;
        case '{': type = GQL_TOKEN_LBRACE; break;
        case '}': type = GQL_TOKEN_RBRACE; break;
        case '=':
            next_char(p);
            tok->type = GQL_TOKEN_EQ;
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        case '<':
            next_char(p);
            if (peek(p) == '-') {
                next_char(p);
                if (peek(p) == '>') {
                    next_char(p);
                    tok->type = GQL_TOKEN_BIDIR;
                } else {
                    tok->type = GQL_TOKEN_LARROW;
                }
            } else if (peek(p) == '=') {
                next_char(p);
                tok->type = GQL_TOKEN_LE;
            } else if (peek(p) == '>') {
                next_char(p);
                tok->type = GQL_TOKEN_NE;
            } else {
                tok->type = GQL_TOKEN_LT;
            }
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        case '>':
            next_char(p);
            if (peek(p) == '=') {
                next_char(p);
                tok->type = GQL_TOKEN_GE;
            } else {
                tok->type = GQL_TOKEN_GT;
            }
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        case '!':
            next_char(p);
            if (peek(p) == '=') {
                next_char(p);
                tok->type = GQL_TOKEN_NE;
            } else {
                tok->type = GQL_TOKEN_ERROR;
            }
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        case ';':
            next_char(p);
            tok->type = GQL_TOKEN_SEMICOLON;
            tok->lexeme = NULL;
            tok->line = start_line;
            tok->column = start_col;
            return tok;
        default:
            free(tok);
            return NULL;
    }

    next_char(p);
    tok->type = type;
    tok->lexeme = NULL;
    tok->line = start_line;
    tok->column = start_col;

    return tok;
}

/* ============================================================
 * 词法分析器接口
 * ============================================================ */

gql_token_t *gql_lexer_next(gql_parser_t *p) {
    if (!p) return NULL;

    skip_whitespace(p);

    if (is_eof(p)) {
        gql_token_t *tok = malloc(sizeof(gql_token_t));
        tok->type = GQL_TOKEN_EOF;
        tok->lexeme = NULL;
        tok->line = p->line;
        tok->column = p->column;
        return tok;
    }

    char c = peek(p);

    if (isalpha(c) || c == '_') {
        return scan_ident(p);
    }

    if (isdigit(c)) {
        return scan_number(p);
    }

    if (c == '\'' || c == '"') {
        return scan_string(p);
    }

    gql_token_t *tok = scan_operator(p);
    if (tok) {
        return tok;
    }

    gql_token_t *err_tok = malloc(sizeof(gql_token_t));
    err_tok->type = GQL_TOKEN_ERROR;
    err_tok->lexeme = NULL;
    err_tok->line = p->line;
    err_tok->column = p->column;
    set_error(p, "Unknown character");
    return err_tok;
}

/* ============================================================
 * AST 操作
 * ============================================================ */

void gql_node_free(gql_node_t *node) {
    if (!node) return;

    switch (node->type) {
        case GQL_NODE_QUERY:
            if (node->u.query.patterns) gql_node_free(node->u.query.patterns);
            if (node->u.query.where) gql_node_free(node->u.query.where);
            if (node->u.query.ret) gql_node_free(node->u.query.ret);
            break;

        case GQL_NODE_CREATE:
            if (node->u.create.patterns) gql_node_free(node->u.create.patterns);
            break;

        case GQL_NODE_DELETE:
            if (node->u.del.targets) gql_node_free(node->u.del.targets);
            break;

        case GQL_NODE_NODE_PATTERN:
            free(node->u.node_pattern.variable);
            free(node->u.node_pattern.label);
            if (node->u.node_pattern.props) gql_node_free(node->u.node_pattern.props);
            break;

        case GQL_NODE_REL_PATTERN:
            free(node->u.rel_pattern.variable);
            free(node->u.rel_pattern.rel_type);
            if (node->u.rel_pattern.props) gql_node_free(node->u.rel_pattern.props);
            if (node->u.rel_pattern.length) gql_node_free(node->u.rel_pattern.length);
            break;

        case GQL_NODE_PATTERN_LIST:
        case GQL_NODE_PATTERN:
            for (size_t i = 0; i < node->u.pattern_list.count; i++) {
                if (node->u.pattern_list.patterns[i]) {
                    gql_node_free(node->u.pattern_list.patterns[i]);
                }
            }
            free(node->u.pattern_list.patterns);
            break;

        case GQL_NODE_PROPERTY_MAP:
            for (size_t i = 0; i < node->u.property_map.count; i++) {
                free(node->u.property_map.keys[i]);
                if (node->u.property_map.values[i]) {
                    gql_node_free(node->u.property_map.values[i]);
                }
            }
            free(node->u.property_map.keys);
            free(node->u.property_map.values);
            break;

        case GQL_NODE_RETURN_ITEM:
            if (node->u.return_item.expr) gql_node_free(node->u.return_item.expr);
            free(node->u.return_item.alias);
            break;

        case GQL_NODE_BINARY_OP:
            if (node->u.binary_op.left) gql_node_free(node->u.binary_op.left);
            if (node->u.binary_op.right) gql_node_free(node->u.binary_op.right);
            break;

        case GQL_NODE_UNARY_OP:
            if (node->u.binary_op.left) gql_node_free(node->u.binary_op.left);
            break;

        case GQL_NODE_FUNCTION_CALL:
            free(node->u.func_call.name);
            for (size_t i = 0; i < node->u.func_call.arg_count; i++) {
                if (node->u.func_call.args[i]) {
                    gql_node_free(node->u.func_call.args[i]);
                }
            }
            free(node->u.func_call.args);
            break;

        case GQL_NODE_VARIABLE:
            free(node->u.variable.name);
            break;

        case GQL_NODE_CONSTANT:
            if (node->u.constant.value_type == GRAPH_STRING) {
                free(node->u.constant.value.u.string_val.str);
            }
            break;

        case GQL_NODE_ALIAS:
            if (node->u.alias.expr) gql_node_free(node->u.alias.expr);
            free(node->u.alias.alias);
            break;

        default:
            break;
    }

    free(node);
}

void gql_node_print(const gql_node_t *node, int indent) {
    if (!node) return;

    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s\n", gql_node_type_name(node->type));

    switch (node->type) {
        case GQL_NODE_NODE_PATTERN:
            if (node->u.node_pattern.variable) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("variable: %s\n", node->u.node_pattern.variable);
            }
            if (node->u.node_pattern.label) {
                for (int i = 0; i < indent + 1; i++) printf("  ");
                printf("label: %s\n", node->u.node_pattern.label);
            }
            break;

        case GQL_NODE_VARIABLE:
            for (int i = 0; i < indent + 1; i++) printf("  ");
            printf("name: %s\n", node->u.variable.name);
            break;

        default:
            break;
    }
}

const char *gql_node_type_name(gql_node_type_t type) {
    static const char *names[] = {
        "QUERY", "CREATE", "MERGE", "SET", "DELETE", "REMOVE",
        "PATTERN", "PATTERN_LIST", "NODE_PATTERN", "REL_PATTERN", "PATH_PATTERN",
        "WHERE", "RETURN", "RETURN_ITEM", "ORDER_BY", "ORDER_ITEM",
        "SKIP", "LIMIT", "WITH",
        "VARIABLE", "PROPERTY", "PROPERTY_MAP", "FUNCTION_CALL",
        "AGGREGATION", "BINARY_OP", "UNARY_OP", "LIST_EXPR", "RANGE",
        "PARAMETER", "ALIAS", "DISTINCT", "CONSTANT"
    };

    if (type >= 0 && type <= GQL_NODE_CONSTANT) {
        return names[type];
    }
    return "UNKNOWN";
}
