/**
 * @file sql_lexer.c
 * @brief SQL 词法分析器实现（简化版）
 */

#include "db/parser/sql/sql.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ============================================================
 * 关键字映射
 * ============================================================ */

typedef struct {
    const char *keyword;
    sql_token_type_t type;
} keyword_entry_t;

static const keyword_entry_t keywords[] = {
    {"SELECT", SQL_TOKEN_SELECT}, {"INSERT", SQL_TOKEN_INSERT},
    {"UPDATE", SQL_TOKEN_UPDATE}, {"DELETE", SQL_TOKEN_DELETE},
    {"CREATE", SQL_TOKEN_CREATE}, {"DROP", SQL_TOKEN_DROP},
    {"TABLE", SQL_TOKEN_TABLE}, {"FROM", SQL_TOKEN_FROM},
    {"WHERE", SQL_TOKEN_WHERE}, {"INTO", SQL_TOKEN_INTO},
    {"VALUES", SQL_TOKEN_VALUES}, {"SET", SQL_TOKEN_SET},
    {"AND", SQL_TOKEN_AND}, {"OR", SQL_TOKEN_OR},
    {"NOT", SQL_TOKEN_NOT}, {"IS", SQL_TOKEN_IS},
    {"IN", SQL_TOKEN_IN}, {"LIKE", SQL_TOKEN_LIKE},
    {"BETWEEN", SQL_TOKEN_BETWEEN},
    {"NULL", SQL_TOKEN_NULL}, {"PRIMARY", SQL_TOKEN_PRIMARY},
    {"KEY", SQL_TOKEN_KEY}, {"INT", SQL_TOKEN_INT_TYPE},
    {"INTEGER", SQL_TOKEN_INT_TYPE}, {"VARCHAR", SQL_TOKEN_VARCHAR},
    {"TEXT", SQL_TOKEN_TEXT}, {"BLOB", SQL_TOKEN_BLOB},
    {"TRUE", SQL_TOKEN_TRUE}, {"FALSE", SQL_TOKEN_FALSE},
};
static const size_t num_keywords = sizeof(keywords) / sizeof(keywords[0]);

/* ============================================================
 * 词法分析器结构
 * ============================================================ */

struct sql_lexer_s {
    const char *input;
    size_t input_len;
    size_t pos;
    size_t line;
    size_t column;

    sql_token_t token;  /* 当前 token */
    bool valid;
};

/* ============================================================
 * 工具函数
 * ============================================================ */

static char cur(const sql_lexer_t *l) {
    return l->pos < l->input_len ? l->input[l->pos] : '\0';
}

static char peek(const sql_lexer_t *l) {
    return l->pos + 1 < l->input_len ? l->input[l->pos + 1] : '\0';
}

static char next(sql_lexer_t *l) {
    char c = cur(l);
    if (c) {
        l->pos++;
        if (c == '\n') { l->line++; l->column = 1; }
        else l->column++;
    }
    return c;
}

static void skip_ws(sql_lexer_t *l) {
    while (isspace((unsigned char)cur(l))) next(l);
}

static void skip_comment(sql_lexer_t *l) {
    if (cur(l) == '-' && peek(l) == '-') {
        while (cur(l) && cur(l) != '\n') next(l);
    }
}

static void skip_trivia(sql_lexer_t *l) {
    skip_ws(l);
    skip_comment(l);
    skip_ws(l);
}

static sql_token_type_t lookup_kw(const char *s, size_t len) {
    char buf[64];
    if (len >= sizeof(buf)) return SQL_TOKEN_IDENT;
    for (size_t i = 0; i < len; i++) buf[i] = toupper((unsigned char)s[i]);
    buf[len] = '\0';
    for (size_t i = 0; i < num_keywords; i++) {
        if (strcmp(buf, keywords[i].keyword) == 0) return keywords[i].type;
    }
    return SQL_TOKEN_IDENT;
}

/* ============================================================
 * Token 扫描
 * ============================================================ */

static void scan_token(sql_lexer_t *l) {
    skip_trivia(l);

    l->token.type = SQL_TOKEN_EOF;
    l->token.lexeme = NULL;
    l->token.line = l->line;
    l->token.column = l->column;

    char c = cur(l);
    if (!c) return;

    size_t start = l->pos;

    /* 标识符或关键字 */
    if (isalpha((unsigned char)c) || c == '_') {
        next(l);
        while (isalnum((unsigned char)cur(l)) || cur(l) == '_') next(l);
        size_t len = l->pos - start;
        l->token.lexeme = (char *)malloc(len + 1);
        memcpy(l->token.lexeme, l->input + start, len);
        l->token.lexeme[len] = '\0';
        l->token.type = lookup_kw(l->token.lexeme, len);
        return;
    }

    /* 数字 */
    if (isdigit((unsigned char)c)) {
        next(l);
        while (isdigit((unsigned char)cur(l))) next(l);

        /* 检查是否有小数点或指数 */
        bool is_float = false;
        size_t float_start = l->pos;

        if (cur(l) == '.') {
            is_float = true;
            next(l);
            /* 跳过小数部分数字 */
            while (isdigit((unsigned char)cur(l))) next(l);
        }

        /* 检查指数部分 */
        if (cur(l) == 'e' || cur(l) == 'E') {
            is_float = true;
            next(l);
            /* 可选的正负号 */
            if (cur(l) == '+' || cur(l) == '-') next(l);
            /* 必须有数字 */
            if (isdigit((unsigned char)cur(l))) {
                while (isdigit((unsigned char)cur(l))) next(l);
            } else {
                /* 无效的指数格式，回退 */
                l->pos = float_start + (is_float && cur(l) == '.' ? 1 : 0);
            }
        }

        size_t len = l->pos - start;
        l->token.lexeme = (char *)malloc(len + 1);
        memcpy(l->token.lexeme, l->input + start, len);
        l->token.lexeme[len] = '\0';

        if (is_float) {
            l->token.type = SQL_TOKEN_FLOAT;
            l->token.float_value = atof(l->token.lexeme);
        } else {
            l->token.type = SQL_TOKEN_INT;
            l->token.int_value = atoi(l->token.lexeme);
        }
        return;
    }

    /* 字符串 */
    if (c == '\'' || c == '"') {
        char quote = next(l);
        start = l->pos;
        while (cur(l) && cur(l) != quote) {
            if (cur(l) == '\\' && peek(l)) next(l);
            next(l);
        }
        size_t len;
        if (cur(l) == quote) {
            next(l);
            len = l->pos - start - 1;
        } else {
            /* 未闭合的字符串，返回 ERROR token */
            l->token.type = SQL_TOKEN_ERROR;
            return;
        }
        l->token.lexeme = (char *)malloc(len + 1);
        memcpy(l->token.lexeme, l->input + start, len);
        l->token.lexeme[len] = '\0';
        l->token.type = SQL_TOKEN_STRING;
        return;
    }

    /* 操作符 */
    next(l);
    switch (c) {
        case '=': l->token.type = SQL_TOKEN_EQ; break;
        case '!':
            if (cur(l) == '=') { next(l); l->token.type = SQL_TOKEN_NE; }
            else l->token.type = SQL_TOKEN_ERROR;
            break;
        case '<':
            if (cur(l) == '=') { next(l); l->token.type = SQL_TOKEN_LE; }
            else if (cur(l) == '>') { next(l); l->token.type = SQL_TOKEN_NE; }
            else l->token.type = SQL_TOKEN_LT;
            break;
        case '>':
            if (cur(l) == '=') { next(l); l->token.type = SQL_TOKEN_GE; }
            else l->token.type = SQL_TOKEN_GT;
            break;
        case '+': l->token.type = SQL_TOKEN_PLUS; break;
        case '-': l->token.type = SQL_TOKEN_MINUS; break;
        case '*': l->token.type = SQL_TOKEN_STAR; break;
        case '/': l->token.type = SQL_TOKEN_SLASH; break;
        case '(': l->token.type = SQL_TOKEN_LPAREN; break;
        case ')': l->token.type = SQL_TOKEN_RPAREN; break;
        case ',': l->token.type = SQL_TOKEN_COMMA; break;
        case ';': l->token.type = SQL_TOKEN_SEMICOLON; break;
        default: l->token.type = SQL_TOKEN_ERROR; break;
    }
}

/* ============================================================
 * 公共 API
 * ============================================================ */

sql_lexer_t *sql_lexer_create(const char *input) {
    if (!input) return NULL;
    sql_lexer_t *l = (sql_lexer_t *)calloc(1, sizeof(sql_lexer_t));
    if (!l) return NULL;
    l->input = input;
    l->input_len = strlen(input);
    l->line = 1; l->column = 1;
    scan_token(l);
    return l;
}

void sql_lexer_destroy(sql_lexer_t *l) {
    if (l) {
        /* lexeme 由 advance/parser 负责释放 */
        free(l);
    }
}

sql_token_t *sql_lexer_next(sql_lexer_t *l) {
    if (!l) return NULL;
    /* 注意：lexeme 由调用者负责释放，这里不再释放 */
    scan_token(l);
    return &l->token;
}

sql_token_t *sql_lexer_peek(sql_lexer_t *l) {
    return l ? &l->token : NULL;
}

void sql_lexer_backup(sql_lexer_t *l) {
    (void)l;  /* 简化版不需要 backup */
}

const char *sql_lexer_errmsg(sql_lexer_t *l) {
    return l ? "OK" : "Invalid lexer";
}