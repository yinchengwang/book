/**
 * @file graph_cypher.h
 * @brief Cypher 查询语言解析器头文件
 *
 * 实现 Cypher 查询语言的词法分析器和递归下降解析器
 */
#ifndef DB_GRAPH_CYPHER_H
#define DB_GRAPH_CYPHER_H

#include "db/graph/graph.h"
#include "db/graph/graph_property.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Token 定义
 * ======================================================================== */

/** Token 类型 */
typedef enum CypherTokenType_e {
    /* 终结符 */
    CYPHER_TOKEN_EOF = 0,

    /* 关键字 */
    CYPHER_TOKEN_MATCH,
    CYPHER_TOKEN_RETURN,
    CYPHER_TOKEN_WHERE,
    CYPHER_TOKEN_CREATE,
    CYPHER_TOKEN_DELETE,
    CYPHER_TOKEN_SET,
    CYPHER_TOKEN_REMOVE,
    CYPHER_TOKEN_WITH,
    CYPHER_TOKEN_OPTIONAL,
    CYPHER_TOKEN_UNWIND,
    CYPHER_TOKEN_AS,
    CYPHER_TOKEN_DISTINCT,
    CYPHER_TOKEN_ORDER,
    CYPHER_TOKEN_BY,
    CYPHER_TOKEN_SKIP,
    CYPHER_TOKEN_LIMIT,

    /* 模式关键字 */
    CYPHER_TOKEN_NODE,          /* () */
    CYPHER_TOKEN_REL,           /* --> 或 <-- */
    CYPHER_TOKEN_REL_DIR,       /* --> 或 <-- */
    CYPHER_TOKEN_REL_TYPE,     /* :Label */
    CYPHER_TOKEN_VAR_LEN,       /* *1..3 */

    /* 标识符 */
    CYPHER_TOKEN_IDENTIFIER,
    CYPHER_TOKEN_STRING,
    CYPHER_TOKEN_INTEGER,
    CYPHER_TOKEN_FLOAT,

    /* 操作符 */
    CYPHER_TOKEN_EQUAL,
    CYPHER_TOKEN_NOT_EQUAL,
    CYPHER_TOKEN_LESS,
    CYPHER_TOKEN_LESS_EQUAL,
    CYPHER_TOKEN_GREATER,
    CYPHER_TOKEN_GREATER_EQUAL,
    CYPHER_TOKEN_PLUS,
    CYPHER_TOKEN_MINUS,
    CYPHER_TOKEN_MULTIPLY,
    CYPHER_TOKEN_DIVIDE,
    CYPHER_TOKEN_MOD,
    CYPHER_TOKEN_AND,
    CYPHER_TOKEN_OR,
    CYPHER_TOKEN_NOT,
    CYPHER_TOKEN_XOR,

    /* 分隔符 */
    CYPHER_TOKEN_LPAREN,
    CYPHER_TOKEN_RPAREN,
    CYPHER_TOKEN_LBRACKET,
    CYPHER_TOKEN_RBRACKET,
    CYPHER_TOKEN_LBRACE,
    CYPHER_TOKEN_RBRACE,
    CYPHER_TOKEN_COMMA,
    CYPHER_TOKEN_DOT,
    CYPHER_TOKEN_SEMICOLON,
    CYPHER_TOKEN_COLON,
    CYPHER_TOKEN_PIPE,         /* | */
    CYPHER_TOKEN_ARROW,         /* --> 或 <-- */
    CYPHER_TOKEN_ATSIGN,        /* @ */

    /* 函数 */
    CYPHER_TOKEN_AGG_FUNC,      /* count, sum, avg, min, max 等 */
    CYPHER_TOKEN_FUNC_CALL,

    /* 特殊 */
    CYPHER_TOKEN_PARAMETER,     /* $param */
    CYPHER_TOKEN_NULL,
    CYPHER_TOKEN_TRUE,
    CYPHER_TOKEN_FALSE,

    CYPHER_TOKEN_ERROR
} CypherTokenType;

/** Token 结构 */
typedef struct CypherToken_s {
    CypherTokenType type;
    char *lexeme;               /**< 词素 */
    char *raw_value;            /**< 原始值 */
    int line;                   /**< 行号 */
    int column;                 /**< 列号 */
} CypherToken;

/* ========================================================================
 * AST 节点类型
 * ======================================================================== */

/** AST 节点类型 */
typedef enum CypherASTType_e {
    CYPHER_AST_QUERY,
    CYPHER_AST_SINGLE_QUERY,
    CYPHER_AST_PATTERN,
    CYPHER_AST_PATTERN_PART,
    CYPHER_AST_PATTERN_CHAIN,
    CYPHER_AST_NODE_PATTERN,
    CYPHER_AST_REL_PATTERN,
    CYPHER_AST_REL_DIR,         /* INCOMING, OUTGOING, undirected */
    CYPHER_AST_VAR_LEN_PATTERN,
    CYPHER_AST_WHERE,
    CYPHER_AST_RETURN,
    CYPHER_AST_RETURN_ITEM,
    CYPHER_AST_RETURN_LIST,
    CYPHER_AST_ORDER_BY,
    CYPHER_AST_SKIP,
    CYPHER_AST_LIMIT,
    CYPHER_AST_CREATE,
    CYPHER_AST_DELETE,
    CYPHER_AST_SET,
    CYPHER_AST_REMOVE,
    CYPHER_AST_PROPERTY_LOOKUP,
    CYPHER_AST_EXPRESSION,
    CYPHER_AST_FUNCTION_CALL,
    CYPHER_AST_IDENTIFIER,
    CYPHER_AST_LITERAL,
    CYPHER_AST_PARAMETER,
    CYPHER_AST_BINARY_OP,
    CYPHER_AST_UNARY_OP,
    CYPHER_AST_LIST_COMPREHENSION,
    CYPHER_AST_PATTERN_COMPREHENSION,
    CYPHER_AST_WITH,
    CYPHER_AST_UNWIND
} CypherASTType;

/** 属性 */
typedef struct CypherProp_s {
    char *name;
    struct CypherASTNode *value;
    struct CypherProp *next;
} CypherProp;

/** 节点模式 */
typedef struct CypherNodePattern_s {
    char *variable;            /**< 变量名 (可选) */
    char **labels;             /**< 标签数组 */
    size_t num_labels;
    CypherProp *props;          /**< 属性列表 */
} CypherNodePattern;

/** 关系方向 */
typedef enum CypherRelDir_e {
    CYPHER_REL_OUTGOING,        /* --> */
    CYPHER_REL_INCOMING,        /* <-- */
    CYPHER_REL_UNDIRECTED        /* --- */
} CypherRelDir;

/** 变量长度关系 */
typedef struct CypherVarLen_s {
    int min;                    /**< 最小跳数，-1 表示不限制 */
    int max;                    /**< 最大跳数，-1 表示不限制 */
    char *variable;             /**< 路径变量 (可选) */
} CypherVarLen;

/** 关系模式 */
typedef struct CypherRelPattern_s {
    char *variable;             /**< 变量名 (可选) */
    char **rel_types;           /**< 关系类型数组 */
    size_t num_rel_types;
    CypherProp *props;          /**< 属性 */
    CypherRelDir dir;           /**< 方向 */
    CypherVarLen *var_len;      /**< 变量长度 (可选) */
} CypherRelPattern;

/** 模式链 (顶点-边-顶点-边-顶点...) */
typedef struct CypherPatternChain_s {
    CypherNodePattern *node;
    CypherRelPattern *rel;
    struct CypherPatternChain_s *next;
} CypherPatternChain;

/** WHERE 子句 */
typedef struct CypherWhere_s {
    struct CypherASTNode *expr;
} CypherWhere;

/** RETURN 项 */
typedef struct CypherReturnItem_s {
    struct CypherASTNode *expr;
    char *alias;                /**< 别名 (可选) */
} CypherReturnItem;

/** RETURN 子句 */
typedef struct CypherReturn_s {
    CypherReturnItem **items;
    size_t num_items;
    bool distinct;
    CypherASTNode *order_by;
    CypherASTNode *skip;
    CypherASTNode *limit;
} CypherReturn;

/** CREATE 子句 */
typedef struct CypherCreate_s {
    CypherPatternChain **patterns;
    size_t num_patterns;
} CypherCreate;

/** DELETE 子句 */
typedef struct CypherDelete_s {
    CypherASTNode **exprs;
    size_t num_exprs;
    bool detach;                 /**< DETACH DELETE */
} CypherDelete;

/** SET 子句 */
typedef struct CypherSet_s {
    CypherASTNode **items;
    size_t num_items;
} CypherSet;

/** ORDER BY */
typedef struct CypherOrderBy_s {
    CypherASTNode **items;
    size_t num_items;
    bool *ascending;            /**< 每项的升序/降序 */
} CypherOrderBy;

/** 单查询 */
typedef struct CypherSingleQuery_s {
    CypherPatternChain **patterns;  /**< MATCH 的模式 */
    size_t num_patterns;
    CypherWhere *where;          /**< WHERE (可选) */
    CypherReturn *ret;           /**< RETURN (可选) */
    CypherCreate *create;        /**< CREATE (可选) */
    CypherDelete *del;           /**< DELETE (可选) */
    CypherSet *set;             /**< SET (可选) */
    CypherASTNode *with;        /**< WITH (可选) */
} CypherSingleQuery;

/** 完整查询 */
typedef struct CypherQuery_s {
    CypherSingleQuery *query;
    char *error_msg;            /**< 解析错误信息 */
} CypherQuery;

/** AST 节点 */
typedef struct CypherASTNode_s {
    CypherASTType type;
    union {
        CypherQuery query;
        CypherSingleQuery single_query;
        CypherPatternChain pattern_chain;
        CypherNodePattern node_pattern;
        CypherRelPattern rel_pattern;
        CypherWhere where;
        CypherReturn ret;
        CypherCreate create;
        CypherDelete del;
        CypherSet set;
        CypherOrderBy order_by;
        char *identifier;
        struct {
            CypherTokenType literal_type;
            char *value;
        } literal;
        char *parameter;
        struct {
            CypherTokenType op;
            struct CypherASTNode_s *left;
            struct CypherASTNode_s *right;
        } binary_op;
        struct {
            CypherTokenType op;
            struct CypherASTNode_s *operand;
        } unary_op;
        struct {
            char *name;
            struct CypherASTNode_s **args;
            size_t num_args;
        } func_call;
    } u;
} CypherASTNode;

/* ========================================================================
 * 解析器
 * ======================================================================== */

/** Cypher 解析器 */
typedef struct CypherParser_s {
    const char *input;           /**< 输入查询 */
    size_t input_len;
    size_t pos;                  /**< 当前解析位置 */
    int line;                    /**< 当前行号 */
    int column;                  /**< 当前列号 */

    CypherToken current_token;   /**< 当前 Token */
    CypherToken previous_token;  /**< 前一个 Token */

    bool had_error;              /**< 是否发生错误 */
    char error_msg[512];         /**< 错误信息 */

    CypherASTNode *ast;          /**< 解析结果 AST */
} CypherParser;

/**
 * @brief 创建解析器
 */
CypherParser *cypher_parser_create(const char *query, size_t len);

/**
 * @brief 销毁解析器
 */
void cypher_parser_destroy(CypherParser *parser);

/**
 * @brief 解析查询
 * @return AST 根节点，失败返回 NULL
 */
CypherQuery *cypher_parse(const char *query);

/**
 * @brief 解析查询字符串
 * @param query 查询字符串
 * @param out_error 输出错误信息 (可选)
 * @return AST 根节点，失败返回 NULL
 */
CypherQuery *cypher_parse_query(const char *query, char *out_error);

/**
 * @brief 获取解析错误
 */
const char *cypher_get_error(const CypherParser *parser);

/**
 * @brief 释放 AST
 */
void cypher_ast_free(CypherASTNode *node);

/**
 * @brief 释放查询
 */
void cypher_query_free(CypherQuery *query);

/* ========================================================================
 * AST 工具
 * ======================================================================== */

/**
 * @brief 打印 AST (调试用)
 */
void cypher_ast_print(const CypherASTNode *node, int indent);

/**
 * @brief AST 转字符串
 */
char *cypher_ast_to_string(const CypherASTNode *node);

/* ========================================================================
 * 查询执行 (需要与 graph_engine 集成)
 * ======================================================================== */

/** 查询结果 */
typedef struct CypherResult_s {
    char **column_names;         /**< 列名 */
    size_t num_columns;
    void **rows;                 /**< 行数据数组 */
    size_t num_rows;
    size_t row_capacity;
} CypherResult;

/**
 * @brief 释放查询结果
 */
void cypher_result_free(CypherResult *result);

/**
 * @brief 执行 Cypher 查询
 * @param graph 图数据库
 * @param query Cypher 查询
 * @param params 参数 (可选)
 * @return 查询结果
 */
CypherResult *cypher_execute(void *graph, const char *query, void *params);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_CYPHER_H */
