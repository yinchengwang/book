/**
 * @file window_test.cpp
 * @brief 窗口函数测试
 *
 * 测试 P1-3 任务的窗口函数和 CTE 功能：
 *   - WindowAgg 节点初始化和生命周期
 *   - 窗口函数框架功能验证
 *   - CTE 上下文管理
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/sql/window.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
#include "db/sql/nodes/nodetags.h"
#include "db/sql/nodes/execnodes.h"
}

/* ========================================================================
 * 全局测试环境（复用 test_executor.cpp 模式）
 * ======================================================================== */

namespace {

/**
 * @brief Task 10: 全局测试 fixture，为所有测试提供 CurrentMemoryContext
 */
class WindowTestEnvironment : public ::testing::Environment {
public:
    MemoryContext old_cxt;
    MemoryContext test_cxt;

    void SetUp() override {
        old_cxt = MemoryContextCurrent();
        test_cxt = AllocSetContextCreate(
            old_cxt, "WindowTest",
            0, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_DEFAULT_BLOCK_SIZE, ALLOCSET_PRESET_DEFAULT);
        MemoryContextSwitchTo(test_cxt);
    }

    void TearDown() override {
        MemoryContextSwitchTo(old_cxt);
        if (test_cxt) {
            MemoryContextDelete(test_cxt);
            test_cxt = nullptr;
        }
    }
};

/* 注册全局环境 */
::testing::Environment* const window_env =
    ::testing::AddGlobalTestEnvironment(new WindowTestEnvironment);

} /* anonymous namespace */

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

/**
 * @brief Window 函数测试夹具
 */
class WindowFunctionTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建 EState */
        estate = CreateEState();
        ASSERT_NE(nullptr, estate);

        /* 创建查询描述符 */
        qdesc = CreateQueryDesc(NULL, NULL);
        ASSERT_NE(nullptr, qdesc);
        qdesc->estate = estate;
    }

    void TearDown() override {
        /* 清理资源 */
        if (qdesc != nullptr) {
            FreeQueryDesc(qdesc);
            qdesc = nullptr;
        }
        if (estate != nullptr) {
            FreeEState(estate);
            estate = nullptr;
        }
    }

    EState *estate = nullptr;
    QueryDesc *qdesc = nullptr;
};

/* ========================================================================
 * 窗口函数类型测试
 * ======================================================================== */

/**
 * @brief 测试窗口函数类型枚举
 */
TEST(WindowFunctionTypeTest, WindowFuncTypes) {
    /* 验证所有窗口函数类型定义正确（按枚举顺序） */
    EXPECT_EQ(WINDOWFUNC_ROW_NUMBER, 0);
    EXPECT_EQ(WINDOWFUNC_RANK, 1);
    EXPECT_EQ(WINDOWFUNC_DENSE_RANK, 2);
    EXPECT_EQ(WINDOWFUNC_NTILE, 3);
    EXPECT_EQ(WINDOWFUNC_PERCENT_RANK, 4);
    EXPECT_EQ(WINDOWFUNC_CUME_DIST, 5);
    EXPECT_EQ(WINDOWFUNC_LAG, 6);
    EXPECT_EQ(WINDOWFUNC_LEAD, 7);
    EXPECT_EQ(WINDOWFUNC_FIRST_VALUE, 8);
    EXPECT_EQ(WINDOWFUNC_LAST_VALUE, 9);
    EXPECT_EQ(WINDOWFUNC_NTH_VALUE, 10);
    EXPECT_EQ(WINDOWFUNC_COUNT, 11);
    EXPECT_EQ(WINDOWFUNC_SUM, 12);
    EXPECT_EQ(WINDOWFUNC_AVG, 13);
    EXPECT_EQ(WINDOWFUNC_MIN, 14);
    EXPECT_EQ(WINDOWFUNC_MAX, 15);
}

/**
 * @brief 测试帧边界类型枚举
 */
TEST(FrameBoundTypeTest, FrameBoundTypes) {
    EXPECT_EQ(FRAME_BOUND_UNBOUNDED_PRECEDING, 0);
    EXPECT_EQ(FRAME_BOUND_PRECEDING, 1);
    EXPECT_EQ(FRAME_BOUND_CURRENT_ROW, 2);
    EXPECT_EQ(FRAME_BOUND_FOLLOWING, 3);
    EXPECT_EQ(FRAME_BOUND_UNBOUNDED_FOLLOWING, 4);
}

/**
 * @brief 测试帧模式枚举
 */
TEST(FrameModeTest, FrameModes) {
    EXPECT_EQ(FRAME_MODE_ROWS, 0);
    EXPECT_EQ(FRAME_MODE_RANGE, 1);
}

/* ========================================================================
 * 窗口函数定义测试
 * ======================================================================== */

/**
 * @brief 测试窗口函数结构体
 */
TEST(WindowFuncTest, WindowFuncStruct) {
    WindowFunc wf;
    memset(&wf, 0, sizeof(wf));

    wf.type = T_WindowFunc;
    wf.wintype = WINDOWFUNC_ROW_NUMBER;
    wf.argIndex = 0;
    wf.winref = 1;

    EXPECT_EQ(wf.type, T_WindowFunc);
    EXPECT_EQ(wf.wintype, WINDOWFUNC_ROW_NUMBER);
    EXPECT_EQ(wf.argIndex, 0);
    EXPECT_EQ(wf.winref, 1);
}

/* ========================================================================
 * WindowAggState 测试
 * ======================================================================== */

/**
 * @brief 测试 WindowAggState 初始化
 */
TEST_F(WindowFunctionTest, WindowAggStateInit) {
    /* 创建 WindowAgg 计划节点 */
    WindowAgg *windowAgg = (WindowAgg *)calloc(1, sizeof(WindowAgg));
    ASSERT_NE(nullptr, windowAgg);

    windowAgg->plan.type = T_WindowAgg;
    windowAgg->plan.lefttree = nullptr;
    windowAgg->plan.righttree = nullptr;
    windowAgg->numCols = 0;
    windowAgg->planColIdx = nullptr;
    windowAgg->windowFuncs = nullptr;
    windowAgg->partClause = nullptr;
    windowAgg->ordClause = nullptr;
    windowAgg->frameOptions = nullptr;
    windowAgg->frame = nullptr;

    /* 初始化 WindowAgg 节点 */
    PlanState *result = ExecInitWindowAgg((Plan *)windowAgg, estate, 0);
    EXPECT_NE(nullptr, result);
    EXPECT_EQ(result->type, T_WindowAggState);

    /* 清理 */
    if (result != nullptr) {
        ExecEndWindowAgg((WindowAggState *)result);
    }
    free(windowAgg);
}

/**
 * @brief 测试 WindowAggState 字段
 */
TEST_F(WindowFunctionTest, WindowAggStateFields) {
    WindowAgg windowAgg;
    memset(&windowAgg, 0, sizeof(windowAgg));

    windowAgg.plan.type = T_WindowAgg;
    windowAgg.numCols = 2;
    windowAgg.planColIdx = (int *)calloc(2, sizeof(int));
    windowAgg.planColIdx[0] = 0;
    windowAgg.planColIdx[1] = 1;

    /* 初始化 */
    PlanState *result = ExecInitWindowAgg((Plan *)&windowAgg, estate, 0);
    ASSERT_NE(nullptr, result);

    WindowAggState *state = (WindowAggState *)result;
    EXPECT_EQ(state->numCols, 2);
    EXPECT_EQ(state->allDone, false);
    EXPECT_NE(nullptr, state->partitionBuffers);
    EXPECT_EQ(state->partitionBufferSize, 1024);

    /* 清理 */
    ExecEndWindowAgg(state);
    free(windowAgg.planColIdx);
}

/* ========================================================================
 * 帧定义测试
 * ======================================================================== */

/**
 * @brief 测试帧边界计算 - 无界前置
 */
TEST_F(WindowFunctionTest, FrameBoundsUnboundedPreceding) {
    WindowAggState state;
    memset(&state, 0, sizeof(state));

    WindowFrame frame;
    frame.frameMode = FRAME_MODE_ROWS;
    frame.startBound = FRAME_BOUND_UNBOUNDED_PRECEDING;
    frame.endBound = FRAME_BOUND_CURRENT_ROW;

    int head, tail;
    window_compute_frame_bounds(&state, 5, 10, &frame, &head, &tail);

    EXPECT_EQ(head, 0);  /* UNBOUNDED PRECEDING -> 0 */
    EXPECT_EQ(tail, 5);  /* CURRENT ROW -> 5 */
}

/**
 * @brief 测试帧边界计算 - 固定偏移
 */
TEST_F(WindowFunctionTest, FrameBoundsFixedOffset) {
    WindowAggState state;
    memset(&state, 0, sizeof(state));

    WindowFrame frame;
    frame.frameMode = FRAME_MODE_ROWS;
    frame.startBound = FRAME_BOUND_PRECEDING;
    frame.startOffset = 2;
    frame.endBound = FRAME_BOUND_FOLLOWING;
    frame.endOffset = 1;

    int head, tail;
    window_compute_frame_bounds(&state, 5, 10, &frame, &head, &tail);

    EXPECT_EQ(head, 3);  /* 5 - 2 = 3 */
    EXPECT_EQ(tail, 6);  /* 5 + 1 = 6 */
}

/**
 * @brief 测试帧边界计算 - 边界保护
 */
TEST_F(WindowFunctionTest, FrameBoundsBoundaryProtection) {
    WindowAggState state;
    memset(&state, 0, sizeof(state));

    WindowFrame frame;
    frame.frameMode = FRAME_MODE_ROWS;
    frame.startBound = FRAME_BOUND_PRECEDING;
    frame.startOffset = 10;  /* 超过范围 */
    frame.endBound = FRAME_BOUND_FOLLOWING;
    frame.endOffset = 10;    /* 超过范围 */

    int head, tail;
    window_compute_frame_bounds(&state, 2, 10, &frame, &head, &tail);

    EXPECT_EQ(head, 0);   /* 不能小于 0 */
    EXPECT_EQ(tail, 9);   /* 不能大于 9 */
}

/* ========================================================================
 * 元组分区匹配测试
 * ======================================================================== */

/**
 * @brief 测试分区匹配 - 相等
 */
TEST_F(WindowFunctionTest, PartitionMatchEqual) {
    TupleTableSlot slot1, slot2;
    memset(&slot1, 0, sizeof(slot1));
    memset(&slot2, 0, sizeof(slot2));

    slot1.type = T_TupleTableSlot;
    slot2.type = T_TupleTableSlot;

    Datum values1[2] = {1, 10};
    Datum values2[2] = {1, 10};
    bool nulls[2] = {false, false};

    slot1.tts_values = values1;
    slot1.tts_isnull = nulls;
    slot1.tts_nvalid = 2;

    slot2.tts_values = values2;
    slot2.tts_isnull = nulls;
    slot2.tts_nvalid = 2;

    int colIdx[1] = {0};

    EXPECT_TRUE(window_tuples_match_partition(&slot1, &slot2, 1, colIdx));
}

/**
 * @brief 测试分区匹配 - 不相等
 */
TEST_F(WindowFunctionTest, PartitionMatchNotEqual) {
    TupleTableSlot slot1, slot2;
    memset(&slot1, 0, sizeof(slot1));
    memset(&slot2, 0, sizeof(slot2));

    slot1.type = T_TupleTableSlot;
    slot2.type = T_TupleTableSlot;

    Datum values1[2] = {1, 10};
    Datum values2[2] = {2, 10};  /* 分区列不同 */
    bool nulls[2] = {false, false};

    slot1.tts_values = values1;
    slot1.tts_isnull = nulls;
    slot1.tts_nvalid = 2;

    slot2.tts_values = values2;
    slot2.tts_isnull = nulls;
    slot2.tts_nvalid = 2;

    int colIdx[1] = {0};

    EXPECT_FALSE(window_tuples_match_partition(&slot1, &slot2, 1, colIdx));
}

/**
 * @brief 测试分区匹配 - NULL 处理
 */
TEST_F(WindowFunctionTest, PartitionMatchNullHandling) {
    TupleTableSlot slot1, slot2;
    memset(&slot1, 0, sizeof(slot1));
    memset(&slot2, 0, sizeof(slot2));

    slot1.type = T_TupleTableSlot;
    slot2.type = T_TupleTableSlot;

    Datum values1[2] = {0, 10};
    Datum values2[2] = {0, 10};
    bool nulls1[2] = {true, false};   /* 分区列为 NULL */
    bool nulls2[2] = {true, false};   /* 分区列为 NULL */

    slot1.tts_values = values1;
    slot1.tts_isnull = nulls1;
    slot1.tts_nvalid = 2;

    slot2.tts_values = values2;
    slot2.tts_isnull = nulls2;
    slot2.tts_nvalid = 2;

    int colIdx[1] = {0};

    /* 两个 NULL 相等 */
    EXPECT_TRUE(window_tuples_match_partition(&slot1, &slot2, 1, colIdx));
}

/**
 * @brief 测试分区匹配 - 无分区列
 */
TEST_F(WindowFunctionTest, PartitionMatchNoColumns) {
    TupleTableSlot slot1, slot2;
    memset(&slot1, 0, sizeof(slot1));
    memset(&slot2, 0, sizeof(slot2));

    /* 无分区列时，所有行都在同一分区 */
    EXPECT_TRUE(window_tuples_match_partition(&slot1, &slot2, 0, nullptr));
}

/* ========================================================================
 * CTE 测试
 * ======================================================================== */

/**
 * @brief 测试 CTE 上下文初始化
 */
TEST_F(WindowFunctionTest, CTEContextInit) {
    CTEContext *cteCtx = ExecInitCTE(estate, nullptr);
    EXPECT_NE(nullptr, cteCtx);
    EXPECT_EQ(cteCtx->numCTEs, 0);
    EXPECT_EQ(cteCtx->ctes, nullptr);

    ExecEndCTE(cteCtx);
}

/**
 * @brief 测试 CTE 查找
 */
TEST_F(WindowFunctionTest, CTEFind) {
    /* 创建简单的 CTE 列表 */
    CommonTableExpr cte;
    memset(&cte, 0, sizeof(cte));
    cte.ctename = (char *)"test_cte";
    cte.recursive = false;

    List *cteList = (List *)calloc(1, sizeof(List));
    cteList->head = (ListCell *)calloc(1, sizeof(ListCell));
    cteList->head->data = &cte;
    cteList->length = 1;

    CTEContext *cteCtx = ExecInitCTE(estate, cteList);
    ASSERT_NE(nullptr, cteCtx);

    /* 查找存在的 CTE */
    CommonTableExpr *found = ExecFindCTE(cteCtx, "test_cte");
    EXPECT_NE(nullptr, found);
    EXPECT_STREQ(found->ctename, "test_cte");

    /* 查找不存在的 CTE */
    found = ExecFindCTE(cteCtx, "nonexistent");
    EXPECT_EQ(nullptr, found);

    /* 清理 */
    ExecEndCTE(cteCtx);
    free(cteList->head);
    free(cteList);
}

/**
 * @brief 测试递归 CTE 标记
 */
TEST_F(WindowFunctionTest, RecursiveCTE) {
    CommonTableExpr cte;
    memset(&cte, 0, sizeof(cte));
    cte.ctename = (char *)"recursive_cte";
    cte.recursive = true;  /* 递归 CTE */

    List *cteList = (List *)calloc(1, sizeof(List));
    cteList->head = (ListCell *)calloc(1, sizeof(ListCell));
    cteList->head->data = &cte;
    cteList->length = 1;

    CTEContext *cteCtx = ExecInitCTE(estate, cteList);
    ASSERT_NE(nullptr, cteCtx);

    CommonTableExpr *found = ExecFindCTE(cteCtx, "recursive_cte");
    EXPECT_NE(nullptr, found);
    EXPECT_TRUE(found->recursive);

    /* 清理 */
    ExecEndCTE(cteCtx);
    free(cteList->head);
    free(cteList);
}

/* ========================================================================
 * 窗口函数状态测试
 * ======================================================================== */

/**
 * @brief 测试窗口函数状态初始化
 */
TEST_F(WindowFunctionTest, WindowFuncStateInit) {
    WindowFuncState state;
    memset(&state, 0, sizeof(state));

    state.wintype = WINDOWFUNC_ROW_NUMBER;
    state.argIndex = -1;
    state.defaultIsNull = true;

    EXPECT_EQ(state.wintype, WINDOWFUNC_ROW_NUMBER);
    EXPECT_EQ(state.argIndex, -1);
    EXPECT_TRUE(state.defaultIsNull);
    EXPECT_EQ(state.rankValue, 0);
    EXPECT_FALSE(state.aggInitialized);
}

/**
 * @brief 测试排名函数状态
 */
TEST_F(WindowFunctionTest, RankFunctionState) {
    WindowFuncState state;
    memset(&state, 0, sizeof(state));

    state.wintype = WINDOWFUNC_RANK;
    state.rankValue = 5;
    state.rankOffset = 3;

    EXPECT_EQ(state.wintype, WINDOWFUNC_RANK);
    EXPECT_EQ(state.rankValue, 5);
    EXPECT_EQ(state.rankOffset, 3);
}

/**
 * @brief 测试聚合窗口函数状态
 */
TEST_F(WindowFunctionTest, AggregateWindowFuncState) {
    WindowFuncState state;
    memset(&state, 0, sizeof(state));

    state.wintype = WINDOWFUNC_SUM;
    state.aggInitialized = true;
    state.aggCount = 10;
    state.aggSum = 55.0;

    EXPECT_EQ(state.wintype, WINDOWFUNC_SUM);
    EXPECT_TRUE(state.aggInitialized);
    EXPECT_EQ(state.aggCount, 10);
    EXPECT_DOUBLE_EQ(state.aggSum, 55.0);
}

/* ========================================================================
 * 窗口函数求值测试
 * ======================================================================== */

/**
 * @brief 测试窗口函数求值接口
 */
TEST_F(WindowFunctionTest, WindowFuncEval) {
    WindowFuncState funcState;
    memset(&funcState, 0, sizeof(funcState));

    funcState.wintype = WINDOWFUNC_COUNT;
    funcState.argIndex = 0;

    Datum args[1] = {0};
    bool argsNull[1] = {false};

    Datum result = window_func_eval(&funcState, args, argsNull, 1);
    /* 框架版本返回 0，实际结果在 exec_window_impl 中计算 */
    (void)result;
}

/* ========================================================================
 * WindowAgg 重置测试
 * ======================================================================== */

/**
 * @brief 测试 WindowAgg 重置
 */
TEST_F(WindowFunctionTest, WindowAggReScan) {
    WindowAgg windowAgg;
    memset(&windowAgg, 0, sizeof(windowAgg));

    windowAgg.plan.type = T_WindowAgg;
    windowAgg.numCols = 0;

    PlanState *result = ExecInitWindowAgg((Plan *)&windowAgg, estate, 0);
    ASSERT_NE(nullptr, result);

    WindowAggState *state = (WindowAggState *)result;

    /* 模拟添加数据后的状态 */
    state->allDone = true;
    state->partitionBufferUsed = 5;

    /* 重置 */
    ExecReScanWindowAgg(state);

    EXPECT_FALSE(state->allDone);
    EXPECT_EQ(state->partitionBufferUsed, 0);
    EXPECT_EQ(state->currentPos, 0);

    /* 清理 */
    ExecEndWindowAgg(state);
}

/* ========================================================================
 * 集成测试
 * ======================================================================== */

/**
 * @brief 测试窗口函数和 CTE 集成
 */
TEST_F(WindowFunctionTest, WindowAndCTEIntegration) {
    /* 创建包含窗口函数的 CTE */
    CommonTableExpr cte;
    memset(&cte, 0, sizeof(cte));
    cte.ctename = (char *)"window_cte";
    cte.recursive = false;

    List *cteList = (List *)calloc(1, sizeof(List));
    cteList->head = (ListCell *)calloc(1, sizeof(ListCell));
    cteList->head->data = &cte;
    cteList->length = 1;

    /* 初始化 CTE 上下文 */
    CTEContext *cteCtx = ExecInitCTE(estate, cteList);
    ASSERT_NE(nullptr, cteCtx);

    /* 初始化 WindowAgg */
    WindowAgg windowAgg;
    memset(&windowAgg, 0, sizeof(windowAgg));
    windowAgg.plan.type = T_WindowAgg;

    PlanState *windowResult = ExecInitWindowAgg((Plan *)&windowAgg, estate, 0);
    EXPECT_NE(nullptr, windowResult);

    /* 清理 */
    ExecEndWindowAgg((WindowAggState *)windowResult);
    ExecEndCTE(cteCtx);
    free(cteList->head);
    free(cteList);
}
