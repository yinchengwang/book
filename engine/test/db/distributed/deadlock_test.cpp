// deadlock_test.cpp —— 跨分片 Waits-For 死锁检测（Tarjan SCC）
extern "C" {
#include "distributed/deadlock.h"
}
#include <gtest/gtest.h>
#include <stdlib.h>

// 单环：100→200→300→100，检测到且环成员齐备
TEST(Deadlock, DetectsCycle) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    EXPECT_EQ(0, wfg_add_txn(g, 100, 1000));
    EXPECT_EQ(0, wfg_add_txn(g, 200, 1010));
    EXPECT_EQ(0, wfg_add_txn(g, 300, 1020));
    uint8_t k[] = {'K'};
    EXPECT_EQ(0, wfg_add_edge(g, 100, 200, k, 1, 2000));
    EXPECT_EQ(0, wfg_add_edge(g, 200, 300, k, 1, 2000));
    EXPECT_EQ(0, wfg_add_edge(g, 300, 100, k, 1, 2000));
    int n = 0;
    int64_t *cyc = wfg_detect_cycles(g, &n);
    ASSERT_NE(nullptr, cyc);
    EXPECT_GE(n, 3);
    free(cyc);
    wfg_free(g);
}

// 有向无环（1→2→3，无回边）：不应误报环，detect 须返回 NULL/0
TEST(Deadlock, NoFalsePositiveOnAcyclic) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    wfg_add_txn(g, 100, 1000); wfg_add_txn(g, 200, 1010); wfg_add_txn(g, 300, 1020);
    uint8_t k[] = {'K'};
    wfg_add_edge(g, 100, 200, k, 1, 2000);
    wfg_add_edge(g, 200, 300, k, 1, 2000);
    int n = 99;
    int64_t *cyc = wfg_detect_cycles(g, &n);
    EXPECT_EQ(nullptr, cyc);
    EXPECT_EQ(0, n);
    wfg_free(g);
}

// victim：两节点互等成环，选 wait_start 更早（等待更久）者
TEST(Deadlock, VictimIsLongestWaiting) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    wfg_add_txn(g, 100, 1000);   /* 等待开始 1000（更早） */
    wfg_add_txn(g, 200, 2000);
    uint8_t k[] = {'K'};
    wfg_add_edge(g, 100, 200, k, 1, 3000);
    wfg_add_edge(g, 200, 100, k, 1, 3000);
    int n = 0;
    int64_t *cyc = wfg_detect_cycles(g, &n);
    ASSERT_NE(nullptr, cyc);
    ASSERT_GE(n, 2);
    EXPECT_EQ(100LL, wfg_pick_victim_scc(g, cyc, n));
    free(cyc);
    wfg_free(g);
}

// 显式登记的事务 wait_start 不被 add_edge 覆盖：已登记真等待时刻得以保留，
// victim 语义与图构造顺序解耦（见证 Critical-1 修复）
TEST(Deadlock, ExplicitWaitStartNotOverwrittenByAddEdge) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    wfg_add_txn(g, 200, 5000);   /* 显式 5000（晚），先登记使环成员顺序为 200→100 */
    wfg_add_txn(g, 100, 1000);   /* 显式 1000（早，等待更久） */
    uint8_t k[] = {'K'};
    wfg_add_edge(g, 100, 200, k, 1, 9000);   /* now=9000：若被无条件覆盖会抹掉显式值 */
    wfg_add_edge(g, 200, 100, k, 1, 9000);
    int n = 0;
    int64_t *cyc = wfg_detect_cycles(g, &n);
    ASSERT_NE(nullptr, cyc);
    ASSERT_GE(n, 2);
    EXPECT_EQ(100LL, wfg_pick_victim_scc(g, cyc, n));
    free(cyc);
    wfg_free(g);
}

// 超期边剪枝：now 推进后旧边移除，环断开不再误报
TEST(Deadlock, PruneStaleEdgesBreaksCycle) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    wfg_add_txn(g, 100, 1000); wfg_add_txn(g, 200, 1010);
    uint8_t k[] = {'K'};
    wfg_add_edge(g, 100, 200, k, 1, 10000);   /* create_ms=10000 */
    wfg_add_edge(g, 200, 100, k, 1, 10000);
    int n = 0;
    int64_t *cyc = wfg_detect_cycles(g, &n);   /* 有环 */
    EXPECT_NE(nullptr, cyc);
    free(cyc);                                  /* 释放 detect 返回数组，防测试内存泄漏 */
    free((int64_t *)wfg_detect_cycles(g, &n));  /* 再取一次（可多次调用） */
    wfg_prune_edges_before(g, 60000, 10000);    /* now=60000, ttl=10000 → 旧边超期 */
    n = 99;
    int64_t *c2 = wfg_detect_cycles(g, &n);
    EXPECT_EQ(nullptr, c2);                     /* 边已剪，环消失 */
    EXPECT_EQ(0, n);
    wfg_free(g);
}