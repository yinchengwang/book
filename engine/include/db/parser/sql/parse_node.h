/**
 * @file parse_node.h
 * @brief SQL 语义分析核心结构定义
 *
 * 包含 ParseState、Query、RangeTblEntry、TargetEntry 等核心结构，
 * 用于将原始 AST 转换为已解析的查询结构。
 */

#ifndef DB_PARSER_SQL_PARSE_NODE_H
#define DB_PARSER_SQL_PARSE_NODE_H

#include "db/parser/sql/parsenodes.h"
#include <stdint.h>
#include <stddef.h>

/* ============================================================
 * 前向声明
 * ============================================================ */

struct ParseState;
struct Query;
struct RangeTblEntry;
struct TargetEntry;
struct Expr;

/* ============================================================
 * 枚举类型
 * ============================================================ */

/**
 * @brief 命令类型
 */
typedef enum CmdType {
    CMD_UNKNOWN,
    CMD_SELECT,
    CMD_INSERT,
    CMD_UPDATE,
    CMD_DELETE,
    CMD_UTILITY,
} CmdType;

/**
 * @brief 范围表项类型
 */
typedef enum RTEKind {
    RTE_RELATION,                   /* 表 */
    RTE_SUBQUERY,                   /* 子查询 */
    RTE_JOIN,                       /* 连接 */
    RTE_FUNCTION,                   /* 函数 */
    RTE_VALUES,                     /* VALUES 子句 */
    RTE_CTE,                        /* CTE */
    RTE_NAMEDTUPLESTORE,            /* 命名元组存储 */
} RTEKind;

/**
 * @brief 表达式解析上下文
 */
typedef enum ParseExprKind {
    EXPR_KIND_NONE,
    EXPR_KIND_SELECT_TARGET,        /* SELECT 目标列表 */
    EXPR_KIND_FROM_SUBSELECT,       /* FROM 子查询 */
    EXPR_KIND_WHERE,                /* WHERE 子句 */
    EXPR_KIND_HAVING,               /* HAVING 子句 */
    EXPR_KIND_FILTER,               /* FILTER 子句 */
    EXPR_KIND_WINDOW_PARTITION,     /* WINDOW PARTITION BY */
    EXPR_KIND_WINDOW_ORDER,         /* WINDOW ORDER BY */
    EXPR_KIND_WINDOW_FRAME,         /* WINDOW FRAME */
    EXPR_KIND_VALUES,               /* VALUES 子句 */
    EXPR_KIND_SET_TARGET,           /* UPDATE SET 目标 */
    EXPR_KIND_GROUP_BY,             /* GROUP BY */
    EXPR_KIND_ORDER_BY,             /* ORDER BY */
    EXPR_KIND_DISTINCT,             /* DISTINCT */
    EXPR_KIND_LIMIT,                /* LIMIT */
    EXPR_KIND_OFFSET,               /* OFFSET */
    EXPR_KIND_CASE,                 /* CASE 表达式 */
    EXPR_KIND_FUNCTION,             /* 函数参数 */
    EXPR_KIND_DEFAULT,              /* DEFAULT 表达式 */
    EXPR_KIND_CONSTRAINT,           /* 约束表达式 */
} ParseExprKind;

/**
 * @brief RLS 支持类型
 */
typedef enum RLS_SUPPORT {
    RLS_NONE,
    RLS_SELECT,
    RLS_INSERT,
    RLS_UPDATE,
    RLS_DELETE,
} RLS_SUPPORT;

/* ============================================================
 * OID 类型定义（简化版）
 * ============================================================ */

typedef uint32_t Oid;
typedef int16_t AttrNumber;
typedef uint32_t Index;

#define InvalidOid        ((Oid) 0)
#define OidIsValid(oid)   ((oid) != InvalidOid)

/* ============================================================
 * Alias 结构
 * ============================================================ */

/**
 * @brief 别名结构
 */
typedef struct Alias {
    NodeTag type;
    char *aliasname;         /* 别名 */
    List *colnames;          /* 列别名列表 */
} Alias;

/* ============================================================
 * Expr 结构（简化版）
 * ============================================================ */

/**
 * @brief 表达式基类
 */
typedef struct Expr {
    NodeTag type;
} Expr;

/**
 * @brief Var 变量引用
 */
typedef struct Var {
    Expr expr;
    Index varno;             /* 范围表索引 */
    AttrNumber varattno;     /* 列编号 */
    Oid vartype;             /* 类型 OID */
    int32_t vartypmod;       /* 类型修饰符 */
    Oid varcollid;           /* 排序规则 OID */
    Index varnoold;          /* 原始范围表索引 */
    AttrNumber varoattno;    /* 原始列编号 */
    int location;            /* 源码位置 */
} Var;

/**
 * @brief Const 常量
 */
typedef struct Const {
    Expr expr;
    Oid consttype;           /* 类型 OID */
    int32_t consttypmod;     /* 类型修饰符 */
    Oid constcollid;         /* 排序规则 OID */
    int constlen;            /* 类型长度 */
    bool constisnull;        /* 是否为 NULL */
    bool constbyval;         /* 是否按值传递 */
    uintptr_t constvalue;    /* 值（简化为 uintptr_t） */
} Const;

/**
 * @brief OpExpr 操作符表达式
 */
typedef struct OpExpr {
    Expr expr;
    Oid opno;                /* 操作符 OID */
    Oid opfuncid;            /* 函数 OID */
    Oid opresulttype;        /* 结果类型 */
    bool opretset;           /* 是否返回集合 */
    Oid opcollid;            /* 排序规则 OID */
    Oid inputcollid;         /* 输入排序规则 */
    List *args;              /* 参数列表 */
    int location;            /* 源码位置 */
} OpExpr;

/**
 * @brief FuncExpr 函数调用
 */
typedef struct FuncExpr {
    Expr expr;
    Oid funcid;              /* 函数 OID */
    Oid funcresulttype;      /* 结果类型 */
    bool funcretset;         /* 是否返回集合 */
    bool funcvariadic;       /* 是否可变参数 */
    Oid funccollid;          /* 排序规则 OID */
    Oid inputcollid;         /* 输入排序规则 */
    List *args;              /* 参数列表 */
    int location;            /* 源码位置 */
} FuncExpr;

/**
 * @brief Aggref 聚合函数引用
 */
typedef struct Aggref {
    Expr expr;
    Oid aggfnoid;            /* 聚合函数 OID */
    Oid aggtype;             /* 结果类型 */
    Oid aggcollid;           /* 排序规则 OID */
    Oid inputcollid;         /* 输入排序规则 */
    Oid aggtranstype;        /* 过渡类型 */
    List *aggargtypes;       /* 参数类型列表 */
    List *aggdirectargs;     /* 直接参数列表 */
    List *args;              /* 参数列表 */
    List *aggorder;          /* ORDER BY 列表 */
    List *aggdistinct;       /* DISTINCT 列表 */
    Node *aggfilter;         /* FILTER 子句 */
    bool aggstar;            /* 是否为 * */
    bool aggvariadic;        /* 是否可变参数 */
    bool aggpresorted;       /* 是否预排序 */
    Index agglevelsup;       /* 外层引用层级 */
    int location;            /* 源码位置 */
} Aggref;

/**
 * @brief SubLink 子链接
 */
typedef struct SubLink {
    Expr expr;
    int subLinkType;         /* 子链接类型 */
    int subLinkId;           /* 子链接 ID */
    Node *testexpr;          /* 测试表达式 */
    List *operName;          /* 操作符名 */
    struct Query *subselect; /* 子查询 */
    int location;            /* 源码位置 */
} SubLink;

/* ============================================================
 * RangeTblEntry - 范围表项
 * ============================================================ */

/**
 * @brief 范围表项（Range Table Entry）
 *
 * 表示 FROM 子句中的每个表、子查询、连接等。
 */
typedef struct RangeTblEntry {
    NodeTag type;

    /* 基本信息 */
    RTEKind rtekind;                  /* RTE 类型 */
    Oid relid;                        /* 表 OID（RTE_RELATION） */
    char *relname;                    /* 表名 */
    char *refname;                    /* 别名 */
    Alias *eref;                      /* 列别名列表 */

    /* 继承 */
    bool inh;                         /* 是否继承子表 */

    /* 列信息 */
    List *lateral_vars;               /* LATERAL 变量 */
    bool lateral;                     /* 是否 LATERAL */

    /* 权限 */
    int32_t rellockmode;              /* 锁模式 */

    /* 子查询信息（RTE_SUBQUERY） */
    struct Query *subquery;           /* 子查询 */

    /* 函数信息（RTE_FUNCTION） */
    Node *funcexpr;                   /* 函数表达式 */

    /* VALUES 信息（RTE_VALUES） */
    List *values_lists;               /* VALUES 列表 */

    /* 连接信息（RTE_JOIN） */
    JoinType jointype;                /* 连接类型 */
    List *joinaliasvars;              /* 连接列别名 */
    int joinleftcols;                 /* 左表列数 */
    int joinrightcols;                /* 右表列数 */
    List *join_using_alias;           /* USING 别名 */

    /* CTE 信息（RTE_CTE） */
    char *ctename;                    /* CTE 名称 */
    Index ctelevelsup;                /* CTE 层级 */
    bool self_reference;              /* 是否自引用 */

    /* 命名元组存储（RTE_NAMEDTUPLESTORE） */
    char *enrname;                    /* 元组存储名称 */

    /* 位置 */
    Index rtepermissions;             /* 权限索引 */
    Index perminfoindex;              /* 权限信息索引 */
} RangeTblEntry;

/* ============================================================
 * TargetEntry - 目标列项
 * ============================================================ */

/**
 * @brief 目标列项
 *
 * 表示 SELECT 目标列表或 INSERT/UPDATE 目标列。
 */
typedef struct TargetEntry {
    NodeTag type;

    Expr *expr;                       /* 表达式 */
    AttrNumber resno;                 /* 结果列编号 */
    char *resname;                    /* 结果列名 */
    Index ressortgroupref;            /* 排序/分组引用 */
    Oid resorigtbl;                   /* 原始表 OID */
    AttrNumber resorigcol;            /* 原始列编号 */
    bool resjunk;                     /* 是否为隐藏列 */
} TargetEntry;

/* ============================================================
 * FromExpr - FROM 子句表达式
 * ============================================================ */

/**
 * @brief FROM 子句表达式
 *
 * 表示 FROM 子句的连接树。
 */
typedef struct FromExpr {
    NodeTag type;
    List *fromlist;                   /* FROM 列表 */
    Node *quals;                      /* WHERE 条件 */
} FromExpr;

/**
 * @brief JoinExpr 连接表达式
 */
typedef struct JoinExprInfo {
    NodeTag type;
    JoinType jointype;                /* 连接类型 */
    List *fromlist;                   /* 连接列表 */
    Node *quals;                      /* 连接条件 */
} JoinExprInfo;

/* ============================================================
 * Query - 已解析的查询结构
 * ============================================================ */

/**
 * @brief 查询结构
 *
 * 语义分析的输出，表示一个完整的 SQL 语句。
 */
typedef struct Query {
    NodeTag type;

    /* 命令类型 */
    CmdType commandType;              /* CMD_SELECT/INSERT/UPDATE/DELETE/UTILITY */

    /* 查询 ID */
    uint64_t queryId;                 /* 查询标识符 */

    /* 是否有聚合函数 */
    bool hasAggs;
    bool hasWindowFuncs;
    bool hasTargetSRFs;
    bool hasSubLinks;
    bool hasDistinctOn;
    bool hasRecursive;
    bool hasModifyingCTE;
    bool hasForUpdate;
    bool hasRowSecurity;

    /* 范围表 */
    List *rtable;                     /* RangeTblEntry 列表 */
    List *rteperminfos;               /* 权限信息列表 */

    /* 目标列表 */
    List *targetList;                 /* TargetEntry 列表 */

    /* 连接信息 */
    FromExpr *jointree;               /* 连接树 */

    /* GROUP BY */
    List *groupClause;                /* GROUP BY 列表 */
    bool groupDistinct;               /* GROUP BY DISTINCT */
    Node *havingQual;                 /* HAVING 条件 */

    /* WINDOW */
    List *windowClause;               /* WINDOW 定义 */

    /* DISTINCT */
    List *distinctClause;             /* DISTINCT ON 列表 */

    /* ORDER BY */
    List *sortClause;                 /* ORDER BY 列表 */

    /* LIMIT/OFFSET */
    Node *limitOffset;                /* OFFSET 值 */
    Node *limitCount;                 /* LIMIT 值 */
    List *limitOptions;               /* LIMIT 选项 */

    /* WITH CHECK OPTION */
    bool withCheckOptions;

    /* 集合操作 */
    Node *setOperations;              /* UNION/INTERSECT/EXCEPT */

    /* 结果关系（DML） */
    Index resultRelation;             /* 结果表的 rtable 索引 */
    List *returningList;              /* RETURNING 列表 */
    List *onConflict;                 /* ON CONFLICT 子句 */

    /* CTE */
    List *cteList;                    /* WITH 子句 */

    /* DDL 语句 */
    Node *utilityStmt;                /* DDL 命令 */

    /* 其他 */
    List *constraintDeps;             /* 依赖的约束 */
    List *withCheckOptionsList;       /* WITH CHECK OPTION 列表 */
} Query;

/* ============================================================
 * ParseState - 解析状态
 * ============================================================ */

/**
 * @brief 解析状态
 *
 * 语义分析期间的上下文，包含当前解析环境信息。
 */
typedef struct ParseState {
    struct ParseState *parentParseState;  /* 父解析状态（子查询） */

    /* 查询环境 */
    List *p_rtable;                  /* 范围表列表 */
    List *p_joinexprs;               /* 连接表达式列表 */
    List *p_namespace;               /* 命名空间（可见列） */

    /* 目标列表 */
    List *p_targetlist;              /* 目标列列表 */

    /* 参数 */
    List *p_paramtypes;              /* 参数类型列表 */
    int p_numparams;                 /* 参数数量 */

    /* CTE */
    List *p_ctenamespace;            /* CTE 命名空间 */
    List *p_parent_ctenamespace;     /* 父级 CTE 命名空间 */

    /* 聚合和窗口函数 */
    List *p_aggs;                    /* 聚合函数列表 */
    List *p_groupClause;             /* GROUP BY 列表 */
    bool p_hasAggs;                  /* 是否有聚合函数 */
    bool p_hasWindowFuncs;           /* 是否有窗口函数 */

    /* 子链接 */
    List *p_sublinks;                /* 子链接列表 */

    /* 表达式解析上下文 */
    ParseExprKind p_expr_kind;       /* 当前表达式类型 */

    /* 错误处理 */
    char *p_error_msg;               /* 错误消息 */
    int p_error_location;            /* 错误位置 */

    /* 源文本 */
    const char *p_sourcetext;        /* SQL 源文本 */

    /* Catalog 引用 */
    void *p_catalog;                 /* Catalog 句柄（简化为 void*） */

    /* 关系信息 */
    List *p_relnamespace;            /* 关系命名空间 */
    List *p_lateral_active;          /* LATERAL 活动列表 */

    /* 权限检查 */
    bool p_hasRowSecurity;           /* 是否有行级安全 */
    bool p_rls_enabled;              /* RLS 是否启用 */
    RLS_SUPPORT p_rls_support;       /* RLS 支持类型 */
} ParseState;

/* ============================================================
 * 函数声明
 * ============================================================ */

/* 创建/销毁 ParseState */
ParseState *make_parsestate(ParseState *parentParseState);
void free_parsestate(ParseState *pstate);

/* 语义分析主入口 */
Query *parse_analyze(Node *parseTree, const char *sourceText,
                     Oid *paramTypes, int numParams);

Query *parse_analyze_varparams(Node *parseTree, const char *sourceText,
                               Oid **paramTypes, int *numParams);

/* 语句分析 */
Query *transformStmt(ParseState *pstate, Node *stmt);
Query *transformSelectStmt(ParseState *pstate, SelectStmt *stmt);
Query *transformInsertStmt(ParseState *pstate, InsertStmt *stmt);
Query *transformUpdateStmt(ParseState *pstate, UpdateStmt *stmt);
Query *transformDeleteStmt(ParseState *pstate, DeleteStmt *stmt);
Query *transformCreateStmt(ParseState *pstate, CreateStmt *stmt);
Query *transformDropStmt(ParseState *pstate, DropStmt *stmt);

/* 表达式分析 */
Node *transformExpr(ParseState *pstate, Node *expr, ParseExprKind exprKind);
Node *transformColumnRef(ParseState *pstate, ColumnRef *cref);
Node *transformConst(ParseState *pstate, A_Const *aconst);
Node *transformA_Expr(ParseState *pstate, A_Expr *a);
Node *transformFuncCall(ParseState *pstate, FuncCall *fc);
Node *transformBoolExpr(ParseState *pstate, BoolExpr *bexpr);
Node *transformNullTest(ParseState *pstate, NullTest *ntest);
Node *transformTypeCast(ParseState *pstate, TypeCast *tc);
Node *transformCaseExpr(ParseState *pstate, CaseExpr *cexpr);
Node *transformSubLink(ParseState *pstate, SubLink *sublink);
Node *transformListExpr(ParseState *pstate, List *list, ParseExprKind exprKind);
bool isAggregateFunction(const char *funcname);

/* 子句分析 */
List *transformFromClause(ParseState *pstate, List *frmList);
List *transformTargetList(ParseState *pstate, List *targetlist);
Node *transformWhereClause(ParseState *pstate, Node *whereClause,
                           ParseExprKind exprKind, const char *constructName);
List *transformGroupClause(ParseState *pstate, List *grouplist,
                           List **groupingSets, List **targetlist,
                           List *sortClause, ParseExprKind exprKind);
List *transformSortClause(ParseState *pstate, List *orderlist,
                          List **targetlist, ParseExprKind exprKind);
List *transformWindowClause(ParseState *pstate, List *windowlist);

/* RangeTblEntry 操作 */
RangeTblEntry *addRangeTableEntry(ParseState *pstate, RangeVar *relation,
                                   Alias *alias, bool inh, bool inFromCl);
RangeTblEntry *addRangeTableEntryForSubquery(ParseState *pstate, Query *subquery,
                                              Alias *alias, bool lateral, bool inFromCl);
RangeTblEntry *addRangeTableEntryForFunction(ParseState *pstate, List *funcnames,
                                              List *funcexprs, List *coldeflists,
                                              Alias *alias, bool lateral, bool inFromCl);
RangeTblEntry *addRangeTableEntryForValues(ParseState *pstate, List *valueslists,
                                            Alias *alias, bool lateral, bool inFromCl);
RangeTblEntry *addRangeTableEntryForJoin(ParseState *pstate, JoinType jointype,
                                          List *joinusing, Alias *join_using_alias,
                                          List *colnames, List *leftcols, List *rightcols,
                                          Alias *alias, bool inFromCl);
RangeTblEntry *addRangeTableEntryForCTE(ParseState *pstate, char *ctename,
                                         List *ctecolnames, List *ctecoltypes,
                                         List *ctecoltypmods, List *ctecolcollations,
                                         bool self_reference);

/* 列解析 */
Node *scanNameSpaceForRefname(ParseState *pstate, const char *refname, int location);
Node *scanNameSpaceForRelid(ParseState *pstate, Oid relid, int location);
void markVarForSelectPriv(ParseState *pstate, Var *var);

/* 表达式构造函数 */
Var *makeVar(Index varno, AttrNumber varattno, Oid vartype,
             int32_t vartypmod, Oid varcollid, int location);
Const *makeConst(Oid consttype, int32_t consttypmod, Oid constcollid,
                 int constlen, uintptr_t constvalue, bool constisnull,
                 bool constbyval);
OpExpr *makeOpExpr(Oid opno, Oid opresulttype, List *args, int location);
BoolExpr *makeBoolExpr(BoolType boolop, List *args);

/* 辅助函数 */
Node *makeNode(NodeTag type);
#define nodeTag(nodeptr) (((Node*)(nodeptr))->type)

#endif /* DB_PARSER_SQL_PARSE_NODE_H */
