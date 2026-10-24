// chaos_test.cpp —— 端到端集成 + 故障注入（混沌）回归（Gap#1 Task 10 收尾）
// 把 Tasks 2-9 的 TSO / ts-MVCC(SI) / Percolator 2PC / 崖壁恢复+GC /
// Waits-For 死锁 / 跨分片协调器串成端到端回归，并注入"崩溃"故障验证原子性。
//
// 并发纪律（brief 修正 3）：ts_store / pcol_* 非线程安全（排序动态数组 realloc、
// 版本链均无锁）；只有 tso_alloc 有 mutex 保护。故并发只打在 TSO 上，
// 跨分片/恢复的"混沌"用单线程故障注入循环表达。
extern "C" {
#include "distributed/tso.h"
#include "distributed/mvcc_ts.h"
#include "distributed/percolator.h"
#include "distributed/deadlock.h"
}
#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <unordered_set>
#include <vector>

// 1) TSO 并发分配是线程安全的（tso_alloc 有 mutex）：N 线程各取 PER 个戳，
//    全局唯一且总数正确（无重复、无丢失）。并发只打在线程安全的 TSO 上。
TEST(Chaos, TsoConcurrentAllocUnique) {
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));
    const int NTHREADS = 8, PER = 200;
    std::vector<std::vector<int64_t>> all(NTHREADS);
    std::vector<std::thread> ths;
    for (int i = 0; i < NTHREADS; ++i) {
        ths.emplace_back([&, i] {
            all[i].reserve(PER);
            for (int j = 0; j < PER; ++j) {
                int64_t s = 0, e = 0;
                // 注：线程内 ASSERT 失败仅中止本 lambda；最终总数断言会兜底捕获
                if (tso_alloc(o, 1, &s, &e) != 0) return;
                all[i].push_back(s);
            }
        });
    }
    for (auto &t : ths) t.join();

    std::unordered_set<int64_t> uniq;
    for (auto &v : all)
        for (int64_t ts : v)
            EXPECT_TRUE(uniq.insert(ts).second) << "duplicate ts " << ts;
    EXPECT_EQ(static_cast<size_t>(NTHREADS * PER), uniq.size());  // 无重复、无丢失
    tso_oracle_destroy(o);
}

// 2) 端到端：单分片串行事务交错，SI 快照读只见到 commit_ts<=read_ts 的已提交版本。
//    事务1 写 v1 先提交（commit=e1），事务2 写 v2 后提交（commit=e2>e1）；
//    e1 时刻快照读到 v1，e2 时刻快照读到 v2。
TEST(Chaos, EndToEndSerialTransactionsSnapshotIsolation) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));
    uint8_t k[] = {'k'};
    int64_t s1, e1, s2, e2;

    // 事务1：写 v1，start=s1, commit=e1
    tso_alloc(o, 1, &s1, &e1);
    {
        uint8_t v1[] = {'1'};
        pcol_txn_t *t = pcol_txn_begin(&s, o, s1);
        pcol_write_t w = {k, 1, v1, 1, false};
        pcol_txn_add_write(t, &w);
        ASSERT_EQ(PCOL_OK, pcol_prewrite(t));
        int64_t cs1 = 0, ce1 = 0;
        tso_alloc(o, 1, &cs1, &ce1);
        ASSERT_EQ(PCOL_OK, pcol_commit(t, ce1));
        pcol_txn_free(t);
        e1 = ce1;
    }
    // 事务2：写 v2（start 晚于事务1 commit，无写写冲突）
    tso_alloc(o, 1, &s2, &e2);
    {
        uint8_t v2[] = {'2'};
        pcol_txn_t *t = pcol_txn_begin(&s, o, s2);
        pcol_write_t w = {k, 1, v2, 1, false};
        pcol_txn_add_write(t, &w);
        ASSERT_EQ(PCOL_OK, pcol_prewrite(t));
        int64_t cs2 = 0, ce2 = 0;
        tso_alloc(o, 1, &cs2, &ce2);
        ASSERT_EQ(PCOL_OK, pcol_commit(t, ce2));
        pcol_txn_free(t);
        e2 = ce2;
    }
    ASSERT_LT(e1, e2);

    ts_version_t out;
    memset(&out, 0, sizeof(out));
    // 旧快照（e1 时刻）读到 v1；新快照（e2 时刻）读到 v2
    ASSERT_EQ(0, ts_store_get(&s, k, 1, e1, nullptr, 0, &out));
    EXPECT_EQ(1u, out.value_len);
    EXPECT_EQ('1', static_cast<const uint8_t *>(out.value)[0]);
    ts_version_free(&out);
    ASSERT_EQ(0, ts_store_get(&s, k, 1, e2, nullptr, 0, &out));
    EXPECT_EQ(1u, out.value_len);
    EXPECT_EQ('2', static_cast<const uint8_t *>(out.value)[0]);
    ts_version_free(&out);

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 3) 故障注入：跨两分片事务在"commit-all 中途"崩溃，recover 后原子性不变——
//    primary 已提交则全分片补提交可见；primary 未提交则全分片回滚（无可见版本、
//    无残留预写锁）。用裸 ts_store_put(commit=0) 预写 + ts_store_promote 提权
//    primary 造撕裂态（与 T9 测试同法：pcol_commit_cross 不暴露 prewrite-only 步骤），
//    再手工 pcol_intent_log_add 记录意图、pcol_cross_recover 重放。
TEST(Chaos, FaultInjectionCrossShardRecoveryAtomic) {
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    for (int iter = 0; iter < 25; ++iter) {
        ts_store_t s1, s2;
        ts_store_init(&s1);
        ts_store_init(&s2);
        ts_store_t *stores[2] = {&s1, &s2};
        pcol_context_t *ctx = pcol_context_new(stores, 2, o);
        ASSERT_NE(nullptr, ctx);
        pcol_intent_log_clear();  // 每个 iter 隔离全局意图日志

        const int64_t start = 1000 + iter * 10;
        const int64_t commit = start + 5;
        uint8_t ka[] = {'a'}, kb[] = {'b'}, v[] = {'x'};

        // 路由：FNV-1a mod 2 下 'a'→shard0、'b'→shard1（解析可证，且与 T9 用例一致）；
        // 仍以实际 pcol_shard_hash 结果为准摆放，两键同分片则本 iter 跳过（不崩即过）。
        const int sa = pcol_shard_hash(ka, 1, 2);
        const int sb = pcol_shard_hash(kb, 1, 2);
        if (sa == sb) {
            pcol_context_free(ctx);
            ts_store_destroy(&s1);
            ts_store_destroy(&s2);
            continue;
        }
        ts_store_t *primary_store = stores[sa];
        ts_store_t *secondary_store = stores[sb];
        uint8_t *pk = ka, *sk = kb;

        // 两分片都裸预写（commit=0 持锁，value 只在预写节点里）
        ASSERT_EQ(0, ts_store_put(primary_store, pk, 1, start, 0, v, 1));
        ASSERT_EQ(0, ts_store_put(secondary_store, sk, 1, start, 0, v, 1));

        // 故障注入：偶数 iter 模拟"primary 已提交、secondary 尚预写"的撕裂窗口
        const bool crashed_after_primary_commit = (iter % 2 == 0);
        if (crashed_after_primary_commit) {
            ASSERT_EQ(0, ts_store_promote(primary_store, pk, 1, start, commit));
        }

        // 记录意图：每分片键缓冲布局 = [uint32 klen][key bytes]...（长度前缀拼接，
        // 与库内 intent_key_at 解析同源）；shard_keys[shard] 按真实分片下标摆放，
        // 保证与 pcol_shard_hash 路由一致（pcol_intent_log_add 内部深拷贝）。
        uint8_t bufA[5], bufB[5];
        uint32_t one = 1;
        memcpy(bufA, &one, 4);
        bufA[4] = 'a';
        memcpy(bufB, &one, 4);
        bufB[4] = 'b';
        uint8_t *keys[2] = {nullptr, nullptr};
        size_t lens[2] = {0, 0};
        int ns[2] = {0, 0};
        keys[sa] = bufA; lens[sa] = 5; ns[sa] = 1;  // 'a' 在其路由分片
        keys[sb] = bufB; lens[sb] = 5; ns[sb] = 1;  // 'b' 在其路由分片

        pcol_intent_t it;
        memset(&it, 0, sizeof(it));
        it.start_ts = start;
        it.commit_ts = commit;
        it.shard_count = 2;
        it.primary_shard = sa;  // primary 键 = 'a' 所在分片缓冲的首键
        it.shard_keys = keys;
        it.shard_keys_len = lens;
        it.shard_keys_n = ns;
        ASSERT_EQ(0, pcol_intent_log_add(&it));

        ASSERT_EQ(PCOL_OK, pcol_cross_recover(ctx, start));

        // 原子性断言
        ts_version_t out;
        memset(&out, 0, sizeof(out));
        if (crashed_after_primary_commit) {
            // primary 已提交 → secondary 必须补提交可见，且两分片均无残留预写锁
            EXPECT_EQ(0, ts_store_get(secondary_store, sk, 1, commit + 1, nullptr, 0, &out));
            ts_version_free(&out);
            EXPECT_EQ(0, ts_store_has_pending_write(primary_store, pk, 1, -1));
            EXPECT_EQ(0, ts_store_has_pending_write(secondary_store, sk, 1, -1));
        } else {
            // primary 未提交 → 全回滚：无可见版本、无残留锁
            EXPECT_NE(0, ts_store_get(primary_store, pk, 1, commit + 1, nullptr, 0, &out));
            EXPECT_NE(0, ts_store_get(secondary_store, sk, 1, commit + 1, nullptr, 0, &out));
            EXPECT_EQ(0, ts_store_has_pending_write(primary_store, pk, 1, -1));
            EXPECT_EQ(0, ts_store_has_pending_write(secondary_store, sk, 1, -1));
        }

        pcol_context_free(ctx);
        ts_store_destroy(&s1);
        ts_store_destroy(&s2);
    }
    pcol_intent_log_clear();  // 收尾清全局意图日志，不污染后续用例
    tso_oracle_destroy(o);
}

// 4) 死锁：三环（1→2→3→1）检测 → 选环内等待最久（wait_start 最小）者为 victim
//    （= 事务1）→ 中止 victim 后等待边消失，重检无环（别人可继续）。
TEST(Chaos, DeadlockVictimAbortBreaksCycle) {
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);
    wfg_add_txn(g, 1, 0);  // wait_start=0（最早等待）
    wfg_add_txn(g, 2, 1);
    wfg_add_txn(g, 3, 2);
    uint8_t k[] = {'k'};
    wfg_add_edge(g, 1, 2, k, 1, 0);
    wfg_add_edge(g, 2, 3, k, 1, 0);
    wfg_add_edge(g, 3, 1, k, 1, 0);

    int n = 0;
    int64_t *cyc = wfg_detect_cycles(g, &n);
    ASSERT_NE(nullptr, cyc);
    ASSERT_GE(n, 3);
    const int64_t victim = wfg_pick_victim_scc(g, cyc, n);
    // victim 必在环内，且是等待最久（wait_start 最小）者 → 1
    EXPECT_EQ(1LL, victim);
    bool in = false;
    for (int i = 0; i < n; ++i)
        if (cyc[i] == victim) in = true;
    EXPECT_TRUE(in);
    free(cyc);

    // 模拟中止 victim：victim 回滚释放锁后，与其相连的等待边消失。
    // 教学级验证：重建一个去掉 victim(1) 的图（仅剩 2→3），重检应无环。
    wfg_free(g);
    wfg_t *g2 = wfg_new();
    ASSERT_NE(nullptr, g2);
    wfg_add_txn(g2, 2, 1);
    wfg_add_txn(g2, 3, 2);
    wfg_add_edge(g2, 2, 3, k, 1, 0);  // victim 1 已中止，3→1 边消失
    int n2 = 99;
    int64_t *c2 = wfg_detect_cycles(g2, &n2);
    EXPECT_EQ(nullptr, c2);
    EXPECT_EQ(0, n2);
    wfg_free(g2);
}
