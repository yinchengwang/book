/**
 * @file parsenodes.h
 * @brief SQL 解析器节点类型定义
 *
 * 包含 Token 定义、AST 节点标签和基础节点结构。
 * 本文件是 Flex/Bison 词法/语法分析器的公共头文件。
 */

#ifndef DB_PARSER_SQL_PARSENODES_H
#define DB_PARSER_SQL_PARSENODES_H

#include <stdint.h>
#include <stdbool.h>

/* Windows 头文件冲突处理：取消 Windows 定义的冲突宏
 * Windows headers define many common words as macros
 */
#ifdef _WIN32
#pragma push_macro("DATE")
#pragma push_macro("TIME")
#pragma push_macro("TEXT")
#pragma push_macro("TEXT_P")
#pragma push_macro("BOOLEAN_P")
#pragma push_macro("DEFAULT")
#pragma push_macro("IDENT")
#pragma push_macro("CHAR_P")
#pragma push_macro("INT_P")
#pragma push_macro("REAL")
#pragma push_macro("FLOAT_P")
#pragma push_macro("FLOAT")
#pragma push_macro("BLOB")
#pragma push_macro("USING")
#pragma push_macro("CASCADE")
#pragma push_macro("RESTRICT")
#pragma push_macro("CONSTRAINT")
#pragma push_macro("IN")
#pragma push_macro("OUT")
#pragma push_macro("AND")
#pragma push_macro("OR")
#undef DATE
#undef TIME
#undef TEXT
#undef TEXT_P
#undef BOOLEAN_P
#undef DEFAULT
#undef IDENT
#undef CHAR_P
#undef INT_P
#undef REAL
#undef FLOAT_P
#undef FLOAT
#undef BLOB
#undef USING
#undef CASCADE
#undef RESTRICT
#undef CONSTRAINT
#undef IN
#undef OUT
#undef AND
#undef OR
#define PARSENODES_UNDEF_WINDOWS_MACROS 1
#endif

/* ============================================================
 * Token 类型定义（Bison 生成）
 * ============================================================ */

/* 操作符常量 */
#define NE  1   /* != 或 <> */
#define LE  2   /* <= */
#define GE  3   /* >= */
#define CONCAT 4    /* || */
#define JSON_OP 5   /* -> */
#define JSON_TEXT_OP 6  /* ->> */

/* ============================================================
 * Token 声明（供 Bison 使用）
 * ============================================================ */

/* 关键字 */
#define SELECT       270
#define FROM         271
#define WHERE        272
#define INSERT       273
#define INTO         274
#define VALUES       275
#define UPDATE       276
#define SET          277
#define DELETE       278
#define CREATE       279
#define DROP         280
#define ALTER        281
#define TABLE        282
#define INDEX        283
#define VIEW         284
#define DATABASE     285
#define SCHEMA       286
#define AND          287
#define OR           288
#define NOT          289
#define NULL_P       290
#define TRUE_P       291
#define FALSE_P      292
#define IS           293
#define IN           294
#define LIKE         295
#define BETWEEN      296
#define CASE         297
#define WHEN         298
#define THEN         299
#define ELSE         300
#define END          301
#define GROUP        302
#define BY           303
#define HAVING       304
#define ORDER        305
#define ASC          306
#define DESC         307
#define LIMIT        308
#define OFFSET       309
#define JOIN         310
#define LEFT         311
#define RIGHT        312
#define FULL         313
#define INNER        314
#define OUTER        315
#define CROSS        316
#define ON           317
#define AS           318
#define DISTINCT     319
#define ALL          320
#define UNION        321
#define INTERSECT    322
#define EXCEPT       323
#define WITH         324
#define RECURSIVE    325
#define PRIMARY      326
#define KEY          327
#define FOREIGN      328
#define REFERENCES   329
#define UNIQUE       330
#define CHECK        331
#define CONSTRAINT   332
#define INT_P        333
#define BIGINT       334
#define SMALLINT     335
#define TINYINT      336
#define REAL         337
#define DOUBLE_P     338
#define FLOAT_P      339
#define DECIMAL_P    340
#define NUMERIC      341
#define VARCHAR      342
#define CHAR_P       343
#define CHARACTER    344
#define TEXT_P       345
#define BOOLEAN_P    346
#define DATE         347
#define TIME         348
#define TIMESTAMP    349
#define BLOB         350
#define DEFAULT      351
#define USING        352
#define CASCADE      353
#define RESTRICT     354

/* 字面量类型 */
#define IDENT        400
#define ICONST       401
#define FCONST       402
#define SCONST       403
#define XCONST       404

/* 操作符 */
#define Op           410

/* 特殊 */
#define YYEOF        0

/* ============================================================
 * 语义值类型
 * ============================================================ */

/* YYSTYPE 定义 - 供 Bison 生成的解析器使用 */
#ifndef YYSTYPE_DEFINED
#define YYSTYPE_DEFINED
typedef union YYSTYPE {
    int     ival;       /* 整数值 */
    double  fval;       /* 浮点数值 */
    char   *str;        /* 字符串值 */
} YYSTYPE;
#endif

/* 全局语义值 */
extern YYSTYPE yylval;

/* ============================================================
 * 位置信息
 * ============================================================ */

typedef struct YYLTYPE {
    int line;
    int column;
} YYLTYPE;

extern YYLTYPE yylloc;

/* ============================================================
 * AST 节点标签
 * ============================================================ */

typedef enum NodeTag {
    T_Node = 0,
    T_List,
    T_IntList,
    T_OidList,
    T_Value,
    T_Integer,
    T_Float,
    T_String,
    T_Null,

    /* DML */
    T_SelectStmt,
    T_InsertStmt,
    T_UpdateStmt,
    T_DeleteStmt,
    T_MergeStmt,

    /* DDL */
    T_CreateStmt,
    T_CreateIndexStmt,
    T_CreateViewStmt,
    T_DropStmt,
    T_AlterStmt,

    /* 表达式 */
    T_A_Expr,
    T_ColumnRef,
    T_A_Const,
    T_FuncCall,
    T_A_Indirection,
    T_TypeCast,
    T_CaseExpr,
    T_SubLink,
    T_WindowDef,
    T_TypeName,
    T_BoolExpr,
    T_NullTest,

    /* 子句 */
    T_ResTarget,
    T_RangeVar,
    T_RangeSubselect,
    T_JoinExpr,
    T_SortBy,
    T_GroupClause,
    T_ColumnDef,
    T_CaseWhen,

    /* 其他 */
    T_RawStmt,
    T_CommentStmt,
    T_VariableSetStmt,

    /* 内存上下文子系统 */
    T_MemoryContext,      /* 内存上下文基类型 */
    T_AllocSetContext,    /* AllocSet 分配器实现 */

    /* 执行器标签起点 - 与解析期标签保持间距，便于未来扩展 */
    T_ExecutorBase = 100,

    /* Executor state */
    T_EState,
    T_ExprContext,
    T_ExprState,
    T_ProjectionInfo,
    T_ResultRelInfo,
    T_TupleTableSlot,
    T_TupleDesc,
    T_DestReceiver,
    T_QueryDesc,           /* Task 1.4: 查询描述符 */

    /* Plan nodes */
    T_Plan,
    T_Result,
    T_Append,
    T_SeqScan,
    T_IndexScan,
    T_NestLoop,
    T_HashJoin,
    T_Hash,
    T_Sort,
    T_Agg,
    T_WindowAgg,
    T_Limit,
    T_ModifyTable,
    T_ProjectSet,
    T_Gather,

    /* Parallel execution nodes */
    T_ParallelContext,
    T_ParallelCoordinator,
    T_ParallelHashJoinState,
    T_GatherMerge,
    T_GatherMergeState,

    /* Plan state nodes */
    T_PlanState,
    T_ResultState,
    T_AppendState,
    T_SeqScanState,
    T_IndexScanState,
    T_NestLoopState,
    T_HashJoinState,
    T_HashState,
    T_SortState,
    T_AggState,
    T_HashAggState,
    T_WindowAggState,
    T_LimitState,
    T_ModifyTableState,
    T_ProjectSetState,
    T_GatherState,

    /* Expression nodes */
    T_Expr,
    T_Var,
    T_Const,
    T_Param,
    T_Aggref,
    T_WindowFunc,
    T_FuncExpr,
    T_OpExpr,
    T_BoolExprNode,        /* 执行器重命名，与解析期 T_BoolExpr 区分 */
    T_CaseExprNode,        /* 执行器重命名，与解析期 T_CaseExpr 区分 */
    T_CaseWhenNode,        /* 执行器重命名，与解析期 T_CaseWhen 区分 */
    T_CaseTestExpr,
    T_ArrayExpr,
    T_RowExpr,
    T_NullTestNode,        /* 执行器重命名，与解析期 T_NullTest 区分 */
    T_BooleanTest,
    T_CoalesceExpr,
    T_NullIfExpr,
    T_DistinctExpr,

    /* Trigger */
    T_TriggerDesc,
    T_TriggerData,

    /* Partition - Task 4.3: 分区表路由 */
    T_PartitionDesc,
    T_PartitionRoutingCtx,

    /* Sentinel */
    T_Invalid = 0
} NodeTag;

/* ============================================================
 * 基础节点结构
 * ============================================================ */

typedef struct Node {
    NodeTag type;
} Node;

typedef struct Value {
    NodeTag type;
    union {
        int ival;
        double fval;
        char *str;
    } val;
} Value;

typedef struct Integer {
    NodeTag type;
    int ival;
} Integer;

typedef struct Float {
    NodeTag type;
    double fval;
} Float;

typedef struct String {
    NodeTag type;
    char *str;
} String;

typedef struct Null {
    NodeTag type;
} Null;

/* ============================================================
 * 列表结构
 * ============================================================ */

typedef struct ListCell {
    void *data;
    struct ListCell *next;
} ListCell;

typedef struct List {
    NodeTag type;
    int length;
    ListCell *head;
} List;

/* 列表操作宏 */
#define lfirst(lc)      ((lc)->data)
#define lnext(lc)       ((lc)->next)
#define list_length(l)  ((l) ? (l)->length : 0)

/* 列表遍历宏 */
#define foreach(cell, l) \
    for ((cell) = (l) ? (l)->head : NULL; (cell) != NULL; (cell) = (cell)->next)

/* ============================================================
 * 表达式类型
 * ============================================================ */

/* A_Expr 类型 */
typedef enum A_Expr_Kind {
    AEXPR_OP,               /* 普通操作符 */
    AEXPR_OP_ANY,           /* ANY 操作符 */
    AEXPR_IN,               /* IN 表达式 */
    AEXPR_LIKE,             /* LIKE 表达式 */
    AEXPR_ILIKE,            /* ILIKE 表达式 */
    AEXPR_SIMILAR,          /* SIMILAR TO */
    AEXPR_BETWEEN,          /* BETWEEN */
    AEXPR_NOT_BETWEEN,      /* NOT BETWEEN */
    AEXPR_NULLIF,           /* NULLIF */
    AEXPR_COALESCE,         /* COALESCE */
} A_Expr_Kind;

/* 布尔表达式类型 */
typedef enum BoolType {
    AND_EXPR,
    OR_EXPR,
    NOT_EXPR,
} BoolType;

/* NULL 测试类型 */
typedef enum NullTestType {
    IS_NULL,
    IS_NOT_NULL,
} NullTestType;

/* 连接类型 */
typedef enum JoinType {
    JOIN_INNER,
    JOIN_LEFT,
    JOIN_RIGHT,
    JOIN_FULL,
    JOIN_CROSS,
} JoinType;

/* 排序方向 */
typedef enum SortByDir {
    SORTBY_DEFAULT,
    SORTBY_ASC,
    SORTBY_DESC,
} SortByDir;

/* NULL 排序方式 */
typedef enum SortByNulls {
    SORTBY_NULLS_DEFAULT,
    SORTBY_NULLS_FIRST,
    SORTBY_NULLS_LAST,
} SortByNulls;

/* 集合操作类型 */
typedef enum SetOperation {
    SETOP_NONE = 0,
    SETOP_UNION,
    SETOP_INTERSECT,
    SETOP_EXCEPT,
} SetOperation;

/* DDL 对象类型 */
typedef enum ObjectType {
    OBJECT_TABLE,
    OBJECT_INDEX,
    OBJECT_VIEW,
    OBJECT_FUNCTION,
    OBJECT_SCHEMA,
} ObjectType;

/* ============================================================
 * DML 语句节点
 * ============================================================ */

/* 前向声明 */
struct RangeVar;
typedef struct RangeVar RangeVar;

/* SELECT 语句 */
typedef struct SelectStmt {
    NodeTag type;

    /* SELECT 子句 */
    List *targetList;       /* 目标列列表 */
    List *intoClause;       /* INTO 子句（SELECT INTO） */
    bool distinctClause;    /* DISTINCT */

    /* FROM 子句 */
    List *fromClause;       /* FROM 表引用列表 */

    /* WHERE 子句 */
    Node *whereClause;      /* WHERE 条件 */

    /* GROUP BY 子句 */
    List *groupClause;      /* GROUP BY 列表 */
    Node *havingClause;     /* HAVING 条件 */

    /* WINDOW 子句 */
    List *windowClause;     /* WINDOW 定义 */

    /* ORDER BY 子句 */
    List *sortClause;       /* ORDER BY 列表 */

    /* LIMIT/OFFSET */
    Node *limitCount;       /* LIMIT 值 */
    Node *limitOffset;      /* OFFSET 值 */

    /* 集合操作 */
    List *withClause;       /* WITH 子句 */
    struct SelectStmt *larg; /* 左操作数（UNION/INTERSECT/EXCEPT） */
    struct SelectStmt *rarg; /* 右操作数 */
    SetOperation op;        /* 集合操作类型 */
    bool all;               /* 是否 ALL */
} SelectStmt;

/* INSERT 语句 */
typedef struct InsertStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *cols;             /* 列名列表 */
    List *valuesLists;      /* VALUES 列表 */
    struct SelectStmt *selectStmt; /* SELECT 子查询 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} InsertStmt;

/* UPDATE 语句 */
typedef struct UpdateStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *targetList;       /* SET 子句 */
    List *fromClause;       /* FROM 子句 */
    Node *whereClause;      /* WHERE 条件 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} UpdateStmt;

/* DELETE 语句 */
typedef struct DeleteStmt {
    NodeTag type;
    RangeVar *relation;     /* 目标表 */
    List *usingClause;      /* USING 子句 */
    Node *whereClause;      /* WHERE 条件 */
    List *returningList;    /* RETURNING 列表 */
    List *withClause;       /* WITH 子句 */
} DeleteStmt;

/* ============================================================
 * DDL 语句节点
 * ============================================================ */

/* CREATE TABLE 语句 */
typedef struct CreateStmt {
    NodeTag type;
    RangeVar *relation;     /* 表名 */
    List *tableElts;        /* 列定义/约束列表 */
    List *ofTypename;       /* OF 类型名（typed table） */
    List *constraints;      /* 约束列表 */
    List *options;          /* 表选项 */
    char *accessMethod;     /* 访问方法 */
    char *tablespacename;   /* 表空间名 */
    char *relpersistence;   /* 持久性类型 */
    bool if_not_exists;     /* IF NOT EXISTS */
} CreateStmt;

/* DROP 语句 */
typedef struct DropStmt {
    NodeTag type;
    ObjectType removeType;  /* 对象类型 */
    List *objects;          /* 对象名列表 */
    bool missing_ok;        /* IF EXISTS */
    bool concurrent;        /* CONCURRENT（索引） */
} DropStmt;

/* 列定义 */
typedef struct ColumnDef {
    NodeTag type;
    char *colname;          /* 列名 */
    struct TypeName *typeName; /* 数据类型 */
    char *colnamespace;     /* 列命名空间 */
    List *constraints;      /* 列约束 */
    bool is_not_null;       /* NOT NULL 约束 */
    bool is_primary_key;    /* PRIMARY KEY 约束 */
    bool is_unique;         /* UNIQUE 约束 */
    Node *default_expr;     /* DEFAULT 表达式 */
    int location;           /* 源码位置 */
} ColumnDef;

/* ============================================================
 * 表引用节点
 * ============================================================ */

/* 范围变量（表引用） */
typedef struct RangeVar {
    NodeTag type;
    char *catalogname;      /* 数据库名 */
    char *schemaname;       /* 模式名 */
    char *relname;          /* 表名 */
    char *alias;            /* 别名 */
    bool inh;               /* 是否继承子表 */
    bool relIsExpr;         /* 是否为表达式 */
    char relPersistence;    /* 持久性：'p'=永久,'t'=临时,'u'=非日志 */
} RangeVar;

/* 连接表达式 */
typedef struct JoinExpr {
    NodeTag type;
    JoinType jointype;      /* 连接类型 */
    Node *larg;             /* 左表 */
    Node *rarg;             /* 右表 */
    Node *quals;            /* 连接条件 */
    List *usingClause;      /* USING 列表 */
    char *alias;            /* 别名 */
} JoinExpr;

/* 子查询范围 */
typedef struct RangeSubselect {
    NodeTag type;
    Node *subquery;         /* 子查询 */
    char *alias;            /* 别名 */
} RangeSubselect;

/* ============================================================
 * 表达式节点
 * ============================================================ */

/* 列引用 */
typedef struct ColumnRef {
    NodeTag type;
    List *fields;           /* 列名字段列表 */
    int location;           /* 源码位置 */
} ColumnRef;

/* A_Expr 表达式 */
typedef struct A_Expr {
    NodeTag type;
    A_Expr_Kind kind;       /* 表达式类型 */
    List *name;             /* 操作符名 */
    Node *lexpr;            /* 左表达式 */
    Node *rexpr;            /* 右表达式 */
    int location;           /* 源码位置 */
} A_Expr;

/* 常量值 */
typedef struct A_Const {
    NodeTag type;
    Value val;              /* 值 */
    bool isnull;            /* 是否 NULL */
    int location;           /* 源码位置 */
} A_Const;

/* 函数调用 */
typedef struct FuncCall {
    NodeTag type;
    List *funcname;         /* 函数名 */
    List *args;             /* 参数列表 */
    List *agg_order;        /* 聚合内 ORDER BY */
    List *agg_filter;       /* FILTER 子句 */
    List *over;             /* OVER 子句 */
    bool agg_distinct;      /* DISTINCT */
    bool agg_star;          /* * */
    bool func_variadic;     /* VARIADIC */
    struct WindowDef *overdef; /* 窗口定义 */
    int location;           /* 源码位置 */
} FuncCall;

/* 布尔表达式 */
typedef struct BoolExpr {
    NodeTag type;
    BoolType boolop;        /* 操作类型 */
    List *args;             /* 参数列表 */
} BoolExpr;

/* NULL 测试 */
typedef struct NullTest {
    NodeTag type;
    NullTestType nulltesttype; /* 测试类型 */
    Node *arg;              /* 测试表达式 */
} NullTest;

/* 类型名 */
typedef struct TypeName {
    NodeTag type;
    List *names;            /* 类型名列表 */
    List *typmods;          /* 类型修饰符 */
    int typemod;            /* 类型修饰符值 */
    bool array_bounds;      /* 是否为数组 */
    bool setof;             /* 是否为集合 */
    bool pct_type;          /* %TYPE 语法 */
    int location;           /* 源码位置 */
} TypeName;

/* ============================================================
 * 子句节点
 * ============================================================ */

/* ResTarget（用于 SELECT 目标列表和 UPDATE SET） */
typedef struct ResTarget {
    NodeTag type;
    char *name;             /* 列别名 */
    List *indirection;      /* 下标/字段访问 */
    Node *val;              /* 值表达式 */
    int location;           /* 源码位置 */
} ResTarget;

/* SortBy（用于 ORDER BY） */
typedef struct SortBy {
    NodeTag type;
    Node *node;             /* 排序表达式 */
    SortByDir sortby_dir;   /* 排序方向 */
    SortByNulls sortby_nulls; /* NULLS FIRST/LAST */
    List *useOp;            /* 排序操作符 */
    int location;           /* 源码位置 */
} SortBy;

/* CASE 表达式 */
typedef struct CaseExpr {
    NodeTag type;
    Node *arg;              /* CASE 表达式（简单 CASE） */
    List *args;             /* WHEN 列表 */
    Node *defresult;        /* ELSE 结果 */
    int location;           /* 源码位置 */
} CaseExpr;

/* CASE WHEN 子句 */
typedef struct CaseWhen {
    NodeTag type;
    Node *expr;             /* WHEN 条件 */
    Node *result;           /* THEN 结果 */
    int location;           /* 源码位置 */
} CaseWhen;

/* ============================================================
 * 类型转换节点
 * ============================================================ */

/* TypeCast */
typedef struct TypeCast {
    NodeTag type;
    Node *arg;              /* 待转换表达式 */
    TypeName *typeName;     /* 目标类型 */
    int location;           /* 源码位置 */
} TypeCast;

/* ============================================================
 * 窗口定义节点
 * ============================================================ */

/* WindowDef - 窗口定义 */
typedef struct WindowDef {
    NodeTag type;
    char *name;             /* 窗口名 */
    char *refname;          /* 引用窗口名 */
    List *partitionClause;  /* PARTITION BY */
    List *orderClause;      /* ORDER BY */
    int location;           /* 源码位置 */
} WindowDef;

/* 恢复 Windows 宏定义 */
#ifdef PARSENODES_UNDEF_WINDOWS_MACROS
#pragma pop_macro("OR")
#pragma pop_macro("AND")
#pragma pop_macro("OUT")
#pragma pop_macro("IN")
#pragma pop_macro("CONSTRAINT")
#pragma pop_macro("RESTRICT")
#pragma pop_macro("CASCADE")
#pragma pop_macro("USING")
#pragma pop_macro("BLOB")
#pragma pop_macro("FLOAT")
#pragma pop_macro("FLOAT_P")
#pragma pop_macro("REAL")
#pragma pop_macro("INT_P")
#pragma pop_macro("CHAR_P")
#pragma pop_macro("IDENT")
#pragma pop_macro("DEFAULT")
#pragma pop_macro("BOOLEAN_P")
#pragma pop_macro("TEXT_P")
#pragma pop_macro("TEXT")
#pragma pop_macro("TIME")
#pragma pop_macro("DATE")
#undef PARSENODES_UNDEF_WINDOWS_MACROS
#endif

#endif /* DB_PARSER_SQL_PARSENODES_H */