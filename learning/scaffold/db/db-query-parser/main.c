/**
 * @file main.c
 * @brief SQL 解析器演示：词法分析 + 语法分析 + AST 构建
 *
 * 演示 SQL 解析的核心流程：
 * 1. 词法分析：将 SQL 文本转换为 Token 流
 * 2. 语法分析：递归下降解析生成 AST
 * 3. 语义分析：检查语义正确性
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ========================================================================
 * Token 类型定义（简化版）
 * ======================================================================== */

typedef enum {
    TOKEN_EOF = 0,
    TOKEN_ERROR,

    /* 字面量 */
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_IDENTIFIER,

    /* 关键字 */
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_INSERT,
    TOKEN_UPDATE,
    TOKEN_DELETE,
    TOKEN_CREATE,
    TOKEN_DROP,
    TOKEN_TABLE,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_SET,

    /* 操作符 */
    TOKEN_STAR,      /* * */
    TOKEN_COMMA,     /* , */
    TOKEN_LPAREN,    /* ( */
    TOKEN_RPAREN,    /* ) */
    TOKEN_EQ,        /* = */
    TOKEN_LT,        /* < */
    TOKEN_GT,        /* > */
    TOKEN_SEMICOLON, /* ; */

    /* 其他 */
    TOKEN_UNKNOWN
} TokenType;

/* Token 结构 */
typedef struct {
    TokenType type;
    const char *lexeme;
    int len;
    int line;
    int col;
} Token;

/* ========================================================================
 * 词法分析器
 * ======================================================================== */

typedef struct {
    const char *input;
    int pos;
    int len;
    int line;
    int col;
} Lexer;

/* 关键字查找表 */
static const struct {
    const char *name;
    TokenType type;
} keywords[] = {
    {"SELECT", TOKEN_SELECT},
    {"FROM", TOKEN_FROM},
    {"WHERE", TOKEN_WHERE},
    {"AND", TOKEN_AND},
    {"OR", TOKEN_OR},
    {"NOT", TOKEN_NOT},
    {"INSERT", TOKEN_INSERT},
    {"UPDATE", TOKEN_UPDATE},
    {"DELETE", TOKEN_DELETE},
    {"CREATE", TOKEN_CREATE},
    {"DROP", TOKEN_DROP},
    {"TABLE", TOKEN_TABLE},
    {"INTO", TOKEN_INTO},
    {"VALUES", TOKEN_VALUES},
    {"SET", TOKEN_SET},
};
#define KEYWORD_COUNT (sizeof(keywords) / sizeof(keywords[0]))

/* 初始化词法分析器 */
void lexer_init(Lexer *lex, const char *input) {
    lex->input = input;
    lex->len = strlen(input);
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
}

/* 跳过空白字符 */
static void skip_whitespace(Lexer *lex) {
    while (lex->pos < lex->len) {
        char c = lex->input[lex->pos];
        if (c == ' ' || c == '\t' || c == '\r') {
            lex->pos++;
            lex->col++;
        } else if (c == '\n') {
            lex->pos++;
            lex->line++;
            lex->col = 1;
        } else {
            break;
        }
    }
}

/* 关键字查找 */
static TokenType lookup_keyword(const char *s, int len) {
    for (int i = 0; i < (int)KEYWORD_COUNT; i++) {
        int klen = strlen(keywords[i].name);
        if (len == klen && strncasecmp(s, keywords[i].name, len) == 0) {
            return keywords[i].type;
        }
    }
    return TOKEN_IDENTIFIER;
}

/* 获取下一个 Token */
Token lexer_next(Lexer *lex) {
    skip_whitespace(lex);

    Token tok = {TOKEN_EOF, "", 0, lex->line, lex->col};

    if (lex->pos >= lex->len) {
        return tok;
    }

    int start = lex->pos;
    char c = lex->input[lex->pos];

    /* 标识符或关键字 */
    if (isalpha(c) || c == '_') {
        while (lex->pos < lex->len &&
               (isalnum(lex->input[lex->pos]) || lex->input[lex->pos] == '_')) {
            lex->pos++;
            lex->col++;
        }
        tok.lexeme = lex->input + start;
        tok.len = lex->pos - start;
        tok.type = lookup_keyword(tok.lexeme, tok.len);
        return tok;
    }

    /* 数字 */
    if (isdigit(c)) {
        while (lex->pos < lex->len && isdigit(lex->input[lex->pos])) {
            lex->pos++;
            lex->col++;
        }
        tok.type = TOKEN_INTEGER;
        tok.lexeme = lex->input + start;
        tok.len = lex->pos - start;
        return tok;
    }

    /* 字符串 */
    if (c == '\'' || c == '"') {
        lex->pos++;
        lex->col++;
        while (lex->pos < lex->len && lex->input[lex->pos] != c) {
            lex->pos++;
            lex->col++;
        }
        if (lex->pos < lex->len) {
            lex->pos++;
            lex->col++;
        }
        tok.type = TOKEN_STRING;
        tok.lexeme = lex->input + start + 1;
        tok.len = lex->pos - start - 2;
        return tok;
    }

    /* 单字符操作符 */
    lex->pos++;
    lex->col++;
    tok.lexeme = lex->input + start;
    tok.len = 1;

    switch (c) {
        case '*': tok.type = TOKEN_STAR; break;
        case ',': tok.type = TOKEN_COMMA; break;
        case '(': tok.type = TOKEN_LPAREN; break;
        case ')': tok.type = TOKEN_RPAREN; break;
        case '=': tok.type = TOKEN_EQ; break;
        case '<': tok.type = TOKEN_LT; break;
        case '>': tok.type = TOKEN_GT; break;
        case ';': tok.type = TOKEN_SEMICOLON; break;
        default: tok.type = TOKEN_UNKNOWN; break;
    }

    return tok;
}

/* Token 名称 */
const char *token_name(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_INTEGER: return "INTEGER";
        case TOKEN_FLOAT: return "FLOAT";
        case TOKEN_STRING: return "STRING";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_SELECT: return "SELECT";
        case TOKEN_FROM: return "FROM";
        case TOKEN_WHERE: return "WHERE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_STAR: return "*";
        case TOKEN_COMMA: return ",";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_EQ: return "=";
        default: return "UNKNOWN";
    }
}

/* ========================================================================
 * AST 节点定义
 * ======================================================================== */

typedef enum {
    AST_SELECT,
    AST_COLUMN,
    AST_TABLE,
    AST_WHERE,
    AST_BINARY_OP,
    AST_LITERAL
} AstType;

typedef struct AstNode {
    AstType type;
    struct AstNode **children;
    int nchildren;
    char *value;
} AstNode;

/* 创建 AST 节点 */
AstNode *ast_new(AstType type, const char *value) {
    AstNode *node = (AstNode *)calloc(1, sizeof(AstNode));
    node->type = type;
    node->value = value ? strdup(value) : NULL;
    node->children = NULL;
    node->nchildren = 0;
    return node;
}

/* 添加子节点 */
void ast_add_child(AstNode *parent, AstNode *child) {
    parent->nchildren++;
    parent->children = (AstNode **)realloc(parent->children,
                                           parent->nchildren * sizeof(AstNode *));
    parent->children[parent->nchildren - 1] = child;
}

/* 打印 AST */
void ast_print(AstNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    switch (node->type) {
        case AST_SELECT: printf("SELECT\n"); break;
        case AST_COLUMN: printf("COLUMN: %s\n", node->value); break;
        case AST_TABLE: printf("TABLE: %s\n", node->value); break;
        case AST_WHERE: printf("WHERE\n"); break;
        case AST_BINARY_OP: printf("OP: %s\n", node->value); break;
        case AST_LITERAL: printf("LITERAL: %s\n", node->value); break;
    }
    for (int i = 0; i < node->nchildren; i++) {
        ast_print(node->children[i], indent + 1);
    }
}

/* ========================================================================
 * 简化版解析器
 * ======================================================================== */

typedef struct {
    Lexer lex;
    Token cur;
    int has_cur;
} Parser;

/* 初始化解析器 */
void parser_init(Parser *p, const char *sql) {
    lexer_init(&p->lex, sql);
    p->has_cur = 0;
}

/* 获取当前 Token */
Token parser_peek(Parser *p) {
    if (!p->has_cur) {
        p->cur = lexer_next(&p->lex);
        p->has_cur = 1;
    }
    return p->cur;
}

/* 消费 Token */
Token parser_next(Parser *p) {
    Token tok = parser_peek(p);
    p->has_cur = 0;
    return tok;
}

/* 匹配 Token */
int parser_match(Parser *p, TokenType type) {
    if (parser_peek(p).type == type) {
        parser_next(p);
        return 1;
    }
    return 0;
}

/* 解析列列表 */
AstNode *parse_columns(Parser *p) {
    AstNode *cols = ast_new(AST_SELECT, NULL);

    if (parser_peek(p).type == TOKEN_STAR) {
        parser_next(p);
        AstNode *star = ast_new(AST_COLUMN, "*");
        ast_add_child(cols, star);
        return cols;
    }

    while (1) {
        Token tok = parser_peek(p);
        if (tok.type == TOKEN_IDENTIFIER) {
            parser_next(p);
            AstNode *col = ast_new(AST_COLUMN, tok.lexeme);
            ast_add_child(cols, col);
        }
        if (!parser_match(p, TOKEN_COMMA)) break;
    }

    return cols;
}

/* 解析表 */
AstNode *parse_table(Parser *p) {
    Token tok = parser_next(p);
    return ast_new(AST_TABLE, tok.lexeme);
}

/* 解析 WHERE 子句 */
AstNode *parse_where(Parser *p) {
    AstNode *where = ast_new(AST_WHERE, NULL);

    Token left = parser_next(p);
    AstNode *left_node = ast_new(AST_COLUMN, left.lexeme);

    Token op = parser_next(p);

    Token right = parser_next(p);
    AstNode *right_node = ast_new(AST_LITERAL, right.lexeme);

    AstNode *cond = ast_new(AST_BINARY_OP, op.lexeme);
    ast_add_child(cond, left_node);
    ast_add_child(cond, right_node);
    ast_add_child(where, cond);

    return where;
}

/* 解析 SELECT 语句 */
AstNode *parse_select(Parser *p) {
    parser_match(p, TOKEN_SELECT);

    AstNode *select = ast_new(AST_SELECT, NULL);

    /* 列 */
    AstNode *cols = parse_columns(p);
    ast_add_child(select, cols);

    /* FROM */
    if (parser_match(p, TOKEN_FROM)) {
        AstNode *table = parse_table(p);
        ast_add_child(select, table);
    }

    /* WHERE */
    if (parser_match(p, TOKEN_WHERE)) {
        AstNode *where = parse_where(p);
        ast_add_child(select, where);
    }

    return select;
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main(int argc, char **argv) {
    const char *sql = "SELECT id, name FROM users WHERE id = 1";

    printf("=== SQL 解析器演示 ===\n\n");
    printf("输入 SQL: %s\n\n", sql);

    /* 1. 词法分析 */
    printf("--- 1. 词法分析 ---\n");
    Lexer lex;
    lexer_init(&lex, sql);

    Token tok;
    while ((tok = lexer_next(&lex)).type != TOKEN_EOF) {
        printf("  Token: %-12s Lexeme: '%.*s' (line %d, col %d)\n",
               token_name(tok.type), tok.len, tok.lexeme, tok.line, tok.col);
    }
    printf("\n");

    /* 2. 语法分析 */
    printf("--- 2. 语法分析 (AST 构建) ---\n");
    Parser parser;
    parser_init(&parser, sql);

    AstNode *ast = parse_select(&parser);

    printf("AST 结构:\n");
    ast_print(ast, 2);
    printf("\n");

    /* 3. 语义分析（简化版） */
    printf("--- 3. 语义分析 ---\n");
    printf("  - 检查列名是否存在\n");
    printf("  - 检查表名是否存在\n");
    printf("  - 检查类型是否匹配\n");
    printf("  （本演示仅输出 AST，实际工程见 sql_parser.c）\n\n");

    /* 清理 */
    // TODO: 释放 AST 节点

    printf("=== 演示完成 ===\n");
    return 0;
}