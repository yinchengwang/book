/**
 * @file jit.c
 * @brief JIT 编译实现（占位）
 *
 * 当前为占位实现，无 LLVM 依赖。
 * 所有函数降级为解释器执行。
 */

#include "db/sql/jit.h"
#include "db/sql/memctx.h"
#include <stdlib.h>

/* ========================================================================
 * 全局状态
 * ======================================================================== */

static bool g_jit_initialized = false;
static bool g_jit_available = false;
static JITConfig g_jit_config = {
    .enabled = false,
    .optimization_level = 2,
    .inline_threshold = 100,
    .dump_ir = false
};

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

int InitJIT(void)
{
    if (g_jit_initialized) {
        return 0;
    }

    /* 占位实现：不初始化 LLVM */
    g_jit_available = false;
    g_jit_initialized = true;

    return -1;  /* JIT 不可用 */
}

bool IsJITAvailable(void)
{
    return g_jit_available;
}

JITConfig GetJITConfig(void)
{
    return g_jit_config;
}

void SetJITConfig(JITConfig config)
{
    g_jit_config = config;
}

JITExprState *JitCompileExpr(ExprState *expr)
{
    /* 占位实现：不编译，返回 NULL */
    (void)expr;
    return NULL;
}

Datum ExecJitExpr(JITExprState *state, ExprContext *econtext)
{
    /* 占位实现：降级为解释器 */
    if (!state) {
        return 0;
    }

    /* 调用解释器执行表达式 */
    bool isnull = false;
    return ExecEvalExpr(&state->base, econtext, &isnull);
}

void FreeJITExprState(JITExprState *state)
{
    if (!state) {
        return;
    }

    /* 占位实现：无 JIT 代码需要释放 */
    free(state);
}
