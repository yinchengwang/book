/**
 * @file sql_parser.c
 * @brief SQL 解析器实现
 */

#include "db/sql/sql_parser.h"
#include "db/parser/sql/parsenodes.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 定义 SqlParser 类型（与 sql_parser.h 中的 void * 兼容） */
typedef struct SqlParser_s {
    void *lexer;
    void *current;
    bool has_current;
    char error_msg[256];
    int error_location;
    int recovery_mode;
} SqlParser;

/* 前向声明 */
static void *parse_expression(void *parser);
static void *parse_select(void *parser);

/* ========================================================================
 * 关键字表
 * ======================================================================== */

static const SqlKeyword g_keywords[] = {
    /* DML */
    {"SELECT", TOKEN_SELECT, 6},
    {"FROM", TOKEN_FROM, 4},
    {"WHERE", TOKEN_WHERE, 5},
    {"AND", TOKEN_AND, 3},
    {"OR", TOKEN_OR, 2},
    {"NOT", TOKEN_NOT, 3},
    {"IN", TOKEN_IN, 2},
    {"BETWEEN", TOKEN_BETWEEN, 7},
    {"LIKE", TOKEN_LIKE, 4},
    {"IS", TOKEN_IS, 2},
    {"EXISTS", TOKEN_EXISTS, 6},

    /* JOIN */
    {"JOIN", TOKEN_JOIN, 4},
    {"LEFT", TOKEN_LEFT, 4},
    {"RIGHT", TOKEN_RIGHT, 5},
    {"FULL", TOKEN_FULL, 4},
    {"INNER", TOKEN_INNER, 5},
    {"OUTER", TOKEN_OUTER, 5},
    {"CROSS", TOKEN_CROSS, 5},
    {"ON", TOKEN_ON, 2},
    {"USING", TOKEN_USING, 5},

    /* DML */
    {"INSERT", TOKEN_INSERT, 6},
    {"INTO", TOKEN_INTO, 4},
    {"VALUES", TOKEN_VALUES, 6},
    {"UPDATE", TOKEN_UPDATE, 6},
    {"SET", TOKEN_SET, 3},
    {"DELETE", TOKEN_DELETE, 6},

    /* DDL */
    {"CREATE", TOKEN_CREATE, 6},
    {"DROP", TOKEN_DROP, 4},
    {"ALTER", TOKEN_ALTER, 5},
    {"TABLE", TOKEN_TABLE, 5},
    {"INDEX", TOKEN_INDEX, 5},
    {"VIEW", TOKEN_VIEW, 4},
    {"MATERIALIZED", TOKEN_MATERIALIZED, 11},
    {"AS", TOKEN_AS, 2},
    {"IF", TOKEN_IF, 2},
    {"EXISTS", TOKEN_EXISTS, 6},

    /* 子句 */
    {"GROUP", TOKEN_GROUP, 5},
    {"BY", TOKEN_BY, 2},
    {"HAVING", TOKEN_HAVING, 6},
    {"ORDER", TOKEN_ORDER, 5},
    {"ASC", TOKEN_ASC, 3},
    {"DESC", TOKEN_DESC, 4},
    {"LIMIT", TOKEN_LIMIT, 5},
    {"OFFSET", TOKEN_OFFSET, 6},
    {"UNION", TOKEN_UNION, 5},
    {"INTERSECT", TOKEN_INTERSECT, 9},
    {"EXCEPT", TOKEN_EXCEPT, 6},

    /* 聚合 */
    {"COUNT", TOKEN_COUNT, 5},
    {"SUM", TOKEN_SUM, 3},
    {"AVG", TOKEN_AVG, 3},
    {"MIN", TOKEN_MIN, 3},
    {"MAX", TOKEN_MAX, 3},
    {"DISTINCT", TOKEN_PARAMETER, 7},
    {"ALL", TOKEN_PARAMETER, 3},

    /* 窗口 */
    {"WINDOW", TOKEN_WINDOW, 6},
    {"OVER", TOKEN_OVER, 4},
    {"PARTITION", TOKEN_PARTITION, 9},

    /* 类型 */
    {"INT", TOKEN_INT, 3},
    {"INT2", TOKEN_INT2, 4},
    {"INT4", TOKEN_INT4, 4},
    {"INT8", TOKEN_INT8, 4},
    {"INTEGER", TOKEN_INT, 7},
    {"BIGINT", TOKEN_INT8, 6},
    {"SMALLINT", TOKEN_INT2, 8},
    {"REAL", TOKEN_FLOAT, 4},
    {"FLOAT", TOKEN_FLOAT, 5},
    {"FLOAT4", TOKEN_FLOAT4, 6},
    {"FLOAT8", TOKEN_FLOAT8, 6},
    {"DOUBLE", TOKEN_FLOAT8, 6},
    {"NUMERIC", TOKEN_NUMERIC, 7},
    {"DECIMAL", TOKEN_DECIMAL, 7},
    {"VARCHAR", TOKEN_VARCHAR, 7},
    {"CHAR", TOKEN_CHAR, 4},
    {"CHARACTER", TOKEN_CHAR, 9},
    {"TEXT", TOKEN_TEXT, 4},
    {"STRING", TOKEN_TEXT, 6},
    {"BLOB", TOKEN_BLOB, 4},
    {"DATE", TOKEN_DATE, 4},
    {"TIME", TOKEN_TIME, 4},
    {"TIMESTAMP", TOKEN_TIMESTAMP, 9},
    {"TIMESTAMPTZ", TOKEN_TIMESTAMPTZ, 11},
    {"INTERVAL", TOKEN_INTERVAL, 8},
    {"BOOLEAN", TOKEN_BOOLEAN, 7},
    {"BOOL", TOKEN_BOOLEAN, 4},

    /* 约束 */
    {"PRIMARY", TOKEN_PRIMARY, 6},
    {"KEY", TOKEN_KEY, 3},
    {"FOREIGN", TOKEN_FOREIGN, 7},
    {"REFERENCES", TOKEN_REFERENCES, 10},
    {"UNIQUE", TOKEN_UNIQUE, 6},
    {"CHECK", TOKEN_CHECK, 5},
    {"DEFAULT", TOKEN_DEFAULT, 7},
    {"NULL", TOKEN_NULL_KW, 4},
    {"CONSTRAINT", TOKEN_CONSTRAINT, 10},
    {"AUTO_INCREMENT", TOKEN_AUTO_INCREMENT, 13},
    {"SERIAL", TOKEN_AUTO_INCREMENT, 6},

    /* 事务 */
    {"BEGIN", TOKEN_BEGIN, 5},
    {"COMMIT", TOKEN_COMMIT, 6},
    {"ROLLBACK", TOKEN_ROLLBACK, 8},
    {"SAVEPOINT", TOKEN_SAVEPOINT, 9},
    {"RELEASE", TOKEN_RELEASE, 7},
    {"TRANSACTION", TOKEN_TRANSACTION, 11},
    {"ISOLATION", TOKEN_ISOLATION, 9},
    {"LEVEL", TOKEN_LEVEL, 5},
    {"READ", TOKEN_READ, 4},
    {"WRITE", TOKEN_WRITE, 5},
    {"SERIALIZABLE", TOKEN_SERIALIZABLE, 12},
    {"REPEATABLE", TOKEN_REPEATABLE, 10},
    {"COMMITTED", TOKEN_COMMITTED, 9},
    {"UNCOMMITTED", TOKEN_UNCOMMITTED, 11},

    /* 其他关键字 */
    {"WITH", TOKEN_WITH, 4},
    {"RECURSIVE", TOKEN_RECURSIVE, 9},
    {"CASCADE", TOKEN_CASCADE, 7},
    {"RESTRICT", TOKEN_RESTRICT, 8},
    {"GRANT", TOKEN_GRANT, 5},
    {"REVOKE", TOKEN_REVOKE, 6},
    {"TO", TOKEN_TO, 2},
    {"USER", TOKEN_USER, 4},
    {"ROLE", TOKEN_ROLE, 4},
    {"DATABASE", TOKEN_DATABASE, 8},
    {"SCHEMA", TOKEN_SCHEMA, 6},
    {"CATALOG", TOKEN_CATALOG, 7},
    {"TRUNCATE", TOKEN_DELETE, 8},

    /* 向量 & 时序 */
    {"VECTOR", TOKEN_VECTOR, 6},
    {"EMBEDDING", TOKEN_EMBEDDING, 9},
    {"SIMILARITY", TOKEN_SIMILARITY, 10},
    {"DISTANCE", TOKEN_DISTANCE, 8},
    {"METRIC", TOKEN_METRIC, 6},

    /* 时序 */
    {"TIMESERIES", TOKEN_TIMESERIES, 10},
    {"TIMESTAMP_", TOKEN_TIMESTAMP_, 10},
    {"TAG", TOKEN_TAG, 3},
    {"FIELD", TOKEN_FIELD, 5},
    {"RETENTION", TOKEN_RETENTION, 9},

    /* 物化视图 */
    {"REFRESH", TOKEN_REFRESH, 7},
    {"COMPLETE", TOKEN_COMPLETE, 8},
    {"FAST", TOKEN_FAST, 4},
    {"CONCURRENTLY", TOKEN_CONCURRENTLY, 11},

    /* ALTER TABLE */
    {"ADD", TOKEN_ADD, 3},
    {"COLUMN", TOKEN_COLUMN, 6},
    {"DATA", TOKEN_DATA, 4},
    {"RENAME", TOKEN_RENAME, 6},
    {"TYPE", TOKEN_TYPE_KW, 4}
};

#define KEYWORD_COUNT (sizeof(g_keywords) / sizeof(g_keywords[0]))

/* ========================================================================
 * 关键字查找
 * ======================================================================== */

/**
 * @brief 二分查找关键字
 */
static SqlTokenType keyword_lookup(const char *text, int len)
{
    int low = 0, high = KEYWORD_COUNT - 1;

    while (low <= high) {
        int mid = (low + high) / 2;
        const SqlKeyword *kw = &g_keywords[mid];
        int cmp = strncmp(text, kw->keyword, len);
        if (cmp == 0 && (int)strlen(kw->keyword) == len) {
            return kw->token;
        }
        if (cmp < 0) {
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }
    return TOKEN_IDENTIFIER;
}

/* ========================================================================
 * 词法分析器实现
 * ======================================================================== */

/**
 * @brief 跳过空白字符
 */
static void skip_whitespace(SqlLexer *lexer)
{
    while (lexer->pos < lexer->input_len) {
        char c = lexer->input[lexer->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (c == '\n') {
                lexer->line++;
                lexer->col = 0;
            } else {
                lexer->col++;
            }
            lexer->pos++;
        } else if (c == '-') {
            /* 检查注释 -- */
            if (lexer->pos + 1 < lexer->input_len &&
                lexer->input[lexer->pos + 1] == '-') {
                /* 单行注释 */
                while (lexer->pos < lexer->input_len &&
                       lexer->input[lexer->pos] != '\n') {
                    lexer->pos++;
                }
            } else {
                break;
            }
        } else if (c == '/') {
            /* 检查注释 /* ... */
            if (lexer->pos + 1 < lexer->input_len &&
                lexer->input[lexer->pos + 1] == '*') {
                lexer->pos += 2;
                while (lexer->pos + 1 < lexer->input_len) {
                    if (lexer->input[lexer->pos] == '*' &&
                        lexer->input[lexer->pos + 1] == '/') {
                        lexer->pos += 2;
                        break;
                    }
                    if (lexer->input[lexer->pos] == '\n') {
                        lexer->line++;
                        lexer->col = 0;
                    }
                    lexer->pos++;
                }
            } else {
                break;
            }
        } else {
            break;
        }
    }
}

/**
 * @brief 读取标识符
 */
static SqlToken read_identifier(SqlLexer *lexer)
{
    int start = lexer->pos;
    int start_col = lexer->col;

    /* 跳过字母、数字和下划线 */
    while (lexer->pos < lexer->input_len) {
        char c = lexer->input[lexer->pos];
        if (isalnum(c) || c == '_') {
            lexer->pos++;
            lexer->col++;
        } else {
            break;
        }
    }

    int len = lexer->pos - start;
    const char *text = &lexer->input[start];

    SqlToken token;
    token.type = keyword_lookup(text, len);
    token.lexeme = text;
    token.lexeme_len = len;
    token.location = start_col;

    /* 如果是关键字，存储原文以便后续处理 */
    if (token.type == TOKEN_IDENTIFIER || token.type == TOKEN_PARAMETER) {
        /* 可能是 DISTINCT, ALL 等 */
        if (strncasecmp(text, "distinct", len) == 0) {
            token.type = TOKEN_PARAMETER;
        } else if (strncasecmp(text, "all", len) == 0) {
            token.type = TOKEN_PARAMETER;
        }
    }

    return token;
}

/**
 * @brief 读取数字
 */
static SqlToken read_number(SqlLexer *lexer)
{
    int start = lexer->pos;
    int start_col = lexer->col;
    bool is_float = false;

    /* 整数部分 */
    while (lexer->pos < lexer->input_len && isdigit(lexer->input[lexer->pos])) {
        lexer->pos++;
        lexer->col++;
    }

    /* 小数部分 */
    if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '.') {
        is_float = true;
        lexer->pos++;
        lexer->col++;
        while (lexer->pos < lexer->input_len &&
               isdigit(lexer->input[lexer->pos])) {
            lexer->pos++;
            lexer->col++;
        }
    }

    /* 指数部分 */
    if (lexer->pos < lexer->input_len &&
        (lexer->input[lexer->pos] == 'e' || lexer->input[lexer->pos] == 'E')) {
        is_float = true;
        lexer->pos++;
        lexer->col++;
        if (lexer->pos < lexer->input_len &&
            (lexer->input[lexer->pos] == '+' || lexer->input[lexer->pos] == '-')) {
            lexer->pos++;
            lexer->col++;
        }
        while (lexer->pos < lexer->input_len &&
               isdigit(lexer->input[lexer->pos])) {
            lexer->pos++;
            lexer->col++;
        }
    }

    int len = lexer->pos - start;
    const char *text = &lexer->input[start];

    SqlToken token;
    token.lexeme = text;
    token.lexeme_len = len;
    token.location = start_col;

    if (is_float) {
        token.type = TOKEN_FLOAT;
        token.val.fval = atof(text);
    } else {
        token.type = TOKEN_INTEGER;
        token.val.ival = atoll(text);
    }

    return token;
}

/**
 * @brief 读取字符串
 */
static SqlToken read_string(SqlLexer *lexer, char quote)
{
    int start = lexer->pos;
    int start_col = lexer->col;

    lexer->pos++;  /* 跳过开始引号 */
    lexer->col++;

    /* 寻找结束引号 */
    while (lexer->pos < lexer->input_len) {
        char c = lexer->input[lexer->pos];
        if (c == quote) {
            /* 检查转义 */
            if (lexer->pos + 1 < lexer->input_len &&
                lexer->input[lexer->pos + 1] == quote) {
                lexer->pos += 2;
                lexer->col += 2;
                continue;
            }
            break;
        }
        if (c == '\n') {
            lexer->line++;
            lexer->col = 0;
        }
        lexer->pos++;
        lexer->col++;
    }

    int content_start = start + 1;
    int content_len = lexer->pos - content_start;

    if (lexer->pos < lexer->input_len) {
        lexer->pos++;  /* 跳过结束引号 */
        lexer->col++;
    }

    SqlToken token;
    token.type = TOKEN_STRING;
    token.lexeme = &lexer->input[content_start];
    token.lexeme_len = content_len;
    token.location = start_col;
    token.val.sval.str = (char *)&lexer->input[content_start];
    token.val.sval.len = content_len;

    return token;
}

/**
 * @brief 读取操作符
 */
static SqlToken read_operator(SqlLexer *lexer)
{
    int start_col = lexer->col;
    char c = lexer->input[lexer->pos];
    lexer->pos++;
    lexer->col++;

    SqlToken token;
    token.lexeme = &lexer->input[start_col];
    token.location = start_col;

    switch (c) {
        case '+':
            token.type = TOKEN_PLUS;
            token.lexeme_len = 1;
            break;
        case '-':
            token.type = TOKEN_MINUS;
            token.lexeme_len = 1;
            break;
        case '*':
            token.type = TOKEN_STAR;
            token.lexeme_len = 1;
            break;
        case '/':
            token.type = TOKEN_SLASH;
            token.lexeme_len = 1;
            break;
        case '%':
            token.type = TOKEN_PERCENT;
            token.lexeme_len = 1;
            break;
        case '=':
            token.type = TOKEN_EQ;
            token.lexeme_len = 1;
            break;
        case '<':
            if (lexer->pos < lexer->input_len) {
                char n = lexer->input[lexer->pos];
                if (n == '=') {
                    token.type = TOKEN_LTE;
                    token.lexeme_len = 2;
                    lexer->pos++;
                    lexer->col++;
                } else if (n == '>') {
                    token.type = TOKEN_NEQ;
                    token.lexeme_len = 2;
                    lexer->pos++;
                    lexer->col++;
                } else if (n == '<') {
                    token.type = TOKEN_PARAMETER;
                    token.lexeme_len = 2;
                    lexer->pos++;
                    lexer->col++;
                } else {
                    token.type = TOKEN_LT;
                    token.lexeme_len = 1;
                }
            } else {
                token.type = TOKEN_LT;
                token.lexeme_len = 1;
            }
            break;
        case '>':
            if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '=') {
                token.type = TOKEN_GTE;
                token.lexeme_len = 2;
                lexer->pos++;
                lexer->col++;
            } else if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '>') {
                token.type = TOKEN_PARAMETER;
                token.lexeme_len = 2;
                lexer->pos++;
                lexer->col++;
            } else {
                token.type = TOKEN_GT;
                token.lexeme_len = 1;
            }
            break;
        case '!':
            if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '=') {
                token.type = TOKEN_NEQ;
                token.lexeme_len = 2;
                lexer->pos++;
                lexer->col++;
            } else {
                token.type = TOKEN_ERROR;
                token.lexeme_len = 1;
            }
            break;
        case '(':
            token.type = TOKEN_LPAREN;
            token.lexeme_len = 1;
            break;
        case ')':
            token.type = TOKEN_RPAREN;
            token.lexeme_len = 1;
            break;
        case '[':
            token.type = TOKEN_LBRACKET;
            token.lexeme_len = 1;
            break;
        case ']':
            token.type = TOKEN_RBRACKET;
            token.lexeme_len = 1;
            break;
        case '{':
            token.type = TOKEN_LBRACE;
            token.lexeme_len = 1;
            break;
        case '}':
            token.type = TOKEN_RBRACE;
            token.lexeme_len = 1;
            break;
        case ',':
            token.type = TOKEN_COMMA;
            token.lexeme_len = 1;
            break;
        case ';':
            token.type = TOKEN_SEMICOLON;
            token.lexeme_len = 1;
            break;
        case ':':
            token.type = TOKEN_COLON;
            token.lexeme_len = 1;
            break;
        case '.':
            if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '.') {
                token.type = TOKEN_DOT_DOT;
                token.lexeme_len = 2;
                lexer->pos++;
                lexer->col++;
            } else {
                token.type = TOKEN_DOT;
                token.lexeme_len = 1;
            }
            break;
        case '&':
            token.type = TOKEN_PARAMETER;
            token.lexeme_len = 1;
            break;
        case '|':
            if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == '|') {
                token.type = TOKEN_CONCAT;
                token.lexeme_len = 2;
                lexer->pos++;
                lexer->col++;
            } else {
                token.type = TOKEN_PARAMETER;
                token.lexeme_len = 1;
            }
            break;
        case '^':
            token.type = TOKEN_PARAMETER;
            token.lexeme_len = 1;
            break;
        case '~':
            token.type = TOKEN_LIKE;
            token.lexeme_len = 1;
            break;
        case '@':
            token.type = TOKEN_PARAMETER;
            token.lexeme_len = 1;
            break;
        case '#':
            token.type = TOKEN_PARAMETER;
            token.lexeme_len = 1;
            break;
        case '?':
            token.type = TOKEN_PARAMETER;
            token.lexeme_len = 1;
            break;
        case '$':
            /* 参数占位符 */
            {
                int start = lexer->pos;
                while (lexer->pos < lexer->input_len &&
                       isdigit(lexer->input[lexer->pos])) {
                    lexer->pos++;
                    lexer->col++;
                }
                token.type = TOKEN_PARAMETER;
                token.lexeme = &lexer->input[start - 1];
                token.lexeme_len = lexer->pos - start + 1;
            }
            break;
        case '`':
            return read_string(lexer, '`');
        default:
            token.type = TOKEN_ERROR;
            token.lexeme_len = 1;
            break;
    }

    return token;
}

/**
 * @brief 读取双字符操作符
 */
static SqlToken read_two_char_op(SqlLexer *lexer, char first, SqlTokenType two,
                                 SqlTokenType single)
{
    if (lexer->pos < lexer->input_len && lexer->input[lexer->pos] == first) {
        lexer->pos++;
        lexer->col++;
        SqlToken token;
        token.type = two;
        token.lexeme = &lexer->input[lexer->pos - 2];
        token.lexeme_len = 2;
        token.location = lexer->col - 2;
        return token;
    }

    SqlToken token;
    token.type = single;
    token.lexeme = &lexer->input[lexer->pos - 1];
    token.lexeme_len = 1;
    token.location = lexer->col - 1;
    return token;
}

/* ========================================================================
 * 词法分析器 API
 * ======================================================================== */

void sql_lexer_init(SqlLexer *lexer, const char *input, int len)
{
    lexer->input = input;
    lexer->input_len = len >= 0 ? len : (int)strlen(input);
    lexer->pos = 0;
    lexer->line = 1;
    lexer->col = 0;
    lexer->has_current = false;
    memset(&lexer->current_token, 0, sizeof(lexer->current_token));
}

SqlToken sql_lexer_next(SqlLexer *lexer)
{
    if (lexer->has_current) {
        lexer->has_current = false;
        return lexer->current_token;
    }

    skip_whitespace(lexer);

    if (lexer->pos >= lexer->input_len) {
        SqlToken token;
        token.type = TOKEN_EOF;
        token.lexeme = "";
        token.lexeme_len = 0;
        token.location = lexer->col;
        return token;
    }

    char c = lexer->input[lexer->pos];

    if (isalpha(c) || c == '_') {
        return read_identifier(lexer);
    }

    if (isdigit(c)) {
        return read_number(lexer);
    }

    if (c == '\'' || c == '"') {
        return read_string(lexer, c);
    }

    return read_operator(lexer);
}

SqlToken sql_lexer_peek(SqlLexer *lexer)
{
    if (!lexer->has_current) {
        lexer->current_token = sql_lexer_next(lexer);
        lexer->has_current = true;
    }
    return lexer->current_token;
}

bool sql_lexer_expect(SqlLexer *lexer, SqlTokenType type)
{
    SqlToken token = sql_lexer_next(lexer);
    if (token.type != type) {
        lexer->has_current = true;
        lexer->current_token = token;
        return false;
    }
    return true;
}

bool sql_lexer_match(SqlLexer *lexer, SqlTokenType type)
{
    if (sql_lexer_check(lexer, type)) {
        sql_lexer_next(lexer);
        return true;
    }
    return false;
}

bool sql_lexer_check(SqlLexer *lexer, SqlTokenType type)
{
    SqlToken token = sql_lexer_peek(lexer);
    return token.type == type;
}

const char *sql_token_name(SqlTokenType type)
{
    switch (type) {
        /* 字面量 */
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_NULL: return "NULL";
        case TOKEN_BOOL: return "BOOLEAN";
        /* 标识符 */
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_PARAMETER: return "PARAMETER";
        case TOKEN_WILDCARD: return "*";
        /* 关键字 */
        case TOKEN_SELECT: return "SELECT";
        case TOKEN_FROM: return "FROM";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_IN: return "IN";
        case TOKEN_BETWEEN: return "BETWEEN";
        case TOKEN_LIKE: return "LIKE";
        case TOKEN_IS: return "IS";
        case TOKEN_EXISTS: return "EXISTS";
        case TOKEN_JOIN: return "JOIN";
        case TOKEN_LEFT: return "LEFT";
        case TOKEN_RIGHT: return "RIGHT";
        case TOKEN_FULL: return "FULL";
        case TOKEN_INNER: return "INNER";
        case TOKEN_OUTER: return "OUTER";
        case TOKEN_CROSS: return "CROSS";
        case TOKEN_ON: return "ON";
        case TOKEN_USING: return "USING";
        case TOKEN_INSERT: return "INSERT";
        case TOKEN_INTO: return "INTO";
        case TOKEN_VALUES: return "VALUES";
        case TOKEN_UPDATE: return "UPDATE";
        case TOKEN_SET: return "SET";
        case TOKEN_DELETE: return "DELETE";
        case TOKEN_CREATE: return "CREATE";
        case TOKEN_DROP: return "DROP";
        case TOKEN_ALTER: return "ALTER";
        case TOKEN_TABLE: return "TABLE";
        case TOKEN_INDEX: return "INDEX";
        case TOKEN_VIEW: return "VIEW";
        case TOKEN_MATERIALIZED: return "MATERIALIZED";
        case TOKEN_AS: return "AS";
        case TOKEN_IF: return "IF";
        case TOKEN_GROUP: return "GROUP";
        case TOKEN_BY: return "BY";
        case TOKEN_HAVING: return "HAVING";
        case TOKEN_ORDER: return "ORDER";
        case TOKEN_ASC: return "ASC";
        case TOKEN_DESC: return "DESC";
        case TOKEN_LIMIT: return "LIMIT";
        case TOKEN_OFFSET: return "OFFSET";
        case TOKEN_UNION: return "UNION";
        case TOKEN_INTERSECT: return "INTERSECT";
        case TOKEN_EXCEPT: return "EXCEPT";
        case TOKEN_COUNT: return "COUNT";
        case TOKEN_SUM: return "SUM";
        case TOKEN_AVG: return "AVG";
        case TOKEN_MIN: return "MIN";
        case TOKEN_MAX: return "MAX";
        case TOKEN_WINDOW: return "WINDOW";
        case TOKEN_OVER: return "OVER";
        case TOKEN_PARTITION: return "PARTITION";
        case TOKEN_INT: return "INT";
        case TOKEN_FLOAT4: return "FLOAT4";
        case TOKEN_FLOAT8: return "FLOAT8";
        case TOKEN_NUMERIC: return "NUMERIC";
        case TOKEN_DECIMAL: return "DECIMAL";
        case TOKEN_VARCHAR: return "VARCHAR";
        case TOKEN_CHAR: return "CHAR";
        case TOKEN_TEXT: return "TEXT";
        case TOKEN_BLOB: return "BLOB";
        case TOKEN_DATE: return "DATE";
        case TOKEN_TIME: return "TIME";
        case TOKEN_TIMESTAMP: return "TIMESTAMP";
        case TOKEN_TIMESTAMPTZ: return "TIMESTAMPTZ";
        case TOKEN_INTERVAL: return "INTERVAL";
        case TOKEN_BOOLEAN: return "BOOLEAN";
        case TOKEN_PRIMARY: return "PRIMARY";
        case TOKEN_KEY: return "KEY";
        case TOKEN_FOREIGN: return "FOREIGN";
        case TOKEN_REFERENCES: return "REFERENCES";
        case TOKEN_UNIQUE: return "UNIQUE";
        case TOKEN_CHECK: return "CHECK";
        case TOKEN_DEFAULT: return "DEFAULT";
        case TOKEN_NULL_KW: return "NULL";
        case TOKEN_CONSTRAINT: return "CONSTRAINT";
        case TOKEN_AUTO_INCREMENT: return "AUTO_INCREMENT";
        case TOKEN_BEGIN: return "BEGIN";
        case TOKEN_COMMIT: return "COMMIT";
        case TOKEN_ROLLBACK: return "ROLLBACK";
        case TOKEN_SAVEPOINT: return "SAVEPOINT";
        case TOKEN_RELEASE: return "RELEASE";
        case TOKEN_TRANSACTION: return "TRANSACTION";
        case TOKEN_ISOLATION: return "ISOLATION";
        case TOKEN_LEVEL: return "LEVEL";
        case TOKEN_READ: return "READ";
        case TOKEN_WRITE: return "WRITE";
        case TOKEN_SERIALIZABLE: return "SERIALIZABLE";
        case TOKEN_REPEATABLE: return "REPEATABLE";
        case TOKEN_COMMITTED: return "COMMITTED";
        case TOKEN_UNCOMMITTED: return "UNCOMMITTED";
        case TOKEN_WITH: return "WITH";
        case TOKEN_RECURSIVE: return "RECURSIVE";
        case TOKEN_CASCADE: return "CASCADE";
        case TOKEN_RESTRICT: return "RESTRICT";
        case TOKEN_GRANT: return "GRANT";
        case TOKEN_REVOKE: return "REVOKE";
        case TOKEN_TO: return "TO";
        case TOKEN_USER: return "USER";
        case TOKEN_ROLE: return "ROLE";
        case TOKEN_DATABASE: return "DATABASE";
        case TOKEN_SCHEMA: return "SCHEMA";
        case TOKEN_CATALOG: return "CATALOG";
        case TOKEN_VECTOR: return "VECTOR";
        case TOKEN_EMBEDDING: return "EMBEDDING";
        case TOKEN_SIMILARITY: return "SIMILARITY";
        case TOKEN_DISTANCE: return "DISTANCE";
        case TOKEN_METRIC: return "METRIC";
        case TOKEN_TIMESERIES: return "TIMESERIES";
        case TOKEN_TIMESTAMP_: return "TIMESTAMP";
        case TOKEN_TAG: return "TAG";
        case TOKEN_FIELD: return "FIELD";
        case TOKEN_RETENTION: return "RETENTION";
        case TOKEN_REFRESH: return "REFRESH";
        case TOKEN_COMPLETE: return "COMPLETE";
        case TOKEN_FAST: return "FAST";
        case TOKEN_CONCURRENTLY: return "CONCURRENTLY";
        case TOKEN_ADD: return "ADD";
        case TOKEN_COLUMN: return "COLUMN";
        case TOKEN_DATA: return "DATA";
        case TOKEN_RENAME: return "RENAME";
        case TOKEN_TYPE_KW: return "TYPE";
        /* 操作符 */
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_EQ: return "=";
        case TOKEN_NEQ: return "<>";
        case TOKEN_LT: return "<";
        case TOKEN_GT: return ">";
        case TOKEN_LTE: return "<=";
        case TOKEN_GTE: return ">=";
        case TOKEN_CEQ: return "::";
        case TOKEN_CONCAT: return "||";
        case TOKEN_ARROW: return "->";
        case TOKEN_DARROW: return "->>";
        /* 分隔符 */
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACKET: return "[";
        case TOKEN_RBRACKET: return "]";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_COMMA: return ",";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_COLON: return ":";
        case TOKEN_DOT: return ".";
        case TOKEN_DOT_DOT: return "..";
        /* CASE 表达式 */
        case TOKEN_CASE: return "CASE";
        case TOKEN_WHEN: return "WHEN";
        case TOKEN_THEN: return "THEN";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_END: return "END";
        /* 结束 */
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/* ========================================================================
 * 语法分析器实现
 * ======================================================================== */

/** 解析器状态 */
typedef struct SqlParser_s {
    SqlLexer lexer;
    SqlToken current;
    bool has_current;
    char error_msg[256];
    int error_location;
    SqlErrorRecoveryMode recovery_mode;
} SqlParser;

/**
 * @brief 创建解析器
 */
void *sql_parser_create(void)
{
    SqlParser *parser = (SqlParser *)calloc(1, sizeof(SqlParser));
    if (!parser) return NULL;
    parser->recovery_mode = ERROR_RECOVERY_SKIP;
    return parser;
}

/**
 * @brief 销毁解析器
 */
void sql_parser_destroy(void *parser)
{
    if (parser) free(parser);
}

/**
 * @brief 获取下一个 Token
 */
static SqlToken parser_next(SqlParser *parser)
{
    if (parser->has_current) {
        parser->has_current = false;
        return parser->current;
    }
    return sql_lexer_next(&parser->lexer);
}

/**
 * @brief 查看当前 Token
 */
static SqlToken parser_peek(SqlParser *parser)
{
    if (!parser->has_current) {
        parser->current = sql_lexer_next(&parser->lexer);
        parser->has_current = true;
    }
    return parser->current;
}

/**
 * @brief 匹配 Token
 */
static bool parser_match(SqlParser *parser, SqlTokenType type)
{
    SqlToken token = parser_peek(parser);
    if (token.type == type) {
        parser_next(parser);
        return true;
    }
    return false;
}

/**
 * @brief 匹配关键字
 */
static bool parser_match_keyword(SqlParser *parser, const char *keyword)
{
    SqlToken token = parser_peek(parser);
    if (token.type == TOKEN_IDENTIFIER) {
        int len = (int)strlen(keyword);
        if (strncasecmp(token.lexeme, keyword, len) == 0 &&
            token.lexeme_len == len) {
            parser_next(parser);
            return true;
        }
    }
    return false;
}

/**
 * @brief 设置错误
 */
static void parser_error(SqlParser *parser, const char *msg)
{
    SqlToken token = parser_peek(parser);
    snprintf(parser->error_msg, sizeof(parser->error_msg),
             "%s at '%.*s'", msg, token.lexeme_len, token.lexeme);
    parser->error_location = token.location;
}

/* ========================================================================
 * 表达式解析
 * ======================================================================== */

/**
 * @brief 解析基本表达式
 */
static void *parse_primary(SqlParser *parser)
{
    SqlToken token = parser_peek(parser);

    /* 括号表达式 */
    if (token.type == TOKEN_LPAREN) {
        parser_next(parser);
        void *expr = parse_expression(parser);
        parser_match(parser, TOKEN_RPAREN);
        return expr;
    }

    /* 标识符 */
    if (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        SqlColumnRef *col = (SqlColumnRef *)calloc(1, sizeof(SqlColumnRef));
        col->type = AST_ColumnRef;
        col->name = (char *)token.lexeme;
        col->relname = NULL;
        col->schema = NULL;
        col->location = token.location;

        /* 检查是否有表名前缀 */
        token = parser_peek(parser);
        if (token.type == TOKEN_DOT) {
            parser_next(parser);
            token = parser_peek(parser);
            if (token.type == TOKEN_IDENTIFIER) {
                parser_next(parser);
                /* schema.table.column 或 table.column */
                SqlColumnRef *new_col = (SqlColumnRef *)calloc(1, sizeof(SqlColumnRef));
                new_col->type = AST_ColumnRef;
                new_col->relname = col->name;
                new_col->name = (char *)token.lexeme;
                new_col->location = token.location;
                free(col);
                return new_col;
            }
        }
        return col;
    }

    /* 参数 */
    if (token.type == TOKEN_PARAMETER) {
        parser_next(parser);
        SqlParamRef *param = (SqlParamRef *)calloc(1, sizeof(SqlParamRef));
        param->type = AST_ParamRef;
        param->number = 1;  /* TODO: 解析参数编号 */
        param->location = token.location;
        return param;
    }

    /* NULL */
    if (token.type == TOKEN_NULL || token.type == TOKEN_NULL_KW) {
        parser_next(parser);
        SqlConst *c = (SqlConst *)calloc(1, sizeof(SqlConst));
        c->type = AST_ConstValue;
        c->location = token.location;
        return c;
    }

    /* 布尔值 */
    if (token.type == TOKEN_BOOL) {
        parser_next(parser);
        SqlConst *c = (SqlConst *)calloc(1, sizeof(SqlConst));
        c->type = AST_ConstValue;
        c->val.ival = (strncasecmp(token.lexeme, "true", token.lexeme_len) == 0) ? 1 : 0;
        c->location = token.location;
        return c;
    }

    /* 整数 */
    if (token.type == TOKEN_INTEGER) {
        parser_next(parser);
        SqlConst *c = (SqlConst *)calloc(1, sizeof(SqlConst));
        c->type = AST_ConstValue;
        c->val.ival = token.val.ival;
        c->type_oid = 23;  /* int4 */
        c->location = token.location;
        return c;
    }

    /* 浮点数 */
    if (token.type == TOKEN_FLOAT) {
        parser_next(parser);
        SqlConst *c = (SqlConst *)calloc(1, sizeof(SqlConst));
        c->type = AST_ConstValue;
        c->val.fval = token.val.fval;
        c->type_oid = 701;  /* float8 */
        c->location = token.location;
        return c;
    }

    /* 字符串 */
    if (token.type == TOKEN_STRING) {
        parser_next(parser);
        SqlConst *c = (SqlConst *)calloc(1, sizeof(SqlConst));
        c->type = AST_ConstValue;
        c->val.sval = (char *)token.lexeme;
        c->type_oid = 25;  /* text */
        c->location = token.location;
        return c;
    }

    /* CASE 表达式 */
    if (token.type == TOKEN_CASE) {
        parser_next(parser);
        SqlCaseExpr *c = (SqlCaseExpr *)calloc(1, sizeof(SqlCaseExpr));
        c->type = AST_CaseExpr;
        c->location = token.location;
        c->whens = NULL;
        c->when_count = 0;
        c->defresult = NULL;

        /* 解析 WHEN 子句 */
        while (!parser_match(parser, TOKEN_END) && !parser_match(parser, TOKEN_EOF)) {
            if (parser_match(parser, TOKEN_WHEN)) {
                void *when_cond = parse_expression(parser);
                parser_match(parser, TOKEN_THEN);
                void *when_result = parse_expression(parser);

                SqlCaseWhen *new_whens = (SqlCaseWhen *)realloc(
                    c->whens, (c->when_count + 1) * sizeof(SqlCaseWhen));
                new_whens[c->when_count].expr = when_cond;
                new_whens[c->when_count].result = when_result;
                c->whens = new_whens;
                c->when_count++;
            } else if (parser_match(parser, TOKEN_ELSE)) {
                c->defresult = parse_expression(parser);
            }
        }

        return c;
    }

    /* 函数调用 */
    if (token.type == TOKEN_COUNT || token.type == TOKEN_SUM ||
        token.type == TOKEN_AVG || token.type == TOKEN_MIN ||
        token.type == TOKEN_MAX) {
        parser_next(parser);
        SqlFuncCall *func = (SqlFuncCall *)calloc(1, sizeof(SqlFuncCall));
        func->type = AST_FuncCall;
        func->location = token.location;

        /* 获取函数名 */
        if (token.type == TOKEN_COUNT) {
            func->funcname = "count";
        } else if (token.type == TOKEN_SUM) {
            func->funcname = "sum";
        } else if (token.type == TOKEN_AVG) {
            func->funcname = "avg";
        } else if (token.type == TOKEN_MIN) {
            func->funcname = "min";
        } else {
            func->funcname = "max";
        }

        /* 解析参数 */
        if (parser_match(parser, TOKEN_LPAREN)) {
            if (!parser_match(parser, TOKEN_STAR)) {
                func->args = parse_expression(parser);
            } else {
                func->agg_star = true;
            }
            parser_match(parser, TOKEN_RPAREN);
        }

        return func;
    }

    /* 类型转换 */
    if (token.type == TOKEN_LPAREN) {
        parser_next(parser);
        void *expr = parse_expression(parser);
        parser_match(parser, TOKEN_RPAREN);
        parser_match(parser, TOKEN_CEQ);  /* :: */
        SqlToken type_name = parser_peek(parser);
        if (type_name.type == TOKEN_IDENTIFIER) {
            parser_next(parser);
            SqlTypeCast *tc = (SqlTypeCast *)calloc(1, sizeof(SqlTypeCast));
            tc->type = AST_TypeCast;
            tc->expr = expr;
            tc->typename = (char *)type_name.lexeme;
            tc->location = token.location;
            return tc;
        }
        return expr;
    }

    return NULL;
}

/**
 * @brief 解析后缀表达式（函数调用、数组下标等）
 */
static void *parse_postfix(SqlParser *parser, void *expr)
{
    SqlToken token = parser_peek(parser);

    /* 函数调用 */
    if (token.type == TOKEN_LPAREN) {
        parser_next(parser);
        SqlFuncCall *func = (SqlFuncCall *)calloc(1, sizeof(SqlFuncCall));
        func->type = AST_FuncCall;
        func->location = token.location;

        if (expr && ((SqlColumnRef *)expr)->type == AST_ColumnRef) {
            func->funcname = ((SqlColumnRef *)expr)->name;
        } else {
            func->funcname = "";
        }

        /* 解析参数 */
        func->args = NULL;
        if (!parser_match(parser, TOKEN_RPAREN)) {
            void *arg = parse_expression(parser);
            if (arg) {
                SqlList *list = (SqlList *)calloc(1, sizeof(SqlList));
                list->type = AST_List;
                list->items = (void **)calloc(16, sizeof(void *));
                list->items[0] = arg;
                list->length = 1;

                while (parser_match(parser, TOKEN_COMMA)) {
                    void *arg2 = parse_expression(parser);
                    if (arg2) {
                        list->items[list->length++] = arg2;
                    }
                }
                func->args = list;
            }
            parser_match(parser, TOKEN_RPAREN);
        }

        free(expr);
        return func;
    }

    /* 数组下标 */
    if (token.type == TOKEN_LBRACKET) {
        parser_next(parser);
        void *idx = parse_expression(parser);
        parser_match(parser, TOKEN_RBRACKET);

        SqlAExpr *aexpr = (SqlAExpr *)calloc(1, sizeof(SqlAExpr));
        aexpr->type = AST_AExpr;
        aexpr->name = "[]";
        aexpr->lexpr = expr;
        aexpr->rexpr = idx;
        return aexpr;
    }

    /* 箭头操作符 */
    if (token.type == TOKEN_ARROW) {
        parser_next(parser);
        SqlToken key = parser_peek(parser);
        parser_next(parser);

        SqlFuncCall *func = (SqlFuncCall *)calloc(1, sizeof(SqlFuncCall));
        func->type = AST_FuncCall;
        func->funcname = "json_extract";
        func->location = token.location;
        return func;
    }

    return expr;
}

/**
 * @brief 解析一元表达式
 */
static void *parse_unary(SqlParser *parser)
{
    SqlToken token = parser_peek(parser);

    /* 一元减号 */
    if (token.type == TOKEN_MINUS) {
        parser_next(parser);
        void *expr = parse_unary(parser);
        SqlAExpr *aexpr = (SqlAExpr *)calloc(1, sizeof(SqlAExpr));
        aexpr->type = AST_AExpr;
        aexpr->name = "-";
        aexpr->lexpr = NULL;
        aexpr->rexpr = expr;
        return aexpr;
    }

    /* 一元加号 */
    if (token.type == TOKEN_PLUS) {
        parser_next(parser);
        return parse_unary(parser);
    }

    /* NOT */
    if (token.type == TOKEN_NOT) {
        parser_next(parser);
        void *expr = parse_unary(parser);
        SqlBoolExpr *not_expr = (SqlBoolExpr *)calloc(1, sizeof(SqlBoolExpr));
        not_expr->type = AST_BoolExpr;
        not_expr->bool_op = 0;  /* NOT */
        not_expr->args = (void **)calloc(1, sizeof(void *));
        not_expr->args[0] = expr;
        not_expr->arg_count = 1;
        return not_expr;
    }

    return parse_primary(parser);
}

/**
 * @brief 解析二元表达式
 */
static void *parse_binary(SqlParser *parser, int precedence)
{
    void *left = parse_unary(parser);

    while (left) {
        SqlToken token = parser_peek(parser);
        int prec = 0;

        /* 确定操作符优先级 */
        switch (token.type) {
            case TOKEN_STAR:
            case TOKEN_SLASH:
            case TOKEN_PERCENT:
                prec = 7;
                break;
            case TOKEN_PLUS:
            case TOKEN_MINUS:
                prec = 6;
                break;
            case TOKEN_EQ:
            case TOKEN_NEQ:
            case TOKEN_LT:
            case TOKEN_GT:
            case TOKEN_LTE:
            case TOKEN_GTE:
            case TOKEN_LIKE:
            case TOKEN_IN:
            case TOKEN_BETWEEN:
                prec = 5;
                break;
            case TOKEN_AND:
                prec = 4;
                break;
            case TOKEN_OR:
                prec = 3;
                break;
            case TOKEN_EQ:
            case TOKEN_CEQ:
                prec = 2;
                break;
            default:
                prec = 0;
                break;
        }

        if (prec == 0 || prec < precedence) {
            break;
        }

        parser_next(parser);

        SqlTokenType op_type = token.type;
        int next_prec = prec;

        /* 右结合操作符 */
        if (op_type == TOKEN_EQ) {
            next_prec = prec + 1;
        }

        void *right = parse_binary(parser, next_prec);

        SqlAExpr *aexpr = (SqlAExpr *)calloc(1, sizeof(SqlAExpr));
        aexpr->type = AST_AExpr;
        aexpr->location = token.location;

        /* 设置操作符名 */
        switch (op_type) {
            case TOKEN_PLUS: aexpr->name = "+"; break;
            case TOKEN_MINUS: aexpr->name = "-"; break;
            case TOKEN_STAR: aexpr->name = "*"; break;
            case TOKEN_SLASH: aexpr->name = "/"; break;
            case TOKEN_PERCENT: aexpr->name = "%"; break;
            case TOKEN_EQ: aexpr->name = "="; break;
            case TOKEN_NEQ: aexpr->name = "<>"; break;
            case TOKEN_LT: aexpr->name = "<"; break;
            case TOKEN_GT: aexpr->name = ">"; break;
            case TOKEN_LTE: aexpr->name = "<="; break;
            case TOKEN_GTE: aexpr->name = ">="; break;
            case TOKEN_LIKE: aexpr->name = "LIKE"; break;
            case TOKEN_CONCAT: aexpr->name = "||"; break;
            default: aexpr->name = "?"; break;
        }

        aexpr->lexpr = left;
        aexpr->rexpr = right;

        left = aexpr;
    }

    return left;
}

/**
 * @brief 解析表达式
 */
static void *parse_expression(SqlParser *parser)
{
    return parse_binary(parser, 0);
}

/* ========================================================================
 * 语句解析
 * ======================================================================== */

/**
 * @brief 解析目标列
 */
static void *parse_target_entry(SqlParser *parser)
{
    void *expr = parse_expression(parser);
    if (!expr) return NULL;

    SqlToken token = parser_peek(parser);

    /* 处理 AS 别名 */
    if (token.type == TOKEN_AS) {
        parser_next(parser);
        token = parser_peek(parser);
        if (token.type == TOKEN_IDENTIFIER) {
            parser_next(parser);
            SqlTargetEntry *entry = (SqlTargetEntry *)calloc(1, sizeof(SqlTargetEntry));
            entry->type = AST_TargetEntry;
            entry->expr = expr;
            entry->name = (char *)token.lexeme;
            entry->location = token.location;
            return entry;
        }
    }

    /* 处理隐式别名 */
    if (token.type == TOKEN_IDENTIFIER) {
        SqlTargetEntry *entry = (SqlTargetEntry *)calloc(1, sizeof(SqlTargetEntry));
        entry->type = AST_TargetEntry;
        entry->expr = expr;
        entry->name = (char *)token.lexeme;
        entry->location = token.location;
        return entry;
    }

    /* 无别名 */
    SqlTargetEntry *entry = (SqlTargetEntry *)calloc(1, sizeof(SqlTargetEntry));
    entry->type = AST_TargetEntry;
    entry->expr = expr;
    entry->name = NULL;
    entry->location = token.location;
    return entry;
}

/**
 * @brief 解析目标列列表
 */
static void *parse_target_list(SqlParser *parser)
{
    SqlList *list = (SqlList *)calloc(1, sizeof(SqlList));
    list->type = AST_List;
    list->items = (void **)calloc(64, sizeof(void *));
    list->length = 0;
    list->location = parser_peek(parser).location;

    /* 检查通配符 */
    if (parser_match(parser, TOKEN_STAR)) {
        SqlTargetEntry *entry = (SqlTargetEntry *)calloc(1, sizeof(SqlTargetEntry));
        entry->type = AST_TargetEntry;
        entry->expr = NULL;  /* * */
        entry->location = list->location;
        list->items[list->length++] = entry;
        return list;
    }

    /* 解析目标列 */
    while (!parser_match(parser, TOKEN_FROM) &&
           !parser_match(parser, TOKEN_WHERE) &&
           !parser_match(parser, TOKEN_ORDER) &&
           !parser_match(parser, TOKEN_GROUP) &&
           !parser_match(parser, TOKEN_HAVING) &&
           !parser_match(parser, TOKEN_LIMIT) &&
           !parser_match(parser, TOKEN_OFFSET) &&
           !parser_match(parser, TOKEN_SEMICOLON) &&
           !parser_match(parser, TOKEN_EOF)) {
        void *entry = parse_target_entry(parser);
        if (entry) {
            list->items[list->length++] = entry;
        }
        if (!parser_match(parser, TOKEN_COMMA)) {
            break;
        }
    }

    /* 回退一个 token */
    parser->has_current = true;

    return list;
}

/**
 * @brief 解析 FROM 子句
 */
static void *parse_from_clause(SqlParser *parser)
{
    if (!parser_match(parser, TOKEN_FROM)) {
        return NULL;
    }

    SqlList *list = (SqlList *)calloc(1, sizeof(SqlList));
    list->type = AST_List;
    list->items = (void **)calloc(32, sizeof(void *));
    list->length = 0;

    while (!parser_match(parser, TOKEN_WHERE) &&
           !parser_match(parser, TOKEN_ORDER) &&
           !parser_match(parser, TOKEN_GROUP) &&
           !parser_match(parser, TOKEN_HAVING) &&
           !parser_match(parser, TOKEN_LIMIT) &&
           !parser_match(parser, TOKEN_SEMICOLON) &&
           !parser_match(parser, TOKEN_EOF)) {
        SqlToken token = parser_peek(parser);
        if (token.type == TOKEN_IDENTIFIER) {
            parser_next(parser);
            SqlRangeVar *rv = (SqlRangeVar *)calloc(1, sizeof(SqlRangeVar));
            rv->type = AST_RangeVar;
            rv->relname = (char *)token.lexeme;
            rv->location = token.location;

            /* 检查别名 */
            token = parser_peek(parser);
            if (token.type == TOKEN_AS) {
                parser_next(parser);
                token = parser_peek(parser);
            }
            if (token.type == TOKEN_IDENTIFIER) {
                rv->alias = (char *)token.lexeme;
                parser_next(parser);
            }

            list->items[list->length++] = rv;
        } else if (token.type == TOKEN_LPAREN) {
            /* 子查询 */
            parser_next(parser);
            void *subquery = parse_select(parser);
            parser_match(parser, TOKEN_RPAREN);
            SqlRangeVar *rv = (SqlRangeVar *)calloc(1, sizeof(SqlRangeVar));
            rv->type = AST_RangeVar;
            rv->relname = NULL;
            rv->alias = "subquery";
            rv->location = token.location;
            list->items[list->length++] = rv;
        }

        if (!parser_match(parser, TOKEN_COMMA)) {
            break;
        }
    }

    parser->has_current = true;
    return list;
}

/**
 * @brief 解析 WHERE 子句
 */
static void *parse_where_clause(SqlParser *parser)
{
    if (!parser_match(parser, TOKEN_WHERE)) {
        return NULL;
    }
    return parse_expression(parser);
}

/**
 * @brief 解析 SELECT 语句
 */
static void *parse_select(SqlParser *parser)
{
    SqlSelectStmt *stmt = (SqlSelectStmt *)calloc(1, sizeof(SqlSelectStmt));
    stmt->type = AST_SELECTStmt;
    stmt->location = parser_peek(parser).location;

    /* DISTINCT */
    stmt->distinct = false;
    if (parser_match(parser, TOKEN_DISTINCT) || parser_match(parser, TOKEN_PARAMETER)) {
        stmt->distinct = true;
    }

    /* 目标列 */
    stmt->target_list = parse_target_list(parser);

    /* FROM */
    stmt->from_clause = parse_from_clause(parser);

    /* WHERE */
    stmt->where_clause = parse_where_clause(parser);

    /* GROUP BY */
    stmt->group_by = NULL;
    if (parser_match(parser, TOKEN_GROUP)) {
        parser_match(parser, TOKEN_BY);
        stmt->group_by = parse_expression(parser);
    }

    /* HAVING */
    stmt->having_clause = NULL;
    if (parser_match(parser, TOKEN_HAVING)) {
        stmt->having_clause = parse_expression(parser);
    }

    /* ORDER BY */
    stmt->sort_clause = NULL;
    if (parser_match(parser, TOKEN_ORDER)) {
        parser_match(parser, TOKEN_BY);
        stmt->sort_clause = parse_expression(parser);
    }

    /* LIMIT */
    stmt->limit_count = NULL;
    if (parser_match(parser, TOKEN_LIMIT)) {
        stmt->limit_count = parse_expression(parser);
    }

    /* OFFSET */
    stmt->limit_offset = NULL;
    if (parser_match(parser, TOKEN_OFFSET)) {
        stmt->limit_offset = parse_expression(parser);
    }

    return stmt;
}

/**
 * @brief 解析 INSERT 语句
 */
static void *parse_insert(SqlParser *parser)
{
    SqlInsertStmt *stmt = (SqlInsertStmt *)calloc(1, sizeof(SqlInsertStmt));
    stmt->type = AST_InsertStmt;
    stmt->location = parser_peek(parser).location;

    parser_match(parser, TOKEN_INTO);

    SqlToken token = parser_peek(parser);
    if (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        stmt->relation = (char *)token.lexeme;
    }

    /* 列列表 */
    if (parser_match(parser, TOKEN_LPAREN)) {
        stmt->cols = parse_target_list(parser);
        parser_match(parser, TOKEN_RPAREN);
    }

    /* VALUES 或 SELECT */
    if (parser_match(parser, TOKEN_VALUES)) {
        /* VALUES ... */
        SqlList *values = (SqlList *)calloc(1, sizeof(SqlList));
        values->type = AST_List;
        values->items = (void **)calloc(64, sizeof(void *));
        values->length = 0;

        while (!parser_match(parser, TOKEN_SEMICOLON) &&
               !parser_match(parser, TOKEN_EOF)) {
            if (parser_match(parser, TOKEN_LPAREN)) {
                SqlList *row = (SqlList *)calloc(1, sizeof(SqlList));
                row->type = AST_List;
                row->items = (void **)calloc(64, sizeof(void *));
                row->length = 0;

                while (!parser_match(parser, TOKEN_RPAREN)) {
                    void *val = parse_expression(parser);
                    if (val) {
                        row->items[row->length++] = val;
                    }
                    parser_match(parser, TOKEN_COMMA);
                }
                values->items[values->length++] = row;
            }
            if (!parser_match(parser, TOKEN_COMMA)) {
                break;
            }
        }
        parser->has_current = true;
    } else {
        /* SELECT */
        stmt->select_stmt = parse_select(parser);
    }

    return stmt;
}

/**
 * @brief 解析 UPDATE 语句
 */
static void *parse_update(SqlParser *parser)
{
    SqlUpdateStmt *stmt = (SqlUpdateStmt *)calloc(1, sizeof(SqlUpdateStmt));
    stmt->type = AST_UpdateStmt;
    stmt->location = parser_peek(parser).location;

    SqlToken token = parser_peek(parser);
    if (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        /* TODO: 解析表名 */
    }

    parser_match(parser, TOKEN_SET);

    /* SET 子句 */
    stmt->target_list = parse_target_list(parser);

    /* WHERE */
    stmt->where_clause = parse_where_clause(parser);

    return stmt;
}

/**
 * @brief 解析 DELETE 语句
 */
static void *parse_delete(SqlParser *parser)
{
    SqlDeleteStmt *stmt = (SqlDeleteStmt *)calloc(1, sizeof(SqlDeleteStmt));
    stmt->type = AST_DeleteStmt;
    stmt->location = parser_peek(parser).location;

    SqlToken token = parser_peek(parser);
    if (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        /* TODO: 解析表名 */
    }

    /* WHERE */
    stmt->where_clause = parse_where_clause(parser);

    return stmt;
}

/**
 * @brief 解析 CREATE TABLE 语句
 */
static void *parse_create_table(SqlParser *parser)
{
    SqlCreateTableStmt *stmt = (SqlCreateTableStmt *)calloc(1, sizeof(SqlCreateTableStmt));
    stmt->type = AST_CreateTableStmt;
    stmt->location = parser_peek(parser).location;

    parser_match(parser, TOKEN_TABLE);

    SqlToken token = parser_peek(parser);
    if (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        stmt->name = (char *)token.lexeme;
    }

    /* 列定义 */
    if (parser_match(parser, TOKEN_LPAREN)) {
        stmt->table_elts = (SqlList *)calloc(1, sizeof(SqlList));
        ((SqlList *)stmt->table_elts)->type = AST_List;
        ((SqlList *)stmt->table_elts)->items = (void **)calloc(64, sizeof(void *));
        ((SqlList *)stmt->table_elts)->length = 0;

        while (!parser_match(parser, TOKEN_RPAREN)) {
            token = parser_peek(parser);
            if (token.type == TOKEN_IDENTIFIER) {
                parser_next(parser);
                SqlColumnDef *col = (SqlColumnDef *)calloc(1, sizeof(SqlColumnDef));
                col->type = AST_ColumnDef;
                col->colname = (char *)token.lexeme;

                token = parser_peek(parser);
                if (token.type == TOKEN_IDENTIFIER ||
                    token.type == TOKEN_VARCHAR ||
                    token.type == TOKEN_INT ||
                    token.type == TOKEN_TEXT ||
                    token.type == TOKEN_FLOAT) {
                    parser_next(parser);
                    col->typename = (char *)token.lexeme;
                }

                ((SqlList *)stmt->table_elts)->items[
                    ((SqlList *)stmt->table_elts)->length++] = col;
            }
            parser_match(parser, TOKEN_COMMA);
        }
    }

    return stmt;
}

/**
 * @brief 解析 DROP TABLE 语句
 */
static void *parse_drop_table(SqlParser *parser)
{
    SqlDropTableStmt *stmt = (SqlDropTableStmt *)calloc(1, sizeof(SqlDropTableStmt));
    stmt->type = AST_DropTableStmt;
    stmt->location = parser_peek(parser).location;

    parser_match(parser, TOKEN_TABLE);

    if (parser_match(parser, TOKEN_IF)) {
        parser_match(parser, TOKEN_EXISTS);
        stmt->missing_ok = true;
    }

    stmt->relations = (SqlList *)calloc(1, sizeof(SqlList));
    ((SqlList *)stmt->relations)->type = AST_List;
    ((SqlList *)stmt->relations)->items = (void **)calloc(32, sizeof(void *));
    ((SqlList *)stmt->relations)->length = 0;

    SqlToken token = parser_peek(parser);
    while (token.type == TOKEN_IDENTIFIER) {
        parser_next(parser);
        ((SqlList *)stmt->relations)->items[
            ((SqlList *)stmt->relations)->length++] = (char *)token.lexeme;
        if (!parser_match(parser, TOKEN_COMMA)) {
            break;
        }
        token = parser_peek(parser);
    }

    if (parser_match(parser, TOKEN_CASCADE)) {
        stmt->behavior = 1;  /* CASCADE */
    } else if (parser_match(parser, TOKEN_RESTRICT)) {
        stmt->behavior = 0;  /* RESTRICT */
    }

    return stmt;
}

/**
 * @brief 解析 ALTER TABLE 语句
 *
 * 支持以下语法：
 *   ALTER TABLE <name> ADD [COLUMN] <col> <type> [NOT NULL] [DEFAULT <expr>]
 *   ALTER TABLE <name> DROP [COLUMN] <col>
 *   ALTER TABLE <name> ALTER [COLUMN] <col> [SET DATA] TYPE <type>
 *   ALTER TABLE <name> RENAME [COLUMN] <old> TO <new>
 *   批量操作：逗号分隔多个命令
 */
static void *parse_alter_table(SqlParser *parser)
{
    AlterTableStmt *stmt = (AlterTableStmt *)calloc(1, sizeof(AlterTableStmt));
    stmt->type = AST_AlterTableStmt;
    stmt->location = parser_peek(parser).location;

    /* 表名 */
    SqlToken token = parser_peek(parser);
    if (token.type != TOKEN_IDENTIFIER) {
        parser_error(parser, "需要表名");
        free(stmt);
        return NULL;
    }
    stmt->relation = strdup(parser->tokens[parser->pos].value);
    parser_next(parser);

    /* 解析命令列表（单条或批量逗号分隔） */
    AlterTableCmd **cmds = NULL;
    int num_cmds = 0, cap_cmds = 0;

    while (1) {
        token = parser_peek(parser);
        if (token.type == TOKEN_EOF) break;

        AlterTableCmd *cmd = (AlterTableCmd *)calloc(1, sizeof(AlterTableCmd));
        cmd->type = AST_AlterTableStmt;
        cmd->location = token.location;

        if (parser_match(parser, TOKEN_ADD)) {
            parser_next(parser);
            /* 可选 COLUMN */
            if (parser_match(parser, TOKEN_COLUMN)) parser_next(parser);
            cmd->subtype = AT_AddColumn;

            /* 列名 */
            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER) {
                parser_error(parser, "ADD COLUMN 需要列名");
                free(cmd);
                goto alter_error;
            }
            cmd->name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

            /* 类型名 */
            token = parser_peek(parser);
            if (token.type == TOKEN_IDENTIFIER || token.type >= TOKEN_INT) {
                cmd->type_name = strdup(parser->tokens[parser->pos].value);
                parser_next(parser);
                /* 检查括号修饰（如 VARCHAR(255)） */
                if (parser_match(parser, TOKEN_LPAREN)) {
                    parser_next(parser);
                    token = parser_peek(parser);
                    if (token.type == TOKEN_INTEGER) parser_next(parser);
                    if (parser_match(parser, TOKEN_RPAREN)) parser_next(parser);
                }
            }

            /* 可选 NOT NULL */
            if (parser_match(parser, TOKEN_NOT)) {
                parser_next(parser);
                if (parser_match(parser, TOKEN_NULL)) {
                    cmd->not_null = true;
                    parser_next(parser);
                }
            }

            /* 可选 DEFAULT */
            if (parser_match(parser, TOKEN_DEFAULT)) {
                parser_next(parser);
                token = parser_peek(parser);
                if (token.type == TOKEN_INTEGER || token.type == TOKEN_STRING) {
                    cmd->default_expr = strdup(parser->tokens[parser->pos].value);
                    parser_next(parser);
                }
            }

        } else if (parser_match(parser, TOKEN_DROP)) {
            parser_next(parser);
            /* 可选 COLUMN */
            if (parser_match(parser, TOKEN_COLUMN)) parser_next(parser);
            cmd->subtype = AT_DropColumn;

            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER) {
                parser_error(parser, "DROP COLUMN 需要列名");
                free(cmd);
                goto alter_error;
            }
            cmd->name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

        } else if (parser_match(parser, TOKEN_ALTER)) {
            parser_next(parser);
            /* 可选 COLUMN */
            if (parser_match(parser, TOKEN_COLUMN)) parser_next(parser);
            cmd->subtype = AT_AlterColumnType;

            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER) {
                parser_error(parser, "ALTER COLUMN 需要列名");
                free(cmd);
                goto alter_error;
            }
            cmd->name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

            /* 期望 SET DATA TYPE 或 TYPE */
            if (parser_match(parser, TOKEN_SET)) {
                parser_next(parser);
                if (parser_match(parser, TOKEN_DATA)) parser_next(parser);
                if (!parser_match(parser, TOKEN_TYPE_KW)) {
                    parser_error(parser, "SET DATA 后需要 TYPE");
                    free(cmd);
                    goto alter_error;
                }
                parser_next(parser);
            } else if (parser_match(parser, TOKEN_TYPE_KW)) {
                parser_next(parser);
            } else {
                parser_error(parser, "需要 TYPE 或 SET DATA TYPE");
                free(cmd);
                goto alter_error;
            }

            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER && token.type < TOKEN_INT) {
                parser_error(parser, "需要类型名");
                free(cmd);
                goto alter_error;
            }
            cmd->type_name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

        } else if (parser_match(parser, TOKEN_RENAME)) {
            parser_next(parser);
            /* 可选 COLUMN */
            if (parser_match(parser, TOKEN_COLUMN)) parser_next(parser);
            cmd->subtype = AT_RenameColumn;

            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER) {
                parser_error(parser, "RENAME COLUMN 需要列名");
                free(cmd);
                goto alter_error;
            }
            cmd->name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

            /* 期望 TO */
            if (!parser_match(parser, TOKEN_TO)) {
                parser_error(parser, "RENAME 后需要 TO");
                free(cmd);
                goto alter_error;
            }
            parser_next(parser);

            token = parser_peek(parser);
            if (token.type != TOKEN_IDENTIFIER) {
                parser_error(parser, "RENAME TO 需要新列名");
                free(cmd);
                goto alter_error;
            }
            cmd->new_name = strdup(parser->tokens[parser->pos].value);
            parser_next(parser);

        } else {
            char buf[128];
            snprintf(buf, sizeof(buf), "需要 ADD/DROP/ALTER/RENAME, 当前: '%s'",
                     token.type == TOKEN_EOF ? "EOF" : (token.lexeme ? token.lexeme : "?"));
            parser_error(parser, buf);
            free(cmd);
            goto alter_error;
        }

        /* 添加到命令列表 */
        if (num_cmds >= cap_cmds) {
            cap_cmds = cap_cmds ? cap_cmds * 2 : 4;
            cmds = (AlterTableCmd **)realloc(cmds, cap_cmds * sizeof(AlterTableCmd *));
        }
        cmds[num_cmds++] = cmd;

        /* 逗号分隔的批量操作 */
        if (parser_match(parser, TOKEN_COMMA)) {
            parser_next(parser);
            continue;
        }
        break;
    }

    stmt->num_cmds = num_cmds;
    stmt->cmds = cmds;
    return stmt;

alter_error:
    for (int i = 0; i < num_cmds; i++) {
        free(cmds[i]->name);
        free(cmds[i]->new_name);
        free(cmds[i]->type_name);
        free(cmds[i]->default_expr);
        free(cmds[i]);
    }
    free(cmds);
    free(stmt->relation);
    free(stmt);
    return NULL;
}

/**
 * @brief 解析语句
 */
static void *parse_stmt(SqlParser *parser)
{
    SqlToken token = parser_peek(parser);

    switch (token.type) {
        case TOKEN_SELECT:
        case TOKEN_WITH:
            parser_next(parser);
            return parse_select(parser);

        case TOKEN_INSERT:
            parser_next(parser);
            return parse_insert(parser);

        case TOKEN_UPDATE:
            parser_next(parser);
            return parse_update(parser);

        case TOKEN_DELETE:
            parser_next(parser);
            return parse_delete(parser);

        case TOKEN_CREATE:
            parser_next(parser);
            if (parser_match(parser, TOKEN_TABLE)) {
                return parse_create_table(parser);
            }
            return NULL;

        case TOKEN_DROP:
            parser_next(parser);
            if (parser_match(parser, TOKEN_TABLE)) {
                return parse_drop_table(parser);
            }
            return NULL;

        case TOKEN_ALTER:
            parser_next(parser);
            if (parser_match(parser, TOKEN_TABLE)) {
                return parse_alter_table(parser);
            }
            return NULL;

        case TOKEN_BEGIN:
        case TOKEN_COMMIT:
        case TOKEN_ROLLBACK:
            {
                SqlTransactionStmt *stmt =
                    (SqlTransactionStmt *)calloc(1, sizeof(SqlTransactionStmt));
                stmt->type = AST_TransactionStmt;
                stmt->location = token.location;
                if (token.type == TOKEN_BEGIN) {
                    stmt->kind = 1;
                } else if (token.type == TOKEN_COMMIT) {
                    stmt->kind = 2;
                } else {
                    stmt->kind = 3;
                }
                parser_next(parser);
                return stmt;
            }

        default:
            parser_next(parser);
            return NULL;
    }
}

/* ========================================================================
 * 解析器 API
 * ======================================================================== */

SqlParseResult *sql_parse(void *parser, const char *sql, int len)
{
    SqlParser *p = (SqlParser *)parser;
    SqlParseResult *result = (SqlParseResult *)calloc(1, sizeof(SqlParseResult));

    sql_lexer_init(&p->lexer, sql, len);
    p->has_current = false;
    p->error_msg[0] = '\0';

    void *stmt = parse_stmt(p);
    if (stmt) {
        result->success = true;
        result->stmt = stmt;
    } else {
        result->success = false;
        result->stmt = NULL;
        result->errmsg = p->error_msg;
        result->err location = p->error_location;
    }

    return result;
}

SqlParseResult *sql_parse_one(const char *sql, int len)
{
    void *parser = sql_parser_create();
    SqlParseResult *result = sql_parse(parser, sql, len);
    sql_parser_destroy(parser);
    return result;
}

SqlList *sql_parse_multiple(const char *sql, int len)
{
    SqlParser parser;
    sql_lexer_init(&parser.lexer, sql, len);
    parser.has_current = false;

    SqlList *list = (SqlList *)calloc(1, sizeof(SqlList));
    list->type = AST_List;
    list->items = (void **)calloc(64, sizeof(void *));
    list->length = 0;

    while (parser_peek(&parser).type != TOKEN_EOF) {
        void *stmt = parse_stmt(&parser);
        if (stmt) {
            list->items[list->length++] = stmt;
        }
        parser_match(&parser, TOKEN_SEMICOLON);
    }

    return list;
}

const char *sql_parser_get_error(void *parser)
{
    SqlParser *p = (SqlParser *)parser;
    return p->error_msg;
}

int sql_parser_get_error_location(void *parser)
{
    SqlParser *p = (SqlParser *)parser;
    return p->error_location;
}

const char *sql_token_name(SqlTokenType type);

/* ========================================================================
 * AST 操作
 * ======================================================================== */

const char *sql_ast_type_name(SqlAstType type)
{
    switch (type) {
        case AST_SELECTStmt: return "SELECT";
        case AST_InsertStmt: return "INSERT";
        case AST_UpdateStmt: return "UPDATE";
        case AST_DeleteStmt: return "DELETE";
        case AST_CreateTableStmt: return "CREATE TABLE";
        case AST_DropTableStmt: return "DROP TABLE";
        case AST_ColumnRef: return "COLUMN";
        case AST_ParamRef: return "PARAM";
        case AST_ConstValue: return "CONST";
        case AST_AExpr: return "AEXPR";
        case AST_FuncCall: return "FUNC";
        case AST_List: return "LIST";
        default: return "NODE";
    }
}

void sql_ast_free(void *node)
{
    if (!node) return;
    free(node);
}

void sql_ast_print(const void *node, int indent)
{
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s\n", sql_ast_type_name(((SqlNode *)node)->type));
}

/* ========================================================================
 * 语义分析器
 * ======================================================================== */

SqlSemContext *sql_sem_create(void)
{
    SqlSemContext *ctx = (SqlSemContext *)calloc(1, sizeof(SqlSemContext));
    return ctx;
}

void sql_sem_destroy(SqlSemContext *ctx)
{
    if (ctx) free(ctx);
}

void *sql_sem_analyze(SqlSemContext *ctx, void *stmt)
{
    /* 简化实现：直接返回原节点 */
    return stmt;
}

void *sql_sem_resolve_column(void *ctx, const char *schema,
                            const char *table, const char *column)
{
    return NULL;
}

void *sql_sem_get_table_columns(void *ctx, const char *schema,
                               const char *table)
{
    return NULL;
}

void sql_parser_error_recovery(void *parser, SqlErrorRecoveryMode mode)
{
    if (parser) {
        ((SqlParser *)parser)->recovery_mode = mode;
    }
}

const char *sql_parser_get_recovery_suggestion(void *parser)
{
    return "Check SQL syntax";
}
