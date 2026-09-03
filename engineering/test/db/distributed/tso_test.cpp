// tso_test.cpp —— TSO 编解码与单调分配
extern "C" {
#include "distributed/tso.h"
}
#include <gtest/gtest.h>
#include <pthread.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
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

// ---------- Task 3：客户端批量缓存 + HLC 单调推进 ----------

TEST(TsoClient, CacheServesUntilExhaustedThenRefills) {
    static int64_t fake_next = 1000;
    auto fake_backend = [](void *, int count, int64_t *s, int64_t *e) -> int {
        *s = fake_next;
        *e = fake_next + count - 1;
        fake_next += count;
        return 0;
    };
    tso_client_t *c = tso_client_new(10);
    tso_client_set_backend(c, fake_backend, nullptr);
    /* 前 10 次走缓存，连续 */
    int64_t first = tso_client_get(c);
    for (int i = 1; i < 10; ++i) EXPECT_EQ(first + i, tso_client_get(c));
    /* 第 11 次触发 refill，从假后端续 */
    EXPECT_EQ(first + 10, tso_client_get(c));
    tso_client_destroy(c);
}

TEST(TsoClient, HlcAdvancesMonotonic) {
    /* 修正：原简报未给 client 设 backend，默认后端无 Oracle ctx 时 get 返 0 使断言失败。
       此处接一个真 Oracle 作后端；核心断言"HLC 单调、物理回退不倒退"保留。 */
    static int64_t fake_clock_ms = 5000;
    tso_oracle_t *o = nullptr;
    tso_oracle_init(&o);
    tso_set_clock_source([](int64_t *ms) -> int { *ms = fake_clock_ms; return 0; });
    tso_client_t *c = tso_client_new(2);
    tso_client_set_backend(c, [](void *ctx, int count, int64_t *s, int64_t *e) {
        return tso_alloc((tso_oracle_t *)ctx, count, s, e);
    }, o);
    /* 物理 5000，首批后端按 5000 供给，戳不低于物理 */
    int64_t t1 = tso_client_get(c);
    EXPECT_GE(t1, (5000LL << TSO_LOGICAL_BITS));
    /* 缓存内严格递增 */
    int64_t t2 = tso_client_get(c);
    EXPECT_GT(t2, t1);
    /* 物理时钟回退到 4999：HLC 不倒退，仍 5000 起，单调推进 */
    fake_clock_ms = 4999;
    int64_t t3 = tso_client_get(c);   /* 缓存耗尽，refill 读取 Oracle -> 仍 5000 */
    EXPECT_GE(t3, (5000LL << TSO_LOGICAL_BITS));
    EXPECT_GE(t3, t2);
    int64_t t4 = tso_client_get(c);
    EXPECT_GE(t4, t3);
    tso_client_destroy(c);
    tso_oracle_destroy(o);
}

TEST(TsoClient, BackendServe) {
    tso_oracle_t *o = nullptr;
    tso_oracle_init(&o);
    tso_client_t *c = tso_client_new(3);
    tso_client_set_backend(c, [](void *ctx, int count, int64_t *s, int64_t *e) {
        return tso_alloc((tso_oracle_t *)ctx, count, s, e);
    }, o);
    int64_t a = tso_client_get(c);
    int64_t b = tso_client_get(c);
    EXPECT_LT(a, b);
    tso_client_destroy(c);
    tso_oracle_destroy(o);
}

// ---------- Task 4：水位线持久化（故障重启时间戳不倒退） ----------
// 注意：tso_set_clock_source(NULL) 不会复位（现有实现仅在 src 非空时替换），
// 故本测试不尝试复位；其末尾 fake_now=1000 不影响后续测试（各自会设置自身时钟源），自洽即可。
TEST(TsoPersist, WatermarkPreventsRegressionAcrossRestart) {
    static int64_t fake_now = 9000;
    tso_set_clock_source([](int64_t *ms) -> int { *ms = fake_now; return 0; });
    const char *path = "test-results/engineering/tso_watermark.bin";
    /* 测试工作目录不固定（ctest 跑在 build/engineering），先确保父目录存在；
       test-results/ 为 .gitignore 忽略的产物目录，不会污染仓库。 */
    std::filesystem::create_directories("test-results/engineering");
    tso_oracle_t *o = nullptr;
    tso_oracle_init(&o);
    int64_t s = 0, e = 0;
    tso_alloc(o, 100, &s, &e);                    /* 已到 ~9000<<18 */
    ASSERT_EQ(0, tso_persist_save(o, path));
    tso_oracle_destroy(o);

    /* 新进程实例，物理时钟回退到 1000 */
    fake_now = 1000;
    tso_oracle_t *o2 = nullptr;
    tso_oracle_init(&o2);
    int64_t wm = tso_persist_load(path);
    ASSERT_GT(wm, 0);
    tso_oracle_insert_watermark(o2, wm);          /* 恢复水位 */
    int64_t s2 = 0, e2 = 0;
    tso_alloc(o2, 1, &s2, &e2);
    EXPECT_GT(s2, e);                             /* 新戳高于旧最大，未倒退 */
    tso_oracle_destroy(o2);
    remove(path);
}

// ---------- C1 回归：Gap#1 物理游标持平逻辑时水位续前 ----------
TEST(TsoPersist, SamePhysicalClockEqualsWatermarkLogicalContinues_RegressionC1) {
    /* C1 回归用例，覆盖 tso_oracle_insert_watermark 的"同物理、逻辑更大"分支。
       旧实现仅当 phys > last_physical 时推进 last_physical/last_logical；
       若重启后物理时钟恰等于水位物理值（phys == last_physical 且 ts&MASK > 0），
       分支不执行，last_logical 保持 init 初值 0，下一次 tso_alloc 在同物理内
       从 logical 0 复发上一轮已发过的旧戳（低于旧最大 e），违反"重启后任何分配
       严格 > 旧最大"的不倒退保证。
       本用例在重启阶段把时钟设成与初次分配相同的物理（9000，而非回退到 1000），
       注入水位后分配 1 条，验证修正后即便物理持平也按"逻辑更大"正确续前。
       注意：tso_set_clock_source 是全局的，此处用本用例独立的可控时钟源并各自
       重设 fake_now，避免与其它 TSO 测试串扰。 */
    static int64_t fake_now = 9000;
    tso_set_clock_source([](int64_t *ms) -> int { *ms = fake_now; return 0; });
    const char *path = "test-results/engineering/tso_watermark_samephys.bin";
    std::filesystem::create_directories("test-results/engineering");

    tso_oracle_t *o = nullptr;
    tso_oracle_init(&o);
    int64_t s = 0, e = 0;
    tso_alloc(o, 100, &s, &e);                    /* 已到 ~9000<<18，旧最大 e */
    ASSERT_EQ(0, tso_persist_save(o, path));
    tso_oracle_destroy(o);

    /* 重启：物理时钟与初次分配一致（9000），使 insert 后水位物理 == last_physical，
       直击"同物理"分支。 */
    fake_now = 9000;                              /* 与初次分配物理相同，非回退 */
    tso_oracle_t *o2 = nullptr;
    tso_oracle_init(&o2);                         /* init 读到 9000 -> last_physical=9000 */
    int64_t wm = tso_persist_load(path);
    ASSERT_GT(wm, 0);
    tso_oracle_insert_watermark(o2, wm);          /* 恢复水位，物理与游标持平 */
    int64_t s2 = 0, e2 = 0;
    tso_alloc(o2, 1, &s2, &e2);
    EXPECT_GT(s2, e);                             /* 新戳严格 > 旧最大 e，未倒退 */
    EXPECT_GT((s2 & TSO_LOGICAL_MASK), (e & TSO_LOGICAL_MASK)); /* 同物理下逻辑序正确续前 */
    tso_oracle_destroy(o2);
    remove(path);
}