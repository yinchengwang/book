/**
 * @file sql_parser.h
 * @brief SQL 解析器头文件
 *
 * 实现轻量级 SQL 解析器：
 * 1. 词法分析器（Token 识别）
 * 2. 语法分析器（递归下降）
 * 3. 语义分析器
 */
#ifndef DB_SQL_PARSER_H
#define DB_SQL_PARSER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Token 类型定义
 * ======================================================================== */

/** Token 类型 */
typedef enum SqlTokenType_e {
    /* 字面量 */
    TOKEN_INTEGER = 1,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_NULL,
    TOKEN_BOOL,

    /* 标识符 */
    TOKEN_IDENTIFIER,
    TOKEN_PARAMETER,       /* $1, $2 */
    TOKEN_WILDCARD,        /* * */

    /* 关键字 */
    TOKEN_SELECT,
    TOKEN_FROM,
    TOKEN_WHERE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_IN,
    TOKEN_BETWEEN,
    TOKEN_LIKE,
    TOKEN_IS,
    TOKEN_EXISTS,

    /* JOIN */
    TOKEN_JOIN,
    TOKEN_LEFT,
    TOKEN_RIGHT,
    TOKEN_FULL,
    TOKEN_INNER,
    TOKEN_OUTER,
    TOKEN_CROSS,
    TOKEN_ON,
    TOKEN_USING,

    /* DML */
    TOKEN_INSERT,
    TOKEN_INTO,
    TOKEN_VALUES,
    TOKEN_UPDATE,
    TOKEN_SET,
    TOKEN_DELETE,

    /* DDL */
    TOKEN_CREATE,
    TOKEN_DROP,
    TOKEN_ALTER,
    TOKEN_TABLE,
    TOKEN_INDEX,
    TOKEN_VIEW,
    TOKEN_MATERIALIZED,
    TOKEN_AS,
    TOKEN_IF,
    /* TOKEN_EXISTS 已在前面定义，此处移除 */

    /* 子句 */
    TOKEN_GROUP,
    TOKEN_BY,
    TOKEN_HAVING,
    TOKEN_ORDER,
    TOKEN_ASC,
    TOKEN_DESC,
    TOKEN_LIMIT,
    TOKEN_OFFSET,
    TOKEN_UNION,
    TOKEN_INTERSECT,
    TOKEN_EXCEPT,

    /* 聚合 & 窗口 */
    TOKEN_COUNT,
    TOKEN_SUM,
    TOKEN_AVG,
    TOKEN_MIN,
    TOKEN_MAX,
    TOKEN_WINDOW,
    TOKEN_OVER,
    TOKEN_PARTITION,

    /* 类型 */
    TOKEN_INT,
    TOKEN_INT2,
    TOKEN_INT4,
    TOKEN_INT8,
    TOKEN_FLOAT4,
    TOKEN_FLOAT8,
    TOKEN_NUMERIC,
    TOKEN_DECIMAL,
    TOKEN_VARCHAR,
    TOKEN_CHAR,
    TOKEN_TEXT,
    TOKEN_BLOB,
    TOKEN_DATE,
    TOKEN_TIME,
    TOKEN_TIMESTAMP,
    TOKEN_TIMESTAMPTZ,
    TOKEN_INTERVAL,
    TOKEN_BOOLEAN,

    /* 约束 */
    TOKEN_PRIMARY,
    TOKEN_KEY,
    TOKEN_FOREIGN,
    TOKEN_REFERENCES,
    TOKEN_UNIQUE,
    TOKEN_CHECK,
    TOKEN_DEFAULT,
    TOKEN_NULL_KW,
    TOKEN_CONSTRAINT,
    TOKEN_AUTO_INCREMENT,

    /* 事务 */
    TOKEN_BEGIN,
    TOKEN_COMMIT,
    TOKEN_ROLLBACK,
    TOKEN_SAVEPOINT,
    TOKEN_RELEASE,
    TOKEN_TRANSACTION,
    TOKEN_ISOLATION,
    TOKEN_LEVEL,
    TOKEN_READ,
    TOKEN_WRITE,
    TOKEN_SERIALIZABLE,
    TOKEN_REPEATABLE,
    TOKEN_COMMITTED,
    TOKEN_UNCOMMITTED,

    /* 其他关键字 */
    TOKEN_WITH,
    TOKEN_RECURSIVE,
    TOKEN_CASCADE,
    TOKEN_RESTRICT,
    TOKEN_GRANT,
    TOKEN_REVOKE,
    TOKEN_TO,
    TOKEN_USER,
    TOKEN_ROLE,
    /* TOKEN_INTO 已在前面定义，此处移除 */

    /* ALTER TABLE 相关 */
    TOKEN_ADD,
    TOKEN_COLUMN,
    TOKEN_DATA,
    TOKEN_RENAME,
    TOKEN_TYPE_KW,

    TOKEN_DATABASE,
    TOKEN_SCHEMA,
    TOKEN_CATALOG,

    /* 向量 & 时序 */
    TOKEN_VECTOR,
    TOKEN_EMBEDDING,
    TOKEN_SIMILARITY,
    TOKEN_DISTANCE,
    TOKEN_METRIC,

    /* 时序 */
    TOKEN_TIMESERIES,
    TOKEN_TIME_SERIES,
    TOKEN_TIMESTAMP_,
    TOKEN_TAG,
    TOKEN_FIELD,
    TOKEN_RETENTION,

    /* 物化视图 */
    TOKEN_REFRESH,
    TOKEN_COMPLETE,
    TOKEN_FAST,
    TOKEN_CONCURRENTLY,

    /* 操作符 */
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQ,
    TOKEN_NEQ,
    TOKEN_LT,
    TOKEN_GT,
    TOKEN_LTE,
    TOKEN_GTE,
    TOKEN_CEQ,         /* :: (类型转换) */
    TOKEN_CONCAT,      /* || */
    TOKEN_ARROW,       /* -> (JSON) */
    TOKEN_DARROW,       /* ->> (JSON 文本) */

    /* 分隔符 */
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    TOKEN_DOT,
    TOKEN_DOT_DOT,     /* .. */
    TOKEN_QUALIFIED,   /* :: */

    /* 特殊 */
    TOKEN_CASE,
    TOKEN_WHEN,
    TOKEN_THEN,
    TOKEN_ELSE,
    TOKEN_END,

    /* 结束 */
    TOKEN_EOF,
    TOKEN_ERROR
} SqlTokenType;

/** Token 结构 */
typedef struct SqlToken_s {
    SqlTokenType type;          /**< Token 类型 */
    const char *lexeme;          /**< 词素原文 */
    int lexeme_len;              /**< 词素长度 */
    int location;                /**< 源文件位置 */
    union {
        int64_t ival;            /**< 整数值 */
        double fval;             /**< 浮点值 */
        struct {
            char *str;           /**< 字符串值 */
            int len;             /**< 长度 */
        } sval;
    } val;
} SqlToken;

/** 关键字映射 */
typedef struct SqlKeyword_s {
    const char *keyword;
    SqlTokenType token;
    int len;
} SqlKeyword;

/* ========================================================================
 * 词法分析器
 * ======================================================================== */

/**
 * @brief 词法分析器状态
 */
typedef struct SqlLexer_s {
    const char *input;          /**< 输入 SQL */
    int input_len;               /**< 输入长度 */
    int pos;                     /**< 当前读取位置 */
    int line;                    /**< 当前行号 */
    int col;                     /**< 当前列号 */
    SqlToken current_token;      /**< 当前 Token */
    bool has_current;            /**< 是否有未消费的 Token */
} SqlLexer;

/**
 * @brief 初始化词法分析器
 */
void sql_lexer_init(SqlLexer *lexer, const char *input, int len);

/**
 * @brief 获取下一个 Token
 */
SqlToken sql_lexer_next(SqlLexer *lexer);

/**
 * @brief 查看下一个 Token（不消费）
 */
SqlToken sql_lexer_peek(SqlLexer *lexer);

/**
 * @brief 消费指定类型的 Token
 */
bool sql_lexer_expect(SqlLexer *lexer, SqlTokenType type);

/**
 * @brief 匹配并消费 Token
 */
bool sql_lexer_match(SqlLexer *lexer, SqlTokenType type);

/**
 * @brief 检查 Token 类型
 */
bool sql_lexer_check(SqlLexer *lexer, SqlTokenType type);

/**
 * @brief 获取 Token 的文本表示
 */
const char *sql_token_name(SqlTokenType type);

/* ========================================================================
 * AST 节点类型
 * ======================================================================== */

/** AST 节点类型 */
typedef enum SqlAstType_e {
    AST_SELECTStmt,
    AST_InsertStmt,
    AST_UpdateStmt,
    AST_DeleteStmt,
    AST_CreateTableStmt,
    AST_DropTableStmt,
    AST_AlterTableStmt,
    AST_CreateIndexStmt,
    AST_DropIndexStmt,
    AST_CreateMViewStmt,
    AST_RefreshMViewStmt,
    AST_TransactionStmt,
    AST_TruncateStmt,
    AST_GrantStmt,
    AST_RevokeStmt,

    /* 表达式 */
    AST_ColumnRef,
    AST_ParamRef,
    AST_ConstValue,
    AST_AExpr,
    AST_BoolExpr,
    AST_NullTest,
    AST_ArrayExpr,
    AST_SubLink,
    AST_FuncCall,
    AST_TypeCast,
    AST_CaseExpr,
    AST_CaseWhen,
    AST_CoalesceExpr,
    AST_In,
    AST_Between,
    AST_Like,

    /* 列表 */
    AST_List,
    AST_RangeVar,
    AST_Relation,
    AST_JoinExpr,
    AST_Alias,

    /* 目标列 */
    AST_TargetEntry,
    AST_TargetList,

    /* 子句 */
    AST_FromClause,
    AST_WhereClause,
    AST_GroupBy,
    AST_Having,
    AST_OrderBy,
    AST_Limit,
    AST_WindowClause,

    /* 窗口函数 */
    AST_WindowDef,
    AST_WindowFunc,

    /* 表表达式 */
    AST_TableSample,
    AST_LockingClause,

    /* 列定义 */
    AST_ColumnDef,
    AST_Constraint,

    /* 索引定义 */
    AST_IndexElem,

    /* 插入值 */
    AST_ValuesList,
    AST_InsertTarget,

    /* 更新/删除 */
    AST_UpdateTarget,
    AST_DeleteTarget
} SqlAstType;

/* ========================================================================
 * 表达式节点
 * ======================================================================== */

/** 列引用 */
typedef struct SqlColumnRef_s {
    SqlAstType type;
    char *name;                  /**< 列名 */
    char *relname;               /**< 表名（可选） */
    char *schema;                /**< 模式名（可选） */
    int location;                /**< 位置 */
} SqlColumnRef;

/** 参数引用 */
typedef struct SqlParamRef_s {
    SqlAstType type;
    int number;                  /**< 参数编号 */
    int location;
} SqlParamRef;

/** 常量值 */
typedef struct SqlConst_s {
    SqlAstType type;
    union {
        int64_t ival;
        double fval;
        char *sval;
    } val;
    int type_oid;                /**< 类型 OID */
    int location;
} SqlConst;

/** 操作符表达式 */
typedef struct SqlAExpr_s {
    SqlAstType type;
    char *name;                  /**< 操作符名 */
    void *lexpr;                 /**< 左操作数 */
    void *rexpr;                 /**< 右操作数 */
    int location;
} SqlAExpr;

/** 函数调用 */
typedef struct SqlFuncCall_s {
    SqlAstType type;
    char *funcname;              /**< 函数名 */
    void *args;                  /**< 参数列表 */
    bool agg_star;               /**< COUNT(*) */
    bool agg_distinct;           /**< DISTINCT */
    void *agg_order;             /**< 聚合 ORDER BY */
    void *agg_filter;            /**< 聚合 FILTER */
    void *over;                  /**< 窗口定义 */
    int location;
} SqlFuncCall;

/** 类型转换 */
typedef struct SqlTypeCast_s {
    SqlAstType type;
    void *expr;                  /**< 被转换表达式 */
    char *type_name;               /**< 目标类型名 */
    int location;
} SqlTypeCast;

/** IN 表达式 */
typedef struct SqlIn_s {
    SqlAstType type;
    void *expr;                  /**< 左表达式 */
    void *list;                  /**< IN 列表或子查询 */
    bool not_in;                 /**< NOT IN */
    int location;
} SqlIn;

/** BETWEEN 表达式 */
typedef struct SqlBetween_s {
    SqlAstType type;
    void *expr;                  /**< 被测表达式 */
    void *left;                  /**< 下界 */
    void *right;                /**< 上界 */
    bool not_between;            /**< NOT BETWEEN */
    int location;
} SqlBetween;

/** LIKE 表达式 */
typedef struct SqlLike_s {
    SqlAstType type;
    void *expr;                  /**< 被匹配表达式 */
    void *pattern;               /**< 模式 */
    void *escape;                /**< 转义字符（可选） */
    bool not_like;               /**< NOT LIKE */
    int location;
} SqlLike;

/** NULL 测试 */
typedef struct SqlNullTest_s {
    SqlAstType type;
    void *expr;                  /**< 被测表达式 */
    bool is_not_null;            /**< IS NOT NULL */
    int location;
} SqlNullTest;

/** CASE 表达式 */
typedef struct SqlCaseWhen_s {
    void *expr;                  /**< WHEN 条件 */
    void *result;                /**< THEN 结果 */
} SqlCaseWhen;

typedef struct SqlCaseExpr_s {
    SqlAstType type;
    void *arg;                   /**< CASE 的参数（可选） */
    SqlCaseWhen *whens;          /**< WHEN 列表 */
    int when_count;
    void *defresult;             /**< ELSE 结果 */
    int location;
} SqlCaseExpr;

/** 子查询 */
typedef struct SqlSubLink_s {
    SqlAstType type;
    int oper;                    /**< EXISTS/IN/ALL/ANY 等 */
    void *subselect;             /**< 子查询 AST */
    int location;
} SqlSubLink;

/** 布尔表达式 */
typedef struct SqlBoolExpr_s {
    SqlAstType type;
    int bool_op;                 /**< AND/OR/NOT */
    void **args;                 /**< 子表达式数组 */
    int arg_count;
    int location;
} SqlBoolExpr;

/* ========================================================================
 * 表引用节点
 * ======================================================================== */

/** 范围变量（表引用） */
typedef struct SqlRangeVar_s {
    SqlAstType type;
    char *catalogname;           /**< 目录名 */
    char *schemaname;            /**< 模式名 */
    char *relname;                /**< 表名 */
    char *alias;                  /**< 别名 */
    int inh;                     /**< 是否包含继承 */
    int location;
} SqlRangeVar;

/** 连接表达式 */
typedef struct SqlJoinExpr_s {
    SqlAstType type;
    int jointype;                 /**< 连接类型 */
    void *larg;                   /**< 左表 */
    void *rarg;                   /**< 右表 */
    void *quals;                  /**< ON 条件 */
    void *using_clause;           /**< USING 列 */
    int location;
} SqlJoinExpr;

/* ========================================================================
 * 语句节点
 * ======================================================================== */

/** 目标列条目 */
typedef struct SqlTargetEntry_s {
    SqlAstType type;
    void *expr;                  /**< 表达式 */
    char *name;                  /**< 结果列名 */
    char *resname;               /**< 结果别名 */
    int resno;                   /**< 结果编号 */
    int location;
} SqlTargetEntry;

/** SELECT 语句 */
typedef struct SqlSelectStmt_s {
    SqlAstType type;
    void *target_list;           /**< SELECT 目标列 */
    void *from_clause;           /**< FROM 子句 */
    void *where_clause;          /**< WHERE 子句 */
    void *group_by;              /**< GROUP BY */
    void *having_clause;         /**< HAVING */
    void *window_clause;         /**< WINDOW */
    void *sort_clause;           /**< ORDER BY */
    void *limit_count;           /**< LIMIT */
    void *limit_offset;          /**< OFFSET */
    bool distinct;               /**< DISTINCT */
    void *locking_clause;        /**< FOR UPDATE 等 */
    int location;
} SqlSelectStmt;

/** INSERT 语句 */
typedef struct SqlInsertStmt_s {
    SqlAstType type;
    char *relation;              /**< 目标表 */
    void *cols;                  /**< 列名列表 */
    void *select_stmt;           /**< SELECT 或 VALUES */
    void *on_conflict;           /**< ON CONFLICT */
    char *returning_list;        /**< RETURNING */
    int location;
} SqlInsertStmt;

/** UPDATE 语句 */
typedef struct SqlUpdateStmt_s {
    SqlAstType type;
    char *relation;              /**< 目标表 */
    void *target_list;           /**< SET 子句 */
    void *where_clause;          /**< WHERE 子句 */
    void *from_clause;           /**< FROM 子句 */
    char *returning_list;         /**< RETURNING */
    int location;
} SqlUpdateStmt;

/** DELETE 语句 */
typedef struct SqlDeleteStmt_s {
    SqlAstType type;
    char *relation;              /**< 目标表 */
    void *using_clause;          /**< USING 子句 */
    void *where_clause;          /**< WHERE 子句 */
    char *returning_list;        /**< RETURNING */
    int location;
} SqlDeleteStmt;

/** CREATE TABLE 语句 */
typedef struct SqlCreateTableStmt_s {
    SqlAstType type;
    char *name;                  /**< 表名 */
    void *table_elts;            /**< 列和约束定义 */
    void *inh_relnames;          /**< 继承的表 */
    char *tablespacename;        /**< 表空间 */
    int location;
} SqlCreateTableStmt;

/** 列定义 */
typedef struct SqlColumnDef_s {
    SqlAstType type;
    char *colname;               /**< 列名 */
    char *type_name;               /**< 类型名 */
    void *raw_default;          /**< 默认值表达式 */
    bool is_not_null;
    bool is_primary_key;
    bool is_unique;
    char *coll_clause;           /**< COLLATE 子句 */
    void *constraints;           /**< 约束列表 */
    int location;
} SqlColumnDef;

/** 约束定义 */
typedef struct SqlConstraint_s {
    SqlAstType type;
    int contype;                 /**< 约束类型 */
    char *name;                  /**< 约束名 */
    void *keys;                  /**< 约束涉及的列 */
    void *raw_expr;             /**< CHECK 表达式 */
    char *fk_match;             /**< 外键匹配类型 */
    void *fk_actions;           /**< 外键动作 */
    int location;
} SqlConstraint;

/** DROP TABLE 语句 */
typedef struct SqlDropTableStmt_s {
    SqlAstType type;
    void *relations;             /**< 表列表 */
    bool behavior;               /**< CASCADE/RESTRICT */
    bool missing_ok;             /**< IF EXISTS */
    int location;
} SqlDropTableStmt;

/** ALTER TABLE 操作类型 */
typedef enum AlterTableOp_e {
    AT_AddColumn = 1,       /**< ADD COLUMN */
    AT_DropColumn = 2,      /**< DROP COLUMN */
    AT_AlterColumnType = 3, /**< ALTER COLUMN TYPE */
    AT_RenameColumn = 4     /**< RENAME COLUMN */
} AlterTableOp;

/** ALTER TABLE 子命令 */
typedef struct AlterTableCmd_s {
    SqlAstType type;
    AlterTableOp subtype;          /**< 子类型 */
    char *name;                    /**< 列名 */
    char *new_name;                /**< 新列名（RENAME） */
    char *type_name;               /**< 新类型名（ALTER TYPE） */
    char *default_expr;            /**< 默认值表达式（ADD） */
    bool not_null;                 /**< NOT NULL（ADD） */
    int location;
} AlterTableCmd;

/** ALTER TABLE 语句 */
typedef struct AlterTableStmt_s {
    SqlAstType type;
    char *relation;                /**< 表名 */
    int num_cmds;                  /**< 命令数量 */
    AlterTableCmd **cmds;          /**< 命令数组 */
    int location;
} AlterTableStmt;

/** 事务语句 */
typedef struct SqlTransactionStmt_s {
    SqlAstType type;
    int kind;                    /**< BEGIN/COMMIT/ROLLBACK */
    char *savepoint_name;        /**< 保存点名 */
    char *options;               /**< 事务选项 */
    int location;
} SqlTransactionStmt;

/** 物化视图刷新语句 */
typedef struct SqlRefreshMViewStmt_s {
    SqlAstType type;
    char *relation;              /**< 视图名 */
    bool concurrent;             /**< CONCURRENTLY */
    bool with_check;             /**< WITH DATA */
    int location;
} SqlRefreshMViewStmt;

/* ========================================================================
 * 通用 AST 节点
 * ======================================================================== */

/** AST 列表节点 */
typedef struct SqlList_s {
    SqlAstType type;
    void **items;                /**< 元素数组 */
    int length;                  /**< 元素数量 */
    int location;
} SqlList;

/** 通用 AST 节点（用于未特化的节点） */
typedef struct SqlNode_s {
    SqlAstType type;
    int location;
    union {
        SqlSelectStmt select;
        SqlInsertStmt insert;
        SqlUpdateStmt update;
        SqlDeleteStmt del_stmt;
        SqlColumnRef column;
        SqlConst constant;
        SqlAExpr aexpr;
        SqlFuncCall func;
        SqlList list;
        AlterTableStmt alter;
    } node;
} SqlNode;

/** 解析结果 */
typedef struct SqlParseResult_s {
    bool success;                /**< 是否成功 */
    void *stmt;                  /**< 解析出的 AST */
    char *errmsg;                /**< 错误信息 */
    int err_location;            /**< 错误位置 */
} SqlParseResult;

/* ========================================================================
 * 解析器 API
 * ======================================================================== */

/**
 * @brief 创建解析器
 */
void *sql_parser_create(void);

/**
 * @brief 销毁解析器
 */
void sql_parser_destroy(void *parser);

/**
 * @brief 解析 SQL 语句
 *
 * @param parser 解析器句柄
 * @param sql SQL 语句
 * @param len SQL 长度（-1 表示自动检测）
 * @return 解析结果
 */
SqlParseResult *sql_parse(void *parser, const char *sql, int len);

/**
 * @brief 解析单行 SQL
 */
SqlParseResult *sql_parse_one(const char *sql, int len);

/**
 * @brief 解析多个语句
 */
SqlList *sql_parse_multiple(const char *sql, int len);

/**
 * @brief 获取最后一条错误信息
 */
const char *sql_parser_get_error(void *parser);

/**
 * @brief 获取错误位置
 */
int sql_parser_get_error_location(void *parser);

/* ========================================================================
 * AST 操作
 * ======================================================================== */

/**
 * @brief 释放 AST 节点
 */
void sql_ast_free(void *node);

/**
 * @brief 复制 AST 节点
 */
void *sql_ast_copy(const void *node);

/**
 * @brief 获取 AST 节点类型名
 */
const char *sql_ast_type_name(SqlAstType type);

/**
 * @brief 打印 AST（调试用）
 */
void sql_ast_print(const void *node, int indent);

/* ========================================================================
 * 语义分析器
 * ======================================================================== */

/** 语义分析上下文 */
typedef struct SqlSemContext_s {
    void *state;                /**< 内部状态 */
} SqlSemContext;

/**
 * @brief 创建语义分析上下文
 */
SqlSemContext *sql_sem_create(void);

/**
 * @brief 销毁语义分析上下文
 */
void sql_sem_destroy(SqlSemContext *ctx);

/**
 * @brief 执行语义分析
 *
 * @param ctx 语义分析上下文
 * @param stmt 解析后的 AST
 * @return 分析后的 AST
 */
void *sql_sem_analyze(SqlSemContext *ctx, void *stmt);

/**
 * @brief 解析列引用
 */
void *sql_sem_resolve_column(void *ctx, const char *schema,
                            const char *table, const char *column);

/**
 * @brief 获取表的所有列
 */
void *sql_sem_get_table_columns(void *ctx, const char *schema,
                               const char *table);

/* ========================================================================
 * 错误恢复
 * ======================================================================== */

/** 错误恢复模式 */
typedef enum SqlErrorRecoveryMode_e {
    ERROR_RECOVERY_NONE = 0,     /**< 不恢复 */
    ERROR_RECOVERY_SKIP,         /**< 跳过错误 Token */
    ERROR_RECOVERY_INFER         /**< 推断并修复 */
} SqlErrorRecoveryMode;

/**
 * @brief 设置错误恢复模式
 */
void sql_parser_set_error_recovery(void *parser, SqlErrorRecoveryMode mode);

/**
 * @brief 获取错误恢复建议
 */
const char *sql_parser_get_recovery_suggestion(void *parser);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_PARSER_H */
