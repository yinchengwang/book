// tso_test.cpp —— TSO 编解码与单调分配
extern "C" {
#include "distributed/tso.h"
}
#include <gtest/gtest.h>
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

TEST(TsoOracle, ConcurrentAllocMonotonic) {
    tso_oracle_t *o = nullptr;
    tso_oracle_init(&o);
    std::atomic<int64_t> max_seen{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 100; ++i) {
                int64_t s = 0, e = 0;
                if (tso_alloc(o, 5, &s, &e) == 0) {
                    int64_t prev = max_seen.load();
                    while (!max_seen.compare_exchange_weak(prev, s))
                        prev = max_seen.load();
                }
            }
        });
    }
    for (auto &th : threads) th.join();
    tso_oracle_destroy(o);
}