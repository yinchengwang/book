/**
 * @file sql.h
 * @brief SQL 解析器接口
 *
 * 支持的 SQL 语句：
 * - DDL: CREATE TABLE, DROP TABLE
 * - DML: INSERT, UPDATE, DELETE, SELECT
 * - WHERE 条件表达式
 * - 简单数据类型: INT, VARCHAR, TEXT
 *
 * 注意：本文件与 MinGW 系统 sql.h 冲突，使用时确保使用 db/parser/sql/sql.h
 */
#ifndef DB_PARSER_SQL_H
#define DB_PARSER_SQL_H

/* 阻止系统 sql.h 干扰 */
#ifdef SQL_SUCCESS
#undef SQL_SUCCESS
#endif

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Token 类型定义
 * ============================================================ */

/** Token 类型 */
typedef enum sql_token_type_e {
    /* 字面量 */
    SQL_TOKEN_INT,         /**< 整数字面量 */
    SQL_TOKEN_FLOAT,       /**< 浮点数字面量 */
    SQL_TOKEN_STRING,      /**< 字符串字面量 */
    SQL_TOKEN_IDENT,       /**< 标识符（表名、列名） */

    /* 关键字 */
    SQL_TOKEN_SELECT,
    SQL_TOKEN_INSERT,
    SQL_TOKEN_UPDATE,
    SQL_TOKEN_DELETE,
    SQL_TOKEN_CREATE,
    SQL_TOKEN_DROP,
    SQL_TOKEN_TABLE,
    SQL_TOKEN_FROM,
    SQL_TOKEN_WHERE,
    SQL_TOKEN_INTO,
    SQL_TOKEN_VALUES,
    SQL_TOKEN_SET,
    SQL_TOKEN_AND,
    SQL_TOKEN_OR,
    SQL_TOKEN_NOT,
    SQL_TOKEN_IS,
    SQL_TOKEN_IN,
    SQL_TOKEN_LIKE,
    SQL_TOKEN_BETWEEN,
    SQL_TOKEN_NULL,
    SQL_TOKEN_PRIMARY,
    SQL_TOKEN_KEY,
    SQL_TOKEN_TRUE,
    SQL_TOKEN_FALSE,

    /* 类型 */
    SQL_TOKEN_INT_TYPE,
    SQL_TOKEN_VARCHAR,
    SQL_TOKEN_TEXT,
    SQL_TOKEN_BLOB,

    /* 操作符 */
    SQL_TOKEN_EQ,          /**< = */
    SQL_TOKEN_NE,          /**< != 或 <> */
    SQL_TOKEN_LT,          /**< < */
    SQL_TOKEN_LE,          /**< <= */
    SQL_TOKEN_GT,          /**< > */
    SQL_TOKEN_GE,          /**< >= */
    SQL_TOKEN_PLUS,        /**< + */
    SQL_TOKEN_MINUS,       /**< - */
    SQL_TOKEN_STAR,        /**< * */
    SQL_TOKEN_SLASH,       /**< / */

    /* 分隔符 */
    SQL_TOKEN_LPAREN,      /**< ( */
    SQL_TOKEN_RPAREN,      /**< ) */
    SQL_TOKEN_COMMA,       /**< , */
    SQL_TOKEN_SEMICOLON,   /**< ; */

    /* 特殊 */
    SQL_TOKEN_EOF,         /**< 文件结束 */
    SQL_TOKEN_ERROR        /**< 错误 */
} sql_token_type_t;

/** Token 结构 */
typedef struct sql_token_s {
    sql_token_type_t type;     /**< Token 类型 */
    char            *lexeme;    /**< 词素（原始文本） */
    int              int_value; /**< 整数值（用于 INT token） */
    double           float_value; /**< 浮点数值（用于 FLOAT token） */
    size_t           line;      /**< 行号 */
    size_t           column;     /**< 列号 */
} sql_token_t;

/* ============================================================
 * AST 节点类型定义
 * ============================================================ */

/** AST 节点类型 */
typedef enum sql_node_type_e {
    SQL_NODE_SELECT,        /**< SELECT 语句 */
    SQL_NODE_INSERT,       /**< INSERT 语句 */
    SQL_NODE_UPDATE,       /**< UPDATE 语句 */
    SQL_NODE_DELETE,       /**< DELETE 语句 */
    SQL_NODE_CREATE_TABLE, /**< CREATE TABLE 语句 */
    SQL_NODE_DROP_TABLE,   /**< DROP TABLE 语句 */

    SQL_NODE_COLUMN_DEF,   /**< 列定义 */
    SQL_NODE_COLUMN_LIST,  /**< 列列表 */
    SQL_NODE_VALUE_LIST,   /**< 值列表 */
    SQL_NODE_EXPR_LIST,    /**< 表达式列表 */

    /* 表达式 */
    SQL_NODE_EXPR,         /**< 表达式 */
    SQL_NODE_BINARY_OP,    /**< 二元操作表达式 */
    SQL_NODE_UNARY_OP,     /**< 一元操作表达式 */
    SQL_NODE_COLUMN_REF,  /**< 列引用 */
    SQL_NODE_CONSTANT,     /**< 常量值 */
    SQL_NODE_PLACEHOLDER,  /**< 参数占位符 */

    /* 增强的表达式操作符 */
    SQL_NODE_LOGICAL_OP,   /**< AND/OR/NOT 逻辑操作 */
    SQL_NODE_IN_LIST,      /**< IN/NOT IN 列表 */
    SQL_NODE_BETWEEN,      /**< BETWEEN 范围 */
    SQL_NODE_LIKE,         /**< LIKE/NOT LIKE 模式 */
    SQL_NODE_IS_NULL       /**< IS NULL/IS NOT NULL */
} sql_node_type_t;

/** 数据类型 */
typedef enum sql_data_type_e {
    SQL_TYPE_NULL = 0,   /**< NULL 值 */
    SQL_TYPE_INT,         /**< 整数 */
    SQL_TYPE_VARCHAR,     /**< 可变长度字符串 */
    SQL_TYPE_TEXT,        /**< 文本 */
    SQL_TYPE_BLOB,        /**< 二进制大对象 */
    SQL_TYPE_REAL         /**< 浮点数 */
} sql_data_type_t;

/** 列定义 */
typedef struct sql_column_def_s {
    char           *name;       /**< 列名 */
    sql_data_type_t type;       /**< 数据类型 */
    size_t         length;      /**< 类型长度（如 VARCHAR(n)） */
    bool           not_null;    /**< 是否 NOT NULL */
    bool           primary_key; /**< 是否主键 */
} sql_column_def_t;

/** AST 节点 */
typedef struct sql_node_s {
    sql_node_type_t type;          /**< 节点类型 */

    union {
        /* SELECT */
        struct {
            struct sql_node_s *columns;    /**< 列列表或 * */
            char              *table_name; /**< 表名 */
            struct sql_node_s *where_cond;       /**< WHERE 条件 */
        } select;

        /* INSERT */
        struct {
            char              *table_name; /**< 表名 */
            struct sql_node_s *columns;    /**< 列列表 */
            struct sql_node_s *values;     /**< 值列表 */
        } insert;

        /* UPDATE */
        struct {
            char              *table_name; /**< 表名 */
            struct sql_node_s *set_list;  /**< SET 列表 */
            struct sql_node_s *where_cond;     /**< WHERE 条件 */
        } update;

        /* DELETE */
        struct {
            char              *table_name; /**< 表名 */
            struct sql_node_s *where_cond;     /**< WHERE 条件 */
        } del;

        /* CREATE TABLE */
        struct {
            char              *table_name; /**< 表名 */
            struct sql_node_s *columns;    /**< 列定义列表 */
        } create_table;

        /* DROP TABLE */
        struct {
            char *table_name;             /**< 表名 */
        } drop_table;

        /* 列定义 */
        sql_column_def_t column_def;

        /* 列列表/值列表 */
        struct {
            struct sql_node_s **items;    /**< 子节点数组 */
            size_t              count;    /**< 元素数量 */
            size_t              capacity;  /**< 数组容量 */
        } list;

        /* 表达式 */
        struct {
            char *name;                   /**< 列名或函数名 */
        } column_ref;

        struct {
            sql_data_type_t type;          /**< 值类型 */
            int      int_value;            /**< 整数值 */
            double   float_value;         /**< 浮点数值 */
            char    *str_value;            /**< 字符串值 */
        } constant;

        struct {
            sql_node_type_t op;           /**< 操作符类型 */
            struct sql_node_s *left;       /**< 左操作数 */
            struct sql_node_s *right;      /**< 右操作数 */
        } binary_op;

        /* 逻辑操作 (AND/OR/NOT) */
        struct {
            int op;                        /**< 0=AND, 1=OR, 2=NOT */
            struct sql_node_s *left;       /**< 左操作数 (AND/OR) */
            struct sql_node_s *right;      /**< 右操作数 (AND/OR) */
            struct sql_node_s *expr;       /**< 操作数 (NOT) */
        } logical_op;

        /* IN/NOT IN 列表 */
        struct {
            struct sql_node_s *expr;       /**< 被检查的表达式 */
            struct sql_node_s **items;     /**< 列表项 */
            size_t              count;     /**< 列表长度 */
            int                 negated;   /**< 是否为 NOT IN */
        } in_list;

        /* BETWEEN 范围 */
        struct {
            struct sql_node_s *expr;       /**< 被检查的表达式 */
            struct sql_node_s *min;        /**< 范围下限 */
            struct sql_node_s *max;        /**< 范围上限 */
            int                 negated;   /**< 是否为 NOT BETWEEN */
        } between;

        /* LIKE/NOT LIKE 模式 */
        struct {
            struct sql_node_s *expr;       /**< 被匹配的表达式 */
            struct sql_node_s *pattern;    /**< 模式表达式 */
            int                 negated;   /**< 是否为 NOT LIKE */
        } like;

        /* IS NULL/IS NOT NULL */
        struct {
            struct sql_node_s *expr;       /**< 被检查的表达式 */
            int                 is_null;   /**< true=IS NULL, false=IS NOT NULL */
            int                 truth_value; /**< 用于 IS TRUE/FALSE (-1=未设置,0=FALSE,1=TRUE) */
        } is_null;
    } u;
} sql_node_t;

/* ============================================================
 * 词法分析器
 * ============================================================ */

/** 词法分析器 */
typedef struct sql_lexer_s sql_lexer_t;

/**
 * @brief 创建词法分析器
 * @param input SQL 输入字符串
 * @return 词法分析器句柄
 */
sql_lexer_t *sql_lexer_create(const char *input);

/**
 * @brief 销毁词法分析器
 * @param lexer 词法分析器
 */
void sql_lexer_destroy(sql_lexer_t *lexer);

/**
 * @brief 获取下一个 token
 * @param lexer 词法分析器
 * @return Token（调用者不负责释放）
 */
sql_token_t *sql_lexer_next(sql_lexer_t *lexer);

/**
 * @brief 获取当前 token
 * @param lexer 词法分析器
 * @return 当前 token
 */
sql_token_t *sql_lexer_peek(sql_lexer_t *lexer);

/**
 * @brief 恢复之前的 token
 * @param lexer 词法分析器
 */
void sql_lexer_backup(sql_lexer_t *lexer);

/**
 * @brief 获取错误信息
 * @param lexer 词法分析器
 * @return 错误信息
 */
const char *sql_lexer_errmsg(sql_lexer_t *lexer);

/* ============================================================
 * 语法分析器
 * ============================================================ */

/** 语法分析器 */
typedef struct sql_parser_s sql_parser_t;

/**
 * @brief 创建语法分析器
 * @param input SQL 输入字符串
 * @return 语法分析器句柄
 */
sql_parser_t *sql_parser_create(const char *input);

/**
 * @brief 销毁语法分析器
 * @param parser 语法分析器
 */
void sql_parser_destroy(sql_parser_t *parser);

/**
 * @brief 解析 SQL 语句
 * @param parser 语法分析器
 * @return AST 根节点（调用者负责释放）
 */
sql_node_t *sql_parse(sql_parser_t *parser);

/**
 * @brief 解析单挑语句（简化接口）
 * @param input SQL 输入字符串
 * @return AST 根节点，失败返回 NULL
 */
sql_node_t *sql_parse_one(const char *input);

/**
 * @brief 获取最后一次解析错误的详细信息
 * @return 错误信息字符串
 */
const char *sql_get_last_parse_error(void);

/**
 * @brief 获取错误信息
 * @param parser 语法分析器
 * @return 错误信息
 */
const char *sql_parser_errmsg(sql_parser_t *parser);

/* ============================================================
 * AST 操作
 * ============================================================ */

/**
 * @brief 释放 AST 树
 * @param node AST 节点
 */
void sql_node_free(sql_node_t *node);

/**
 * @brief 打印 AST 树（调试用）
 * @param node AST 节点
 * @param indent 缩进
 */
void sql_node_print(const sql_node_t *node, int indent);

/**
 * @brief 获取节点类型的字符串表示
 * @param type 节点类型
 * @return 类型名称
 */
const char *sql_node_type_name(sql_node_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* DB_PARSER_SQL_H */