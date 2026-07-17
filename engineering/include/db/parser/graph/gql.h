/**
 * @file gql.h
 * @brief GQL (Graph Query Language) 解析器接口
 *
 * 支持类 Cypher 语法：
 * - CREATE (n:Label {prop: val})
 * - MATCH (n:Label) RETURN n
 * - MATCH (n)-[r:REL]->(m) RETURN n, r, m
 * - DELETE n / DELETE r
 */
#ifndef DB_GRAPH_GQL_H
#define DB_GRAPH_GQL_H

#include "../graph/types.h"
#include "../graph/graph.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Token 类型
 * ============================================================ */

/** GQL Token 类型 */
typedef enum gql_token_type_e {
    /* 字面量 */
    GQL_TOKEN_INT = 256,       /**< 整数字面量 */
    GQL_TOKEN_FLOAT,           /**< 浮点字面量 */
    GQL_TOKEN_STRING,          /**< 字符串字面量 */
    GQL_TOKEN_IDENT,           /**< 标识符 */
    GQL_TOKEN_PARAMETER,       /**< 参数占位符 $name */

    /* 关键字 */
    GQL_TOKEN_CREATE,
    GQL_TOKEN_MATCH,
    GQL_TOKEN_RETURN,
    GQL_TOKEN_WHERE,
    GQL_TOKEN_DELETE,
    GQL_TOKEN_SET,
    GQL_TOKEN_REMOVE,
    GQL_TOKEN_WITH,
    GQL_TOKEN_AS,
    GQL_TOKEN_DISTINCT,
    GQL_TOKEN_ORDER,
    GQL_TOKEN_BY,
    GQL_TOKEN_ASC,
    GQL_TOKEN_DESC,
    GQL_TOKEN_SKIP,
    GQL_TOKEN_LIMIT,
    GQL_TOKEN_COUNT,
    GQL_TOKEN_SUM,
    GQL_TOKEN_AVG,
    GQL_TOKEN_MIN,
    GQL_TOKEN_MAX,
    GQL_TOKEN_COLLECT,
    GQL_TOKEN_SIZE,
    GQL_TOKEN_TYPE,
    GQL_TOKEN_ID,
    GQL_TOKEN_PROPERTIES,
    GQL_TOKEN_LABELS,
    GQL_TOKEN_SHORTESTPATH,
    GQL_TOKEN_EXISTS,
    GQL_TOKEN_NULL,
    GQL_TOKEN_TRUE,
    GQL_TOKEN_FALSE,

    /* 图模式符号 */
    GQL_TOKEN_LPAREN,          /**< ( */
    GQL_TOKEN_RPAREN,          /**< ) */
    GQL_TOKEN_LBRACKET,        /**< [ */
    GQL_TOKEN_RBRACKET,        /**< ] */
    GQL_TOKEN_LBRACE,          /**< { */
    GQL_TOKEN_RBRACE,          /**< } */

    /* 操作符 */
    GQL_TOKEN_MINUS,           /**< - */
    GQL_TOKEN_PLUS,            /**< + */
    GQL_TOKEN_STAR,            /**< * */
    GQL_TOKEN_SLASH,           /**< / */
    GQL_TOKEN_PERCENT,         /**< % */
    GQL_TOKEN_ARROW,           /**< -> */
    GQL_TOKEN_LARROW,          /**< <- */
    GQL_TOKEN_BIDIR,           /**< <- -> */
    GQL_TOKEN_DOT,             /**< . */
    GQL_TOKEN_COMMA,           /**< , */
    GQL_TOKEN_COLON,           /**< : */
    GQL_TOKEN_PIPE,            /**< | */
    GQL_TOKEN_QUESTION,        /**< ? */
    GQL_TOKEN_EQ,              /**< = */
    GQL_TOKEN_NE,              /**< <> 或 != */
    GQL_TOKEN_LT,              /**< < */
    GQL_TOKEN_LE,              /**< <= */
    GQL_TOKEN_GT,              /**< > */
    GQL_TOKEN_GE,              /**< >= */
    GQL_TOKEN_AND,             /**< AND */
    GQL_TOKEN_OR,              /**< OR */
    GQL_TOKEN_NOT,             /**< NOT */
    GQL_TOKEN_IN,              /**< IN */
    GQL_TOKEN_STARTS_WITH,
    GQL_TOKEN_ENDS_WITH,
    GQL_TOKEN_CONTAINS,
    GQL_TOKEN_IS,
    GQL_TOKEN_NOTNULL,

    /* 辅助 */
    GQL_TOKEN_SEMICOLON,        /**< ; */
    GQL_TOKEN_EOF,
    GQL_TOKEN_ERROR
} gql_token_type_t;

/** Token 结构 */
typedef struct gql_token_s {
    gql_token_type_t type;     /**< Token 类型 */
    char            *lexeme;   /**< 词素（原始文本） */
    union {
        int64_t     int_val;   /**< 整数值 */
        double      float_val; /**< 浮点值 */
        struct {
            char    *str;      /**< 字符串指针 */
            size_t  len;       /**< 字符串长度 */
        } str_val;             /**< 字符串值 */
    } value;
    size_t          line;      /**< 行号 */
    size_t          column;    /**< 列号 */
} gql_token_t;

/* ============================================================
 * AST 节点类型
 * ============================================================ */

/** AST 节点类型 */
typedef enum gql_node_type_e {
    /* 语句 */
    GQL_NODE_QUERY,            /**< 主查询（包含 MATCH/RETURN 等） */
    GQL_NODE_CREATE,           /**< CREATE 语句 */
    GQL_NODE_MERGE,            /**< MERGE 语句 */
    GQL_NODE_SET,              /**< SET 语句 */
    GQL_NODE_DELETE,           /**< DELETE 语句 */
    GQL_NODE_REMOVE,           /**< REMOVE 语句 */

    /* 图模式 */
    GQL_NODE_PATTERN,          /**< 图模式（多个模式元素） */
    GQL_NODE_PATTERN_LIST,      /**< 模式列表 */
    GQL_NODE_NODE_PATTERN,     /**< 节点模式 (n:Label {props}) */
    GQL_NODE_REL_PATTERN,      /**< 关系模式 -[r:TYPE*1..3]-> */
    GQL_NODE_PATH_PATTERN,     /**< 路径模式 */

    /* 子句 */
    GQL_NODE_WHERE,            /**< WHERE 子句 */
    GQL_NODE_RETURN,           /**< RETURN 子句 */
    GQL_NODE_RETURN_ITEM,      /**< RETURN 项 */
    GQL_NODE_ORDER_BY,         /**< ORDER BY 子句 */
    GQL_NODE_ORDER_ITEM,       /**< ORDER BY 项 */
    GQL_NODE_SKIP,             /**< SKIP 子句 */
    GQL_NODE_LIMIT,            /**< LIMIT 子句 */
    GQL_NODE_WITH,             /**< WITH 子句 */

    /* 表达式 */
    GQL_NODE_VARIABLE,         /**< 变量引用 */
    GQL_NODE_PROPERTY,         /**< 属性访问 n.prop */
    GQL_NODE_PROPERTY_MAP,     /**< 属性映射 {key: val, ...} */
    GQL_NODE_FUNCTION_CALL,    /**< 函数调用 COUNT(*), LABELS(n) */
    GQL_NODE_AGGREGATION,      /**< 聚合函数 */
    GQL_NODE_BINARY_OP,        /**< 二元操作表达式 */
    GQL_NODE_UNARY_OP,         /**< 一元操作表达式 */
    GQL_NODE_LIST_EXPR,        /**< 列表表达式 [1, 2, 3] */
    GQL_NODE_RANGE,            /**< 范围表达式 *1..3 */

    /* 其他 */
    GQL_NODE_PARAMETER,        /**< 参数占位符 */
    GQL_NODE_ALIAS,            /**< 别名 AS */
    GQL_NODE_DISTINCT,         /**< DISTINCT 标记 */
    GQL_NODE_CONSTANT          /**< 常量 */
} gql_node_type_t;

/* ============================================================
 * AST 节点
 * ============================================================ */

/** AST 节点 */
typedef struct gql_node_s {
    gql_node_type_t type;      /**< 节点类型 */

    union {
        /* 查询语句 */
        struct {
            struct gql_node_s *patterns;   /**< 图模式 */
            struct gql_node_s *where;      /**< WHERE 条件 */
            struct gql_node_s *ret;        /**< RETURN 子句 */
            struct gql_node_s *order_by;   /**< ORDER BY */
            struct gql_node_s *skip;       /**< SKIP */
            struct gql_node_s *limit;      /**< LIMIT */
        } query;

        /* CREATE 语句 */
        struct {
            struct gql_node_s *patterns;   /**< 要创建的模式 */
        } create;

        /* DELETE 语句 */
        struct {
            struct gql_node_s *targets;    /**< 删除目标 */
            bool               detach_delete; /**< 是否 DETACH DELETE */
        } del;

        /* 节点模式 (n:Label {props}) */
        struct {
            char               *variable;  /**< 变量名 */
            char               *label;     /**< 标签名 */
            struct gql_node_s  *props;     /**< 属性 */
            bool               anonymous;  /**< 是否匿名 () */
        } node_pattern;

        /* 关系模式 -[r:TYPE*1..3]-> */
        struct {
            char               *variable;  /**< 变量名 */
            char               *rel_type;  /**< 关系类型 */
            struct gql_node_s  *props;     /**< 属性 */
            struct gql_node_s  *length;    /**< 长度范围 */
            bool               direction;  /**< 方向: 0=双向, 1=正向, 2=反向 */
            bool               anonymous;  /**< 是否匿名 -[:TYPE]-> */
        } rel_pattern;

        /* 路径模式 */
        struct {
            struct gql_node_s  **patterns; /**< 节点/关系模式数组 */
            size_t              count;     /**< 模式数量 */
        } pattern_list;

        /* 属性 {key: value, ...} */
        struct {
            char               **keys;     /**< 属性名数组 */
            struct gql_node_s  **values;   /**< 值数组 */
            size_t              count;     /**< 属性数量 */
        } property_map;

        /* RETURN 项 */
        struct {
            struct gql_node_s  *expr;      /**< 表达式 */
            char               *alias;     /**< 别名 */
        } return_item;

        /* ORDER BY 项 */
        struct {
            struct gql_node_s  *expr;      /**< 表达式 */
            bool               descending; /**< 是否降序 */
        } order_item;

        /* 二元操作 */
        struct {
            gql_node_type_t   op;          /**< 操作符 */
            struct gql_node_s *left;       /**< 左操作数 */
            struct gql_node_s *right;      /**< 右操作数 */
        } binary_op;

        /* 函数调用 */
        struct {
            char               *name;      /**< 函数名 */
            struct gql_node_s  **args;     /**< 参数数组 */
            size_t              arg_count; /**< 参数数量 */
            bool                distinct;  /**< DISTINCT 标记 */
        } func_call;

        /* 变量 */
        struct {
            char               *name;      /**< 变量名 */
        } variable;

        /* 常量 */
        struct {
            graph_value_type_t value_type; /**< 值类型 */
            graph_value_t      value;      /**< 值 */
        } constant;

        /* 别名 AS */
        struct {
            struct gql_node_s *expr;       /**< 表达式 */
            char               *alias;     /**< 别名 */
        } alias;

        /* 范围 *1..3 */
        struct {
            int                min;        /**< 最小值 */
            int                max;        /**< 最大值 */
        } range;
    } u;
} gql_node_t;

/* ============================================================
 * 解析器接口
 * ============================================================ */

/** 解析器（不透明类型） */
typedef struct gql_parser_s gql_parser_t;

/**
 * @brief 创建解析器
 * @param input GQL 输入字符串
 * @return 解析器句柄
 */
gql_parser_t *gql_parser_create(const char *input);

/**
 * @brief 销毁解析器
 * @param parser 解析器
 */
void gql_parser_destroy(gql_parser_t *parser);

/**
 * @brief 解析 GQL 语句
 * @param parser 解析器
 * @param out_count 输出语句数量
 * @return AST 节点数组（调用者负责释放）
 */
gql_node_t **gql_parse(gql_parser_t *parser, size_t *out_count);

/**
 * @brief 解析单条语句
 * @param parser 解析器
 * @return AST 根节点，失败返回 NULL
 */
gql_node_t *gql_parse_one(gql_parser_t *parser);

/**
 * @brief 获取错误信息
 * @param parser 解析器
 * @return 错误信息
 */
const char *gql_parser_errmsg(const gql_parser_t *parser);

/**
 * @brief 获取错误位置
 * @param parser 解析器
 * @param out_line 输出行号
 * @param out_col 输出列号
 */
void gql_parser_error_pos(const gql_parser_t *parser, size_t *out_line, size_t *out_col);

/**
 * @brief 释放 AST 节点
 * @param node AST 节点
 */
void gql_node_free(gql_node_t *node);

/**
 * @brief 打印 AST（调试用）
 * @param node AST 节点
 * @param indent 缩进
 */
void gql_node_print(const gql_node_t *node, int indent);

/**
 * @brief 获取节点类型名称
 * @param type 节点类型
 * @return 类型名称
 */
const char *gql_node_type_name(gql_node_type_t type);

/* ============================================================
 * 执行引擎
 * ============================================================ */

/** 执行结果 */
typedef struct gql_result_s {
    char        **columns;      /**< 列名数组 */
    size_t      col_count;      /**< 列数 */
    void        **rows;         /**< 行数据数组 */
    size_t      row_count;      /**< 行数 */
} gql_result_t;

/**
 * @brief 执行 GQL 查询
 * @param g 图数据库
 * @param query GQL 查询字符串
 * @param out_result 输出结果
 * @return 0 成功，-1 失败
 */
int gql_exec(graph_t *g, const char *query, gql_result_t **out_result);

/**
 * @brief 执行 AST
 * @param g 图数据库
 * @param node AST 节点
 * @param out_result 输出结果
 * @return 0 成功，-1 失败
 */
int gql_exec_ast(graph_t *g, gql_node_t *node, gql_result_t **out_result);

/**
 * @brief 释放执行结果
 * @param result 执行结果
 */
void gql_result_free(gql_result_t *result);

/**
 * @brief 打印查询结果（调试用）
 * @param result 执行结果
 */
void gql_result_print(const gql_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_GQL_H */
