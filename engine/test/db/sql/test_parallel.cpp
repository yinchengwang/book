/**
 * @file test_parallel.cpp
 * @brief 并行执行框架单元测试
 */

#include <gtest/gtest.h>

extern "C" {
#include "db/sql/parallel.h"
#include "db/sql/nodeGather.h"
#include "db/sql/executor.h"
#include "db/sql/memctx.h"
}

namespace {

/* ========================================================================
 * ParallelContext 测试
 * ======================================================================== */

TEST(ParallelTest, ContextCreate) {
    ParallelContext *pcxt = CreateParallelContext(4);
    
    ASSERT_NE(pcxt, nullptr);
    EXPECT_EQ(pcxt->nworkers, 4);
    EXPECT_EQ(pcxt->nworkers_to_launch, 4);
    
    DestroyParallelContext(pcxt);
}

TEST(ParallelTest, ContextNullDestroy) {
    /* 空指针销毁不应崩溃 */
    DestroyParallelContext(nullptr);
}

TEST(ParallelTest, ParallelAlloc) {
    ParallelContext *pcxt = CreateParallelContext(2);
    
    void *mem = ParallelAlloc(pcxt, 1024);
    ASSERT_NE(mem, nullptr);
    
    /* 所有 worker 应能访问相同地址 */
    EXPECT_EQ(ParallelGetWorkerAddr(pcxt, mem, 0), mem);
    EXPECT_EQ(ParallelGetWorkerAddr(pcxt, mem, 1), mem);
    
    free(mem);
    DestroyParallelContext(pcxt);
}

TEST(ParallelTest, LaunchAndWait) {
    ParallelContext *pcxt = CreateParallelContext(2);
    
    int launched = LaunchParallelWorkers(pcxt);
    EXPECT_EQ(launched, 2);
    
    WaitForParallelWorkers(pcxt);
    EXPECT_TRUE(ParallelCoordinatorIsFinished(pcxt->coordinator));
    
    DestroyParallelContext(pcxt);
}

TEST(ParallelTest, CoordinatorFinished) {
    ParallelCoordinator coord;
    coord.type = T_ParallelCoordinator;
    coord.finished = false;
    
    EXPECT_FALSE(ParallelCoordinatorIsFinished(&coord));
    
    ParallelCoordinatorSetFinished(&coord);
    EXPECT_TRUE(ParallelCoordinatorIsFinished(&coord));
}

TEST(ParallelTest, CoordinatorAttach) {
    ParallelCoordinator coord;
    coord.type = T_ParallelCoordinator;
    coord.nworkers_attached = 0;
    
    ParallelCoordinatorAttach(&coord);
    EXPECT_EQ(coord.nworkers_attached, 1);
    
    ParallelCoordinatorAttach(&coord);
    EXPECT_EQ(coord.nworkers_attached, 2);
}

/* ========================================================================
 * ParallelHash 测试
 * ======================================================================== */

TEST(ParallelTest, HashCreate) {
    ParallelHashJoinState *state = CreateParallelHash(2, 1024);
    
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->nparticipants, 2);
    EXPECT_EQ(state->nbuckets, 1024);
    
    DestroyParallelHash(state);
}

TEST(ParallelTest, HashInsertLookup) {
    ParallelHashJoinState *state = CreateParallelHash(2, 16);
    
    int key = 42;
    int value = 100;
    
    ParallelHashInsert(state, &key, &value);
    EXPECT_EQ(state->ntuples, 1);
    
    void *result = ParallelHashLookup(state, &key);
    EXPECT_EQ(result, &value);
    
    DestroyParallelHash(state);
}

TEST(ParallelTest, HashNullDestroy) {
    DestroyParallelHash(nullptr);
}

/* ========================================================================
 * Gather 节点测试
 * ======================================================================== */

TEST(ParallelTest, GatherInit) {
    Gather gather = {};
    gather.plan.type = T_Gather;
    gather.num_workers = 2;
    
    EState *estate = CreateEState();
    GatherState *state = ExecInitGather(&gather, estate, 0);
    
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ps.type, T_GatherState);
    EXPECT_EQ(state->num_workers, 2);
    EXPECT_FALSE(state->initialized);
    
    ExecEndGather(state);
    FreeEState(estate);
}

TEST(ParallelTest, GatherNullEnd) {
    /* 空指针结束不应崩溃 */
    ExecEndGather(nullptr);
}

TEST(ParallelTest, GatherReScan) {
    Gather gather = {};
    gather.plan.type = T_Gather;
    gather.num_workers = 2;
    
    EState *estate = CreateEState();
    GatherState *state = ExecInitGather(&gather, estate, 0);
    
    /* 标记为已初始化 */
    state->initialized = true;
    
    /* ReScan 应重置 */
    ExecReScanGather(state);
    EXPECT_FALSE(state->initialized);
    
    ExecEndGather(state);
    FreeEState(estate);
}

}  // namespace
