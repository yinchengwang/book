/**
 * @file expr.h
 * @brief 表达式求值器（ExprState + ExprEvalStep 字节码解释器）
 *
 * 本文件是 SQL 执行器 Phase 1 基础设施层的第三个任务（Task 1.3）。
 *
 * 设计概述：
 *   - Expr：解析/规划阶段生成的原始表达式树节点
 *   - ExprState：编译后的运行时状态，包含 ExprEvalStep 字节码序列
 *   - ExprEvalStep：单个求值操作（取常量、取变量、调函数、跳转型控制流）
 *
 * 字节码解释器（expr_interp.c）按 steps 顺序遍历，遇 EEOP_DONE 返回结果。
 * 当前版本支持的子集：
 *   - EEOP_CONST / EEOP_CONST_NULL
 *   - EEOP_NULLTEST_ISNULL / EEOP_NULLTEST_ISNOTNULL
 *   - EEOP_SCAN_VAR / EEOP_INNER_VAR / EEOP_OUTER_VAR
 *   - EEOP_DONE
 *   - EEOP_FUNCEXPR_STANDARD / EEOP_FUNCEXPR_STRICT（占位实现）
 *   - EEOP_JUMP* 控制流（占位实现）
 *
 * 后续任务将逐步扩展操作码。
 */

#ifndef DB_SQL_EXPR_H
#define DB_SQL_EXPR_H

#include <stdint.h>
#include <stdbool.h>

/* 获取共享类型定义 */
#include "db/sql/sql_types.h"
#include "db/sql/nodes/nodetags.h"
/* 获取 Oid、PlanState 等类型定义 */
#include "db/sql/nodes/execnodes.h"
#include "db/sql/memctx.h"

/* 前向声明：Expr 完整定义在 sql_planner.h */
#ifndef EXPR_DEFINED
#define EXPR_DEFINED
typedef struct Expr_s Expr;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct ExprEvalStep ExprEvalStep;

/* Expr 前向声明（完整定义在 sql_planner.h 中） */
typedef struct Expr_s Expr;

/* FmgrInfo 占位结构（函数管理信息，后续任务会补充） */
typedef struct FmgrInfo {
    Oid          fn_oid;        /**< 函数 OID */
    void        *fn_handler;    /**< 函数指针（占位） */
} FmgrInfo;

/* ========================================================================
 * ExprEvalOp - 字节码操作码
 *
 * 第一版只要求完整定义（用于测试枚举值），
 * 解释器实际支持的子集见 expr_interp.c。
 * ======================================================================== */

typedef enum ExprEvalOp {
    /* 控制流 */
    EEOP_DONE = 0,
    EEOP_JUMP,
    EEOP_JUMP_IF_NULL,
    EEOP_JUMP_IF_NOT_NULL,
    EEOP_JUMP_IF_NOT_TRUE,
    EEOP_BOOL_AND_STEP_FIRST,
    EEOP_BOOL_AND_STEP,
    EEOP_BOOL_OR_STEP_FIRST,
    EEOP_OR_STEP,
    EEOP_BOOL_NOT_STEP,

    /* Var 访问 */
    EEOP_INNER_VAR,
    EEOP_OUTER_VAR,
    EEOP_SCAN_VAR,
    EEOP_ASSIGN_INNER_VAR,
    EEOP_ASSIGN_OUTER_VAR,
    EEOP_ASSIGN_SCAN_VAR,

    /* 参数 */
    EEOP_PARAM_EXEC,
    EEOP_PARAM_EXTERN,

    /* 常量 */
    EEOP_CONST,
    EEOP_CONST_NULL,

    /* 运算 */
    EEOP_FUNCEXPR_STANDARD,
    EEOP_FUNCEXPR_STRICT,
    EEOP_AGGREF,
    EEOP_WINDOW_FUNC,

    /* NULL 测试 */
    EEOP_NULLTEST_ISNULL,
    EEOP_NULLTEST_ISNOTNULL,

    /* 布尔测试 */
    EEOP_BOOLTEST_IS_TRUE,
    EEOP_BOOLTEST_IS_FALSE,

    /* 复杂表达式 */
    EEOP_CASE_TESTVAL,
    EEOP_ARRAYEXPR,
    EEOP_ROW,
    EEOP_NULLIF,
    EEOP_COALESCE,

    EEOP_MAX
} ExprEvalOp;

/* ========================================================================
 * ExprEvalStep - 单个字节码步骤
 *
 * union d 描述该步骤需要的参数：
 *   - var:  Var 访问（scan/inner/outer）
 *   - const_: 常量加载
 *   - jump: 跳转目标
 *   - func: 函数调用
 *
 * 当前字节码解释器约定：
 *   - 顺序执行，步骤的输入/输出通过 result/isnull 显式读写
 *   - 每个步骤必须正确初始化 union 中所需的字段
 * ======================================================================== */

struct ExprEvalStep {
    ExprEvalOp      op;
    union {
        /* Var 访问 */
        struct {
            int         var_varno;   /**< Var 节点：varno（占位，本任务未启用） */
            int         var_attno;   /**< Var 节点：attno */
            Datum      *result;      /**< 输出 Datum 指针 */
            bool       *isnull;      /**< 输出 NULL 标志指针 */
        } var;

        /* 常量 */
        struct {
            Datum       constval;
            Oid         consttype;
            bool        constisnull;
        } const_;

        /* 跳转 */
        struct {
            int         jumpdest;    /**< 目标步骤索引 */
        } jump;

        /* 函数调用 */
        struct {
            FmgrInfo            func;
            int                 nargs;
            struct ExprState  **args;
        } func;
    } d;
};

/* ========================================================================
 * ExprState - 表达式编译产物
 *
 * 编译时由 ExecInitExpr 生成，使用方调用 ExecEvalExpr 执行，
 * 调用方负责在合适的内存上下文中分配并最终调用 ExecFreeExpr 释放。
 * ======================================================================== */

struct ExprState {
    NodeTag                  type;          /**< T_ExprState */
    Expr                    *expr;          /**< 原始表达式树（与 sql_planner.h 一致） */
    ExprEvalStep            *steps;         /**< 编译后的步骤数组 */
    int                      nsteps;        /**< 步骤数量 */
    struct ExprContext      *eval_ctx;       /**< 求值上下文 */
    Datum                   *resvalue;      /**< 最终结果 Datum */
    bool                    *resnull;       /**< 最终结果 NULL 标志 */
};

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 表达式编译：将 Expr 树编译为 ExprEvalStep 序列
 *
 * @param expr   原始表达式树
 * @param parent 父 PlanState（用于上下文；可为空）
 *
 * @return 新创建的 ExprState；失败返回 NULL
 */
ExprState *ExecInitExpr(Expr *expr, PlanState *parent);

/**
 * @brief 表达式求值
 *
 * 按 steps 顺序遍历，EEOP_DONE 时返回结果 Datum。
 *
 * @param state   ExprState（必须非空）
 * @param context 求值上下文（可为 NULL）
 * @param isnull  输出参数：结果是否为 NULL
 *
 * @return 求值结果 Datum
 */
Datum ExecEvalExpr(ExprState *state, ExprContext *context, bool *isnull);

/**
 * @brief 布尔表达式求值（用于 WHERE 条件）
 *
 * 等价于 ExecEvalExpr 然后把 Datum 转为 bool。
 *
 * @param state   ExprState（必须非空）
 * @param context 求值上下文（可为 NULL）
 *
 * @return 布尔结果；NULL 被视为 false
 */
bool ExecCheckEvalExpr(ExprState *state, ExprContext *context);

/**
 * @brief 释放表达式状态
 *
 * @param state ExprState（可为 NULL）
 */
void ExecFreeExpr(ExprState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_EXPR_H */