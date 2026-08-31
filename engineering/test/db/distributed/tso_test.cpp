// tso_test.cpp —— TSO 编解码与单调分配
extern "C" {
#include "distributed/tso.h"
}
#include <gtest/gtest.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

TEST(TsoCodec, RoundTrip) {
    tso_ts_t in = {1700000000123LL, 42};
    int64_t raw = tso_encode(&in);
    tso_ts_t out;
    tso_decode(raw, &out);
    EXPECT_EQ(in.physical_ms, out.physical_ms);
    EXPECT_EQ(in.logical, out.logical);
}

TEST(TsoCodec, LogicalOverflowCarriesToPhysical_BlockedAtMask) {
    tso_ts_t ts = {1000, 262142};                  /* 18 位上限前 */
    int64_t raw = tso_encode(&ts);                 /* logical=262142 */
    EXPECT_EQ(raw & 0x3FFFFLL, 262142);
    tso_ts_t overflow = {1000, 262143};            /* 恰好上限 */
    EXPECT_EQ(tso_encode(&overflow) & 0x3FFFFLL, 262143);  /* 合法上限值 */
}

/* 用可控时钟源验证单调性：时钟不后退 */
static int64_t fake_now_ms = 1000;
static int fake_clock(int64_t *ms) { *ms = fake_now_ms; return 0; }

TEST(TsoOracle, MonotonicAcrossBatch) {
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));
    tso_set_clock_source(fake_clock);
    int64_t s = 0, e = 0;
    ASSERT_EQ(0, tso_alloc(o, 100, &s, &e));
    EXPECT_EQ(e - s + 1, 100);
    int64_t s2 = 0, e2 = 0;
    fake_now_ms = 999;                     /* 物理时钟轻微回退 */
    ASSERT_EQ(0, tso_alloc(o, 10, &s2, &e2));
    EXPECT_GT(s2, e);                     /* 仍全局单调，未受回退影响 */
    EXPECT_GT(e2, s2);
    tso_oracle_destroy(o);
}

TEST(TsoOracle, CrossLogicalBoundaryReturnLen) {
    /* 针对性坏例：先推进 last_logical 至接近逻辑容量，再分配一批跨进位，验证返回区间长度始终 == count */
    tso_oracle_t *o = nullptr;
    fake_now_ms = 3000;                    /* 恒定物理时钟，屏蔽时钟跳跃 */
    tso_oracle_init(&o);

    /* 第一批占满除 6 槽外的逻辑空间：new_logical = 262138，slots_left = 6 */
    int64_t s1 = 0, e1 = 0;
    ASSERT_EQ(0, tso_alloc(o, TSO_LOGICAL_MASK - 5, &s1, &e1));
    EXPECT_EQ(e1 - s1 + 1, (int64_t)TSO_LOGICAL_MASK - 5);

    /* 第二批 count=10 > 6：必须跨物理进位，返回区间长度仍须为 10 */
    int64_t s2 = 0, e2 = 0;
    ASSERT_EQ(0, tso_alloc(o, 10, &s2, &e2));
    EXPECT_EQ(e2 - s2 + 1, 10);

    /* 进位后游标推进正确：再分配 1 条应续在第二也之后 +1 */
    int64_t s3 = 0, e3 = 0;
    ASSERT_EQ(0, tso_alloc(o, 1, &s3, &e3));
    EXPECT_EQ(s3, e2 + 1);                 /* 旧实现会因游标多推而跳号 */

    tso_oracle_destroy(o);
}

TEST(TsoOracle, ExactBoundaryCountWrapsPhysical) {
    /* 精确占满边界：count 恰好等于当前物理剩余逻辑槽（slots_left）时，
       旧实现走 count<=slots_left 分支取 new_logical = start_logical + count，
       会越界到 262144（== TSO_LOGICAL_MASK+1），且 last_physical 未进位；
       下一次分配以 start_logical=262144 开局，low-18-bit 溢出进位进 physical，
       在奇数（非 0）base 下会返回重复/过小戳，破坏全局单调。
       本用例直击该边界：第一批 count==262144==slots_left（last_logical 初始为 0）。 */
    tso_oracle_t *o = nullptr;
    fake_now_ms = 4000;                    /* 恒定物理时钟，屏蔽时钟跳跃 */
    ASSERT_EQ(0, tso_oracle_init(&o));
    tso_set_clock_source(fake_clock);

    /* 第一批恰好占满整个物理的 262144 个逻辑槽 */
    int64_t s1 = 0, e1 = 0;
    int64_t slots = (int64_t)TSO_LOGICAL_MASK + 1;   /* 262144 */
    ASSERT_EQ(0, tso_alloc(o, (int)slots, &s1, &e1));
    EXPECT_EQ(e1 - s1 + 1, slots);                    /* 区间长度必须 == count */
    /* 终点必须是当前物理的 MASK，物理尚未进位，故仍落在当前物理 */
    EXPECT_EQ((int64_t)(e1 & TSO_LOGICAL_MASK), (int64_t)TSO_LOGICAL_MASK);

    /* 第二批仅取 1 条：游标必须已进位到下一物理且 logical 归 0，严格 > 上一批终点 */
    /* 时钟在边界后前进 1ms：旧实现因未推进 last_physical，raw 低 18 位进位会
       使本批起点跳到 P+2（多跳一拍/留空洞），物理比终点多 2；新实现正确为 P+1。 */
    fake_now_ms += 1;                          /* 4000 -> 4001 */
    int64_t s2 = 0, e2 = 0;
    ASSERT_EQ(0, tso_alloc(o, 1, &s2, &e2));
    EXPECT_GT(s2, e1);                                 /* 严格单调，绝不重复/回退 */
    /* 下一戳 physical 比上一批终点物理恰好 +1，logical==0 */
    EXPECT_EQ((e1 >> TSO_LOGICAL_BITS) + 1, s2 >> TSO_LOGICAL_BITS);
    EXPECT_EQ((int64_t)(s2 & TSO_LOGICAL_MASK), 0);

    tso_oracle_destroy(o);
}

TEST(TsoOracle, ConcurrentAllocMonotonic) {
    tso_oracle_t *o = nullptr;
    fake_now_ms = 2000;                    /* 恒定物理时钟：让分配区间严格 +1 */
    tso_oracle_init(&o);

    const int kThreads = 8, kPerThread = 1000;
    const int64_t kTotal = kThreads * kPerThread;
    const int64_t first = fake_now_ms << TSO_LOGICAL_BITS;  /* 首条戳 */

    pthread_mutex_t mtx;
    pthread_mutex_init(&mtx, nullptr);
    std::vector<int64_t> stamps;           /* 全局聚合所有已分配戳 */

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&] {
            int64_t prev = -1;             /* 本线程上一条戳 */
            for (int i = 0; i < kPerThread; ++i) {
                int64_t s = 0, e = 0;
                if (tso_alloc(o, 1, &s, &e) != 0) continue;   /* 每批取单戳 */
                if (prev >= 0) {
                    EXPECT_GT(s, prev);   /* 线程内严格单调 +1 递增 */
                }
                EXPECT_GE(e, s);                     /* 区间端点合法 */
                prev = s;
                pthread_mutex_lock(&mtx);
                stamps.push_back(s);
                pthread_mutex_unlock(&mtx);
            }
        });
    }
    for (auto &th : threads) th.join();

    /* 全局严格单调：排序后应恰好是 [first, first+kTotal-1] 且无重无漏 */
    std::sort(stamps.begin(), stamps.end());
    ASSERT_EQ((int64_t)stamps.size(), kTotal);
    int64_t expect_next = first;
    for (int64_t got : stamps) {
        EXPECT_EQ(got, expect_next);       /* 期望值逐次 +1，无碰撞/无缺口 */
        expect_next += 1;
    }

    pthread_mutex_destroy(&mtx);
    tso_oracle_destroy(o);
}