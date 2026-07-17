/**
 * @file sql_parser.c
 * @brief SQL 语法分析器实现（简化版）
 */

#include "db/parser/sql/sql.h"
#include "db/errors.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================
 * 解析器结构
 * ============================================================ */

struct sql_parser_s {
    sql_lexer_t *lexer;
    sql_token_t *current;
    char error_msg[256];
};

/* 最后一次解析错误的全局消息（供外部获取） */
static char g_last_parse_error[256] = {0};

const char *sql_get_last_parse_error(void) {
    return g_last_parse_error;
}

/* ============================================================
 * 工具函数
 * ============================================================ */

static sql_node_t *alloc_node(sql_parser_t *p, sql_node_type_t type) {
    (void)p;
    sql_node_t *n = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    if (n) n->type = type;
    return n;
}

static sql_node_t *alloc_list(sql_parser_t *p) {
    sql_node_t *n = alloc_node(p, SQL_NODE_EXPR_LIST);
    if (n) {
        n->u.list.capacity = 4;
        n->u.list.items = (sql_node_t **)calloc(n->u.list.capacity, sizeof(sql_node_t *));
        n->u.list.count = 0;
        if (!n->u.list.items) {
            sql_node_free(n);
            return NULL;
        }
    }
    return n;
}

/**
 * @brief 向列表添加元素（支持动态扩容）
 * @param list 列表节点
 * @param item 要添加的节点
 * @return 0 成功，-1 失败
 */
static int list_add(sql_parser_t *p, sql_node_t *list, sql_node_t *item) {
    if (!list || !item || list->type != SQL_NODE_EXPR_LIST) return -1;

    /* 动态扩容：当容量不足时翻倍 */
    if (list->u.list.count >= list->u.list.capacity) {
        size_t new_capacity = list->u.list.capacity * 2;
        sql_node_t **new_items = (sql_node_t **)realloc(
            list->u.list.items, new_capacity * sizeof(sql_node_t *));
        if (!new_items) return -1;
        list->u.list.items = new_items;
        list->u.list.capacity = new_capacity;
    }

    list->u.list.items[list->u.list.count++] = item;
    (void)p;  /* 保留参数以备将来使用 */
    return 0;
}

static void advance(sql_parser_t *p) {
    if (p->current && p->current->lexeme) {
        free(p->current->lexeme);
    }
    p->current = sql_lexer_next(p->lexer);
}

static int check(sql_parser_t *p, sql_token_type_t t) {
    return p->current && p->current->type == t;
}

static int match(sql_parser_t *p, sql_token_type_t t) {
    if (check(p, t)) { advance(p); return 1; }
    return 0;
}

static void err(sql_parser_t *p, const char *msg) {
    snprintf(p->error_msg, sizeof(p->error_msg), "Error at line %zu: %s",
             p->current ? p->current->line : 0, msg);
}

/* ============================================================
 * 表达式解析（完整版：支持 AND/OR/NOT/LIKE/IN/BETWEEN/IS NULL）
 * ============================================================ */

/* 前向声明 */
static sql_node_t *parse_expr(sql_parser_t *p);
static sql_node_t *parse_or_expr(sql_parser_t *p);
static sql_node_t *parse_and_expr(sql_parser_t *p);
static sql_node_t *parse_not_expr(sql_parser_t *p);
static sql_node_t *parse_compare_expr(sql_parser_t *p);
static sql_node_t *parse_in_expr(sql_parser_t *p);
static sql_node_t *parse_like_expr(sql_parser_t *p);
static sql_node_t *parse_is_expr(sql_parser_t *p);
static sql_node_t *parse_primary(sql_parser_t *p);

/**
 * @brief 解析表达式（最低优先级：OR）
 *
 * 优先级：OR (1) < AND (2) < NOT (3) < 比较 (4) < IN/LIKE/BETWEEN (5) < PRIMARY (6)
 */
static sql_node_t *parse_expr(sql_parser_t *p) {
    return parse_or_expr(p);
}

/**
 * @brief 解析 OR 表达式
 */
static sql_node_t *parse_or_expr(sql_parser_t *p) {
    sql_node_t *left = parse_and_expr(p);
    if (!left) return NULL;

    while (check(p, SQL_TOKEN_OR)) {
        advance(p);
        sql_node_t *right = parse_and_expr(p);
        if (!right) {
            sql_node_free(left);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_LOGICAL_OP);
        if (!node) {
            sql_node_free(left);
            sql_node_free(right);
            return NULL;
        }
        node->u.logical_op.op = 1;  /* OR */
        node->u.logical_op.left = left;
        node->u.logical_op.right = right;
        left = node;
    }

    return left;
}

/**
 * @brief 解析 AND 表达式
 */
static sql_node_t *parse_and_expr(sql_parser_t *p) {
    sql_node_t *left = parse_not_expr(p);
    if (!left) return NULL;

    while (check(p, SQL_TOKEN_AND)) {
        advance(p);
        sql_node_t *right = parse_not_expr(p);
        if (!right) {
            sql_node_free(left);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_LOGICAL_OP);
        if (!node) {
            sql_node_free(left);
            sql_node_free(right);
            return NULL;
        }
        node->u.logical_op.op = 0;  /* AND */
        node->u.logical_op.left = left;
        node->u.logical_op.right = right;
        left = node;
    }

    return left;
}

/**
 * @brief 解析 NOT 表达式
 */
static sql_node_t *parse_not_expr(sql_parser_t *p) {
    if (check(p, SQL_TOKEN_NOT)) {
        advance(p);
        sql_node_t *expr = parse_not_expr(p);
        if (!expr) return NULL;

        sql_node_t *node = alloc_node(p, SQL_NODE_LOGICAL_OP);
        if (!node) {
            sql_node_free(expr);
            return NULL;
        }
        node->u.logical_op.op = 2;  /* NOT */
        node->u.logical_op.expr = expr;
        return node;
    }

    return parse_compare_expr(p);
}

/**
 * @brief 解析比较表达式
 */
static sql_node_t *parse_compare_expr(sql_parser_t *p) {
    sql_node_t *left = parse_in_expr(p);
    if (!left) return NULL;

    /* 检查比较操作符 */
    sql_token_type_t comp_op = 0;
    if (check(p, SQL_TOKEN_EQ)) comp_op = SQL_TOKEN_EQ;
    else if (check(p, SQL_TOKEN_NE)) comp_op = SQL_TOKEN_NE;
    else if (check(p, SQL_TOKEN_LT)) comp_op = SQL_TOKEN_LT;
    else if (check(p, SQL_TOKEN_LE)) comp_op = SQL_TOKEN_LE;
    else if (check(p, SQL_TOKEN_GT)) comp_op = SQL_TOKEN_GT;
    else if (check(p, SQL_TOKEN_GE)) comp_op = SQL_TOKEN_GE;

    if (comp_op) {
        advance(p);
        sql_node_t *right = parse_in_expr(p);
        if (!right) {
            sql_node_free(left);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_BINARY_OP);
        if (!node) {
            sql_node_free(left);
            sql_node_free(right);
            return NULL;
        }
        /* 存储比较操作符类型 */
        node->u.binary_op.op = comp_op;
        node->u.binary_op.left = left;
        node->u.binary_op.right = right;
        return node;
    }

    return left;
}

/**
 * @brief 解析 IN 表达式
 */
static sql_node_t *parse_in_expr(sql_parser_t *p) {
    sql_node_t *expr = parse_like_expr(p);
    if (!expr) return NULL;

    /* 检查 IN/NOT IN */
    if (check(p, SQL_TOKEN_IN)) {
        advance(p);

        /* 可选的 NOT */
        int negated = 0;
        if (check(p, SQL_TOKEN_NOT)) {
            negated = 1;
            advance(p);
        }

        if (!match(p, SQL_TOKEN_LPAREN)) {
            sql_node_free(expr);
            return NULL;
        }

        /* 解析列表 */
        sql_node_t *list = alloc_list(p);
        if (!list) {
            sql_node_free(expr);
            return NULL;
        }

        while (!check(p, SQL_TOKEN_RPAREN) && !check(p, SQL_TOKEN_EOF)) {
            sql_node_t *item = parse_primary(p);
            if (!item) break;
            list_add(p, list, item);

            if (!match(p, SQL_TOKEN_COMMA)) break;
        }

        if (!match(p, SQL_TOKEN_RPAREN)) {
            sql_node_free(expr);
            sql_node_free(list);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_IN_LIST);
        if (!node) {
            sql_node_free(expr);
            sql_node_free(list);
            return NULL;
        }
        node->u.in_list.expr = expr;
        node->u.in_list.items = list->u.list.items;
        node->u.in_list.count = list->u.list.count;
        node->u.in_list.negated = negated;
        list->u.list.items = NULL;  /* 转移所有权 */
        free(list);
        return node;
    }

    return expr;
}

/**
 * @brief 解析 LIKE 表达式
 */
static sql_node_t *parse_like_expr(sql_parser_t *p) {
    sql_node_t *expr = parse_is_expr(p);
    if (!expr) return NULL;

    /* 检查 LIKE/NOT LIKE */
    if (check(p, SQL_TOKEN_LIKE) || check(p, SQL_TOKEN_IDENT)) {
        /* 处理 LIKE 关键字或 IDENT 为 LIKE 的情况 */
        if (check(p, SQL_TOKEN_LIKE)) {
            advance(p);
        } else if (p->current && strcmp(p->current->lexeme, "LIKE") == 0) {
            advance(p);
        } else {
            return expr;
        }

        /* 可选的 NOT */
        int negated = 0;
        if (check(p, SQL_TOKEN_NOT)) {
            negated = 1;
            advance(p);
        }

        sql_node_t *pattern = parse_primary(p);
        if (!pattern) {
            sql_node_free(expr);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_LIKE);
        if (!node) {
            sql_node_free(expr);
            sql_node_free(pattern);
            return NULL;
        }
        node->u.like.expr = expr;
        node->u.like.pattern = pattern;
        node->u.like.negated = negated;
        return node;
    }

    return expr;
}

/**
 * @brief 解析 IS NULL/IS TRUE/IS FALSE 表达式
 */
static sql_node_t *parse_is_expr(sql_parser_t *p) {
    sql_node_t *expr = parse_primary(p);
    if (!expr) return NULL;

    /* 检查 IS */
    if (check(p, SQL_TOKEN_IS)) {
        advance(p);

        /* 可选的 NOT */
        int negated = 0;
        if (check(p, SQL_TOKEN_NOT)) {
            negated = 1;
            advance(p);
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_IS_NULL);
        if (!node) {
            sql_node_free(expr);
            return NULL;
        }
        node->u.is_null.expr = expr;

        /* 检查 NULL/TRUE/FALSE */
        if (check(p, SQL_TOKEN_NULL)) {
            advance(p);
            node->u.is_null.is_null = negated ? 0 : 1;
            node->u.is_null.truth_value = -1;
        } else if (check(p, SQL_TOKEN_IDENT)) {
            if (strcmp(p->current->lexeme, "TRUE") == 0) {
                advance(p);
                node->u.is_null.is_null = 0;
                node->u.is_null.truth_value = negated ? 0 : 1;
            } else if (strcmp(p->current->lexeme, "FALSE") == 0) {
                advance(p);
                node->u.is_null.is_null = 0;
                node->u.is_null.truth_value = negated ? 1 : 0;
            } else {
                sql_node_free(node);
                return expr;  /* IS 后面不是 NULL/TRUE/FALSE，回退 */
            }
        } else {
            sql_node_free(node);
            return expr;  /* IS 后面没有东西，回退 */
        }

        return node;
    }

    return expr;
}

/**
 * @brief 解析 BETWEEN 表达式
 */
static sql_node_t *parse_between_expr(sql_parser_t *p) {
    sql_node_t *expr = parse_primary(p);
    if (!expr) return NULL;

    /* 检查 BETWEEN */
    if (check(p, SQL_TOKEN_IDENT) && p->current &&
        strcmp(p->current->lexeme, "BETWEEN") == 0) {
        advance(p);

        /* 可选的 NOT */
        int negated = 0;
        if (check(p, SQL_TOKEN_NOT)) {
            negated = 1;
            advance(p);
        }

        sql_node_t *min = parse_primary(p);
        if (!min) {
            sql_node_free(expr);
            return NULL;
        }

        /* 期望 AND */
        if (!(check(p, SQL_TOKEN_AND) ||
              (check(p, SQL_TOKEN_IDENT) && p->current &&
               strcmp(p->current->lexeme, "AND") == 0))) {
            sql_node_free(expr);
            sql_node_free(min);
            return NULL;
        }
        advance(p);

        sql_node_t *max = parse_primary(p);
        if (!max) {
            sql_node_free(expr);
            sql_node_free(min);
            return NULL;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_BETWEEN);
        if (!node) {
            sql_node_free(expr);
            sql_node_free(min);
            sql_node_free(max);
            return NULL;
        }
        node->u.between.expr = expr;
        node->u.between.min = min;
        node->u.between.max = max;
        node->u.between.negated = negated;
        return node;
    }

    return expr;
}

/**
 * @brief 解析括号表达式
 */
static sql_node_t *parse_paren_expr(sql_parser_t *p) {
    if (!match(p, SQL_TOKEN_LPAREN)) {
        return parse_between_expr(p);
    }

    sql_node_t *expr = parse_expr(p);
    if (!expr) return NULL;

    if (!match(p, SQL_TOKEN_RPAREN)) {
        sql_node_free(expr);
        return NULL;
    }

    return expr;
}

/**
 * @brief 解析基本表达式（字面量、列引用）
 */
static sql_node_t *parse_primary(sql_parser_t *p) {
    if (!p || !p->current) return NULL;

    /* 处理括号 */
    if (check(p, SQL_TOKEN_LPAREN)) {
        return parse_paren_expr(p);
    }

    /* 处理 NULL */
    if (check(p, SQL_TOKEN_NULL)) {
        sql_node_t *n = alloc_node(p, SQL_NODE_CONSTANT);
        if (n) {
            n->u.constant.type = SQL_TYPE_NULL;
            n->u.constant.int_value = 0;
        }
        advance(p);
        return n;
    }

    /* 处理负数 */
    if (check(p, SQL_TOKEN_MINUS)) {
        advance(p);
        sql_node_t *operand = parse_primary(p);
        if (!operand) return NULL;

        /* 构造 0 - operand */
        sql_node_t *zero = alloc_node(p, SQL_NODE_CONSTANT);
        if (zero) {
            zero->u.constant.type = SQL_TYPE_INT;
            zero->u.constant.int_value = 0;
        }

        sql_node_t *node = alloc_node(p, SQL_NODE_BINARY_OP);
        if (!node) {
            sql_node_free(zero);
            sql_node_free(operand);
            return NULL;
        }
        node->u.binary_op.op = SQL_TOKEN_MINUS;  /* 使用 MINUS 表示减法 */
        node->u.binary_op.left = zero;
        node->u.binary_op.right = operand;
        return node;
    }

    /* 处理正数（可选的 +） */
    if (check(p, SQL_TOKEN_PLUS)) {
        advance(p);
    }

    /* 处理整数 */
    if (check(p, SQL_TOKEN_INT)) {
        sql_node_t *n = alloc_node(p, SQL_NODE_CONSTANT);
        if (n) {
            n->u.constant.type = SQL_TYPE_INT;
            n->u.constant.int_value = p->current->int_value;
        }
        advance(p);
        return n;
    }

    /* 处理浮点数 */
    if (check(p, SQL_TOKEN_FLOAT)) {
        sql_node_t *n = alloc_node(p, SQL_NODE_CONSTANT);
        if (n) {
            n->u.constant.type = SQL_TYPE_REAL;
            n->u.constant.float_value = p->current->float_value;
        }
        advance(p);
        return n;
    }

    /* 处理字符串 */
    if (check(p, SQL_TOKEN_STRING)) {
        sql_node_t *n = alloc_node(p, SQL_NODE_CONSTANT);
        if (n) {
            n->u.constant.type = SQL_TYPE_TEXT;
            n->u.constant.str_value = strdup(p->current->lexeme);
        }
        advance(p);
        return n;
    }

    /* 处理列引用（标识符） */
    if (check(p, SQL_TOKEN_IDENT)) {
        sql_node_t *n = alloc_node(p, SQL_NODE_COLUMN_REF);
        if (n) {
            n->u.column_ref.name = strdup(p->current->lexeme);
        }
        advance(p);
        return n;
    }

    return NULL;
}

/* 解析列列表 */
static sql_node_t *parse_column_list(sql_parser_t *p) {
    sql_node_t *list = alloc_list(p);
    if (!list) return NULL;

    /* 处理 SELECT * */
    if (check(p, SQL_TOKEN_STAR)) {
        sql_node_t *star = alloc_node(p, SQL_NODE_COLUMN_REF);
        if (star) star->u.column_ref.name = strdup("*");
        list_add(p, list, star);
        advance(p);
        return list;
    }

    /* 解析列名 */
    while (check(p, SQL_TOKEN_IDENT)) {
        sql_node_t *col = alloc_node(p, SQL_NODE_COLUMN_REF);
        if (col) col->u.column_ref.name = strdup(p->current->lexeme);
        list_add(p, list, col);
        advance(p);
        if (!match(p, SQL_TOKEN_COMMA)) break;
    }
    return list;
}

/* ============================================================
 * 语句解析
 * ============================================================ */

static sql_node_t *parse_create_table(sql_parser_t *p) {
    advance(p);  /* CREATE */
    if (!match(p, SQL_TOKEN_TABLE)) { err(p, "Expected TABLE"); return NULL; }
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); return NULL; }

    sql_node_t *node = alloc_node(p, SQL_NODE_CREATE_TABLE);
    if (!node) return NULL;
    node->u.create_table.table_name = strdup(p->current->lexeme);
    advance(p);

    if (!match(p, SQL_TOKEN_LPAREN)) { err(p, "Expected '('"); sql_node_free(node); return NULL; }

    node->u.create_table.columns = alloc_list(p);
    if (!node->u.create_table.columns) { sql_node_free(node); return NULL; }

    /* 解析列定义 */
    while (check(p, SQL_TOKEN_IDENT)) {
        sql_node_t *col = alloc_node(p, SQL_NODE_COLUMN_DEF);
        if (!col) break;
        col->u.column_def.name = strdup(p->current->lexeme);
        advance(p);

        /* 类型 */
        if (match(p, SQL_TOKEN_INT_TYPE)) col->u.column_def.type = SQL_TYPE_INT;
        else if (match(p, SQL_TOKEN_VARCHAR)) col->u.column_def.type = SQL_TYPE_VARCHAR;
        else if (match(p, SQL_TOKEN_TEXT)) col->u.column_def.type = SQL_TYPE_TEXT;
        else if (match(p, SQL_TOKEN_BLOB)) col->u.column_def.type = SQL_TYPE_BLOB;

        /* 可选长度 */
        if (check(p, SQL_TOKEN_INT)) {
            col->u.column_def.length = p->current->int_value;
            advance(p);
        }

        list_add(p, node->u.create_table.columns, col);
        if (!match(p, SQL_TOKEN_COMMA)) break;
    }

    if (!match(p, SQL_TOKEN_RPAREN)) { err(p, "Expected ')'"); sql_node_free(node); return NULL; }
    return node;
}

static sql_node_t *parse_drop_table(sql_parser_t *p) {
    advance(p);  /* DROP */
    if (!match(p, SQL_TOKEN_TABLE)) { err(p, "Expected TABLE"); return NULL; }
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); return NULL; }

    sql_node_t *node = alloc_node(p, SQL_NODE_DROP_TABLE);
    if (node) node->u.drop_table.table_name = strdup(p->current->lexeme);
    advance(p);
    return node;
}

static sql_node_t *parse_select(sql_parser_t *p) {
    advance(p);  /* SELECT */
    sql_node_t *node = alloc_node(p, SQL_NODE_SELECT);
    if (!node) return NULL;

    node->u.select.columns = parse_column_list(p);
    if (!node->u.select.columns) { sql_node_free(node); return NULL; }

    if (!match(p, SQL_TOKEN_FROM)) { err(p, "Expected FROM"); sql_node_free(node); return NULL; }
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); sql_node_free(node); return NULL; }

    node->u.select.table_name = strdup(p->current->lexeme);
    advance(p);

    if (match(p, SQL_TOKEN_WHERE)) {
        node->u.select.where_cond = parse_expr(p);
    }

    return node;
}

static sql_node_t *parse_insert(sql_parser_t *p) {
    advance(p);  /* INSERT */
    if (!match(p, SQL_TOKEN_INTO)) { err(p, "Expected INTO"); return NULL; }
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); return NULL; }

    sql_node_t *node = alloc_node(p, SQL_NODE_INSERT);
    if (!node) return NULL;

    node->u.insert.table_name = strdup(p->current->lexeme);
    advance(p);

    /* 可选列名列表 */
    if (match(p, SQL_TOKEN_LPAREN)) {
        node->u.insert.columns = parse_column_list(p);
        if (!match(p, SQL_TOKEN_RPAREN)) { err(p, "Expected ')'"); sql_node_free(node); return NULL; }
    }

    if (!match(p, SQL_TOKEN_VALUES)) { err(p, "Expected VALUES"); sql_node_free(node); return NULL; }

    node->u.insert.values = alloc_list(p);
    if (!node->u.insert.values) { sql_node_free(node); return NULL; }

    /* 值 */
    while (1) {
        sql_node_t *val = parse_expr(p);
        if (!val) break;
        list_add(p, node->u.insert.values, val);
        if (!match(p, SQL_TOKEN_COMMA)) break;
    }

    return node;
}

static sql_node_t *parse_update(sql_parser_t *p) {
    advance(p);  /* UPDATE */
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); return NULL; }

    sql_node_t *node = alloc_node(p, SQL_NODE_UPDATE);
    if (!node) return NULL;

    node->u.update.table_name = strdup(p->current->lexeme);
    advance(p);

    if (!match(p, SQL_TOKEN_SET)) { err(p, "Expected SET"); sql_node_free(node); return NULL; }

    node->u.update.set_list = alloc_list(p);
    if (!node->u.update.set_list) { sql_node_free(node); return NULL; }

    /* SET col = val */
    while (check(p, SQL_TOKEN_IDENT)) {
        sql_node_t *col = alloc_node(p, SQL_NODE_COLUMN_REF);
        if (col) col->u.column_ref.name = strdup(p->current->lexeme);
        advance(p);

        if (!match(p, SQL_TOKEN_EQ)) { sql_node_free(col); break; }

        sql_node_t *val = parse_expr(p);
        if (!val) { sql_node_free(col); break; }

        sql_node_t *set = alloc_node(p, SQL_NODE_BINARY_OP);
        if (set) { set->u.binary_op.left = col; set->u.binary_op.right = val; }
        list_add(p, node->u.update.set_list, set);

        if (!match(p, SQL_TOKEN_COMMA)) break;
    }

    if (match(p, SQL_TOKEN_WHERE)) {
        node->u.update.where_cond = parse_expr(p);
    }

    return node;
}

static sql_node_t *parse_delete(sql_parser_t *p) {
    advance(p);  /* DELETE */
    if (!match(p, SQL_TOKEN_FROM)) { err(p, "Expected FROM"); return NULL; }
    if (!check(p, SQL_TOKEN_IDENT)) { err(p, "Expected table name"); return NULL; }

    sql_node_t *node = alloc_node(p, SQL_NODE_DELETE);
    if (!node) return NULL;

    node->u.del.table_name = strdup(p->current->lexeme);
    advance(p);

    if (match(p, SQL_TOKEN_WHERE)) {
        node->u.del.where_cond = parse_expr(p);
    }

    return node;
}

static sql_node_t *parse_statement(sql_parser_t *p) {
    if (!p->current) return NULL;
    switch (p->current->type) {
        case SQL_TOKEN_CREATE: return parse_create_table(p);
        case SQL_TOKEN_DROP:   return parse_drop_table(p);
        case SQL_TOKEN_SELECT: return parse_select(p);
        case SQL_TOKEN_INSERT: return parse_insert(p);
        case SQL_TOKEN_UPDATE: return parse_update(p);
        case SQL_TOKEN_DELETE: return parse_delete(p);
        default: err(p, "Expected SQL statement"); return NULL;
    }
}

/* ============================================================
 * 公共 API
 * ============================================================ */

sql_parser_t *sql_parser_create(const char *input) {
    if (!input) return NULL;
    sql_parser_t *p = (sql_parser_t *)calloc(1, sizeof(sql_parser_t));
    if (!p) return NULL;
    p->lexer = sql_lexer_create(input);
    if (!p->lexer) { free(p); return NULL; }
    p->current = sql_lexer_peek(p->lexer);  /* peek 第一个 token */
    return p;
}

void sql_parser_destroy(sql_parser_t *p) {
    if (p) {
        if (p->current && p->current->lexeme) free(p->current->lexeme);
        if (p->lexer) sql_lexer_destroy(p->lexer);
        free(p);
    }
}

sql_node_t *sql_parse(sql_parser_t *p) {
    return p ? parse_statement(p) : NULL;
}

sql_node_t *sql_parse_one(const char *input) {
    g_last_parse_error[0] = '\0';  /* 清除之前的错误 */
    sql_parser_t *p = sql_parser_create(input);
    if (!p) return NULL;
    sql_node_t *node = sql_parse(p);
    if (!node) {
        strncpy(g_last_parse_error, sql_parser_errmsg(p), sizeof(g_last_parse_error) - 1);
        LOG_ERROR("SQL解析失败: %s", sql_parser_errmsg(p));
    }
    sql_parser_destroy(p);
    return node;
}

const char *sql_parser_errmsg(sql_parser_t *p) {
    return p ? p->error_msg : "Invalid parser";
}

/* ============================================================
 * AST 释放和调试
 * ============================================================ */

void sql_node_free(sql_node_t *n) {
    if (!n) return;
    switch (n->type) {
        case SQL_NODE_SELECT:
            if (n->u.select.columns) sql_node_free(n->u.select.columns);
            if (n->u.select.table_name) free(n->u.select.table_name);
            if (n->u.select.where_cond) sql_node_free(n->u.select.where_cond);
            break;
        case SQL_NODE_INSERT:
            if (n->u.insert.table_name) free(n->u.insert.table_name);
            if (n->u.insert.columns) sql_node_free(n->u.insert.columns);
            if (n->u.insert.values) sql_node_free(n->u.insert.values);
            break;
        case SQL_NODE_UPDATE:
            if (n->u.update.table_name) free(n->u.update.table_name);
            if (n->u.update.set_list) sql_node_free(n->u.update.set_list);
            if (n->u.update.where_cond) sql_node_free(n->u.update.where_cond);
            break;
        case SQL_NODE_DELETE:
            if (n->u.del.table_name) free(n->u.del.table_name);
            if (n->u.del.where_cond) sql_node_free(n->u.del.where_cond);
            break;
        case SQL_NODE_CREATE_TABLE:
            if (n->u.create_table.table_name) free(n->u.create_table.table_name);
            if (n->u.create_table.columns) sql_node_free(n->u.create_table.columns);
            break;
        case SQL_NODE_DROP_TABLE:
            if (n->u.drop_table.table_name) free(n->u.drop_table.table_name);
            break;
        case SQL_NODE_EXPR_LIST:
            for (size_t i = 0; i < n->u.list.count; i++) sql_node_free(n->u.list.items[i]);
            free(n->u.list.items);
            break;
        case SQL_NODE_COLUMN_REF:
            if (n->u.column_ref.name) free(n->u.column_ref.name);
            break;
        case SQL_NODE_CONSTANT:
            if (n->u.constant.str_value) free(n->u.constant.str_value);
            break;
        case SQL_NODE_BINARY_OP:
            if (n->u.binary_op.left) sql_node_free(n->u.binary_op.left);
            if (n->u.binary_op.right) sql_node_free(n->u.binary_op.right);
            break;
        case SQL_NODE_LOGICAL_OP:
            if (n->u.logical_op.left) sql_node_free(n->u.logical_op.left);
            if (n->u.logical_op.right) sql_node_free(n->u.logical_op.right);
            if (n->u.logical_op.expr) sql_node_free(n->u.logical_op.expr);
            break;
        case SQL_NODE_IN_LIST:
            if (n->u.in_list.expr) sql_node_free(n->u.in_list.expr);
            for (size_t i = 0; i < n->u.in_list.count; i++) {
                if (n->u.in_list.items[i]) sql_node_free(n->u.in_list.items[i]);
            }
            free(n->u.in_list.items);
            break;
        case SQL_NODE_BETWEEN:
            if (n->u.between.expr) sql_node_free(n->u.between.expr);
            if (n->u.between.min) sql_node_free(n->u.between.min);
            if (n->u.between.max) sql_node_free(n->u.between.max);
            break;
        case SQL_NODE_LIKE:
            if (n->u.like.expr) sql_node_free(n->u.like.expr);
            if (n->u.like.pattern) sql_node_free(n->u.like.pattern);
            break;
        case SQL_NODE_IS_NULL:
            if (n->u.is_null.expr) sql_node_free(n->u.is_null.expr);
            break;
        default: break;
    }
    free(n);
}

void sql_node_print(const sql_node_t *n, int indent) {
    if (!n) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s\n", sql_node_type_name(n->type));
}

const char *sql_node_type_name(sql_node_type_t t) {
    switch (t) {
        case SQL_NODE_SELECT: return "SELECT";
        case SQL_NODE_INSERT: return "INSERT";
        case SQL_NODE_UPDATE: return "UPDATE";
        case SQL_NODE_DELETE: return "DELETE";
        case SQL_NODE_CREATE_TABLE: return "CREATE_TABLE";
        case SQL_NODE_DROP_TABLE: return "DROP_TABLE";
        case SQL_NODE_COLUMN_DEF: return "COLUMN_DEF";
        case SQL_NODE_EXPR_LIST: return "EXPR_LIST";
        case SQL_NODE_COLUMN_REF: return "COLUMN_REF";
        case SQL_NODE_CONSTANT: return "CONSTANT";
        case SQL_NODE_BINARY_OP: return "BINARY_OP";
        case SQL_NODE_LOGICAL_OP: return "LOGICAL_OP";
        case SQL_NODE_IN_LIST: return "IN_LIST";
        case SQL_NODE_BETWEEN: return "BETWEEN";
        case SQL_NODE_LIKE: return "LIKE";
        case SQL_NODE_IS_NULL: return "IS_NULL";
        default: return "UNKNOWN";
    }
}