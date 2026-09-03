/**
 * @file query_filter.c
 * @brief 标量过滤表达式解析和执行实现
 *
 * 实现自顶向下的递归下降解析器，支持：
 *   - 比较操作符: =, !=, <, <=, >, >=
 *   - 逻辑操作符: AND, OR, NOT
 *   - 括号: ( )
 *   - 类型: 整数、浮点、字符串、布尔
 */

#include "db/core/query_filter.h"
#include "db/errors.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================
 * Token 定义
 * ======================================================================== */

/** Token 类型 */
typedef enum TokenType_e {
    TOKEN_INVALID = -1,
    TOKEN_EOF = 0,
    TOKEN_FIELD,           /**< 字段名 */
    TOKEN_STRING,          /**< 字符串常量 */
    TOKEN_INT,             /**< 整数常量 */
    TOKEN_FLOAT,           /**< 浮点常量 */
    TOKEN_BOOL,            /**< 布尔常量 */
    TOKEN_EQ,              /**< = */
    TOKEN_NE,              /**< != */
    TOKEN_LT,              /**< < */
    TOKEN_LE,              /**< <= */
    TOKEN_GT,              /**< > */
    TOKEN_GE,              /**< >= */
    TOKEN_AND,             /**< AND */
    TOKEN_OR,              /**< OR */
    TOKEN_NOT,             /**< NOT */
    TOKEN_LPAREN,          /**< ( */
    TOKEN_RPAREN,          /**< ) */
    TOKEN_COMMA            /**< , */
} TokenType;

/** Token */
typedef struct Token_s {
    TokenType type;
    char *lexeme;          /**< 词素 */
    VDBFilterValue value;     /**< 字面量值 */
    int pos;               /**< 位置 */
} Token;

/** Token 列表 */
typedef struct TokenList_s {
    Token *tokens;
    int count;
    int capacity;
    int pos;               /**< 当前读取位置 */
} TokenList;

/* ========================================================================
 * 解析器状态
 * ======================================================================== */

/** 解析器 */
typedef struct Parser_s {
    const char *expr;      /**< 表达式字符串 */
    int len;               /**< 表达式长度 */
    int pos;               /**< 当前解析位置 */
    TokenList tokens;      /**< token 列表 */
    FilterParseError error; /**< 错误码 */
    int error_pos;         /**< 错误位置 */
    char error_msg[256];   /**< 错误消息 */
} Parser;

/* ========================================================================
 * 内存管理
 * ======================================================================== */

static void *filter_malloc(size_t size) {
    void *ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "[query_filter] 内存分配失败: %zu bytes\n", size);
    }
    return ptr;
}

static void *filter_calloc(size_t nmemb, size_t size) {
    void *ptr = calloc(nmemb, size);
    if (!ptr && nmemb > 0 && size > 0) {
        fprintf(stderr, "[query_filter] 内存分配失败: %zu x %zu bytes\n", nmemb, size);
    }
    return ptr;
}

static char *filter_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = filter_malloc(len);
    if (copy) {
        memcpy(copy, s, len);
    }
    return copy;
}

/* ========================================================================
 * Token 操作
 * ======================================================================== */

static Token *token_list_add(TokenList *list) {
    if (list->count >= list->capacity) {
        list->capacity = list->capacity > 0 ? list->capacity * 2 : 16;
        Token *new_tokens = realloc(list->tokens, list->capacity * sizeof(Token));
        if (!new_tokens) return NULL;
        list->tokens = new_tokens;
    }
    return &list->tokens[list->count++];
}

static Token *token_peek(TokenList *list) {
    if (list->pos >= list->count) return NULL;
    return &list->tokens[list->pos];
}

static Token *token_advance(TokenList *list) {
    if (list->pos >= list->count) return NULL;
    return &list->tokens[list->pos++];
}

static void token_list_free(TokenList *list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        if (list->tokens[i].lexeme) {
            free(list->tokens[i].lexeme);
        }
        if (list->tokens[i].type == TOKEN_STRING && list->tokens[i].value.data.str_val) {
            free(list->tokens[i].value.data.str_val);
        }
    }
    free(list->tokens);
    list->tokens = NULL;
    list->count = 0;
    list->capacity = 0;
}

/* ========================================================================
 * 词法分析
 * ======================================================================== */

static bool is_alpha(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static bool is_alnum(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

static void skip_whitespace(Parser *parser) {
    while (parser->pos < parser->len && isspace((unsigned char)parser->expr[parser->pos])) {
        parser->pos++;
    }
}

static bool match_keyword(const char *s, const char *keyword) {
    return strcasecmp(s, keyword) == 0;
}

static int parse_escape_char(char c) {
    switch (c) {
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case '\\': return '\\';
        case '"': return '"';
        case '\'': return '\'';
        default: return c;
    }
}

static TokenType token_type_from_keyword(const char *lexeme) {
    if (match_keyword(lexeme, "AND")) return TOKEN_AND;
    if (match_keyword(lexeme, "OR")) return TOKEN_OR;
    if (match_keyword(lexeme, "NOT")) return TOKEN_NOT;
    if (match_keyword(lexeme, "true") || match_keyword(lexeme, "TRUE")) return TOKEN_BOOL;
    if (match_keyword(lexeme, "false") || match_keyword(lexeme, "FALSE")) return TOKEN_BOOL;
    return TOKEN_FIELD;
}

static bool scan_string(Parser *parser, int start_pos, char **out_str, int *out_len) {
    // 跳过开始的引号
    parser->pos++;
    int string_start = parser->pos;
    int string_len = 0;

    // 分配初始缓冲区
    int buf_capacity = 64;
    char *buf = filter_malloc(buf_capacity);
    if (!buf) return false;

    while (parser->pos < parser->len) {
        char c = parser->expr[parser->pos];

        if (c == '\\' && parser->pos + 1 < parser->len) {
            // 转义字符
            parser->pos++;
            c = parse_escape_char(parser->expr[parser->pos]);
        } else if (c == '"') {
            // 结束字符串
            parser->pos++;
            buf[string_len] = '\0';
            *out_str = buf;
            if (out_len) *out_len = string_len;
            return true;
        } else if (c == '\n' || c == '\r') {
            // 未终止的字符串
            parser->error = FILTER_PARSE_ERR_UNTERMINATED_STRING;
            parser->error_pos = start_pos;
            snprintf(parser->error_msg, sizeof(parser->error_msg),
                    "字符串未终止: 期望 \" 在行尾");
            free(buf);
            return false;
        }

        // 扩展缓冲区
        if (string_len >= buf_capacity - 1) {
            buf_capacity *= 2;
            char *new_buf = realloc(buf, buf_capacity);
            if (!new_buf) {
                free(buf);
                return false;
            }
            buf = new_buf;
        }

        buf[string_len++] = c;
        parser->pos++;
    }

    // 字符串未终止
    parser->error = FILTER_PARSE_ERR_UNTERMINATED_STRING;
    parser->error_pos = start_pos;
    snprintf(parser->error_msg, sizeof(parser->error_msg),
            "字符串未终止: 缺少结束引号");
    free(buf);
    return false;
}

static bool scan_number(Parser *parser, int start_pos, VDBFilterValue *out_value) {
    int num_start = parser->pos;
    bool has_dot = false;
    bool has_exp = false;

    // 处理可选的负号
    if (parser->pos < parser->len && parser->expr[parser->pos] == '-') {
        parser->pos++;
    }

    while (parser->pos < parser->len) {
        char c = parser->expr[parser->pos];
        if (isdigit((unsigned char)c)) {
            parser->pos++;
        } else if (c == '.' && !has_dot && !has_exp) {
            has_dot = true;
            parser->pos++;
        } else if ((c == 'e' || c == 'E') && !has_exp) {
            has_exp = true;
            parser->pos++;
            if (parser->pos < parser->len && (parser->expr[parser->pos] == '+' || parser->expr[parser->pos] == '-')) {
                parser->pos++;
            }
        } else {
            break;
        }
    }

    int num_len = parser->pos - num_start;
    if (num_len == 0) {
        parser->error = FILTER_PARSE_ERR_INVALID_TOKEN;
        parser->error_pos = start_pos;
        snprintf(parser->error_msg, sizeof(parser->error_msg), "数字格式错误");
        return false;
    }

    // 提取数字字符串
    char *num_str = filter_malloc(num_len + 1);
    if (!num_str) return false;
    memcpy(num_str, &parser->expr[num_start], num_len);
    num_str[num_len] = '\0';

    if (has_dot || has_exp) {
        // 浮点数
        char *endptr;
        double val = strtod(num_str, &endptr);
        if (*endptr != '\0') {
            parser->error = FILTER_PARSE_ERR_INVALID_TOKEN;
            parser->error_pos = start_pos;
            snprintf(parser->error_msg, sizeof(parser->error_msg), "浮点数格式错误");
            free(num_str);
            return false;
        }
        out_value->type = VDB_FILTER_VALUE_FLOAT;
        out_value->data.float_val = val;
    } else {
        // 整数
        char *endptr;
        int64_t val = strtoll(num_str, &endptr, 10);
        if (*endptr != '\0') {
            parser->error = FILTER_PARSE_ERR_INVALID_TOKEN;
            parser->error_pos = start_pos;
            snprintf(parser->error_msg, sizeof(parser->error_msg), "整数格式错误");
            free(num_str);
            return false;
        }
        out_value->type = VDB_FILTER_VALUE_INT;
        out_value->data.int_val = val;
    }

    free(num_str);
    return true;
}

static bool tokenizer(Parser *parser) {
    while (parser->pos < parser->len) {
        skip_whitespace(parser);
        if (parser->pos >= parser->len) break;

        int token_start = parser->pos;
        char c = parser->expr[parser->pos];

        Token *token = token_list_add(&parser->tokens);
        if (!token) {
            parser->error = FILTER_PARSE_ERR_OUT_OF_MEMORY;
            return false;
        }

        memset(token, 0, sizeof(Token));
        token->pos = token_start;

        if (c == '"') {
            // 字符串
            token->type = TOKEN_STRING;
            if (!scan_string(parser, token_start, &token->value.data.str_val, NULL)) {
                return false;
            }
            token->value.type = VDB_FILTER_VALUE_STRING;
        } else if (isdigit((unsigned char)c) || (c == '-' && parser->pos + 1 < parser->len && isdigit((unsigned char)parser->expr[parser->pos + 1]))) {
            // 数字
            if (!scan_number(parser, token_start, &token->value)) {
                return false;
            }
            token->type = (token->value.type == VDB_FILTER_VALUE_INT) ? TOKEN_INT : TOKEN_FLOAT;
        } else if (is_alpha((unsigned char)c)) {
            // 标识符或关键字
            int ident_start = parser->pos;
            while (parser->pos < parser->len && is_alnum((unsigned char)parser->expr[parser->pos])) {
                parser->pos++;
            }
            int ident_len = parser->pos - ident_start;
            char *ident = filter_malloc(ident_len + 1);
            if (!ident) {
                parser->error = FILTER_PARSE_ERR_OUT_OF_MEMORY;
                return false;
            }
            memcpy(ident, &parser->expr[ident_start], ident_len);
            ident[ident_len] = '\0';

            token->lexeme = ident;
            token->type = token_type_from_keyword(ident);

            if (token->type == TOKEN_BOOL) {
                token->value.type = VDB_FILTER_VALUE_BOOL;
                token->value.data.bool_val = (strcasecmp(ident, "true") == 0);
            }
        } else if (c == '=') {
            // = 或 ==（单个 = 也接受）
            token->type = TOKEN_EQ;
            parser->pos++;
        } else if (c == '!' && parser->pos + 1 < parser->len && parser->expr[parser->pos + 1] == '=') {
            token->type = TOKEN_NE;
            parser->pos += 2;
        } else if (c == '<' && parser->pos + 1 < parser->len && parser->expr[parser->pos + 1] == '=') {
            token->type = TOKEN_LE;
            parser->pos += 2;
        } else if (c == '<' && parser->pos + 1 < parser->len && parser->expr[parser->pos + 1] == '>') {
            // <> 作为 !=
            token->type = TOKEN_NE;
            parser->pos += 2;
        } else if (c == '<') {
            token->type = TOKEN_LT;
            parser->pos++;
        } else if (c == '>' && parser->pos + 1 < parser->len && parser->expr[parser->pos + 1] == '=') {
            token->type = TOKEN_GE;
            parser->pos += 2;
        } else if (c == '>') {
            token->type = TOKEN_GT;
            parser->pos++;
        } else if (c == '(') {
            token->type = TOKEN_LPAREN;
            parser->pos++;
        } else if (c == ')') {
            token->type = TOKEN_RPAREN;
            parser->pos++;
        } else if (c == ',') {
            token->type = TOKEN_COMMA;
            parser->pos++;
        } else {
            parser->error = FILTER_PARSE_ERR_INVALID_TOKEN;
            parser->error_pos = token_start;
            snprintf(parser->error_msg, sizeof(parser->error_msg),
                    "无效字符: '%c' (0x%02X)", c, (unsigned char)c);
            return false;
        }
    }

    // 添加 EOF token
    Token *eof = token_list_add(&parser->tokens);
    if (!eof) {
        parser->error = FILTER_PARSE_ERR_OUT_OF_MEMORY;
        return false;
    }
    eof->type = TOKEN_EOF;
    eof->pos = parser->pos;

    return true;
}

/* ========================================================================
 * 语法分析 (递归下降)
 * ======================================================================== */

/*
 * 语法规则:
 *   expression  := or_expr
 *   or_expr     := and_expr (OR and_expr)*
 *   and_expr    := not_expr (AND not_expr)*
 *   not_expr    := NOT not_expr | primary
 *   primary     := '(' expression ')' | comparison
 *   comparison  := field_name op value
 *   op          := '=' | '!=' | '<' | '<=' | '>' | '>='
 *   value       := INT | FLOAT | STRING | BOOL
 */

static FilterNode *parse_expression(Parser *parser);
static FilterNode *parse_or_expr(Parser *parser);
static FilterNode *parse_and_expr(Parser *parser);
static FilterNode *parse_not_expr(Parser *parser);
static FilterNode *parse_primary(Parser *parser);
static FilterNode *parse_comparison(Parser *parser);

static Token *current_token(Parser *parser) {
    return token_peek(&parser->tokens);
}

static void advance_token(Parser *parser) {
    token_advance(&parser->tokens);
}

static bool check_token_type(Parser *parser, TokenType type) {
    Token *token = current_token(parser);
    return token && token->type == type;
}

static VDBFilterOp token_to_filter_op(TokenType type) {
    switch (type) {
        case TOKEN_EQ: return VDB_FILTER_OP_EQ;
        case TOKEN_NE: return VDB_FILTER_OP_NE;
        case TOKEN_LT: return VDB_FILTER_OP_LT;
        case TOKEN_LE: return VDB_FILTER_OP_LE;
        case TOKEN_GT: return VDB_FILTER_OP_GT;
        case TOKEN_GE: return VDB_FILTER_OP_GE;
        default: return VDB_FILTER_OP_INVALID;
    }
}

static FilterNode *create_comparison_node(const char *field_name, VDBFilterOp op, VDBFilterValue *value) {
    FilterNode *node = filter_calloc(1, sizeof(FilterNode));
    if (!node) return NULL;

    node->type = FILTER_NODE_COMPARISON;
    node->data.comparison.field_name = filter_strdup(field_name);
    if (!node->data.comparison.field_name) {
        free(node);
        return NULL;
    }

    node->data.comparison.op = op;
    memcpy(&node->data.comparison.value, value, sizeof(VDBFilterValue));

    return node;
}

static FilterNode *create_logical_node(VDBFilterOp op, FilterNode *left, FilterNode *right) {
    FilterNode *node = filter_calloc(1, sizeof(FilterNode));
    if (!node) return NULL;

    node->type = FILTER_NODE_LOGICAL;
    node->data.logical.op = op;
    node->data.logical.left = left;
    node->data.logical.right = right;

    return node;
}

static void parser_error(Parser *parser, const char *msg) {
    if (parser->error == FILTER_PARSE_OK) {
        Token *token = current_token(parser);
        parser->error_pos = token ? token->pos : parser->pos;
        snprintf(parser->error_msg, sizeof(parser->error_msg), "%s", msg);
    }
}

static FilterNode *parse_expression(Parser *parser) {
    return parse_or_expr(parser);
}

static FilterNode *parse_or_expr(Parser *parser) {
    FilterNode *left = parse_and_expr(parser);
    if (!left) return NULL;

    while (check_token_type(parser, TOKEN_OR)) {
        advance_token(parser);
        FilterNode *right = parse_and_expr(parser);
        if (!right) {
            filter_ast_free(left);
            return NULL;
        }

        FilterNode *node = create_logical_node(VDB_FILTER_OP_OR, left, right);
        if (!node) {
            filter_ast_free(left);
            filter_ast_free(right);
            return NULL;
        }
        left = node;
    }

    return left;
}

static FilterNode *parse_and_expr(Parser *parser) {
    FilterNode *left = parse_not_expr(parser);
    if (!left) return NULL;

    while (check_token_type(parser, TOKEN_AND)) {
        advance_token(parser);
        FilterNode *right = parse_not_expr(parser);
        if (!right) {
            filter_ast_free(left);
            return NULL;
        }

        FilterNode *node = create_logical_node(VDB_FILTER_OP_AND, left, right);
        if (!node) {
            filter_ast_free(left);
            filter_ast_free(right);
            return NULL;
        }
        left = node;
    }

    return left;
}

static FilterNode *parse_not_expr(Parser *parser) {
    if (check_token_type(parser, TOKEN_NOT)) {
        advance_token(parser);
        FilterNode *operand = parse_not_expr(parser);
        if (!operand) return NULL;

        FilterNode *node = create_logical_node(VDB_FILTER_OP_NOT, operand, NULL);
        if (!node) {
            filter_ast_free(operand);
            return NULL;
        }
        return node;
    }

    return parse_primary(parser);
}

static FilterNode *parse_primary(Parser *parser) {
    if (check_token_type(parser, TOKEN_LPAREN)) {
        advance_token(parser);
        FilterNode *expr = parse_expression(parser);
        if (!expr) return NULL;

        if (!check_token_type(parser, TOKEN_RPAREN)) {
            parser_error(parser, "期望 ')'");
            filter_ast_free(expr);
            return NULL;
        }
        advance_token(parser);
        return expr;
    }

    return parse_comparison(parser);
}

static FilterNode *parse_comparison(Parser *parser) {
    // 获取字段名
    Token *field_token = current_token(parser);
    if (!field_token || (field_token->type != TOKEN_FIELD && field_token->type != TOKEN_STRING)) {
        parser_error(parser, "期望字段名");
        return NULL;
    }

    char *field_name = field_token->lexeme ? filter_strdup(field_token->lexeme) : NULL;
    if (!field_name) return NULL;

    advance_token(parser);

    // 获取操作符
    Token *op_token = current_token(parser);
    if (!op_token) {
        parser_error(parser, "期望操作符");
        free(field_name);
        return NULL;
    }

    VDBFilterOp op = token_to_filter_op(op_token->type);
    if (op == VDB_FILTER_OP_INVALID) {
        parser_error(parser, "无效的操作符");
        free(field_name);
        return NULL;
    }

    advance_token(parser);

    // 获取值
    Token *value_token = current_token(parser);
    if (!value_token) {
        parser_error(parser, "期望值");
        free(field_name);
        return NULL;
    }

    VDBFilterValue value;
    memset(&value, 0, sizeof(value));

    switch (value_token->type) {
        case TOKEN_INT:
        case TOKEN_FLOAT:
            memcpy(&value, &value_token->value, sizeof(VDBFilterValue));
            break;
        case TOKEN_STRING:
            value.type = VDB_FILTER_VALUE_STRING;
            value.data.str_val = filter_strdup(value_token->value.data.str_val);
            if (!value.data.str_val) {
                free(field_name);
                return NULL;
            }
            break;
        case TOKEN_BOOL:
            memcpy(&value, &value_token->value, sizeof(VDBFilterValue));
            break;
        default:
            parser_error(parser, "期望值（整数、浮点、字符串或布尔）");
            free(field_name);
            return NULL;
    }

    advance_token(parser);

    return create_comparison_node(field_name, op, &value);
}

/* ========================================================================
 * 公开 API 实现
 * ======================================================================== */

FilterParseResult *filter_parse(const char *expr) {
    FilterParseResult *result = filter_calloc(1, sizeof(FilterParseResult));
    if (!result) return NULL;

    if (!expr || *expr == '\0') {
        result->error = FILTER_PARSE_ERR_EMPTY_EXPRESSION;
        result->error_pos = 0;
        snprintf(result->error_msg, sizeof(result->error_msg), "空表达式");
        return result;
    }

    Parser parser;
    memset(&parser, 0, sizeof(parser));
    parser.expr = expr;
    parser.len = strlen(expr);
    parser.pos = 0;
    parser.error = FILTER_PARSE_OK;

    // 词法分析
    if (!tokenizer(&parser)) {
        result->error = parser.error;
        result->error_pos = parser.error_pos;
        snprintf(result->error_msg, sizeof(result->error_msg), "%s", parser.error_msg);
        token_list_free(&parser.tokens);
        return result;
    }

    // 重置 token 位置
    parser.tokens.pos = 0;

    // 语法分析
    FilterNode *ast = parse_expression(&parser);

    token_list_free(&parser.tokens);

    if (parser.error != FILTER_PARSE_OK) {
        result->error = parser.error;
        result->error_pos = parser.error_pos;
        snprintf(result->error_msg, sizeof(result->error_msg), "%s", parser.error_msg);
        return result;
    }

    if (!ast) {
        result->error = FILTER_PARSE_ERR_EMPTY_EXPRESSION;
        snprintf(result->error_msg, sizeof(result->error_msg), "解析失败");
        return result;
    }

    // 检查是否还有剩余 token
    if (parser.tokens.pos < parser.tokens.count) {
        Token *token = &parser.tokens.tokens[parser.tokens.pos];
        if (token->type != TOKEN_EOF) {
            result->error = FILTER_PARSE_ERR_UNEXPECTED_TOKEN;
            result->error_pos = token->pos;
            snprintf(result->error_msg, sizeof(result->error_msg),
                    "意外的 token: '%s'", token->lexeme ? token->lexeme : "?");
            filter_ast_free(ast);
            return result;
        }
    }

    result->ast = ast;
    result->error = FILTER_PARSE_OK;
    return result;
}

void filter_parse_result_free(FilterParseResult *result) {
    if (!result) return;
    if (result->ast) {
        filter_ast_free(result->ast);
    }
    free(result);
}

void filter_ast_free(FilterNode *ast) {
    if (!ast) return;

    switch (ast->type) {
        case FILTER_NODE_COMPARISON:
            if (ast->data.comparison.field_name) {
                free(ast->data.comparison.field_name);
            }
            if (ast->data.comparison.value.type == VDB_FILTER_VALUE_STRING &&
                ast->data.comparison.value.data.str_val) {
                free(ast->data.comparison.value.data.str_val);
            }
            break;

        case FILTER_NODE_LOGICAL:
            if (ast->data.logical.left) {
                filter_ast_free(ast->data.logical.left);
            }
            if (ast->data.logical.right) {
                filter_ast_free(ast->data.logical.right);
            }
            break;

        case FILTER_NODE_VALUE:
            if (ast->data.value.type == VDB_FILTER_VALUE_STRING &&
                ast->data.value.data.str_val) {
                free(ast->data.value.data.str_val);
            }
            break;

        default:
            break;
    }

    free(ast);
}

/* ========================================================================
 * 表达式执行
 * ======================================================================== */

static bool compare_values(VDBFilterValueType lhs_type, VDBFilterValueData lhs_data,
                           VDBFilterValueType rhs_type, VDBFilterValueData rhs_data,
                           VDBFilterOp op) {
    // 类型不匹配时使用字符串比较
    if (lhs_type != rhs_type) {
        char lhs_buf[64], rhs_buf[64];
        // 将两个值都转为字符串比较
        switch (lhs_type) {
            case VDB_FILTER_VALUE_INT: snprintf(lhs_buf, sizeof(lhs_buf), "%lld", (long long)lhs_data.int_val); break;
            case VDB_FILTER_VALUE_FLOAT: snprintf(lhs_buf, sizeof(lhs_buf), "%.10g", lhs_data.float_val); break;
            case VDB_FILTER_VALUE_BOOL: snprintf(lhs_buf, sizeof(lhs_buf), "%s", lhs_data.bool_val ? "true" : "false"); break;
            case VDB_FILTER_VALUE_STRING: snprintf(lhs_buf, sizeof(lhs_buf), "%s", lhs_data.str_val ? lhs_data.str_val : ""); break;
            default: lhs_buf[0] = '\0';
        }
        switch (rhs_type) {
            case VDB_FILTER_VALUE_INT: snprintf(rhs_buf, sizeof(rhs_buf), "%lld", (long long)rhs_data.int_val); break;
            case VDB_FILTER_VALUE_FLOAT: snprintf(rhs_buf, sizeof(rhs_buf), "%.10g", rhs_data.float_val); break;
            case VDB_FILTER_VALUE_BOOL: snprintf(rhs_buf, sizeof(rhs_buf), "%s", rhs_data.bool_val ? "true" : "false"); break;
            case VDB_FILTER_VALUE_STRING: snprintf(rhs_buf, sizeof(rhs_buf), "%s", rhs_data.str_val ? rhs_data.str_val : ""); break;
            default: rhs_buf[0] = '\0';
        }
        int cmp = strcmp(lhs_buf, rhs_buf);
        switch (op) {
            case VDB_FILTER_OP_EQ: return cmp == 0;
            case VDB_FILTER_OP_NE: return cmp != 0;
            case VDB_FILTER_OP_LT: return cmp < 0;
            case VDB_FILTER_OP_LE: return cmp <= 0;
            case VDB_FILTER_OP_GT: return cmp > 0;
            case VDB_FILTER_OP_GE: return cmp >= 0;
            default: return false;
        }
    }

    // 类型匹配时的比较
    switch (lhs_type) {
        case VDB_FILTER_VALUE_INT: {
            int64_t a = lhs_data.int_val;
            int64_t b = rhs_data.int_val;
            switch (op) {
                case VDB_FILTER_OP_EQ: return a == b;
                case VDB_FILTER_OP_NE: return a != b;
                case VDB_FILTER_OP_LT: return a < b;
                case VDB_FILTER_OP_LE: return a <= b;
                case VDB_FILTER_OP_GT: return a > b;
                case VDB_FILTER_OP_GE: return a >= b;
                default: return false;
            }
        }

        case VDB_FILTER_VALUE_FLOAT: {
            double a = lhs_data.float_val;
            double b = rhs_data.float_val;
            // 使用 epsilon 处理浮点比较
            const double eps = 1e-9;
            switch (op) {
                case VDB_FILTER_OP_EQ: return fabs(a - b) < eps;
                case VDB_FILTER_OP_NE: return fabs(a - b) >= eps;
                case VDB_FILTER_OP_LT: return a < b - eps;
                case VDB_FILTER_OP_LE: return a <= b + eps;
                case VDB_FILTER_OP_GT: return a > b + eps;
                case VDB_FILTER_OP_GE: return a >= b - eps;
                default: return false;
            }
        }

        case VDB_FILTER_VALUE_BOOL: {
            bool a = lhs_data.bool_val;
            bool b = rhs_data.bool_val;
            switch (op) {
                case VDB_FILTER_OP_EQ: return a == b;
                case VDB_FILTER_OP_NE: return a != b;
                default: return false;
            }
        }

        case VDB_FILTER_VALUE_STRING: {
            const char *a = lhs_data.str_val ? lhs_data.str_val : "";
            const char *b = rhs_data.str_val ? rhs_data.str_val : "";
            int cmp = strcmp(a, b);
            switch (op) {
                case VDB_FILTER_OP_EQ: return cmp == 0;
                case VDB_FILTER_OP_NE: return cmp != 0;
                case VDB_FILTER_OP_LT: return cmp < 0;
                case VDB_FILTER_OP_LE: return cmp <= 0;
                case VDB_FILTER_OP_GT: return cmp > 0;
                case VDB_FILTER_OP_GE: return cmp >= 0;
                default: return false;
            }
        }

        default:
            return false;
    }
}

static bool evaluate_comparison(FilterNode *node, FilterContext *ctx) {
    if (!node || node->type != FILTER_NODE_COMPARISON) return false;

    VDBFilterValue field_value;
    memset(&field_value, 0, sizeof(field_value));

    // 通过回调获取字段值
    if (!ctx || !ctx->get_field) return false;

    int ret = ctx->get_field(ctx->record_ctx, node->data.comparison.field_name, &field_value);
    if (ret != 0) {
        // 字段不存在，视为 NULL，不匹配
        return false;
    }

    bool result = compare_values(field_value.type, field_value.data,
                                  node->data.comparison.value.type, node->data.comparison.value.data,
                                  node->data.comparison.op);

    // 释放字段值（如果是字符串）
    if (field_value.type == VDB_FILTER_VALUE_STRING && field_value.data.str_val) {
        free(field_value.data.str_val);
    }

    return result;
}

static bool evaluate_logical(FilterNode *node, FilterContext *ctx) {
    if (!node || node->type != FILTER_NODE_LOGICAL) return false;

    switch (node->data.logical.op) {
        case VDB_FILTER_OP_AND: {
            bool left = filter_evaluate(node->data.logical.left, ctx);
            if (!left) return false;  // 短路求值
            return filter_evaluate(node->data.logical.right, ctx);
        }

        case VDB_FILTER_OP_OR: {
            bool left = filter_evaluate(node->data.logical.left, ctx);
            if (left) return true;  // 短路求值
            return filter_evaluate(node->data.logical.right, ctx);
        }

        case VDB_FILTER_OP_NOT: {
            return !filter_evaluate(node->data.logical.left, ctx);
        }

        default:
            return false;
    }
}

bool filter_evaluate(FilterNode *ast, FilterContext *ctx) {
    if (!ast) return false;

    switch (ast->type) {
        case FILTER_NODE_COMPARISON:
            return evaluate_comparison(ast, ctx);

        case FILTER_NODE_LOGICAL:
            return evaluate_logical(ast, ctx);

        case FILTER_NODE_VALUE:
            return false;

        default:
            return false;
    }
}

int filter_evaluate_batch(FilterNode *ast, FilterContext *ctx,
                          const int64_t *record_ids, int num_records,
                          bool *results, FilterStats *stats) {
    if (!ast || !results || num_records <= 0) return 0;

    int match_count = 0;

    if (stats) {
        stats->total_records = num_records;
        stats->matched_records = 0;
        stats->filtered_records = 0;
    }

    for (int i = 0; i < num_records; i++) {
        bool matched = filter_evaluate(ast, ctx);
        results[i] = matched;

        if (matched) {
            match_count++;
            if (stats) stats->matched_records++;
        } else {
            if (stats) stats->filtered_records++;
        }
    }

    return match_count;
}

/* ========================================================================
 * 工具函数
 * ======================================================================== */

const char *filter_op_to_string(VDBFilterOp op) {
    switch (op) {
        case VDB_FILTER_OP_EQ: return "=";
        case VDB_FILTER_OP_NE: return "!=";
        case VDB_FILTER_OP_LT: return "<";
        case VDB_FILTER_OP_LE: return "<=";
        case VDB_FILTER_OP_GT: return ">";
        case VDB_FILTER_OP_GE: return ">=";
        case VDB_FILTER_OP_AND: return "AND";
        case VDB_FILTER_OP_OR: return "OR";
        case VDB_FILTER_OP_NOT: return "NOT";
        default: return "?";
    }
}

const char *filter_error_to_string(FilterParseError error) {
    switch (error) {
        case FILTER_PARSE_OK: return "成功";
        case FILTER_PARSE_ERR_INVALID_TOKEN: return "无效的 token";
        case FILTER_PARSE_ERR_UNEXPECTED_TOKEN: return "意外的 token";
        case FILTER_PARSE_ERR_UNTERMINATED_STRING: return "未终止的字符串";
        case FILTER_PARSE_ERR_INVALID_FIELD: return "无效的字段名";
        case FILTER_PARSE_ERR_INVALID_OP: return "无效的操作符";
        case FILTER_PARSE_ERR_MISSING_VALUE: return "缺少值";
        case FILTER_PARSE_ERR_MISMATCHED_PAREN: return "括号不匹配";
        case FILTER_PARSE_ERR_EMPTY_EXPRESSION: return "空表达式";
        case FILTER_PARSE_ERR_OUT_OF_MEMORY: return "内存不足";
        default: return "未知错误";
    }
}

VDBFilterValue *filter_value_create_string(const char *str) {
    VDBFilterValue *value = filter_calloc(1, sizeof(VDBFilterValue));
    if (!value) return NULL;

    value->type = VDB_FILTER_VALUE_STRING;
    value->data.str_val = filter_strdup(str);
    if (!value->data.str_val) {
        free(value);
        return NULL;
    }

    return value;
}

VDBFilterValue *filter_value_create_int(int64_t val) {
    VDBFilterValue *value = filter_calloc(1, sizeof(VDBFilterValue));
    if (!value) return NULL;

    value->type = VDB_FILTER_VALUE_INT;
    value->data.int_val = val;

    return value;
}

VDBFilterValue *filter_value_create_float(double val) {
    VDBFilterValue *value = filter_calloc(1, sizeof(VDBFilterValue));
    if (!value) return NULL;

    value->type = VDB_FILTER_VALUE_FLOAT;
    value->data.float_val = val;

    return value;
}

VDBFilterValue *filter_value_create_bool(bool val) {
    VDBFilterValue *value = filter_calloc(1, sizeof(VDBFilterValue));
    if (!value) return NULL;

    value->type = VDB_FILTER_VALUE_BOOL;
    value->data.bool_val = val;

    return value;
}

void filter_value_free(VDBFilterValue *value) {
    if (!value) return;

    if (value->type == VDB_FILTER_VALUE_STRING && value->data.str_val) {
        free(value->data.str_val);
    }
    free(value);
}
