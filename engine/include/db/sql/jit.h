/**
 * @file jit.h
 * @brief JIT 编译接口（占位）
 *
 * 提供表达式 JIT 编译的接口定义。
 * 当前为占位实现，无 LLVM 依赖时降级为解释器。
 */

#ifndef DB_SQL_JIT_H
#define DB_SQL_JIT_H

#include "db/sql/nodes/nodetags.h"
#include "db/sql/expr.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * JITExprState - JIT 编译后的表达式状态
 * ======================================================================== */

typedef struct JITExprState {
    ExprState    base;               /* 基类 */
    void        *jit_func;           /* JIT 编译后的函数指针 */
    bool         is_compiled;        /* 是否已编译 */
    bool         jit_available;      /* JIT 是否可用 */
} JITExprState;

/* ========================================================================
 * JIT 配置
 * ======================================================================== */

typedef struct JITConfig {
    bool     enabled;                /* 是否启用 JIT */
    int      optimization_level;    /* 优化级别 (0-3) */
    int      inline_threshold;      /* 内联阈值 */
    bool     dump_ir;               /* 是否导出 IR */
} JITConfig;

/* ========================================================================
 * 公共 API
 * ======================================================================== */

/**
 * @brief 初始化 JIT 子系统
 *
 * @return 0 成功；-1 不可用
 */
int InitJIT(void);

/**
 * @brief 检查 JIT 是否可用
 *
 * @return true 可用
 */
bool IsJITAvailable(void);

/**
 * @brief 获取 JIT 配置
 *
 * @return 当前配置
 */
JITConfig GetJITConfig(void);

/**
 * @brief 设置 JIT 配置
 *
 * @param config 配置
 */
void SetJITConfig(JITConfig config);

/**
 * @brief JIT 编译表达式
 *
 * @param expr 表达式状态
 *
 * @return JITExprState；降级时返回 NULL
 */
JITExprState *JitCompileExpr(ExprState *expr);

/**
 * @brief 执行 JIT 编译的表达式
 *
 * @param state    JIT 表达式状态
 * @param econtext 表达式上下文
 *
 * @return 结果值
 */
Datum ExecJitExpr(JITExprState *state, ExprContext *econtext);

/**
 * @brief 释放 JIT 表达式状态
 *
 * @param state JITExprState
 */
void FreeJITExprState(JITExprState *state);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_JIT_H */
