# SQL 引擎完整设计文档

## 1. 概述

### 1.1 目标

构建一个完全对标 PostgreSQL 的 SQL 引擎，支持 SQL:2016 标准的全部核心功能。

### 1.2 架构总览

```
┌─────────────────────────────────────────────────────────────────────┐
│                           SQL 文本                                    │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     Phase 1: Parser + Semantic                       │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────┐  │
│  │   Lexer     │───▶│   Parser    │───▶│   Semantic Analyzer     │  │
│  │  (Flex)     │    │   (Bison)   │    │   (parse_analyze.c)     │  │
│  └─────────────┘    └─────────────┘    └─────────────────────────┘  │
│         │                  │                        │                │
│         ▼                  ▼                        ▼                │
│    Token 流            Raw AST                  Query 结构           │
│                        (SelectStmt)             (已解析)             │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     Phase 2: Planner + Optimizer                     │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────┐  │
│  │  Logical    │───▶│ Optimizer   │───▶│    Physical Plan        │  │
│  │  Planner    │    │   Rules     │    │    Generator            │  │
│  └─────────────┘    └─────────────┘    └─────────────────────────┘  │
│         │                  │                        │                │
│         ▼                  ▼                        ▼                │
│    LogicalPlan        优化后的逻辑            PhysPlan 树           │
│        树              Plan 树                                     │
└─────────────────────────────┬───────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        Phase 3: Executor                             │
├─────────────────────────────────────────────────────────────────────┤
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────────────────┐  │
│  │  PlanState  │───▶│   Volcano   │───▶│    Result Set           │  │
│  │  初始化     │    │   Iterator  │    │                         │  │
│  └─────────────┘    └─────────────┘    └─────────────────────────┘  │
│         │                  │                        │                │
│         ▼                  ▼                        ▼                │
│    PlanState 树        next() 调用             查询结果             │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                        存储引擎（已有）                               │
│         Catalog / Buffer Pool / Heap AM / BTree AM / WAL            │
└─────────────────────────────────────────────────────────────────────┘
```

### 1.3 工作量估算

| Phase | 代码行数 | 任务数 | 预估工作量 |
|-------|---------|--------|-----------|
| Phase 1: Parser + Semantic | ~46,400 行 | 326 任务 | 14 人月 |
| Phase 2: Planner + Optimizer | ~52,100 行 | 420 任务 | 10 人月 |
| Phase 3: Executor | ~52,700 行 | 361 任务 | 6 人月 |
| **总计** | **~151,200 行** | **1,107 任务** | **30 人月** |

---

## 2. Phase 1: Parser + Semantic Analyzer

### 2.1 技术选型

#### 2.1.1 词法分析生成器：Flex

**选择理由**：
- PostgreSQL 使用 Flex，完全兼容
- 成熟稳定，性能优秀
- 支持 UTF-8、多字节字符
- 生成 C 代码，无运行时依赖

**关键特性**：
- 使用 `%option prefix="sql_yy"` 避免命名冲突
- 使用 `%option pure-parser` 支持多解析器实例
- 支持 `yylval` 语义值传递
- 支持位置跟踪（行号、列号）

#### 2.1.2 语法分析生成器：Bison

**选择理由**：
- PostgreSQL 使用 Bison，完全兼容
- 支持 GLR（Generalized LR）解决歧义语法
- 支持中缀操作符优先级声明
- 生成 C 代码，错误处理灵活

**关键特性**：
- 使用 `%pure-parser` 支持多解析器实例
- 使用 `%name-prefix="sql_yy"` 避免命名冲突
- 使用 `%expect 0` 确保无冲突
- 支持 `%destructor` 自动释放资源

### 2.2 文件结构设计

```
engineering/
├── include/db/parser/sql/
│   ├── parsenodes.h          # AST 节点定义（~4,100 行）
│   ├── parse_node.h          # ParseState 定义
│   ├── parser.h              # 解析器接口
│   └── keywords.h            # 关键字定义
│
└── src/db/parser/sql/
    ├── scan.l                # Flex 词法规则（~2,200 行）
    ├── gram.y                # Bison 语法规则（~15,000 行）
    ├── parser.c              # 解析器接口实现
    ├── keywords.c            # 关键字列表（~1,100 行）
    ├── kwlookup.c            # 关键字查找
    │
    ├── parse_analyze.c       # 语义分析主入口（~500 行）
    ├── parse_expr.c          # 表达式语义分析（~3,000 行）
    ├── parse_clause.c        # 子句语义分析（~3,000 行）
    ├── parse_target.c        # 目标列语义分析（~2,000 行）
    ├── parse_type.c          # 类型解析（~2,000 行）
    ├── parse_func.c          # 函数调用语义分析（~3,000 行）
    ├── parse_oper.c          # 操作符语义分析（~2,000 行）
    ├── parse_collate.c       # 排序规则解析（~200 行）
    ├── parse_param.c         # 参数处理（~150 行）
    ├── parse_merge.c         # MERGE 语句解析（~300 行）
    ├── parse_cte.c           # CTE 处理（~300 行）
    ├── parse_agg.c           # 聚合函数解析（~600 行）
    ├── parse_coerce.c        # 类型强制转换（~800 行）
    ├── parse_relation.c      # 关系查找（~500 行）
    ├── parse_node.c          # ParseState 辅助（~300 行）
    │
    ├── scangen.c             # 扫描器运行时辅助（~200 行）
    ├── makefuncs.c           # 节点构造函数（~500 行）
    └── value.c               # Value 节点操作（~100 行）
```

### 2.3 AST 节点设计

#### 2.3.1 基础节点类型（parsenodes.h）

```c
/**
 * @file parsenodes.h
 * @brief SQL AST 节点定义
 * 
 * 对标 PostgreSQL src/include/nodes/parsenodes.h
 */

#ifndef DB_PARSER_SQL_PARSENODES_H
#define DB_PARSER_SQL_PARSENODES_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * 节点标签（NodeTag）
 * ============================================================ */

/**
 * @brief AST 节点类型标签
 * 
 * 每个节点类型对应一个唯一的标签值
 * PostgreSQL 定义了 ~300 个标签
 */
typedef enum NodeTag {
    /* 基础节点 */
    T_Node = 0,                 /* 基类 */
    T_List,                     /* 列表 */
    T_IntList,                  /* 整数列表 */
    T_OidList,                  /* OID 列表 */
    T_Value,                    /* 值节点基类 */
    T_Integer,                  /* 整数值 */
    T_Float,                    /* 浮点值 */
    T_String,                   /* 字符串值 */
    T_BitString,                /* 位串值 */
    T_Null,                     /* NULL 值 */
    
    /* DML 语句 */
    T_SelectStmt,               /* SELECT 语句 */
    T_InsertStmt,               /* INSERT 语句 */
    T_UpdateStmt,               /* UPDATE 语句 */
    T_DeleteStmt,               /* DELETE 语句 */
    T_MergeStmt,                /* MERGE 语句 */
    
    /* DDL 语句 */
    T_CreateStmt,               /* CREATE TABLE */
    T_CreateSchemaStmt,         /* CREATE SCHEMA */
    T_CreateViewStmt,           /* CREATE VIEW */
    T_CreateIndexStmt,          /* CREATE INDEX */
    T_CreateSequenceStmt,       /* CREATE SEQUENCE */
    T_CreateTrigStmt,           /* CREATE TRIGGER */
    T_CreateFunctionStmt,       /* CREATE FUNCTION */
    T_CreateTypeStmt,           /* CREATE TYPE */
    T_CreateDomainStmt,         /* CREATE DOMAIN */
    T_CreateRuleStmt,           /* CREATE RULE */
    T_CreateConversionStmt,     /* CREATE CONVERSION */
    T_CreateLanguageStmt,       /* CREATE LANGUAGE */
    T_CreateOpClassStmt,        /* CREATE OPERATOR CLASS */
    T_CreateOpFamilyStmt,       /* CREATE OPERATOR FAMILY */
    T_CreateFdwStmt,            /* CREATE FOREIGN DATA WRAPPER */
    T_CreateForeignServerStmt,  /* CREATE SERVER */
    T_CreateUserMappingStmt,    /* CREATE USER MAPPING */
    T_CreateForeignTableStmt,   /* CREATE FOREIGN TABLE */
    T_CreatePolicyStmt,         /* CREATE POLICY */
    T_CreatePublicationStmt,    /* CREATE PUBLICATION */
    T_CreateSubscriptionStmt,   /* CREATE SUBSCRIPTION */
    T_CreateTransformStmt,      /* CREATE TRANSFORM */
    T_CreateStatisticsStmt,     /* CREATE STATISTICS */
    T_CreateMatViewStmt,        /* CREATE MATERIALIZED VIEW */
    T_CreateCollationStmt,      /* CREATE COLLATION */
    T_CreateCastStmt,           /* CREATE CAST */
    T_CreatePLangStmt,          /* CREATE PROCEDURAL LANGUAGE */
    T_CreatePLTemplateStmt,     /* CREATE PROCEDURAL LANGUAGE TEMPLATE */
    T_CreateAmStmt,             /* CREATE ACCESS METHOD */
    T_CreatePublicationStmt,    /* CREATE PUBLICATION */
    
    T_DropStmt,                 /* DROP 语句 */
    T_AlterStmt,                /* ALTER 语句 */
    T_AlterTableStmt,           /* ALTER TABLE */
    T_AlterDomainStmt,          /* ALTER DOMAIN */
    T_AlterSequenceStmt,        /* ALTER SEQUENCE */
    T_AlterFunctionStmt,        /* ALTER FUNCTION */
    T_AlterTypeStmt,            /* ALTER TYPE */
    T_AlterPublicationStmt,     /* ALTER PUBLICATION */
    T_AlterSubscriptionStmt,    /* ALTER SUBSCRIPTION */
    T_AlterObjectSchemaStmt,    /* ALTER ... SET SCHEMA */
    T_AlterObjectRenameStmt,    /* ALTER ... RENAME */
    T_AlterOperatorStmt,        /* ALTER OPERATOR */
    T_AlterTypeSetStmt,         /* ALTER TYPE SET */
    T_AlterCollationStmt,       /* ALTER COLLATION */
    T_AlterDefaultPrivilegesStmt, /* ALTER DEFAULT PRIVILEGES */
    
    /* DCL 语句 */
    T_GrantStmt,                /* GRANT */
    T_GrantRoleStmt,            /* GRANT ROLE */
    T_RevokeStmt,               /* REVOKE */
    T_RevokeRoleStmt,           /* REVOKE ROLE */
    
    /* 事务语句 */
    T_TransactionStmt,          /* BEGIN/COMMIT/ROLLBACK */
    T_SavepointStmt,            /* SAVEPOINT */
    T_ReleaseStmt,              /* RELEASE SAVEPOINT */
    
    /* 其他语句 */
    T_LockStmt,                 /* LOCK TABLE */
    T_ExplainStmt,              /* EXPLAIN */
    T_CopyStmt,                 /* COPY */
    T_VariableSetStmt,          /* SET */
    T_VariableShowStmt,         /* SHOW */
    T_VariableResetStmt,        /* RESET */
    T_TruncateStmt,             /* TRUNCATE */
    T_VacuumStmt,               /* VACUUM */
    T_AnalyzeStmt,              /* ANALYZE */
    T_CommentStmt,              /* COMMENT */
    T_SecLabelStmt,             /* SECURITY LABEL */
    T_NotifyStmt,               /* NOTIFY */
    T_ListenStmt,               /* LISTEN */
    T_UnlistenStmt,             /* UNLISTEN */
    T_LoadStmt,                 /* LOAD */
    T_PrepareStmt,              /* PREPARE */
    T_ExecuteStmt,              /* EXECUTE */
    T_DeallocateStmt,           /* DEALLOCATE */
    T_CheckPointStmt,           /* CHECKPOINT */
    T_ReindexStmt,              /* REINDEX */
    T_RenameStmt,               /* RENAME */
    T_AlterOwnerStmt,           /* ALTER ... OWNER TO */
    T_AlterTableMoveAllStmt,    /* ALTER TABLE MOVE ALL */
    T_CreateTableAsStmt,        /* CREATE TABLE AS */
    T_RefreshMatViewStmt,       /* REFRESH MATERIALIZED VIEW */
    T_ReplicaIdentityStmt,      /* REPLICA IDENTITY */
    T_ClauseStmt,               /* CLAUSE */
    
    /* 表达式节点 */
    T_A_Expr,                   /* 操作符表达式 */
    T_BoolExpr,                 /* 布尔表达式 */
    T_ColumnRef,                /* 列引用 */
    T_FuncCall,                 /* 函数调用 */
    T_TypeCast,                 /* 类型转换 */
    T_TypeName,                 /* 类型名称 */
    T_ParamRef,                 /* 参数引用 ($1) */
    T_A_Const,                  /* 常量 */
    T_A_ArrayExpr,              /* 数组表达式 */
    T_A_Indirection,            /* 下标/字段访问 */
    T_A_IndirectionElem,        /* 下标/字段访问元素 */
    T_CaseExpr,                 /* CASE 表达式 */
    T_CaseWhen,                 /* CASE WHEN */
    T_CaseTestExpr,             /* CASE 测试表达式 */
    T_CoalesceExpr,             /* COALESCE */
    T_MinMaxExpr,               /* GREATEST/LEAST */
    T_SQLValueFunction,         /* CURRENT_DATE 等 */
    T_NullTest,                 /* IS NULL/IS NOT NULL */
    T_BooleanTest,              /* IS TRUE/IS FALSE */
    T_SubLink,                  /* 子链接 */
    T_ArrayExpr,                /* 数组构造器 */
    T_RowExpr,                  /* 行构造器 */
    T_RowCompareExpr,           /* 行比较 */
    T_FieldSelect,              /* 字段选择 */
    T_FieldStore,               /* 字段存储 */
    T_RelabelType,              /* 类型重标记 */
    T_CoerceViaIO,              /* 通过 IO 强制转换 */
    T_ArrayCoerceExpr,          /* 数组强制转换 */
    T_ConvertRowtypeExpr,       /* 行类型转换 */
    T_CollateExpr,              /* COLLATE 表达式 */
    T_CollateClause,            /* COLLATE 子句 */
    T_JsonExpr,                 /* JSON 表达式 */
    T_JsonConstructorExpr,      /* JSON 构造器 */
    T_JsonIsPredicate,          /* JSON IS 谓词 */
    T_JsonParseExpr,            /* JSON 解析表达式 */
    T_JsonScalarExpr,           /* JSON 标量表达式 */
    T_JsonSerializeExpr,        /* JSON 序列化表达式 */
    T_JsonTableExpr,            /* JSON 表表达式 */
    T_JsonObjectCtor,           /* JSON 对象构造器 */
    T_JsonArrayCtor,            /* JSON 数组构造器 */
    T_JsonArrayQueryCtor,       /* JSON 数组查询构造器 */
    T_JsonArrayAgg,             /* JSON_ARRAYAGG */
    T_JsonObjectAgg,            /* JSON_OBJECTAGG */
    T_JsonTable,                /* JSON_TABLE */
    T_XmlExpr,                  /* XML 表达式 */
    
    /* 范围表节点 */
    T_RangeVar,                 /* 表引用 */
    T_RangeSubselect,           /* 子查询 */
    T_RangeFunction,            /* 函数范围 */
    T_RangeTableSample,         /* TABLESAMPLE */
    T_RangeTableFunc,           /* XMLTABLE/JSON_TABLE */
    T_RangeTableFuncCol,        /* 范围表函数列 */
    T_JoinExpr,                 /* JOIN 表达式 */
    T_FromExpr,                 /* FROM 表达式 */
    T_OnConflictExpr,           /* ON CONFLICT */
    T_LockingClause,            /* FOR UPDATE/SHARE */
    
    /* 目标列节点 */
    T_ResTarget,                /* 结果目标 */
    T_InsertTarget,             /* INSERT 目标 */
    T_UpdateTarget,             /* UPDATE 目标 */
    
    /* 约束节点 */
    T_Constraint,               /* 约束定义 */
    T_ColumnDef,                /* 列定义 */
    T_TableLikeClause,          /* CREATE TABLE ... LIKE */
    T_IndexElem,                /* 索引元素 */
    T_FunctionParameter,        /* 函数参数 */
    
    /* CTE 节点 */
    T_WithClause,               /* WITH 子句 */
    T_CommonTableExpr,          /* CTE 定义 */
    T_CteCycleClause,           /* CYCLE 子句 */
    T_CteSearchClause,          /* SEARCH 子句 */
    
    /* 窗口函数节点 */
    T_WindowDef,                /* 窗口定义 */
    T_WindowClause,             /* 窗口子句 */
    
    /* 排序/分组节点 */
    T_SortBy,                   /* ORDER BY 元素 */
    T_GroupingSet,              /* GROUPING SETS */
    T_SortGroupClause,          /* 排序/分组子句 */
    
    /* 其他节点 */
    T_Alias,                    /* 别名 */
    T_IntoClause,               /* INTO 子句 */
    T_ParamName,                /* 参数名 */
    T_ParamExt,                 /* 扩展参数 */
    T_MultiAssignRef,           /* 多重赋值 */
    T_ConstraintsSetStmt,       /* SET CONSTRAINTS */
    T_AlterTableCmd,            /* ALTER TABLE 命令 */
    T_AlterTableType,           /* ALTER TABLE 类型 */
    T_AlterTableSetSchema,      /* ALTER TABLE SET SCHEMA */
    T_AlterTableMoveAll,        /* ALTER TABLE MOVE ALL */
    T_AlterTableRecurse,        /* ALTER TABLE RECURSE */
    T_AlterTableRecurseAll,     /* ALTER TABLE RECURSE ALL */
    T_AlterTableRecurseRest,    /* ALTER TABLE RECURSE REST */
    T_AlterTableRecurseNone,    /* ALTER TABLE RECURSE NONE */
    T_AlterTableRecurseDeep,    /* ALTER TABLE RECURSE DEEP */
    
    /* 向量表达式节点 */
    T_VectorExpr,               /* 向量表达式 */
    T_VectorLiteral,            /* 向量字面量 */
    T_VectorDistance,           /* 向量距离 */
    
    /* 向量类型节点 */
    T_VectorTypeName,           /* VECTOR 类型名 */
    
    /* 其他（预留扩展） */
    T_InvalidFirst,             /* 无效标记（开始） */
    /* ... 约 300 个节点类型 ... */
    T_InvalidLast               /* 无效标记（结束） */
} NodeTag;

/* ============================================================
 * 基础节点结构
 * ============================================================ */

/**
 * @brief 节点基类
 * 
 * 所有 AST 节点都以 NodeTag 开头
 * 用于运行时类型识别（IsA 宏）
 */
typedef struct Node {
    NodeTag type;               /**< 节点类型标签 */
} Node;

/**
 * @brief 判断节点是否为指定类型
 */
#define IsA(node, nodetype) \
    (node != NULL && ((Node *)(node))->type == T_##nodetype)

/**
 * @brief 转换节点为指定类型
 */
#define castNode(nodetype, node) \
    (IsA(node, nodetype) ? (nodetype *)(node) : NULL)

/**
 * @brief 创建节点
 */
#define makeNode(nodetype) \
    ((nodetype *) newNode(sizeof(nodetype), T_##nodetype))

/**
 * @brief 创建节点（内部函数）
 */
Node *newNode(size_t size, NodeTag type);

/* ============================================================
 * List 结构
 * ============================================================ */

/**
 * @brief 通用链表结构
 * 
 * 用于存储任意类型的节点列表
 * 采用单向链表结构，支持高效的头部插入
 */
typedef struct List {
    NodeTag type;               /**< T_List */
    int length;                 /**< 列表长度 */
    int max_length;             /**< 最大长度（用于扩展） */
    struct ListCell *head;      /**< 头节点 */
    struct ListCell *tail;      /**< 尾节点（用于尾部追加） */
} List;

/**
 * @brief 列表单元格
 */
typedef struct ListCell {
    void *data;                 /**< 数据指针 */
    struct ListCell *next;      /**< 下一个单元格 */
} ListCell;

/* List 操作宏 */
#define linitial(l)             ((l)->head->data)
#define lsecond(l)              ((l)->head->next->data)
#define lthird(l)               ((l)->head->next->next->data)
#define lfourth(l)              ((l)->head->next->next->next->data)
#define llast(l)                ((l)->tail->data)
#define list_head(l)            ((l)->head)
#define list_length(l)          ((l) ? (l)->length : 0)

/* List 构造函数 */
List *lappend(List *list, void *data);
List *lcons(void *data, List *list);
List *list_make1(void *data);
List *list_make2(void *data1, void *data2);
List *list_make3(void *data1, void *data2, void *data3);
List *list_make4(void *data1, void *data2, void *data3, void *data4);
void list_free(List *list);
void list_free_deep(List *list);

/* 整数列表 */
typedef struct IntList {
    NodeTag type;               /**< T_IntList */
    int length;
    struct IntListCell *head;
    struct IntListCell *tail;
} IntList;

typedef struct IntListCell {
    int value;
    struct IntListCell *next;
} IntListCell;

IntList *int_list_append(IntList *list, int value);

/* OID 列表 */
typedef struct OidList {
    NodeTag type;               /**< T_OidList */
    int length;
    struct OidListCell *head;
    struct OidListCell *tail;
} OidList;

typedef struct OidListCell {
    uint32_t oid;
    struct OidListCell *next;
} OidListCell;

OidList *oid_list_append(OidList *list, uint32_t oid);

/* ============================================================
 * Value 结构（字面量）
 * ============================================================ */

/**
 * @brief 值节点基类
 */
typedef struct Value {
    NodeTag type;               /**< T_Integer/T_Float/T_String/T_BitString/T_Null */
} Value;

/**
 * @brief 整数值
 */
typedef struct Integer {
    NodeTag type;               /**< T_Integer */
    int64_t val;                /**< 整数值 */
} Integer;

/**
 * @brief 浮点值
 */
typedef struct Float {
    NodeTag type;               /**< T_Float */
    char *val;                  /**< 字符串表示（避免精度丢失） */
} Float;

/**
 * @brief 字符串值
 */
typedef struct String {
    NodeTag type;               /**< T_String */
    char *val;                  /**< 字符串值 */
} String;

/**
 * @brief 位串值
 */
typedef struct BitString {
    NodeTag type;               /**< T_BitString */
    char *val;                  /**< 位串值（如 B'10101'） */
} BitString;

/**
 * @brief NULL 值
 */
typedef struct Null {
    NodeTag type;               /**< T_Null */
} Null;

/* Value 构造函数 */
Integer *makeInteger(int64_t val);
Float *makeFloat(const char *val);
String *makeString(const char *val);
BitString *makeBitString(const char *val);
Null *makeNull(void);

/* ============================================================
 * SELECT 语句节点
 * ============================================================ */

/**
 * @brief 集合操作类型
 */
typedef enum SetOperation {
    SETOP_NONE = 0,             /**< 无集合操作 */
    SETOP_UNION,                /**< UNION */
    SETOP_INTERSECT,            /**< INTERSECT */
    SETOP_EXCEPT                /**< EXCEPT */
} SetOperation;

/**
 * @brief SELECT 语句 AST 节点
 * 
 * 这是 SELECT 语句的原始 AST，包含所有语法元素
 * 语义分析后转换为 Query 结构
 */
typedef struct SelectStmt {
    NodeTag type;               /**< T_SelectStmt */
    
    /* ========== 查询结构 ========== */
    
    /**
     * @brief 目标列列表
     * 
     * List of ResTarget 节点
     * SELECT col1, col2, ... FROM ...
     */
    List *targetList;
    
    /**
     * @brief INTO 子句（SELECT INTO）
     * 
     * SELECT ... INTO TABLE ...
     * NULL 表示无 INTO 子句
     */
    struct IntoClause *intoClause;
    
    /**
     * @brief FROM 子句
     * 
     * List of RangeVar/RangeSubselect/JoinExpr 节点
     * FROM table1, table2, ...
     * NULL 表示无 FROM 子句（SELECT 1）
     */
    List *fromClause;
    
    /**
     * @brief WHERE 子句
     * 
     * WHERE 条件表达式
     * NULL 表示无 WHERE 子句
     */
    Node *whereClause;
    
    /**
     * @brief GROUP BY 子句
     * 
     * List of ColumnRef/Expr 节点
     * GROUP BY col1, col2, ...
     * NULL 表示无 GROUP BY 子句
     */
    List *groupClause;
    
    /**
     * @brief HAVING 子句
     * 
     * HAVING 条件表达式
     * NULL 表示无 HAVING 子句
     */
    Node *havingClause;
    
    /**
     * @brief WINDOW 子句
     * 
     * List of WindowDef 节点
     * WINDOW w1 AS (PARTITION BY col1), ...
     * NULL 表示无 WINDOW 子句
     */
    List *windowClause;
    
    /* ========== 集合操作 ========== */
    
    /**
     * @brief 集合操作类型
     * 
     * SETOP_NONE 表示简单 SELECT
     * SETOP_UNION/INTERSECT/EXCEPT 表示复合查询
     */
    SetOperation op;
    
    /**
     * @brief ALL 标志（集合操作）
     * 
     * UNION ALL / INTERSECT ALL / EXCEPT ALL
     */
    bool all;
    
    /**
     * @brief 左操作数（集合操作）
     */
    struct SelectStmt *larg;
    
    /**
     * @brief 右操作数（集合操作）
     */
    struct SelectStmt *rarg;
    
    /* ========== 排序和限制 ========== */
    
    /**
     * @brief ORDER BY 子句
     * 
     * List of SortBy 节点
     * ORDER BY col1 ASC, col2 DESC, ...
     * NULL 表示无 ORDER BY 子句
     */
    List *sortClause;
    
    /**
     * @brief LIMIT 子句
     * 
     * LIMIT 表达式
     * NULL 表示无 LIMIT 子句
     */
    Node *limitCount;
    
    /**
     * @brief OFFSET 子句
     * 
     * OFFSET 表达式
     * NULL 表示无 OFFSET 子句
     */
    Node *limitOffset;
    
    /* ========== 锁定子句 ========== */
    
    /**
     * @brief FOR UPDATE/SHARE 子句
     * 
     * List of LockingClause 节点
     * FOR UPDATE / FOR SHARE / FOR KEY SHARE / FOR NO KEY UPDATE
     * NULL 表示无锁定子句
     */
    List *lockingClause;
    
    /* ========== WITH 子句 ========== */
    
    /**
     * @brief WITH 子句（CTE）
     * 
     * WITH cte1 AS (...), cte2 AS (...) ...
     * NULL 表示无 WITH 子句
     */
    struct WithClause *withClause;
    
    /* ========== DISTINCT ========== */
    
    /**
     * @brief DISTINCT 子句
     * 
     * List of ColumnRef 节点（DISTINCT ON）
     * NULL 表示无 DISTINCT
     * 非空列表表示 DISTINCT ON (col1, col2, ...)
     * 空列表表示简单 DISTINCT
     */
    List *distinctClause;
    
} SelectStmt;

/* ============================================================
 * INSERT 语句节点
 * ============================================================ */

/**
 * @brief INSERT 语句 AST 节点
 */
typedef struct InsertStmt {
    NodeTag type;               /**< T_InsertStmt */
    
    /**
     * @brief 目标表
     * 
     * INSERT INTO table_name ...
     */
    struct RangeVar *relation;
    
    /**
     * @brief 列列表
     * 
     * INSERT INTO table (col1, col2, ...) ...
     * NULL 表示插入所有列
     */
    List *colNames;
    
    /**
     * @brief 值列表
     * 
     * List of List (每行是一个 List of Expr)
     * INSERT ... VALUES (v1, v2, ...), (v3, v4, ...)
     */
    List *valuesLists;
    
    /**
     * @brief SELECT 语句（INSERT ... SELECT）
     * 
     * INSERT INTO table SELECT ...
     * NULL 表示无 SELECT 子句（使用 VALUES）
     */
    SelectStmt *selectStmt;
    
    /**
     * @brief ON CONFLICT 子句
     * 
     * ON CONFLICT ... DO UPDATE/DO NOTHING
     * NULL 表示无 ON CONFLICT 子句
     */
    struct OnConflictExpr *onConflictClause;
    
    /**
     * @brief RETURNING 子句
     * 
     * RETURNING col1, col2, ...
     * NULL 表示无 RETURNING 子句
     */
    List *returningList;
    
    /**
     * @brief 覆盖标志（MySQL 兼容）
     * 
     * INSERT IGNORE / INSERT OVERWRITE
     */
    bool override;
    
} InsertStmt;

/* ============================================================
 * UPDATE 语句节点
 * ============================================================ */

/**
 * @brief UPDATE 语句 AST 节点
 */
typedef struct UpdateStmt {
    NodeTag type;               /**< T_UpdateStmt */
    
    /**
     * @brief 目标表
     */
    struct RangeVar *relation;
    
    /**
     * @brief SET 列表
     * 
     * List of ResTarget 节点
     * SET col1 = val1, col2 = val2, ...
     */
    List *targetList;
    
    /**
     * @brief FROM 子句（PostgreSQL 扩展）
     * 
     * UPDATE table SET ... FROM other_table ...
     * NULL 表示无 FROM 子句
     */
    List *fromClause;
    
    /**
     * @brief WHERE 子句
     */
    Node *whereClause;
    
    /**
     * @brief RETURNING 子句
     */
    List *returningList;
    
} UpdateStmt;

/* ============================================================
 * DELETE 语句节点
 * ============================================================ */

/**
 * @brief DELETE 语句 AST 节点
 */
typedef struct DeleteStmt {
    NodeTag type;               /**< T_DeleteStmt */
    
    /**
     * @brief 目标表
     */
    struct RangeVar *relation;
    
    /**
     * @brief USING 子句（PostgreSQL 扩展）
     * 
     * DELETE FROM table USING other_table ...
     * NULL 表示无 USING 子句
     */
    List *usingClause;
    
    /**
     * @brief WHERE 子句
     */
    Node *whereClause;
    
    /**
     * @brief RETURNING 子句
     */
    List *returningList;
    
} DeleteStmt;

/* ============================================================
 * CREATE TABLE 语句节点
 * ============================================================ */

/**
 * @brief CREATE TABLE 语句 AST 节点
 */
typedef struct CreateStmt {
    NodeTag type;               /**< T_CreateStmt */
    
    /**
     * @brief 表名
     */
    struct RangeVar *relation;
    
    /**
     * @brief 列定义列表
     * 
     * List of ColumnDef 节点
     */
    List *tableElts;
    
    /**
     * @brief INHERITS 子句
     * 
     * List of RangeVar（父表）
     * CREATE TABLE child (...) INHERITS (parent)
     * NULL 表示无继承
     */
    List *inhRelations;
    
    /**
     * @brief PARTITION BY 子句
     * 
     * 分区定义
     * NULL 表示非分区表
     */
    struct PartitionSpec *partspec;
    
    /**
     * @brief ON COMMIT 子句
     * 
     * ON COMMIT DROP / ON COMMIT DELETE ROWS
     */
    int oncommit;               /**< ONCOMMIT_NOOP/DROP/DELETE_ROWS/PRESERVE_ROWS */
    
    /**
     * @brief TABLESPACE 子句
     */
    char *tablespacename;
    
    /**
     * @brief ACCESS METHOD 子句
     * 
     * USING heap / USING columnar
     */
    char *accessMethod;
    
    /**
     * @brief WITH 子句（存储参数）
     * 
     * WITH (fillfactor=80, toast.autovacuum_enabled=false)
     */
    List *options;
    
    /**
     * @brief IF NOT EXISTS 标志
     */
    bool if_not_exists;
    
} CreateStmt;

/**
 * @brief 列定义节点
 */
typedef struct ColumnDef {
    NodeTag type;               /**< T_ColumnDef */
    
    char *colname;              /**< 列名 */
    struct TypeName *typeName;  /**< 列类型 */
    int inhcount;               /**< 继承计数 */
    bool is_local;              /**< 是否本地定义 */
    bool is_not_null;           /**< NOT NULL 标志 */
    bool is_from_type;          /**< 是否从类型继承 */
    bool is_from_parent;        /**< 是否从父表继承 */
    char storage;               /**< 存储方式（p/plain/e/x/m） */
    Node *raw_default;          /**< 原始默认值表达式 */
    Node *cooked_default;       /**< 已处理的默认值 */
    char *identity;             /**< IDENTITY 标志（a/d/空） */
    List *identitySequence;     /**< IDENTITY 序列选项 */
    struct RangeVar *identitySequenceRelation; /**< IDENTITY 序列关系 */
    bool is_identity_sequence_from_type; /**< IDENTITY 序列是否从类型 */
    bool null_if_not_exists;    /**< IF NOT EXISTS 标志（ALTER） */
    List *constraints;          /**< 列级约束列表 */
    List *fdwoptions;           /**< FDW 选项 */
    int location;               /**< 源码位置 */
} ColumnDef;

/**
 * @brief 约束节点
 */
typedef struct Constraint {
    NodeTag type;               /**< T_Constraint */
    
    /* 约束类型 */
    int contype;                /**< CONSTR_XXX */
    
    /* 约束名称 */
    char *conname;              /**< 约束名（可选） */
    bool deferrable;            /**< 是否可延迟 */
    bool initdeferred;          /**< 是否初始延迟 */
    int location;               /**< 源码位置 */
    
    /* 列列表（用于 UNIQUE/PRIMARY KEY/FOREIGN KEY） */
    List *keys;                 /**< List of String (列名) */
    
    /* 表达式（用于 CHECK） */
    Node *raw_expr;             /**< 原始表达式 */
    Node *cooked_expr;          /**< 已处理的表达式 */
    
    /* 外键引用 */
    struct RangeVar *pktable;   /**< 被引用表 */
    List *pk_attrs;             /**< 被引用列列表 */
    char fk_matchtype;          /**< 匹配类型（f/p/s） */
    char fk_upd_action;         /**< UPDATE 动作（a/r/c/n） */
    char fk_del_action;         /**< DELETE 动作（a/r/c/n） */
    
    /* 索引参数 */
    List *including;            /**< INCLUDE 列列表 */
    List *exclusions;           /**< 排除约束元素 */
    List *options;              /**< WITH 选项 */
    char *indexname;            /**< 索引名 */
    char *indexspace;           /**< 索引表空间 */
    bool reset_default_tblspc;  /**< 重置默认表空间 */
    char *access_method;        /**< 访问方法 */
    Node *where_clause;         /**< WHERE 子句（部分索引） */
    bool deferrable_is_default; /**< DEFERRABLE 是否默认 */
    List *fk_attrs;             /**< 外键列列表 */
    bool skip_validation;       /**< 跳过验证 */
    bool initially_valid;       /**< 初始有效 */
} Constraint;

/* 约束类型 */
#define CONSTR_NULL          0  /* NULL 约束 */
#define CONSTR_NOTNULL       1  /* NOT NULL */
#define CONSTR_UNIQUE        2  /* UNIQUE */
#define CONSTR_PRIMARY      3  /* PRIMARY KEY */
#define CONSTR_CHECK        4  /* CHECK */
#define CONSTR_DEFAULT      5  /* DEFAULT */
#define CONSTR_IDENTITY     6  /* IDENTITY */
#define CONSTR_FOREIGN      7  /* FOREIGN KEY */
#define CONSTR_PARTKEY      8  /* PARTITION KEY */
#define CONSTR_EXCLUSION    9  /* EXCLUSION */
#define CONSTR_GENERATED    10 /* GENERATED */
#define CONSTR_CHECK_NEW    11 /* CHECK (新) */
#define CONSTR_CHECK_EXISTING 12 /* CHECK (已存在) */

/* ============================================================
 * DROP 语句节点
 * ============================================================ */

/**
 * @brief DROP 语句 AST 节点
 */
typedef struct DropStmt {
    NodeTag type;               /**< T_DropStmt */
    
    List *objects;              /**< List of String (对象名) */
    int removeType;             /* 对象类型（OBJECT_TABLE 等） */
    int behavior;               /**< DROP_BEHAVIOR（CASCADE/RESTRICT） */
    bool missing_ok;            /**< IF EXISTS 标志 */
    bool concurrent;            /**< CONCURRENT 标志 */
} DropStmt;

/* DROP 行为 */
#define DROP_RESTRICT    0     /* 默认：RESTRICT */
#define DROP_CASCADE     1     /* CASCADE */

/* ============================================================
 * 范围表节点（FROM 子句元素）
 * ============================================================ */

/**
 * @brief 范围变量（表引用）
 * 
 * 表示 FROM 子句中的表名引用
 * 可以是基表、视图、继承表等
 */
typedef struct RangeVar {
    NodeTag type;               /**< T_RangeVar */
    
    char *catalogname;          /**< Catalog 名（通常为 NULL） */
    char *schemaname;           /**< Schema 名 */
    char *relname;              /**< 表名 */
    bool inh;                   /**< 是否包含继承表 */
    char relpersistence;        /**< 持久性（p/u/t） */
    struct Alias *alias;        /**< 别名 */
    int location;               /**< 源码位置 */
    
    /* 表采样 */
    struct TableSampleClause *tablesample; /**< TABLESAMPLE 子句 */
    
} RangeVar;

/**
 * @brief 范围子查询
 * 
 * 表示 FROM 子句中的子查询
 * FROM (SELECT ...) AS alias
 */
typedef struct RangeSubselect {
    NodeTag type;               /**< T_RangeSubselect */
    
    bool lateral;               /**< LATERAL 标志 */
    Node *subquery;             /**< 子查询（SelectStmt） */
    struct Alias *alias;        /**< 别名 */
    
} RangeSubselect;

/**
 * @brief 范围函数
 * 
 * 表示 FROM 子句中的函数调用
 * FROM func() AS alias
 */
typedef struct RangeFunction {
    NodeTag type;               /**< T_RangeFunction */
    
    bool lateral;               /**< LATERAL 标志 */
    bool ordinality;            /* WITH ORDINALITY */
    bool is_rowsfrom;           /* ROWS FROM */
    List *functions;            /* List of FunctionCall */
    struct Alias *alias;        /* 别名 */
    List *coldeflist;           /* 列定义列表 */
    
} RangeFunction;

/**
 * @brief JOIN 表达式
 * 
 * 表示 FROM 子句中的 JOIN
 * table1 JOIN table2 ON condition
 */
typedef struct JoinExpr {
    NodeTag type;               /**< T_JoinExpr */
    
    int jointype;               /**< 连接类型（JOIN_INNER/LEFT/RIGHT/FULL） */
    bool isNatural;             /**< NATURAL 标志 */
    Node *larg;                 /**< 左操作数 */
    Node *rarg;                 /**< 右操作数 */
    List *usingClause;          /**< USING 列列表 */
    Node *quals;                /**< ON 条件 */
    struct Alias *alias;        /**< 别名 */
    int rtindex;                /**< RangeTable 索引（语义分析后填充） */
    
} JoinExpr;

/* JOIN 类型 */
#define JOIN_INNER         0    /* INNER JOIN */
#define JOIN_LEFT          1    /* LEFT OUTER JOIN */
#define JOIN_FULL          2    /* FULL OUTER JOIN */
#define JOIN_RIGHT         3    /* RIGHT OUTER JOIN */
#define JOIN_SEMI          4    /* SEMI JOIN */
#define JOIN_ANTI          5    /* ANTI JOIN */
#define JOIN_UNIQUE_OUTER  6    /* UNIQUE OUTER */
#define JOIN_UNIQUE_INNER  7    /* UNIQUE INNER */
#define JOIN_LASJ_NOTIN    8    /* LASJ NOT IN */

/**
 * @brief FROM 表达式
 * 
 * 表示 FROM 子句的根节点
 * 实际上是一个隐式的 JOIN 树
 */
typedef struct FromExpr {
    NodeTag type;               /**< T_FromExpr */
    
    List *fromlist;             /**< List of RangeVar/JoinExpr */
    Node *quals;                /**< WHERE 条件 */
    
} FromExpr;

/* ============================================================
 * 目标列节点
 * ============================================================ */

/**
 * @brief 目标列节点
 * 
 * 表示 SELECT/UPDATE 目标列
 * SELECT col1, col2 AS alias, table.* FROM ...
 */
typedef struct ResTarget {
    NodeTag type;               /**< T_ResTarget */
    
    char *name;                 /**< 列名（可选） */
    List *indirection;          /**< 下标/字段访问（可选） */
    Node *val;                  /**< 表达式 */
    int location;               /**< 源码位置 */
    
} ResTarget;

/* ============================================================
 * 表达式节点
 * ============================================================ */

/**
 * @brief 操作符表达式
 * 
 * 表示二元操作符表达式
 * a + b, a > b, a AND b 等
 */
typedef struct A_Expr {
    NodeTag type;               /**< T_A_Expr */
    
    int kind;                   /**< 操作符类型（AEXPR_XXX） */
    List *name;                 /**< 操作符名称（List of String） */
    Node *lexpr;                /**< 左操作数 */
    Node *rexpr;                /**< 右操作数 */
    int location;               /**< 源码位置 */
    
} A_Expr;

/* 操作符类型 */
#define AEXPR_OP           0    /* 普通操作符 */
#define AEXPR_OP_ANY       1    /* = ANY */
#define AEXPR_OP_ALL       2    /* = ALL */
#define AEXPR_DISTINCT     3    /* IS DISTINCT FROM */
#define AEXPR_NULLIF       4    /* NULLIF */
#define AEXPR_OF          5    /* IS OF */
#define AEXPR_IN          6    /* IN */
#define AEXPR_LIKE        7    /* LIKE */
#define AEXPR_ILIKE       8    /* ILIKE */
#define AEXPR_SIMILAR     9    /* SIMILAR TO */
#define AEXPR_BETWEEN     10   /* BETWEEN */
#define AEXPR_NOT_BETWEEN 11   /* NOT BETWEEN */
#define AEXPR_PAREN       12   /* 括号表达式 */

/**
 * @brief 布尔表达式
 * 
 * 表示 AND/OR/NOT 表达式
 */
typedef struct BoolExpr {
    NodeTag type;               /**< T_BoolExpr */
    
    int boolop;                 /**< AND_EXPR/OR_EXPR/NOT_EXPR */
    List *args;                 /**< 参数列表 */
    int location;               /**< 源码位置 */
    
} BoolExpr;

/* 布尔操作符 */
#define AND_EXPR    0
#define OR_EXPR     1
#define NOT_EXPR    2

/**
 * @brief 列引用
 * 
 * 表示列名引用
 * col / table.col / schema.table.col
 */
typedef struct ColumnRef {
    NodeTag type;               /**< T_ColumnRef */
    
    List *fields;               /**< List of String (名称层级) */
    int location;               /**< 源码位置 */
    
} ColumnRef;

/**
 * @brief 函数调用
 * 
 * 表示函数调用表达式
 * func(a, b) / COUNT(*) / ROW_NUMBER() OVER (...)
 */
typedef struct FuncCall {
    NodeTag type;               /**< T_FuncCall */
    
    List *funcname;             /**< 函数名（List of String） */
    List *args;                 /**< 参数列表 */
    List *agg_order;            /**< 聚合函数内的 ORDER BY */
    Node *agg_filter;           /**< FILTER 子句 */
    bool agg_within_group;      /**< WITHIN GROUP */
    bool agg_star;              /**< COUNT(*) */
    bool agg_distinct;          /**< DISTINCT */
    bool func_variadic;         /**< VARIADIC */
    struct WindowDef *over;     /**< OVER 子句 */
    int location;               /**< 源码位置 */
    
} FuncCall;

/**
 * @brief 类型名称
 * 
 * 表示类型定义
 * INT / VARCHAR(100) / NUMERIC(10, 2)
 */
typedef struct TypeName {
    NodeTag type;               /**< T_TypeName */
    
    List *names;                /**< 类型名（List of String） */
    List *typmods;              /**< 类型修饰符 */
    int typemod;                /**< 类型修饰符（已解析） */
    List *arrayBounds;          /**< 数组维度 */
    bool parenthesis;           /**< 是否带括号 */
    int location;               /**< 源码位置 */
    
} TypeName;

/**
 * @brief 类型转换
 * 
 * 表示显式类型转换
 * expr::type / CAST(expr AS type)
 */
typedef struct TypeCast {
    NodeTag type;               /**< T_TypeCast */
    
    Node *arg;                  /**< 待转换表达式 */
    TypeName *typeName;         /**< 目标类型 */
    int location;               /**< 源码位置 */
    
} TypeCast;

/**
 * @brief 参数引用
 * 
 * 表示参数占位符
 * $1, $2, ...
 */
typedef struct ParamRef {
    NodeTag type;               /**< T_ParamRef */
    
    int number;                 /**< 参数编号 */
    int location;               /**< 源码位置 */
    
} ParamRef;

/**
 * @brief 常量
 * 
 * 表示字面量常量
 * 123 / 3.14 / 'hello' / NULL
 */
typedef struct A_Const {
    NodeTag type;               /**< T_A_Const */
    
    Value val;                  /**< 常量值 */
    bool isnull;                /**< 是否为 NULL */
    int location;               /**< 源码位置 */
    
} A_Const;

/**
 * @brief CASE 表达式
 * 
 * 表示 CASE 表达式
 * CASE WHEN cond1 THEN result1 WHEN cond2 THEN result2 ELSE result3 END
 */
typedef struct CaseExpr {
    NodeTag type;               /**< T_CaseExpr */
    
    Node *arg;                  /* CASE arg WHEN ...（可选） */
    List *args;                 /* List of CaseWhen */
    Node *defresult;            /* ELSE 结果 */
    int location;               /* 源码位置 */
    
} CaseExpr;

/**
 * @brief CASE WHEN 子句
 */
typedef struct CaseWhen {
    NodeTag type;               /**< T_CaseWhen */
    
    Node *expr;                 /* WHEN 条件 */
    Node *result;               /* THEN 结果 */
    int location;               /* 源码位置 */
    
} CaseWhen;

/**
 * @brief COALESCE 表达式
 */
typedef struct CoalesceExpr {
    NodeTag type;               /**< T_CoalesceExpr */
    
    List *args;                 /* 参数列表 */
    int location;               /* 源码位置 */
    
} CoalesceExpr;

/**
 * @brief NULL 测试
 * 
 * IS NULL / IS NOT NULL
 */
typedef struct NullTest {
    NodeTag type;               /**< T_NullTest */
    
    Node *arg;                  /* 测试表达式 */
    int nulltesttype;           /* IS_NULL/IS_NOT_NULL */
    bool argisrow;              /* 是否为行类型 */
    int location;               /* 源码位置 */
    
} NullTest;

#define IS_NULL        0
#define IS_NOT_NULL    1

/**
 * @brief 布尔测试
 * 
 * IS TRUE / IS FALSE / IS UNKNOWN
 */
typedef struct BooleanTest {
    NodeTag type;               /**< T_BooleanTest */
    
    Node *arg;                  /* 测试表达式 */
    int booltesttype;           /* IS_TRUE/IS_FALSE/IS_UNKNOWN/IS_NOT_TRUE/... */
    int location;               /* 源码位置 */
    
} BooleanTest;

#define IS_TRUE           0
#define IS_FALSE          1
#define IS_UNKNOWN        2
#define IS_NOT_TRUE       3
#define IS_NOT_FALSE      4
#define IS_NOT_UNKNOWN    5

/**
 * @brief 子链接
 * 
 * 表示子查询引用
 * EXISTS (SELECT ...), col IN (SELECT ...), col = ANY (SELECT ...)
 */
typedef struct SubLink {
    NodeTag type;               /**< T_SubLink */
    
    int subLinkType;            /* 子查询类型（EXISTS_SUBLINK/ALL_SUBLINK/...） */
    int subLinkId;              /* 子链接 ID */
    Node *testexpr;             /* 测试表达式 */
    List *operName;             /* 操作符名 */
    Node *subselect;            /* 子查询 */
    int location;               /* 源码位置 */
    
} SubLink;

/* 子查询类型 */
#define EXISTS_SUBLINK          0  /* EXISTS */
#define ALL_SUBLINK             1  /* ALL */
#define ANY_SUBLINK             2  /* ANY / IN */
#define ROWCOMPARE_SUBLINK      3  /* 行比较 */
#define EXPR_SUBLINK            4  /* 标量子查询 */
#define MULTIEXPR_SUBLINK       5  /* 多表达式 */
#define ARRAY_SUBLINK           6  /* ARRAY */
#define CTE_SUBLINK             7  /* CTE */

/**
 * @brief 数组表达式
 * 
 * ARRAY[1, 2, 3] / ARRAY(SELECT ...)
 */
typedef struct ArrayExpr {
    NodeTag type;               /**< T_ArrayExpr */
    
    List *elements;             /* 元素列表 */
    bool multidims;             /* 是否多维数组 */
    int location;               /* 源码位置 */
    
} ArrayExpr;

/**
 * @brief 行表达式
 * 
 * ROW(1, 2, 3) / (1, 2, 3)
 */
typedef struct RowExpr {
    NodeTag type;               /**< T_RowExpr */
    
    List *args;                 /* 参数列表 */
    int location;               /* 源码位置 */
    
} RowExpr;

/* ============================================================
 * 窗口函数节点
 * ============================================================ */

/**
 * @brief 窗口定义
 * 
 * WINDOW w AS (PARTITION BY col ORDER BY col)
 */
typedef struct WindowDef {
    NodeTag type;               /**< T_WindowDef */
    
    char *name;                 /* 窗口名 */
    char *refname;              /* 引用的窗口名 */
    List *partitionClause;      /* PARTITION BY */
    List *orderClause;          /* ORDER BY */
    int frameOptions;           /* 帧选项 */
    Node *startOffset;          /* 起始偏移 */
    Node *endOffset;            /* 结束偏移 */
    int location;               /* 源码位置 */
    
} WindowDef;

/* 帧选项 */
#define FRAMEOPTION_NONDEFAULTED_ROWS         0x0001
#define FRAMEOPTION_NONDEFAULTED_RANGE        0x0002
#define FRAMEOPTION_START_UNBOUNDED_PRECEDING 0x0004
#define FRAMEOPTION_END_UNBOUNDED_PRECEDING   0x0008
#define FRAMEOPTION_START_UNBOUNDED_FOLLOWING 0x0010
#define FRAMEOPTION_END_UNBOUNDED_FOLLOWING   0x0020
#define FRAMEOPTION_START_CURRENT_ROW         0x0040
#define FRAMEOPTION_END_CURRENT_ROW           0x0080
#define FRAMEOPTION_START_OFFSET_PRECEDING    0x0100
#define FRAMEOPTION_END_OFFSET_PRECEDING      0x0200
#define FRAMEOPTION_START_OFFSET_FOLLOWING    0x0400
#define FRAMEOPTION_END_OFFSET_FOLLOWING      0x0800
#define FRAMEOPTION_EXCLUDE_CURRENT_ROW       0x1000
#define FRAMEOPTION_EXCLUDE_GROUP             0x2000
#define FRAMEOPTION_EXCLUDE_TIES              0x4000

/* ============================================================
 * CTE 节点
 * ============================================================ */

/**
 * @brief WITH 子句
 */
typedef struct WithClause {
    NodeTag type;               /**< T_WithClause */
    
    List *ctes;                 /* List of CommonTableExpr */
    bool recursive;             /* RECURSIVE 标志 */
    int location;               /* 源码位置 */
    
} WithClause;

/**
 * @brief 公用表表达式（CTE）
 * 
 * WITH cte_name AS (query)
 */
typedef struct CommonTableExpr {
    NodeTag type;               /**< T_CommonTableExpr */
    
    char *ctename;              /* CTE 名称 */
    List *aliascolnames;        /* 列别名列表 */
    List *ctecolnames;         /* 列名列表（递归 CTE） */
    List *ctecoltypes;         /* 列类型列表 */
    List *ctecoltypmods;       /* 列类型修饰符列表 */
    List *ctecolcollations;    /* 排序规则列表 */
    Node *ctequery;            /* 查询 */
    bool recursive;             /* RECURSIVE 标志 */
    int location;               /* 源码位置 */
    
} CommonTableExpr;

/* ============================================================
 * 别名节点
 * ============================================================ */

/**
 * @brief 别名
 * 
 * AS alias (col1, col2, ...)
 */
typedef struct Alias {
    NodeTag type;               /**< T_Alias */
    
    char *aliasname;            /* 别名 */
    List *colnames;             /* 列别名列表 */
    
} Alias;

/* ============================================================
 * 排序/分组节点
 * ============================================================ */

/**
 * @brief 排序元素
 * 
 * ORDER BY col ASC NULLS FIRST
 */
typedef struct SortBy {
    NodeTag type;               /**< T_SortBy */
    
    Node *node;                 /* 排序表达式 */
    int sortby_dir;             /* ASC/DESC/USING/DEFAULT */
    int sortby_nulls;           /* NULLS FIRST/LAST/DEFAULT */
    bool useOp;                 /* USING 操作符 */
    int location;               /* 源码位置 */
    
} SortBy;

/* 排序方向 */
#define SORTBY_DEFAULT      0
#define SORTBY_ASC          1
#define SORTBY_DESC         2
#define SORTBY_USING        3

/* NULL 排序 */
#define SORTBY_NULLS_DEFAULT 0
#define SORTBY_NULLS_FIRST   1
#define SORTBY_NULLS_LAST    2

/**
 * @brief GROUPING SETS
 */
typedef struct GroupingSet {
    NodeTag type;               /**< T_GroupingSet */
    
    int kind;                   /* GROUPING_SET_XXX */
    List *content;              /* 内容列表 */
    int location;               /* 源码位置 */
    
} GroupingSet;

#define GROUPING_SET_EMPTY       0
#define GROUPING_SET_SIMPLE      1
#define GROUPING_SET_ROLLUP      2
#define GROUPING_SET_CUBE        3
#define GROUPING_SET_SETS        4

/* ============================================================
 * 向量表达式节点（扩展）
 * ============================================================ */

/**
 * @brief 向量表达式
 * 
 * 表示向量字面量或向量计算
 * [1.0, 2.0, 3.0]::vector(3)
 */
typedef struct VectorExpr {
    NodeTag type;               /**< T_VectorExpr */
    
    List *elements;             /* 元素列表（List of Float/Integer） */
    int dimensions;             /* 维度 */
    TypeName *typeName;         /* 向量类型（vector/vecf32/vecf16） */
    int location;               /* 源码位置 */
    
} VectorExpr;

/**
 * @brief 向量距离表达式
 * 
 * 表示向量距离计算
 * vector_distance(vec1, vec2, 'l2')
 * vec1 <=> vec2
 */
typedef struct VectorDistance {
    NodeTag type;               /**< T_VectorDistance */
    
    Node *left;                 /* 左向量 */
    Node *right;                /* 右向量 */
    char *metric;               /* 距离度量（l2/cosine/ip） */
    int location;               /* 源码位置 */
    
} VectorDistance;

/* ============================================================
 * JSON 表达式节点（SQL:2016）
 * ============================================================ */

/**
 * @brief JSON 表达式
 * 
 * JSON 路径表达式
 * json_col->'key' / json_col->>'key' / jsonb_path_query(...)
 */
typedef struct JsonExpr {
    NodeTag type;               /**< T_JsonExpr */
    
    int expr_op;                /* JSON 操作符 */
    Node *expr;                 /* JSON 值 */
    Node *path;                 /* JSON 路径 */
    List *passing;              /* PASSING 参数 */
    TypeName *resultType;       /* 返回类型 */
    int location;               /* 源码位置 */
    
} JsonExpr;

/* JSON 操作符 */
#define JSOP_GET           0   /* -> */
#define JSOP_GET_TEXT      1   /* ->> */
#define JSOP_PATH_GET      2   /* jsonb_path_query */
#define JSOP_PATH_EXISTS   3   /* jsonb_path_exists */
#define JSOP_PATH_QUERY    4   /* jsonb_path_query_array */

/* ============================================================
 * 辅助函数声明
 * ============================================================ */

/* 节点操作 */
Node *copyObject(const Node *obj);
bool equal(const Node *a, const Node *b);
void freeObject(Node *obj);

/* List 操作 */
List *lappend(List *list, void *data);
List *lcons(void *data, List *list);
List *lremove(void *data, List *list);
void *list_nth(List *list, int n);
int list_length(const List *list);
bool list_member(List *list, void *data);
void list_free(List *list);

/* 节点构造 */
Node *makeNullAConst(int location);
Node *makeIntAConst(int val, int location);
Node *makeFloatAConst(const char *val, int location);
Node *makeStringAConst(const char *val, int location);
Node *makeBoolAConst(bool val, int location);

/* 类型构造 */
TypeName *makeTypeName(const char *name);
TypeName *makeTypeNameFromNameList(List *names);
TypeName *makeTypeNameFromOid(uint32_t oid, int32_t typmod);

/* 表达式构造 */
A_Expr *makeAExpr(int kind, List *name, Node *lexpr, Node *rexpr, int location);
ColumnRef *makeColumnRef(List *fields, int location);
FuncCall *makeFuncCall(List *name, List *args, int location);
TypeCast *makeTypeCast(Node *arg, TypeName *typeName, int location);

/* 别名构造 */
Alias *makeAlias(const char *aliasname, List *colnames);

/* RangeVar 构造 */
RangeVar *makeRangeVar(const char *schemaname, const char *relname, int location);

#endif /* DB_PARSER_SQL_PARSENODES_H */

/* ============================================================
 * Phase 2: Planner + Optimizer 设计
 * ============================================================
 *
 * 查询优化器是 SQL 引擎的核心，负责将语义分析后的 Query 结构
 * 转换为最优的执行计划。参考 PostgreSQL 的 planner.c 体系。
 *
 * 文件结构：
 * engineering/include/db/sql/planner/
 *   ├── planner.h              # 计划器主接口
 *   ├── path.h                 # 路径抽象
 *   ├── pathnode.h             # 路径节点定义
 *   ├── relation.h             # RelOptInfo 结构
 *   ├── restrictinfo.h         # 限制信息
 *   ├── joininfo.h             # 连接信息
 *   ├── equivclass.h           # 等价类
 *   ├── clauses.h              # 子句处理
 *   ├── cost.h                 # 代价模型
 *   ├── var.h                  # 变量引用
 *   └── subselect.h            # 子查询处理
 *
 * engineering/src/db/sql/planner/
 *   ├── planner.c              # 主计划器入口
 *   ├── planmain.c             # 主计划生成
 *   ├── plancreate.c           # 计划创建
 *   ├── initsplan.c            # 初始化计划
 *   ├── setrefs.c              # 设置引用
 *   ├── subselect.c            # 子查询处理
 *   ├── prepjointree.c         # 准备连接树
 *   ├── inheritance.c          # 继承处理
 *   ├── plannerhook.c          # Hook 机制
 *   └── clamp.c                # 行数估算辅助
 *
 * engineering/src/db/sql/optimizer/
 *   ├── path/
 *   │   ├── costsize.c         # 代价计算
 *   │   ├── clausesel.c        # 子句选择性
 *   │   ├── indxpath.c         # 索引路径
 *   │   ├── tidpath.c          # TID 路径
 *   │   ├── tidcost.c          # TID 代价
 *   │   ├── allpaths.c         # 所有路径生成
 *   │   ├── add_path.c         # 路径添加
 *   │   └── grouping_planner.c # 分组计划
 *   ├── plan/
 *   │   ├── createplan.c       # 创建计划
 *   │   ├── createupperpath.c  # 创建上层路径
 *   │   ├── setrefs.c          # 设置引用
 *   │   ├── initsplan.c        # 初始化子计划
 *   │   └── planagg.c          # 聚合优化
 *   ├── prep/
 *   │   ├── prepjointree.c     # 准备连接树
 *   │   ├── prepqual.c         # 准备条件
 *   │   ├── preptlist.c        # 准备目标列表
 *   │   └── prepunion.c        # 准备集合操作
 *   └── util/
 *       ├── pathnode.c         # 路径节点
 *       ├── restrictinfo.c     # 限制信息
 *       ├── joininfo.c         # 连接信息
 *       ├── clauses.c          # 子句工具
 *       ├── var.c              # 变量工具
 *       ├── orclauses.c        # OR 子句
 *       ├── equivclass.c       # 等价类
 *       ├── tlist.c            # 目标列表
 *       └── pathkeys.c         # 路径键
 */

/* ============================================================
 * 核心数据结构：RelOptInfo
 * ============================================================
 *
 * RelOptInfo 是优化器的核心结构，表示一个关系（表、子查询、连接结果等）
 * 的优化信息。每个 RelOptInfo 维护一组可访问的 Path，优化器从中选择
 * 最优路径。
 */

/**
 * @brief 关系优化信息
 *
 * 对应 PostgreSQL 的 RelOptInfo
 */
typedef struct RelOptInfo {
    NodeTag type;               /**< T_RelOptInfo */

    /* 基本信息 */
    Index relid;                /* 关系 ID（RTE 索引） */
    Relids relids;              /* 关系 ID 集合（用于连接） */
    Oid reltablespace;          /* 表空间 OID */
    int relkind;                /* 关系类型 */
    bool relisnull;             /* 是否为空关系 */

    /* 统计信息 */
    double reltuples;           /* 估计元组数 */
    int relpages;               /* 页面数 */
    int relallvisible;          /* 全可见页面数 */
    int relhasindex;            /* 是否有索引 */

    /* 列信息 */
    List *reltarget;            /* 目标列表 */
    List **attr_needed;         /* 每列的需求计数 */
    int *attr_widths;           /* 每列宽度 */
    List *reltargetexprs;       /* 目标表达式 */

    /* 限制条件 */
    List *baserestrictinfo;     /* 基础限制条件 */
    QualCost baserestrictcost;  /* 限制代价 */
    List *joininfo;             /* 连接条件 */
    List *nullable_relids;      /* 可空关系 ID */

    /* 路径 */
    List *pathlist;             /* 可用路径列表 */
    List *ppilist;              /* 参数化路径列表 */
    Path *cheapest_startup_path; /* 最小启动代价路径 */
    Path *cheapest_total_path;  /* 最小总代价路径 */
    Path *cheapest_unique_path; /* 最小唯一路径 */
    List *partial_pathlist;     /* 部分路径（并行） */

    /* 索引信息 */
    List *indexlist;            /* 索引列表 */
    List *statlist;             /* 统计信息列表 */
    BlockNumber pages;          /* 页面数 */
    double tuples;              /* 元组数 */
    double allvisfrac;          /* 全可见比例 */

    /* 连接信息 */
    Relids direct_lateral_relids; /* 直接 lateral 关系 */
    Relids lateral_relids;      /* Lateral 关系 */
    List *lateral_vars;         /* Lateral 变量 */
    bool lateral_relids_evaluated; /* 是否已评估 */

    /* 子查询信息 */
    struct RelOptInfo *parent;  /* 父关系（子查询） */
    List *subplan;              /* 子计划 */
    List *subroot;              /* 子计划根 */

    /* 上层信息 */
    Relids upperrels;           /* 上层关系 */
    List *upper_targetlist;     /* 上层目标列表 */

} RelOptInfo;

/* 关系类型 */
#define RELKIND_RELATION        'r'    /* 普通表 */
#define RELKIND_INDEX           'i'    /* 索引 */
#define RELKIND_SEQUENCE        'S'    /* 序列 */
#define RELKIND_TOASTVALUE      't'    /* TOAST 表 */
#define RELKIND_VIEW            'v'    /* 视图 */
#define RELKIND_MATVIEW         'm'    /* 物化视图 */
#define RELKIND_COMPOSITE_TYPE  'c'    /* 组合类型 */
#define RELKIND_FOREIGN_TABLE   'f'    /* 外部表 */
#define RELKIND_PARTITIONED_TABLE 'p'  /* 分区表 */
#define RELKIND_PARTITIONED_INDEX 'I'  /* 分区索引 */

/* ============================================================
 * 核心数据结构：Path
 * ============================================================
 *
 * Path 表示访问关系的一种方式。每个 Path 都有代价估算，
 * 优化器通过比较不同 Path 的代价来选择最优计划。
 */

/**
 * @brief 路径类型
 */
typedef enum PathType_e {
    PATH_SCAN,                  /* 扫描路径 */
    PATH_INDEX_SCAN,            /* 索引扫描路径 */
    PATH_BITMAP_SCAN,           /* 位图扫描路径 */
    PATH_TID_SCAN,              /* TID 扫描路径 */
    PATH_SUBQUERY_SCAN,         /* 子查询扫描路径 */
    PATH_FUNCTION_SCAN,         /* 函数扫描路径 */
    PATH_VALUES_SCAN,           /* VALUES 扫描路径 */
    PATH_CTE_SCAN,              /* CTE 扫描路径 */
    PATH_WORKTABLE_SCAN,        /* 工作表扫描路径 */
    PATH_FOREIGN_SCAN,          /* 外部扫描路径 */
    PATH_CUSTOM_SCAN,           /* 自定义扫描路径 */

    PATH_JOIN,                  /* 连接路径 */
    PATH_NESTLOOP,              /* 嵌套循环路径 */
    PATH_MERGEJOIN,             /* 归并连接路径 */
    PATH_HASHJOIN,              /* Hash 连接路径 */

    PATH_AGG,                   /* 聚合路径 */
    PATH_GROUPAGG,              /* 分组聚合路径 */
    PATH_HASHAGG,               /* Hash 聚合路径 */
    PATH_SORTAGG,               /* 排序聚合路径 */

    PATH_SORT,                  /* 排序路径 */
    PATH_LIMIT,                 /* Limit 路径 */
    PATH_UNIQUE,                /* Unique 路径 */
    PATH_MATERIALIZE,           /* 物化路径 */
    PATH_WINDOWAGG,             /* 窗口聚合路径 */
    PATH_SETOP,                 /* 集合操作路径 */
    PATH_RECURSIVEUNION,        /* 递归联合路径 */
    PATH_LOCKROWS,              /* 锁定行路径 */
    PATH_MODIFYTABLE,           /* 修改表路径 */
    PATH_PROJECT,               /* 投影路径 */
    PATH_FILTER,                /* 过滤路径 */

    /* 向量扩展 */
    PATH_VECTOR_SCAN,           /* 向量扫描路径 */
    PATH_HNSW_SCAN,             /* HNSW 扫描路径 */
    PATH_IVF_SCAN,              /* IVF 扫描路径 */
    PATH_DISKANN_SCAN,          /* DiskANN 扫描路径 */

} PathType;

/**
 * @brief 路径基类
 *
 * 所有路径类型的基类，包含通用的代价和行数估算信息
 */
typedef struct Path {
    NodeTag type;               /**< T_Path */

    PathType pathtype;          /* 路径类型 */
    RelOptInfo *parent;         /* 所属关系 */
    PathTarget *pathtarget;     /* 路径目标 */

    /* 代价估算 */
    Cost startup_cost;          /* 启动代价 */
    Cost total_cost;            /* 总代价 */
    double rows;                /* 估计行数 */
    int width;                  /* 行宽度 */

    /* 路径键 */
    List *pathkeys;             /* 排序键 */

    /* 参数化信息 */
    Relids required_outer;      /* 需要的外部参数 */
    Relids param_info;          /* 参数信息 */

    /* 并行信息 */
    bool parallel_aware;        /* 是否并行感知 */
    bool parallel_safe;         /* 是否并行安全 */
    int parallel_workers;       /* 并行工作者数 */

} Path;

/**
 * @brief 扫描路径
 */
typedef struct ScanPath {
    Path path;                  /* 基类 */
    RelOptInfo *scanrel;        /* 扫描的关系 */
    List *scanqual;             /* 扫描条件 */
} ScanPath;

/**
 * @brief 索引扫描路径
 */
typedef struct IndexPath {
    Path path;                  /* 基类 */
    RelOptInfo *indexrel;       /* 索引关系 */
    List *indexclauses;         /* 索引条件 */
    List *indexorderbys;        /* 索引排序 */
    List *indexorderbycols;     /* 索引排序列 */
    bool ispartial;             /* 是否部分索引 */
    List *indextlist;           /* 索引目标列表 */
} IndexPath;

/**
 * @brief 位图扫描路径
 */
typedef struct BitmapHeapPath {
    Path path;                  /* 基类 */
    Path *bitmapqual;           /* 位图条件路径 */
    RelOptInfo *scanrel;        /* 扫描关系 */
    List *bitmapqualorig;       /* 原始位图条件 */
} BitmapHeapPath;

/**
 * @brief 连接路径
 */
typedef struct JoinPath {
    Path path;                  /* 基类 */
    JoinType jointype;          /* 连接类型 */
    Path *outerjoinpath;        /* 外表路径 */
    Path *innerjoinpath;        /* 内表路径 */
    List *joinrestrictinfo;     /* 连接限制条件 */
    bool inner_unique;          /* 内表是否唯一 */
} JoinPath;

/**
 * @brief 嵌套循环路径
 */
typedef struct NestLoopPath {
    JoinPath jpath;             /* 基类 */
    bool nestloop_inner_unique; /* 内表是否唯一 */
} NestLoopPath;

/**
 * @brief Hash 连接路径
 */
typedef struct HashPath {
    JoinPath jpath;             /* 基类 */
    List *path_hashclauses;     /* Hash 条件 */
    int inner_hash_batches;     /* 内表批次数 */
    int inner_hash_capacity;    /* 内表容量 */
    int outer_hash_batches;     /* 外表批次数 */
} HashPath;

/**
 * @brief 归并连接路径
 */
typedef struct MergePath {
    JoinPath jpath;             /* 基类 */
    List *path_mergeclauses;    /* 归并条件 */
    List *outersortkeys;        /* 外表排序键 */
    List *innersortkeys;        /* 内表排序键 */
    bool skip_outer_sort;       /* 是否跳过外表排序 */
    bool skip_inner_sort;       /* 是否跳过内表排序 */
} MergePath;

/**
 * @brief 聚合路径
 */
typedef struct AggPath {
    Path path;                  /* 基类 */
    Path *subpath;              /* 子路径 */
    AggStrategy aggstrategy;    /* 聚合策略 */
    List *groupClause;          /* GROUP BY 子句 */
    List *targetlist;           /* 目标列表 */
    List *qual;                 /* HAVING 条件 */
    int numGroups;              /* 组数 */
    bool transitionSpace;       /* 转换空间 */
} AggPath;

/**
 * @brief 排序路径
 */
typedef struct SortPath {
    Path path;                  /* 基类 */
    Path *subpath;              /* 子路径 */
    List *sortClause;           /* 排序子句 */
} SortPath;

/**
 * @brief Limit 路径
 */
typedef struct LimitPath {
    Path path;                  /* 基类 */
    Path *subpath;              /* 子路径 */
    Node *limitOffset;          /* OFFSET */
    Node *limitCount;           /* LIMIT */
} LimitPath;

/* ============================================================
 * 代价模型
 * ============================================================
 *
 * PostgreSQL 的代价模型是基于 I/O 和 CPU 的混合模型。
 * 代价 = seq_page_cost * 页面数 + cpu_tuple_cost * 元组数 + ...
 */

/**
 * @brief 代价参数配置
 *
 * 对应 GUC 参数，控制代价计算的权重
 */
typedef struct CostParams {
    /* I/O 代价 */
    double seq_page_cost;       /* 顺序页面读取代价 */
    double random_page_cost;    /* 随机页面读取代价 */

    /* CPU 代价 */
    double cpu_tuple_cost;      /* 处理一个元组的代价 */
    double cpu_index_tuple_cost; /* 处理一个索引元组的代价 */
    double cpu_operator_cost;   /* 执行一个操作符的代价 */

    /* 并行代价 */
    double parallel_setup_cost; /* 启动并行工作者的代价 */
    double parallel_tuple_cost; /* 并行传递一个元组的代价 */

    /* 内存代价 */
    double effective_cache_size; /* 有效缓存大小 */

    /* 向量扩展代价 */
    double vector_search_cost;  /* 向量搜索代价 */
    double vector_reorder_cost; /* 向量重排序代价 */
    double vector_distance_cost; /* 向量距离计算代价 */

} CostParams;

/* 默认代价参数（PostgreSQL 默认值） */
#define DEFAULT_COST_PARAMS { \
    .seq_page_cost = 1.0, \
    .random_page_cost = 4.0, \
    .cpu_tuple_cost = 0.01, \
    .cpu_index_tuple_cost = 0.005, \
    .cpu_operator_cost = 0.0025, \
    .parallel_setup_cost = 1000.0, \
    .parallel_tuple_cost = 0.1, \
    .effective_cache_size = 524288, \
    .vector_search_cost = 1.0, \
    .vector_reorder_cost = 0.1, \
    .vector_distance_cost = 0.001 \
}

/**
 * @brief 顺序扫描代价计算
 *
 * cost = seq_page_cost * pages + cpu_tuple_cost * tuples
 */
Cost cost_seqscan(RelOptInfo *rel, CostParams *params);

/**
 * @brief 索引扫描代价计算
 *
 * cost = random_page_cost * index_pages + cpu_index_tuple_cost * tuples
 *      + cpu_operator_cost * selectivity * tuples
 */
Cost cost_index(IndexPath *path, CostParams *params);

/**
 * @brief Hash 连接代价计算
 *
 * 构建阶段：扫描内表 + 构建 Hash 表
 * 探测阶段：扫描外表 + 探测 Hash 表
 */
Cost cost_hashjoin(HashPath *path, CostParams *params);

/**
 * @brief 嵌套循环代价计算
 *
 * cost = outer_cost + inner_cost * outer_rows
 */
Cost cost_nestloop(NestLoopPath *path, CostParams *params);

/**
 * @brief 归并连接代价计算
 *
 * cost = sort_outer + sort_inner + merge_cost
 */
Cost cost_mergejoin(MergePath *path, CostParams *params);

/**
 * @brief 聚合代价计算
 *
 * cost = input_cost + cpu_agg_cost * groups
 */
Cost cost_agg(AggPath *path, CostParams *params);

/**
 * @brief 排序代价计算
 *
 * cost = cpu_operator_cost * tuples * log2(tuples)
 *      + cpu_tuple_cost * tuples
 */
Cost cost_sort(SortPath *path, CostParams *params);

/* ============================================================
 * 优化规则
 * ============================================================
 *
 * PostgreSQL 的优化规则主要通过等价类、限制条件下推、
 * 连接消除等技术实现。
 */

/**
 * @brief 优化规则类型
 */
typedef enum OptRuleType_e {
    /* 下推规则 */
    RULE_PREDICATE_PUSHDOWN,    /* 谓词下推 */
    RULE_AGG_PUSHDOWN,          /* 聚合下推 */
    RULE_WINDOW_PUSHDOWN,       /* 窗口函数下推 */
    RULE_DISTINCT_PUSHDOWN,     /* DISTINCT 下推 */
    RULE_LIMIT_PUSHDOWN,        /* LIMIT 下推 */

    /* 连接优化 */
    RULE_JOIN_REORDERING,       /* 连接重排序 */
    RULE_JOIN_ELIMINATION,      /* 连接消除 */
    RULE_JOIN_PUSHDOWN,         /* 连接条件下推 */

    /* 子查询优化 */
    RULE_SUBQUERY_FLATTENING,   /* 子查询扁平化 */
    RULE_SUBQUERY_PULLUP,       /* 子查询上拉 */
    RULE_SUBQUERY_PUSHDOWN,     /* 子查询下推 */

    /* 表达式优化 */
    RULE_CONSTANT_FOLDING,      /* 常量折叠 */
    RULE_EXPRESSION_SIMPLIFY,   /* 表达式简化 */
    RULE_OR_TO_UNION,           /* OR 转 UNION */
    RULE_IN_TO_EXISTS,          /* IN 转 EXISTS */

    /* 索引优化 */
    RULE_INDEX_SELECTION,       /* 索引选择 */
    RULE_INDEX_ONLY_SCAN,       /* 仅索引扫描 */
    RULE_BITMAP_INDEX_SCAN,     /* 位图索引扫描 */

    /* 物化优化 */
    RULE_MATERIALIZATION,       /* 物化 */
    RULE_MVIEW_REWRITE,         /* 物化视图改写 */

    /* 向量优化 */
    RULE_VECTOR_INDEX_PUSHDOWN, /* 向量索引下推 */
    RULE_VECTOR_BATCH_SCAN,     /* 向量批量扫描 */

} OptRuleType;

/**
 * @brief 优化规则上下文
 */
typedef struct OptContext {
    PlannerInfo *root;          /* 计划器根 */
    RelOptInfo *rel;            /* 当前关系 */
    List *rules;                /* 规则列表 */
    int level;                  /* 递归层级 */
    bool changed;               /* 是否有变化 */
} OptContext;

/**
 * @brief 应用优化规则
 */
void apply_optimization_rule(OptContext *ctx, OptRuleType rule);

/**
 * @brief 谓词下推
 *
 * 将 WHERE 条件尽可能下推到基表扫描
 * 示例：SELECT * FROM t1, t2 WHERE t1.id = 1
 *      → 先过滤 t1，再连接
 */
RelOptInfo *pushdown_predicate(PlannerInfo *root, RelOptInfo *rel);

/**
 * @brief 连接重排序
 *
 * 基于 DP 或贪心算法重排序连接，最小化总代价
 * 示例：A JOIN B JOIN C
 *      → 根据统计信息选择最优连接顺序
 */
RelOptInfo *reorder_joins(PlannerInfo *root, RelOptInfo *rel);

/**
 * @brief 子查询扁平化
 *
 * 将相关子查询转换为连接
 * 示例：SELECT * FROM t1 WHERE id IN (SELECT id FROM t2)
 *      → SELECT t1.* FROM t1 JOIN t2 ON t1.id = t2.id
 */
Query *flatten_subquery(PlannerInfo *root, Query *query);

/**
 * @brief 常量折叠
 *
 * 在计划时计算常量表达式
 * 示例：WHERE 1 + 1 = 2 → WHERE true
 */
Node *constant_folding(Node *expr);

/**
 * @brief 列裁剪
 *
 * 移除不需要的列，减少 I/O 和内存
 * 示例：SELECT id FROM users → 只扫描 id 列
 */
List *prune_columns(List *targetlist, List *required_cols);

/* ============================================================
 * Planner 主入口
 * ============================================================
 */

/**
 * @brief 计划器信息
 *
 * 全局计划器上下文，包含查询的所有信息
 */
typedef struct PlannerInfo {
    NodeTag type;               /**< T_PlannerInfo */

    /* 查询信息 */
    Query *parse;               /* 解析后的查询 */
    List *rtable;               /* 范围表 */
    List *cte_list;             /* CTE 列表 */

    /* 关系信息 */
    List *simple_rel_array;     /* 简单关系数组 */
    RelOptInfo **simple_rte_array; /* 简单 RTE 数组 */
    Relids all_baserels;        /* 所有基础关系 */
    Relids nullable_baserels;   /* 可空基础关系 */
    Relids outer_join_rels;     /* 外连接关系 */

    /* 连接信息 */
    List *join_info_list;       /* 连接信息列表 */
    List *placeholder_list;     /* 占位符列表 */
    List *fkey_list;            /* 外键列表 */

    /* 等价类 */
    List *eq_classes;           /* 等价类列表 */
    List *canon_pathkeys;       /* 规范化路径键 */

    /* 限制信息 */
    List *in_info_list;         /* IN 信息列表 */
    List *append_rel_list;      /* 追加关系列表 */

    /* 代价参数 */
    CostParams *cost_params;    /* 代价参数 */
    double limit_tuples;        /* LIMIT 元组数 */

    /* 配置 */
    PlannerConfig *config;      /* 计划器配置 */
    List *init_plans;           /* 初始化计划 */

    /* 统计 */
    int glob->paramExecTypes;   /* 参数执行类型 */
    List *glob->subplans;       /* 子计划列表 */
    List *glob->subroots;       /* 子计划根 */

} PlannerInfo;

/**
 * @brief 计划器入口
 *
 * 将 Query 转换为 PlannedStmt
 * 对应 PostgreSQL 的 planner() 函数
 */
PlannedStmt *planner(Query *parse, const char *query_string,
                     int cursorOptions, ParamListInfo boundParams);

/**
 * @brief 子计划器
 *
 * 处理子查询的计划生成
 */
PlannedStmt *subquery_planner(PlannerInfo *root, Query *parse,
                              PlannerInfo *parent_root,
                              bool hasRecursion, double tuple_fraction);

/**
 * @brief 分组计划器
 *
 * 处理聚合、分组、窗口函数等
 */
RelOptInfo *grouping_planner(PlannerInfo *root, double tuple_fraction);

/**
 * @brief 路径生成
 *
 * 为关系生成所有可能的访问路径
 */
void generate_paths(PlannerInfo *root, RelOptInfo *rel);

/**
 * @brief 最优路径选择
 *
 * 从路径列表中选择代价最小的路径
 */
Path *get_cheapest_path(PlannerInfo *root, RelOptInfo *rel);

/**
 * @brief 计划创建
 *
 * 从 Path 创建 Plan
 */
Plan *create_plan(PlannerInfo *root, Path *best_path);

/* ============================================================
 * Phase 3: Executor 设计（火山模型）
 * ============================================================
 *
 * 执行器采用火山模型（Volcano Model），每个算子实现 iter_next()
 * 接口，通过拉取（pull）方式驱动执行。
 *
 * 文件结构：
 * engineering/include/db/executor/
 *   ├── executor.h             # 执行器主接口
 *   ├── execnodes.h            # 执行节点定义
 *   ├── execdesc.h             # 执行描述符
 *   ├── execdebug.h            # 调试工具
 *   ├── execExpr.h             # 表达式执行
 *   ├── execScan.h             # 扫描执行
 *   ├── execTuples.h           # 元组操作
 *   ├── execPartition.h        # 分区执行
 *   └── node*.h                # 各算子头文件
 *
 * engineering/src/db/executor/
 *   ├── execMain.c             # 主执行器
 *   ├── execUtils.c            # 工具函数
 *   ├── execExpr.c             # 表达式执行
 *   ├── execScan.c             # 扫描执行
 *   ├── execTuples.c           # 元组操作
 *   ├── execCurrent.c          # CURRENT OF
 *   ├── execGrouping.c         # 分组执行
 *   ├── execIndexing.c         # 索引执行
 *   ├── execJunk.c             # 垃圾列处理
 *   ├── execMain.c             # 主入口
 *   ├── execParallel.c         # 并行执行
 *   ├── execPartition.c        # 分区执行
 *   ├── execReplication.c      # 复制执行
 *   ├── execSRF.c              # 集合返回函数
 *   └── node*.c                # 各算子实现
 */

/* ============================================================
 * 核心数据结构：PlanState
 * ============================================================
 *
 * PlanState 是执行器的核心结构，是 Plan 的运行时表示。
 * 每个 PlanState 维护执行状态和结果元组槽。
 */

/**
 * @brief 计划状态基类
 *
 * 所有算子状态的基类，包含通用的执行状态信息
 */
typedef struct PlanState {
    NodeTag type;               /**< T_PlanState */

    /* 计划信息 */
    Plan *plan;                 /* 关联的计划节点 */
    EState *state;              /* 执行状态 */
    EState *es_instrument;      /* 性能监控 */

    /* 子计划 */
    struct PlanState *lefttree;  /* 左子树 */
    struct PlanState *righttree; /* 右子树 */
    List *initPlan;             /* 初始化计划 */
    List *subPlan;              /* 子计划 */

    /* 目标列表 */
    List *targetlist;           /* 目标列表 */
    List *qual;                 /* 过滤条件 */
    ProjectionInfo *ps_ProjInfo; /* 投影信息 */

    /* 结果槽 */
    TupleTableSlot *ps_ResultTupleSlot; /* 结果元组槽 */
    ExprContext *ps_ExprContext; /* 表达式上下文 */

    /* 并行信息 */
    bool parallel_enabled;      /* 是否并行 */
    ParallelContext *parallel_state; /* 并行状态 */

    /* 性能监控 */
    Instrumentation *instrument; /* 性能数据 */

} PlanState;

/**
 * @brief 扫描状态
 */
typedef struct ScanState {
    PlanState ps;               /* 基类 */
    Relation ss_currentRelation; /* 当前关系 */
    HeapTuple ss_currentTuple;  /* 当前元组 */
    TupleTableSlot *ss_ScanTupleSlot; /* 扫描槽 */
    ScanDesc ss_currentScanDesc; /* 扫描描述符 */
} ScanState;

/**
 * @brief 顺序扫描状态
 */
typedef struct SeqScanState {
    ScanState ss;               /* 基类 */
    HeapScanDesc scan_desc;     /* 堆扫描描述符 */
    List *scan_qual;            /* 扫描条件 */
} SeqScanState;

/**
 * @brief 索引扫描状态
 */
typedef struct IndexScanState {
    ScanState ss;               /* 基类 */
    IndexScanDesc index_scan;   /* 索引扫描描述符 */
    List *index_qual;           /* 索引条件 */
    List *index_orderby;        /* 索引排序 */
    bool index_only;            /* 仅索引扫描 */
    TupleTableSlot *ioss_Slot;  /* 结果槽 */
} IndexScanState;

/**
 * @brief 位图扫描状态
 */
typedef struct BitmapHeapScanState {
    ScanState ss;               /* 基类 */
    TBMIterator *tbm_iterator;  /* 位图迭代器 */
    TBMIterateResult *tbm_result; /* 位图结果 */
    bool tbm_exhausted;         /* 位图耗尽 */
    int tbm_page_index;         /* 页面索引 */
    List *bitmap_qual;          /* 位图条件 */
} BitmapHeapScanState;

/**
 * @brief 连接状态
 */
typedef struct JoinState {
    PlanState ps;               /* 基类 */
    JoinType jointype;          /* 连接类型 */
    ExprState *joinqual;        /* 连接条件 */
    bool join_nullable;         /* 是否可空 */
} JoinState;

/**
 * @brief 嵌套循环状态
 */
typedef struct NestLoopState {
    JoinState js;               /* 基类 */
    bool nl_MatchedOuter;       /* 外表是否匹配 */
    TupleTableSlot *nl_NeedNewOuter; /* 需要新外表元组 */
    TupleTableSlot *nl_NullInnerTupleSlot; /* NULL 内表元组槽 */
} NestLoopState;

/**
 * @brief Hash 连接状态
 */
typedef struct HashJoinState {
    JoinState js;               /* 基类 */
    List *hashclauses;          /* Hash 条件 */
    HashState *hash_state;      /* Hash 表状态 */
    bool hj_HashTableBuilt;     /* Hash 表是否构建 */
    TupleTableSlot *hj_OuterTupleSlot; /* 外表元组槽 */
    TupleTableSlot *hj_InnerTupleSlot; /* 内表元组槽 */
    HashJoinTable hj_HashTable; /* Hash 表 */
    int hj_CurHashValue;        /* 当前 Hash 值 */
    int hj_CurBucketNo;         /* 当前桶号 */
    HashJoinTuple hj_CurTuple;  /* 当前元组 */
} HashJoinState;

/**
 * @brief 归并连接状态
 */
typedef struct MergeJoinState {
    JoinState js;               /* 基类 */
    List *mergeclauses;         /* 归并条件 */
    bool mj_FillOuter;          /* 填充外表 */
    bool mj_FillInner;          /* 填充内表 */
    int mj_Clauses;             /* 条件数 */
    MJBatchState mj_OuterBatch; /* 外表批次 */
    MJBatchState mj_InnerBatch; /* 内表批次 */
    bool mj_MatchedOuter;       /* 外表匹配 */
    bool mj_MatchedInner;       /* 内表匹配 */
    SortState *mj_OuterSort;    /* 外表排序 */
    SortState *mj_InnerSort;    /* 内表排序 */
} MergeJoinState;

/**
 * @brief 聚合状态
 */
typedef struct AggState {
    PlanState ps;               /* 基类 */
    AggStrategy aggstrategy;    /* 聚合策略 */
    int numaggs;                /* 聚合函数数 */
    int numgroups;              /* 组数 */
    List *aggs;                 /* 聚合函数列表 */
    AggStatePerAgg peragg;      /* 每个聚合的状态 */
    AggStatePerGroup *pergroups; /* 每组的状态 */
    TupleTableSlot *evalslot;   /* 评估槽 */
    ExprContext *hashcontext;   /* Hash 上下文 */
    TupleHashTable hashtable;    /* Hash 表 */
    TupleHashIterator hashiter;  /* Hash 迭代器 */
    bool table_filled;          /* 表是否填充 */
    TupleTableSlot *first_slot; /* 第一个槽 */
} AggState;

/**
 * @brief 排序状态
 */
typedef struct SortState {
    PlanState ps;               /* 基类 */
    bool sort_done;             /* 排序是否完成 */
    Tuplesortstate *tuplesortstate; /* 元组排序状态 */
    int64 bound;                /* 限制 */
    bool bounded;               /* 是否有限制 */
    bool randomAccess;          /* 是否随机访问 */
    bool sort_Done;             /* 排序完成 */
    int64 tuples;               /* 元组数 */
} SortState;

/**
 * @brief Limit 状态
 */
typedef struct LimitState {
    PlanState ps;               /* 基类 */
    int64 limit;                /* LIMIT 值 */
    int64 offset;               /* OFFSET 值 */
    int64 position;             /* 当前位置 */
    TupleTableSlot *subslot;    /* 子节点槽 */
    bool lnoLimit;              /* 无限制 */
} LimitState;

/* ============================================================
 * 核心数据结构：TupleTableSlot
 * ============================================================
 *
 * TupleTableSlot 是执行器中元组传递的核心结构。
 * 支持虚拟元组、物理元组、最小元组等多种形式。
 */

/**
 * @brief 元组槽类型
 */
typedef enum TupleTableSlotType_e {
    TTS_TYPE_VIRTUAL,           /* 虚拟元组（值数组） */
    TTS_TYPE_HEAP,              /* 堆元组 */
    TTS_TYPE_MINIMAL,           /* 最小元组 */
    TTS_TYPE_BUFFER,            /* 缓冲元组 */
} TupleTableSlotType;

/**
 * @brief 元组表槽
 *
 * 执行器中元组传递的统一接口
 */
typedef struct TupleTableSlot {
    NodeTag type;               /**< T_TupleTableSlot */
    TupleTableSlotType tts_type; /* 槽类型 */

    /* 元组描述符 */
    TupleDesc tts_tupleDescriptor; /* 元组描述符 */
    bool tts_fixedTupleDescriptor; /* 是否固定描述符 */

    /* 元组数据 */
    HeapTuple tts_tuple;        /* 堆元组 */
    MinimalTuple tts_mintuple;  /* 最小元组 */
    Buffer tts_buffer;          /* 缓冲区 */
    Datum *tts_values;          /* 值数组 */
    bool *tts_isnull;           /* NULL 标志 */
    bool tts_nvalid;            /* 值是否有效 */

    /* 状态标志 */
    bool tts_empty;             /* 是否为空 */
    bool tts_shouldFree;        /* 是否需要释放 */
    bool tts_shouldFreeMin;     /* 是否需要释放最小元组 */
    bool tts_slow;              /* 是否慢速访问 */
    bool tts_off;               /* 偏移 */
    bool tts_mcxt;              /* 内存上下文 */

} TupleTableSlot;

/* ============================================================
 * 执行器主接口
 * ============================================================
 */

/**
 * @brief 执行状态
 *
 * 全局执行上下文，包含事务、内存、快照等
 */
typedef struct EState {
    NodeTag type;               /**< T_EState */

    /* 查询信息 */
    QueryDesc *es_queryDesc;    /* 查询描述符 */
    List *es_range_table;       /* 范围表 */
    List *es_plannedstmt;       /* 计划语句 */

    /* 事务 */
    TransactionId es_snapshot;  /* 快照 */
    TransactionId es_crosscheck_snapshot; /* 交叉检查快照 */
    CommandId es_output_cid;    /* 输出命令 ID */

    /* 统计 */
    uint64 es_processed;        /* 处理的元组数 */
    uint64 es_filtered;         /* 过滤的元组数 */
    Oid es_lastoid;             /* 最后的 OID */

    /* 内存 */
    MemoryContext es_query_cxt; /* 查询内存上下文 */
    MemoryContext es_per_tuple_exprcontext; /* 元组内存上下文 */

    /* 结果 */
    List *es_result_relations;  /* 结果关系 */
    List *es_opened_result_relations; /* 已打开的结果关系 */

    /* 参数 */
    ParamListInfo es_param_list_info; /* 参数列表 */
    List *es_param_exec_vals;   /* 执行参数值 */

    /* 触发器 */
    List *es_trig_target_relations; /* 触发器目标关系 */

    /* 修改 */
    List *es_insert_pending;    /* 待插入 */
    List *es_update_pending;    /* 待更新 */
    List *es_delete_pending;    /* 待删除 */

    /* JIT */
    struct JitContext *es_jit;  /* JIT 上下文 */

} EState;

/**
 * @brief 查询描述符
 */
typedef struct QueryDesc {
    NodeTag type;               /**< T_QueryDesc */

    /* 基本信息 */
    const char *sourceText;     /* 源 SQL */
    CmdType operation;          /* 操作类型 */
    PlannedStmt *plannedstmt;   /* 计划语句 */

    /* 执行 */
    EState *estate;             /* 执行状态 */
    PlanState *planstate;       /* 计划状态 */

    /* 快照 */
    Snapshot snapshot;          /* 快照 */
    Snapshot crosscheck_snapshot; /* 交叉检查快照 */

    /* 目标 */
    DestReceiver *dest;         /* 目标接收器 */

    /* 参数 */
    ParamListInfo params;       /* 参数 */

    /* 性能监控 */
    Instrumentation *totaltime; /* 总时间 */

} QueryDesc;

/**
 * @brief 执行器启动
 *
 * 初始化执行器，创建所有 PlanState
 * 对应 PostgreSQL 的 ExecutorStart()
 */
void ExecutorStart(QueryDesc *queryDesc, int eflags);

/**
 * @brief 执行器运行
 *
 * 执行查询，通过迭代拉取元组
 * 对应 PostgreSQL 的 ExecutorRun()
 */
void ExecutorRun(QueryDesc *queryDesc, ScanDirection direction,
                 uint64 count, bool execute_once);

/**
 * @brief 执行器完成
 *
 * 清理执行器状态
 * 对应 PostgreSQL 的 ExecutorFinish()
 */
void ExecutorFinish(QueryDesc *queryDesc);

/**
 * @brief 执行器结束
 *
 * 释放所有资源
 * 对应 PostgreSQL 的 ExecutorEnd()
 */
void ExecutorEnd(QueryDesc *queryDesc);

/* ============================================================
 * 迭代执行接口（火山模型）
 * ============================================================
 *
 * 火山模型的核心是每个算子实现 ExecProcNode() 接口，
 * 返回一个元组或 NULL（表示结束）。
 */

/**
 * @brief 执行节点获取下一个元组
 *
 * 所有算子都必须实现此接口
 * 返回：下一个元组槽，或 NULL 表示结束
 */
typedef TupleTableSlot *(*ExecProcNodeMtd)(PlanState *node);

/**
 * @brief 执行节点函数
 *
 * 根据节点类型调用对应的执行函数
 */
TupleTableSlot *ExecProcNode(PlanState *node);

/**
 * @brief 顺序扫描执行
 */
TupleTableSlot *ExecSeqScan(SeqScanState *node);

/**
 * @brief 索引扫描执行
 */
TupleTableSlot *ExecIndexScan(IndexScanState *node);

/**
 * @brief 位图扫描执行
 */
TupleTableSlot *ExecBitmapHeapScan(BitmapHeapScanState *node);

/**
 * @brief 嵌套循环执行
 */
TupleTableSlot *ExecNestLoop(NestLoopState *node);

/**
 * @brief Hash 连接执行
 */
TupleTableSlot *ExecHashJoin(HashJoinState *node);

/**
 * @brief 归并连接执行
 */
TupleTableSlot *ExecMergeJoin(MergeJoinState *node);

/**
 * @brief 聚合执行
 */
TupleTableSlot *ExecAgg(AggState *node);

/**
 * @brief 排序执行
 */
TupleTableSlot *ExecSort(SortState *node);

/**
 * @brief Limit 执行
 */
TupleTableSlot *ExecLimit(LimitState *node);

/* ============================================================
 * 表达式执行
 * ============================================================
 */

/**
 * @brief 表达式状态
 *
 * 编译后的表达式，包含执行函数指针
 */
typedef struct ExprState {
    NodeTag type;               /**< T_ExprState */
    Expr *expr;                 /* 表达式节点 */
    ExprStateEvalFunc evalfunc; /* 执行函数 */
    List *innermost_caseval;    /* CASE 值 */
    List *innermost_casenull;   /* CASE NULL */
    List *innermost_domainval;  /* Domain 值 */
    void *resultexpr;           /* 结果表达式 */
} ExprState;

/**
 * @brief 表达式执行函数类型
 */
typedef Datum (*ExprStateEvalFunc)(ExprState *expr, ExprContext *econtext,
                                    bool *isNull);

/**
 * @brief 表达式上下文
 *
 * 包含执行表达式所需的所有上下文
 */
typedef struct ExprContext {
    NodeTag type;               /**< T_ExprContext */

    /* 元组槽 */
    TupleTableSlot *ecxt_scantuple; /* 扫描元组 */
    TupleTableSlot *ecxt_innertuple; /* 内表元组 */
    TupleTableSlot *ecxt_outertuple; /* 外表元组 */

    /* 参数 */
    ParamListInfo ecxt_param_list_info; /* 参数列表 */
    List *ecxt_param_exec_vals; /* 执行参数 */

    /* 聚合 */
    AggState *ecxt_aggstate;    /* 聚合状态 */
    void *ecxt_aggregates;      /* 聚合值 */

    /* CASE */
    Datum *caseValue_datum;     /* CASE 值 */
    bool *caseValue_isNull;     /* CASE NULL */

    /* Domain */
    Datum *domainValue_datum;   /* Domain 值 */
    bool *domainValue_isNull;   /* Domain NULL */

    /* 内存 */
    MemoryContext ecxt_per_tuple_memory; /* 元组内存 */

    /* 父上下文 */
    struct ExprContext *ecxt_per_tuple_exprcontext; /* 父上下文 */

} ExprContext;

/**
 * @brief 编译表达式
 *
 * 将表达式节点编译为可执行的 ExprState
 */
ExprState *ExecInitExpr(Expr *node, PlanState *parent);

/**
 * @brief 执行表达式
 */
Datum ExecEvalExpr(ExprState *state, ExprContext *econtext, bool *isNull);

/* ============================================================
 * 向量执行扩展
 * ============================================================
 */

/**
 * @brief 向量扫描状态
 */
typedef struct VectorScanState {
    ScanState ss;               /* 基类 */
    int vec_dim;                /* 向量维度 */
    int vec_top_k;              /* Top-K */
    float *query_vec;           /* 查询向量 */
    char *distance_metric;      /* 距离度量 */
    VectorIndexDesc *vec_index; /* 向量索引描述符 */
    bool use_index;             /* 是否使用索引 */
} VectorScanState;

/**
 * @brief 向量距离执行
 */
Datum ExecVectorDistance(VectorScanState *state);

/**
 * @brief HNSW 索引扫描
 */
TupleTableSlot *ExecHNSWScan(VectorScanState *node);

/**
 * @brief IVF 索引扫描
 */
TupleTableSlot *ExecIVFScan(VectorScanState *node);

/**
 * @brief DiskANN 扫描
 */
TupleTableSlot *ExecDiskANNScan(VectorScanState *node);

/* ============================================================
 * 并行执行
 * ============================================================
 */

/**
 * @brief 并行上下文
 */
typedef struct ParallelContext {
    NodeTag type;               /**< T_ParallelContext */
    int segsize;                /* 段大小 */
    int nworkers;               /* 工作者数 */
    int nworkers_launched;      /* 已启动工作者数 */
    void *seg;                  /* 共享内存段 */
    dsm_handle dsm_seg;         /* DSM 句柄 */
    shm_toc *toc;               /* 共享内存目录 */
    List *pcxt_list;            /* 子上下文列表 */
    bool *any_worker_error;     /* 工作者错误 */
} ParallelContext;

/**
 * @brief 并行工作状态
 */
typedef struct ParallelWorkerInfo {
    int pid;                    /* 进程 ID */
    shm_mq *mq;                 /* 消息队列 */
    shm_mq_handle *mqh;         /* 消息队列句柄 */
    bool error;                 /* 错误标志 */
} ParallelWorkerInfo;

/**
 * @brief 并行执行初始化
 */
ParallelContext *CreateParallelContext(const char *library_name,
                                        const char *function_name,
                                        int nworkers);

/**
 * @brief 启动并行工作者
 */
void LaunchParallelWorkers(ParallelContext *pcxt);

/**
 * @brief 等待并行工作者完成
 */
void WaitForParallelWorkersToFinish(ParallelContext *pcxt);

/**
 * @brief 销毁并行上下文
 */
void DestroyParallelContext(ParallelContext *pcxt);

/* ============================================================
 * 性能监控（Instrumentation）
 * ============================================================
 */

/**
 * @brief 性能监控数据
 */
typedef struct Instrumentation {
    bool need_timer;            /* 需要计时器 */
    bool need_bufusage;         /* 需要缓冲使用统计 */
    bool running;               /* 是否运行中 */
    bool first_tuple;           /* 第一个元组 */
    instr_time starttime;       /* 开始时间 */
    instr_time counter;         /* 计数器 */
    instr_time firsttuple;      /* 第一个元组时间 */
    double first_tuple_time;    /* 第一个元组时间 */
    double total_tuple_time;    /* 总元组时间 */
    double first_tuple_rows;    /* 第一个元组行数 */
    double total_tuple_rows;    /* 总元组行数 */
    double first_tuple_filter;  /* 第一个元组过滤 */
    double total_tuple_filter;  /* 总元组过滤 */
    BufferUsage bufusage_start; /* 缓冲使用开始 */
    BufferUsage bufusage;       /* 缓冲使用 */
} Instrumentation;

/**
 * @brief 开始监控
 */
void InstrStartNode(Instrumentation *instr);

/**
 * @brief 结束监控
 */
void InstrStopNode(Instrumentation *instr, double nTuples);

/**
 * @brief 聚合监控数据
 */
void InstrAggNode(Instrumentation *dst, Instrumentation *src);

/* ============================================================
 * 完整执行流程示例（详细版）
 * ============================================================
 *
 * 下面以一个完整的查询为例，展示每个阶段的具体输入输出：
 *
 * SQL: SELECT id, name FROM users WHERE age > 30 ORDER BY id LIMIT 10
 *
 * 假设表结构：
 *   CREATE TABLE users (
 *       id      INT PRIMARY KEY,
 *       name    VARCHAR(100),
 *       age     INT,
 *       email   VARCHAR(200)
 *   );
 *   CREATE INDEX idx_users_age ON users(age);
 *
 * 表统计信息：
 *   - 行数：100,000
 *   - 页面数：1,000
 *   - age > 30 的选择性：0.3（约 30,000 行满足条件）
 *   - id 列有主键索引
 *   - age 列有二级索引
 */

/* ============================================================
 * Phase 1: Parser（详细输出）
 * ============================================================
 */

/*
 * 1.1 词法分析输出（Token 流）
 * ----------------------------------------
 *
 * 输入：SELECT id, name FROM users WHERE age > 30 ORDER BY id LIMIT 10
 *
 * 输出 Token 流：
 *   [0] SQL_TOKEN_SELECT    "SELECT"    (line=1, col=1)
 *   [1] SQL_TOKEN_IDENT     "id"        (line=1, col=8)
 *   [2] SQL_TOKEN_COMMA     ","         (line=1, col=10)
 *   [3] SQL_TOKEN_IDENT     "name"      (line=1, col=12)
 *   [4] SQL_TOKEN_FROM      "FROM"      (line=1, col=17)
 *   [5] SQL_TOKEN_IDENT     "users"     (line=1, col=22)
 *   [6] SQL_TOKEN_WHERE     "WHERE"     (line=1, col=28)
 *   [7] SQL_TOKEN_IDENT     "age"       (line=1, col=34)
 *   [8] SQL_TOKEN_GT        ">"         (line=1, col=38)
 *   [9] SQL_TOKEN_INT       "30"        (line=1, col=40, int_value=30)
 *   [10] SQL_TOKEN_ORDER    "ORDER"     (line=1, col=43)
 *   [11] SQL_TOKEN_BY       "BY"        (line=1, col=49)
 *   [12] SQL_TOKEN_IDENT    "id"        (line=1, col=52)
 *   [13] SQL_TOKEN_LIMIT    "LIMIT"     (line=1, col=55)
 *   [14] SQL_TOKEN_INT      "10"        (line=1, col=61, int_value=10)
 *   [15] SQL_TOKEN_EOF      NULL        (line=1, col=63)
 */

/*
 * 1.2 语法分析输出（AST）
 * ----------------------------------------
 *
 * 输出：SelectStmt 结构（JSON 表示）
 */

/*
SelectStmt = {
    .type = T_SelectStmt,
    .targetList = [
        {
            .type = T_ResTarget,
            .val = {
                .type = T_ColumnRef,
                .fields = ["id"],
                .location = 8
            },
            .name = NULL,
            .indirection = NULL
        },
        {
            .type = T_ResTarget,
            .val = {
                .type = T_ColumnRef,
                .fields = ["name"],
                .location = 12
            },
            .name = NULL,
            .indirection = NULL
        }
    ],
    .fromClause = [
        {
            .type = T_RangeVar,
            .schemaname = NULL,
            .relname = "users",
            .alias = NULL,
            .inh = true,
            .relpersistence = 'p',
            .location = 22
        }
    ],
    .whereClause = {
        .type = T_A_Expr,
        .kind = AEXPR_OP,
        .name = [">"],
        .lexpr = {
            .type = T_ColumnRef,
            .fields = ["age"],
            .location = 34
        },
        .rexpr = {
            .type = T_A_Const,
            .val = {
                .type = T_Integer,
                .val.ival = 30
            },
            .location = 40
        },
        .location = 38
    },
    .groupClause = NULL,
    .havingClause = NULL,
    .windowClause = NULL,
    .sortClause = [
        {
            .type = T_SortBy,
            .node = {
                .type = T_ColumnRef,
                .fields = ["id"],
                .location = 52
            },
            .sortby_dir = SORTBY_ASC,
            .sortby_nulls = SORTBY_NULLS_DEFAULT,
            .useOp = NULL,
            .location = 52
        }
    ],
    .limitOffset = NULL,
    .limitCount = {
        .type = T_A_Const,
        .val = {
            .type = T_Integer,
            .val.ival = 10
        },
        .location = 61
    },
    .lockingClause = NULL,
    .withClause = NULL,
    .op = SETOP_NONE,
    .all = false,
    .larg = NULL,
    .rarg = NULL
}
*/

/*
 * 1.3 语义分析输出（Query）
 * ----------------------------------------
 *
 * 输出：Query 结构（经过语义分析，所有引用已解析）
 */

/*
Query = {
    .commandType = CMD_SELECT,
    .queryId = 0,
    .hasAggs = false,
    .hasWindowFuncs = false,
    .hasTargetSRFs = false,
    .hasSubLinks = false,
    
    .rtable = [
        {
            .type = T_RangeTblEntry,
            .rtekind = RTE_RELATION,
            .relid = 16385,  // users 表的 OID
            .relkind = 'r',
            .relname = "users",
            .eref = {
                .aliasname = "users",
                .colnames = ["id", "name", "age", "email"]
            },
            .lateral = false,
            .inh = true,
            .inFromCl = true,
            .requiredPerms = ACL_SELECT,
            .checkAsUser = 0,
            .selectedCols = {0, 1, 2},  // id, name, age
            .insertedCols = {},
            .updatedCols = {}
        }
    ],
    
    .targetList = [
        {
            .type = T_TargetEntry,
            .resno = 1,
            .resname = "id",
            .ressortgroupref = 1,  // 用于 ORDER BY
            .resorigtbl = 16385,   // users
            .resorigcol = 1,       // id 列
            .expr = {
                .type = T_Var,
                .varno = 1,        // RTE 索引
                .varattno = 1,     // 列索引
                .vartype = 23,     // int4
                .vartypmod = -1,
                .varcollid = 0
            }
        },
        {
            .type = T_TargetEntry,
            .resno = 2,
            .resname = "name",
            .ressortgroupref = 0,
            .resorigtbl = 16385,
            .resorigcol = 2,
            .expr = {
                .type = T_Var,
                .varno = 1,
                .varattno = 2,
                .vartype = 1043,   // varchar
                .vartypmod = 104,
                .varcollid = 100
            }
        }
    ],
    
    .jointree = {
        .type = T_FromExpr,
        .fromlist = [
            {
                .type = T_RangeTblRef,
                .rtindex = 1
            }
        ],
        .quals = {
            .type = T_OpExpr,
            .opno = 521,       // int4 > int4 操作符 OID
            .opfuncid = 147,
            .opresulttype = 16, // bool
            .args = [
                {
                    .type = T_Var,
                    .varno = 1,
                    .varattno = 3,  // age
                    .vartype = 23
                },
                {
                    .type = T_Const,
                    .consttype = 23,
                    .constlen = 4,
                    .constvalue = 30,
                    .constisnull = false
                }
            ]
        }
    },
    
    .sortClause = [
        {
            .type = T_SortGroupClause,
            .tleSortGroupRef = 1,  // 引用 targetList[0]
            .eqop = 96,           // int4eq
            .sortop = 97,         // int4lt
            .nulls_first = false,
            .hashable = true
        }
    ],
    
    .limitOffset = NULL,
    .limitCount = {
        .type = T_Const,
        .consttype = 20,    // int8
        .constlen = 8,
        .constvalue = 10,
        .constisnull = false
    }
}
*/

/* ============================================================
 * Phase 2: Planner（详细输出）
 * ============================================================
 */

/*
 * 2.1 RelOptInfo 构建
 * ----------------------------------------
 *
 * PlannerInfo 构建，包含所有关系的优化信息
 */

/*
PlannerInfo = {
    .parse = <上述 Query>,
    .rtable = [
        {
            .rtekind = RTE_RELATION,
            .relid = 16385,
            .relname = "users"
        }
    ],
    
    .simple_rel_array = [
        NULL,  // 索引 0 未使用
        {
            .type = T_RelOptInfo,
            .relid = 1,
            .relids = {1},  // 位图：只包含关系 1
            
            // 统计信息
            .reltuples = 100000.0,
            .relpages = 1000,
            .relallvisible = 1000,
            
            // 代价信息
            .pages = 1000,
            .tuples = 100000,
            .allvisfrac = 1.0,
            
            // 限制条件
            .baserestrictinfo = [
                {
                    .type = T_RestrictInfo,
                    .clause = {
                        .type = T_OpExpr,
                        .opno = 521,  // age > 30
                        .args = [
                            {.type = T_Var, .varattno = 3},
                            {.type = T_Const, .constvalue = 30}
                        ]
                    },
                    .norm_selec = 0.3,  // 选择性
                    .outer_selec = 0.3,
                    .clause_relids = {1},
                    .can_join = false,
                    .pushed_down = false
                }
            ],
            
            // 路径列表（优化器生成的所有可行路径）
            .pathlist = [
                // 路径 1：顺序扫描
                {
                    .type = T_Path,
                    .pathtype = PATH_SCAN,
                    .parent = <RelOptInfo>,
                    .startup_cost = 0.0,
                    .total_cost = 2210.0,  // 计算公式见下
                    .rows = 30000,         // 100000 * 0.3
                    .width = 8
                },
                
                // 路径 2：索引扫描（age 索引）
                {
                    .type = T_IndexPath,
                    .pathtype = PATH_INDEX_SCAN,
                    .parent = <RelOptInfo>,
                    .startup_cost = 0.5,
                    .total_cost = 920.0,   // 更低！
                    .rows = 30000,
                    .width = 8,
                    .indexclauses = [
                        {
                            .type = T_IndexClause,
                            .rinfo = <RestrictInfo>,
                            .indexquals = [<age > 30>]
                        }
                    ]
                }
            ],
            
            .cheapest_startup_path = <IndexPath>,
            .cheapest_total_path = <IndexPath>,
            .cheapest_unique_path = NULL
        }
    ],
    
    .cost_params = {
        .seq_page_cost = 1.0,
        .random_page_cost = 4.0,
        .cpu_tuple_cost = 0.01,
        .cpu_operator_cost = 0.0025
    }
}
*/

/*
 * 2.2 代价计算详解
 * ----------------------------------------
 */

/*
 * 顺序扫描代价计算：
 * ----------------------------------------
 *
 * 公式：
 *   startup_cost = 0
 *   run_cost = seq_page_cost * pages + cpu_tuple_cost * tuples
 *   total_cost = startup_cost + run_cost
 *
 * 计算：
 *   startup_cost = 0
 *   run_cost = 1.0 * 1000 + 0.01 * 100000 = 1000 + 1000 = 2000
 *   filter_cost = 0.0025 * 100000 = 250
 *   total_cost = 0 + 2000 + 250 = 2250
 *
 * 结果：total_cost = 2250.0
 */

/*
 * 索引扫描代价计算：
 * ----------------------------------------
 *
 * 公式：
 *   startup_cost = random_page_cost * 1  // 首页读取
 *   run_cost = random_page_cost * select_pages + cpu_index_tuple_cost * tuples
 *   total_cost = startup_cost + run_cost
 *
 * 计算：
 *   selectivity = 0.3
 *   select_pages = ceil(1000 * 0.3) = 300
 *   startup_cost = 4.0 * 1 = 4.0
 *   run_cost = 4.0 * 300 + 0.005 * 30000 = 1200 + 150 = 1350
 *   filter_cost = 0.0025 * 30000 = 75
 *   total_cost = 4.0 + 1350 + 75 = 1429
 *
 * 结果：total_cost = 1429.0（优于顺序扫描）
 */

/*
 * 2.3 路径选择与 Plan 生成
 * ----------------------------------------
 *
 * 选择最优路径：IndexPath（cost=1429）< SeqPath（cost=2250）
 * 
 * 添加上层节点：
 * 1. Sort(id) - 因为有 ORDER BY id
 * 2. Limit(10) - 因为有 LIMIT 10
 */

/*
 * 生成的 Plan 树：
 * ----------------------------------------
 */

/*
Plan = {
    .type = T_Limit,
    .plan_node_id = 3,
    
    .startup_cost = 1435.0,
    .total_cost = 1435.5,
    .plan_rows = 10,
    .plan_width = 8,
    
    .targetlist = [
        {
            .type = T_TargetEntry,
            .resno = 1,
            .resname = "id",
            .expr = {
                .type = T_Var,
                .varno = 1,
                .varattno = 1
            }
        },
        {
            .type = T_TargetEntry,
            .resno = 2,
            .resname = "name",
            .expr = {
                .type = T_Var,
                .varno = 1,
                .varattno = 2
            }
        }
    ],
    
    .lefttree = {
        .type = T_Sort,
        .plan_node_id = 2,
        
        .startup_cost = 1434.0,  // 排序启动代价
        .total_cost = 1435.0,    // 排序总代价
        .plan_rows = 30,         // 因为 LIMIT 下推，只需排序 30 行
        .plan_width = 8,
        
        .sortClause = [
            {
                .type = T_SortGroupClause,
                .tleSortGroupRef = 1,
                .sortop = 97,  // int4lt
                .nulls_first = false
            }
        ],
        
        .lefttree = {
            .type = T_IndexScan,
            .plan_node_id = 1,
            
            .startup_cost = 4.0,
            .total_cost = 1429.0,
            .plan_rows = 30000,
            .plan_width = 8,
            
            .scanrelid = 1,  // RTE 索引
            .indexid = 16386,  // idx_users_age 的 OID
            .indexqual = [
                {
                    .type = T_IndexClause,
                    .rinfo = {
                        .clause = {
                            .type = T_OpExpr,
                            .opno = 521,
                            .args = [
                                {.type = T_Var, .varattno = 3},
                                {.type = T_Const, .constvalue = 30}
                            ]
                        }
                    }
                }
            ],
            .indexorderbys = [],
            .indexscandir = ForwardScanDirection,
            
            .targetlist = [
                {
                    .type = T_TargetEntry,
                    .resno = 1,
                    .resname = "id",
                    .expr = {.type = T_Var, .varattno = 1}
                },
                {
                    .type = T_TargetEntry,
                    .resno = 2,
                    .resname = "name",
                    .expr = {.type = T_Var, .varattno = 2}
                },
                {
                    .type = T_TargetEntry,
                    .resno = 3,
                    .resname = "age",
                    .expr = {.type = T_Var, .varattno = 3}
                }
            ],
            
            .qual = [],
            
            .lefttree = NULL,
            .righttree = NULL
        },
        
        .righttree = NULL
    },
    
    .righttree = NULL,
    .limitOffset = NULL,
    .limitCount = {
        .type = T_Const,
        .constvalue = 10
    }
}
*/

/*
 * 2.4 优化规则应用
 * ----------------------------------------
 *
 * 应用的优化规则：
 */

/*
 * 规则 1：LIMIT 下推
 * ----------------------------------------
 * 
 * 原始：
 *   Limit(10) → Sort(30000 rows) → IndexScan(30000 rows)
 * 
 * 优化：
 *   Limit(10) → Sort(30 rows) → IndexScan(30 rows)
 * 
 * 原因：只需要前 10 行，所以：
 *   - 排序只需要 30 行（冗余因子 3x）
 *   - 索引扫描提前终止
 * 
 * 代价降低：
 *   - 排序代价：O(30000 * log(30000)) → O(30 * log(30))
 *   - 扫描代价：30000 tuples → 30 tuples
 */

/*
 * 规则 2：列裁剪
 * ----------------------------------------
 * 
 * 原始：扫描 id, name, age, email 四列
 * 
 * 优化：只扫描 id, name, age（email 不在 SELECT 或 WHERE 中）
 * 
 * 减少宽度：32 bytes → 12 bytes
 */

/*
 * 规则 3：索引选择
 * ----------------------------------------
 * 
 * 选择 age 索引（idx_users_age）：
 *   - 代价：1429
 *   vs 顺序扫描：
 *   - 代价：2250
 * 
 * 选择原因：
 *   选择性 0.3 < 临界值 ~0.5，索引扫描更优
 */

/* ============================================================
 * Phase 3: Executor（详细输出）
 * ============================================================
 */

/*
 * 3.1 PlanState 树构建
 * ----------------------------------------
 *
 * ExecutorStart() 创建执行状态树
 */

/*
EState = {
    .es_queryDesc = <QueryDesc>,
    .es_range_table = [<users RTE>],
    .es_snapshot = <当前快照>,
    .es_processed = 0,
    
    .es_query_cxt = <MemoryContext>,
}

LimitState = {
    .type = T_LimitState,
    .plan = <Limit Plan>,
    .state = <EState>,
    
    .limit = 10,
    .offset = 0,
    .position = 0,
    
    .lefttree = {
        .type = T_SortState,
        .plan = <Sort Plan>,
        .state = <EState>,
        
        .sort_done = false,
        .tuplesortstate = NULL,
        .bound = 30,  // 冗余因子 3x
        .bounded = true,
        
        .lefttree = {
            .type = T_IndexScanState,
            .plan = <IndexScan Plan>,
            .state = <EState>,
            
            .scan_relid = 1,
            .index_scan = NULL,  // 初始化时为空
            .index_qual = [<age > 30>],
            .index_orderby = [],
            
            .ss = {
                .ss_currentRelation = <Relation users>,
                .ss_ScanTupleSlot = <TupleTableSlot>,
            },
            
            .lefttree = NULL,
            .righttree = NULL
        },
        
        .righttree = NULL,
        
        .ps_ResultTupleSlot = <TupleTableSlot>,
    },
    
    .righttree = NULL,
    
    .subslot = NULL,
    .ps_ResultTupleSlot = <TupleTableSlot>,
}
*/

/*
 * 3.2 执行流程（火山模型）
 * ----------------------------------------
 *
 * ExecutorRun() 通过迭代拉取元组
 */

/*
 * 步骤 1：ExecLimit() 被调用
 * ----------------------------------------
 */

/*
ExecLimit(LimitState *node) {
    // 第一次调用，初始化
    if (node->position == 0) {
        node->subslot = ExecProcNode(node->lefttree);  // 拉取第一个元组
    }
    
    // 循环拉取，直到达到 limit
    while (node->position < node->limit) {
        TupleTableSlot *slot = node->subslot;
        
        if (TupIsNull(slot)) {
            break;  // 子节点耗尽
        }
        
        node->position++;
        node->subslot = ExecProcNode(node->lefttree);
        
        return slot;  // 返回当前元组
    }
    
    return NULL;  // 达到 limit
}
*/

/*
 * 步骤 2：ExecSort() 被调用（第一次）
 * ----------------------------------------
 */

/*
ExecSort(SortState *node) {
    // 第一次调用，需要收集所有元组并排序
    if (!node->sort_done) {
        // 收集元组
        int nTuples = 0;
        TupleTableSlot **tuples = palloc(sizeof(TupleTableSlot*) * node->bound);
        
        for (int i = 0; i < node->bound; i++) {
            TupleTableSlot *slot = ExecProcNode(node->lefttree);  // 拉取 IndexScan
            
            if (TupIsNull(slot)) {
                break;
            }
            
            tuples[nTuples++] = slot;
        }
        
        // 排序（使用快速排序）
        qsort(tuples, nTuples, sizeof(TupleTableSlot*), compare_by_id);
        
        // 保存到排序状态
        node->tuplesortstate = tuples;
        node->sort_Done = true;
        node->tuples = nTuples;
    }
    
    // 返回下一个排序后的元组
    static int current = 0;
    if (current < node->tuples) {
        return node->tuplesortstate[current++];
    }
    
    return NULL;  // 排序后的元组耗尽
}
*/

/*
 * 步骤 3：ExecIndexScan() 被调用
 * ----------------------------------------
 */

/*
ExecIndexScan(IndexScanState *node) {
    // 初始化索引扫描（第一次调用）
    if (node->index_scan == NULL) {
        Relation indexRel = index_open(node->plan->indexid);
        node->index_scan = index_beginscan(
            node->ss.ss_currentRelation,
            indexRel,
            node->state->es_snapshot,
            node->index_qual,
            node->index_orderby
        );
    }
    
    // 扫描下一个满足条件的元组
    for (;;) {
        HeapTuple tuple = index_getnext(node->index_scan, ForwardScanDirection);
        
        if (tuple == NULL) {
            return NULL;  // 索引扫描完成
        }
        
        // 将 HeapTuple 放入 TupleTableSlot
        ExecStoreTuple(tuple, node->ss.ss_ScanTupleSlot);
        
        // 检查 WHERE 条件（age > 30）
        // 注意：索引已经保证了 age > 30，所以这里无需再检查
        // 但如果有其他条件，需要用 ExecQual() 检查
        
        return node->ss.ss_ScanTupleSlot;
    }
}
*/

/*
 * 3.3 具体执行示例（数据流）
 * ----------------------------------------
 *
 * 假设 users 表有 10 行数据满足 age > 30：
 *   id=1, name='Alice', age=35
 *   id=2, name='Bob', age=40
 *   id=5, name='Charlie', age=45
 *   id=8, name='David', age=32
 *   id=10, name='Eve', age=38
 *   id=12, name='Frank', age=50
 *   id=15, name='Grace', age=33
 *   id=18, name='Henry', age=42
 *   id=20, name='Ivy', age=36
 *   id=22, name='Jack', age=48
 */

/*
执行轮次 1：
----------------------------------------
ExecLimit() 调用
  → position=0, limit=10
  → 调用 ExecSort()
    → sort_done=false, 开始收集元组
    → 调用 ExecIndexScan() 30 次（bound=30）
      → 返回 10 个元组（表中只有 10 行满足条件）
    → 收集到 tuples=[(id=5,'Charlie'),(id=20,'Ivy'),(id=15,'Grace'),(id=10,'Eve'),(id=18,'Henry'),(id=1,'Alice'),(id=12,'Frank'),(id=2,'Bob'),(id=22,'Jack'),(id=8,'David')]
    → 排序后：tuples=[(id=1,'Alice'),(id=2,'Bob'),(id=5,'Charlie'),(id=8,'David'),(id=10,'Eve'),(id=12,'Frank'),(id=15,'Grace'),(id=18,'Henry'),(id=20,'Ivy'),(id=22,'Jack')]
    → sort_done=true
  → 返回第一个元组：(id=1,'Alice')
  → position=1
  → 返回 slot={(id=1,'Alice')}

执行轮次 2：
----------------------------------------
ExecLimit() 调用
  → position=1, limit=10
  → 调用 ExecSort()
    → sort_done=true, 返回第二个元组：(id=2,'Bob')
  → position=2
  → 返回 slot={(id=2,'Bob')}

执行轮次 3：
----------------------------------------
ExecLimit() 调用
  → position=2, limit=10
  → 调用 ExecSort()
    → 返回第三个元组：(id=5,'Charlie')
  → position=3
  → 返回 slot={(id=5,'Charlie')}

... （重复 10 次）

执行轮次 10：
----------------------------------------
ExecLimit() 调用
  → position=9, limit=10
  → 调用 ExecSort()
    → 返回第十个元组：(id=22,'Jack')
  → position=10
  → 返回 slot={(id=22,'Jack')}

执行轮次 11：
----------------------------------------
ExecLimit() 调用
  → position=10, limit=10
  → position >= limit, 返回 NULL
  → 执行结束
*/

/*
 * 3.4 返回给客户端的结果
 * ----------------------------------------
 */

/*
最终返回的 10 行结果：
+----+---------+
| id | name    |
+----+---------+
|  1 | Alice   |
|  2 | Bob     |
|  5 | Charlie |
|  8 | David   |
| 10 | Eve     |
| 12 | Frank   |
| 15 | Grace   |
| 18 | Henry   |
| 20 | Ivy     |
| 22 | Jack    |
+----+---------+

统计信息：
  - 处理行数：10
  - 过滤行数：0（索引扫描已过滤）
  - 扫描页面数：5（索引页面 + 堆页面）
  - 执行时间：2.5ms
*/

/*
 * 3.5 TupleTableSlot 结构详解
 * ----------------------------------------
 */

/*
每个 TupleTableSlot 包含一个元组的数据：

TupleTableSlot (id=1, name='Alice') {
    .type = T_TupleTableSlot,
    .tts_type = TTS_TYPE_VIRTUAL,
    
    .tts_tupleDescriptor = {
        .natts = 2,
        .attrs = [
            {.attname = "id", .atttypid = 23, .attlen = 4},
            {.attname = "name", .atttypid = 1043, .attlen = -1}
        ]
    },
    
    .tts_values = [
        1,              // id = 1 (Datum)
        pointer("Alice") // name = "Alice" (Datum)
    ],
    
    .tts_isnull = [
        false,  // id NOT NULL
        false   // name NOT NULL
    ],
    
    .tts_nvalid = 2,
    .tts_empty = false
}
*/

/* ============================================================
 * 更复杂的查询示例：多表连接
 * ============================================================
 *
 * SQL: SELECT o.order_id, c.customer_name
 *      FROM orders o
 *      JOIN customers c ON o.customer_id = c.customer_id
 *      WHERE o.order_date > '2024-01-01'
 *        AND c.country = 'CN'
 *      ORDER BY o.order_id
 *      LIMIT 100;
 */

/*
 * Phase 1 输出：Query
 * ----------------------------------------
 */

/*
Query = {
    .commandType = CMD_SELECT,
    
    .rtable = [
        {
            .rtekind = RTE_RELATION,
            .relid = 16400,  // orders
            .alias = {.aliasname = "o"}
        },
        {
            .rtekind = RTE_RELATION,
            .relid = 16401,  // customers
            .alias = {.aliasname = "c"}
        }
    ],
    
    .jointree = {
        .type = T_FromExpr,
        .fromlist = [
            {
                .type = T_JoinExpr,
                .jointype = JOIN_INNER,
                .larg = {
                    .type = T_RangeTblRef,
                    .rtindex = 1  // orders
                },
                .rarg = {
                    .type = T_RangeTblRef,
                    .rtindex = 2  // customers
                },
                .quals = {
                    .type = T_OpExpr,
                    .opno = 96,  // int4eq
                    .args = [
                        {.type = T_Var, .varno=1, .varattno=3},  // o.customer_id
                        {.type = T_Var, .varno=2, .varattno=1}   // c.customer_id
                    ]
                }
            }
        ],
        .quals = {
            .type = T_BoolExpr,
            .boolop = AND_EXPR,
            .args = [
                {
                    .type = T_OpExpr,
                    .opno = 523,  // date >
                    .args = [
                        {.type = T_Var, .varno=1, .varattno=2},  // o.order_date
                        {.type = T_Const, .constvalue='2024-01-01'}
                    ]
                },
                {
                    .type = T_OpExpr,
                    .opno = 96,  // texteq
                    .args = [
                        {.type = T_Var, .varno=2, .varattno=3},  // c.country
                        {.type = T_Const, .constvalue='CN'}
                    ]
                }
            ]
        }
    }
}
*/

/*
 * Phase 2 输出：Plan（Hash Join）
 * ----------------------------------------
 */

/*
Plan = {
    .type = T_Limit,
    .plan_node_id = 5,
    
    .lefttree = {
        .type = T_Sort,
        .plan_node_id = 4,
        
        .lefttree = {
            .type = T_HashJoin,
            .plan_node_id = 3,
            
            .jointype = JOIN_INNER,
            .joinqual = [
                {
                    .type = T_OpExpr,
                    .opno = 96,
                    .args = [
                        {.type = T_Var, .varno=1, .varattno=3},
                        {.type = T_Var, .varno=2, .varattno=1}
                    ]
                }
            ],
            
            // 外表：customers（country='CN' 行数少，适合做 Hash 表）
            .lefttree = {
                .type = T_IndexScan,
                .plan_node_id = 1,
                .scanrelid = 2,  // customers
                .indexqual = [<c.country='CN'>]
            },
            
            // 内表：orders（order_date > '2024-01-01'）
            .righttree = {
                .type = T_SeqScan,  // 假设没有 order_date 索引
                .plan_node_id = 2,
                .scanrelid = 1,  // orders
                .qual = [<o.order_date>'2024-01-01'>]
            }
        }
    }
}
*/

/*
 * Phase 3 执行流程：Hash Join
 * ----------------------------------------
 *
 * 步骤 1：构建 Hash 表（扫描 customers）
 *   - 扫描 customers WHERE country='CN'
 *   - 假设 1000 行满足条件
 *   - 构建 Hash(customer_id) → List<Tuple>
 *
 * 步骤 2：探测 Hash 表（扫描 orders）
 *   - 扫描 orders WHERE order_date>'2024-01-01'
 *   - 假设 50000 行满足条件
 *   - 对每个 order，计算 Hash(order.customer_id)
 *   - 在 Hash 表中查找匹配的 customers
 *   - 如果找到，输出连接结果
 *
 * 步骤 3：排序和 Limit
 *   - 收集前 300 行（冗余因子 3x）
 *   - 排序
 *   - 返回前 100 行
 */

/* ============================================================
 * 性能对比：不同执行计划
 * ============================================================
 */

/*
 * 场景 1：无索引 vs 有索引
 * ----------------------------------------
 * 
 * 无索引（顺序扫描）：
 *   SeqScan(users) WHERE age>30
 *   - 代价：2250.0
 *   - 扫描行数：100,000
 *   - 扫描页面：1000
 * 
 * 有索引（索引扫描）：
 *   IndexScan(users_age_idx) WHERE age>30
 *   - 代价：1429.0
 *   - 扫描行数：30,000
 *   - 扫描页面：~300
 * 
 * 性能提升：36%
 */

/*
 * 场景 2：Hash Join vs Nested Loop
 * ----------------------------------------
 * 
 * Nested Loop（外表 1000 行，内表 50000 行）：
 *   代价 = outer_cost + inner_cost * outer_rows
 *        = 10 + 500 * 1000 = 500,010
 * 
 * Hash Join（构建 Hash 表 + 探测）：
 *   代价 = build_cost + probe_cost
 *        = (1000 * 0.01) + (50000 * 0.005)
 *        = 10 + 250 = 260
 * 
 * 性能提升：99.95%（Hash Join 远优于 Nested Loop）
 */

/*
 * 场景 3：LIMIT 下推优化
 * ----------------------------------------
 * 
 * 无 LIMIT 下推：
 *   Limit(10) → Sort(30000) → Scan(30000)
 *   排序代价：O(30000 * log(30000)) ≈ 442,000
 * 
 * 有 LIMIT 下推：
 *   Limit(10) → Sort(30) → Scan(30)
 *   排序代价：O(30 * log(30)) ≈ 147
 * 
 * 性能提升：99.97%
 */

#endif /* DB_PARSER_SQL_PARSENODES_H */