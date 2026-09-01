// percolator_test.cpp —— Percolator 2PC（Prewrite/Commit/回滚，集成 MVCC 行 + 锁）
// 核心断言：prewrite 后未提交不可见；commit 后对足够新 read_ts 可见、对过旧
// read_ts 不可见；提交幂等（二次提交返回 ALREADY_COMMITTED）；冲突检测正确。
#include <gtest/gtest.h>

#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "distributed/tso.h"
#include "distributed/mvcc_ts.h"
#include "distributed/deadlock.h"
#include "distributed/percolator.h"
}

namespace {

// 便捷：构建单键写事务
pcol_txn_t *mkTxn(ts_store_t *s, tso_oracle_t *o, int64_t start,
                  const uint8_t *k, size_t klen,
                  const uint8_t *v, size_t vlen) {
    pcol_txn_t *t = pcol_txn_begin(s, o, start);
    pcol_write_t w = {k, klen, v, vlen, false};
    pcol_txn_add_write(t, &w);
    return t;
}

// 便捷：从版本副本取 value 字符串
std::string val_of(const ts_version_t &v) {
    return v.value_len ? std::string(static_cast<char *>(v.value), v.value_len)
                       : std::string();
}

}  // namespace

// 提交后：新 read_ts 可见、旧 read_ts(<=start_ts) 不可见；prewrite 期间不可见
TEST(Percolator, CommitPersistsVisibleVersionAfterCommitTs) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "pk";
    const uint8_t v[] = "va";
    pcol_txn_t *t1 = mkTxn(&s, o, 100, k, 2, v, 2);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(t1));

    // prewrite 后未提交：commit_ts==0 恒不可见，即便读极新 read_ts 也命中 -1
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(-1, ts_store_get(&s, k, 2, 1000, nullptr, 0, &out));

    ASSERT_EQ(PCOL_OK, pcol_commit(t1, 200));
    // 提交后：read_ts>=commit_ts 可见
    ASSERT_EQ(0, ts_store_get(&s, k, 2, 300, nullptr, 0, &out));
    EXPECT_EQ(val_of(out), "va");
    EXPECT_EQ(out.commit_ts, 200);
    EXPECT_EQ(out.start_ts, 100);
    ts_version_free(&out);

    // 旧 read_ts<=start_ts 读不到（MVCC 天然成立）
    EXPECT_EQ(-1, ts_store_get(&s, k, 2, 100, nullptr, 0, &out));
    EXPECT_EQ(-1, ts_store_get(&s, k, 2, 0, nullptr, 0, &out));

    pcol_txn_free(t1);
    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 提交幂等：二次提交返回 PCOL_ERR_ALREADY_COMMITTED 而非重复写
TEST(Percolator, CommitIsIdempotent) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "k", v[] = "vv";
    pcol_txn_t *t = mkTxn(&s, o, 100, k, 1, v, 2);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(t));
    EXPECT_EQ(PCOL_OK, pcol_commit(t, 200));
    EXPECT_EQ(PCOL_ERR_ALREADY_COMMITTED, pcol_commit(t, 300));  // 不可重复提交
    pcol_txn_free(t);

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 回滚后不可见：未提交版本本就不可见，rollback 弃写
TEST(Percolator, RollbackLeavesNothingVisible) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "k", v[] = "v";
    pcol_txn_t *t = mkTxn(&s, o, 100, k, 1, v, 1);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(t));
    EXPECT_EQ(PCOL_OK, pcol_rollback(t));
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(-1, ts_store_get(&s, k, 1, 1000, nullptr, 0, &out));
    pcol_txn_free(t);

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 写写冲突：键有晚于本事务 start_ts 的已提交版本 → WRITE_CONFLICT
TEST(Percolator, WriteWriteConflict) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "pk", v1[] = "a", v2[] = "b";
    pcol_txn_t *tA = mkTxn(&s, o, 100, k, 2, v1, 1);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(tA));
    ASSERT_EQ(PCOL_OK, pcol_commit(tA, 200));   // A(st=100) 提交于 200
    pcol_txn_free(tA);

    pcol_txn_t *tB = mkTxn(&s, o, 150, k, 2, v2, 1);  // st=150 < 200 → 冲突
    EXPECT_EQ(PCOL_ERR_WRITE_CONFLICT, pcol_prewrite(tB));
    pcol_txn_free(tB);

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 锁冲突：他人未提交的预写(commit_ts==0)存在 → LOCK_CONFLICT
TEST(Percolator, LockConflict) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "pk", v1[] = "x", v2[] = "y";
    pcol_txn_t *tA = mkTxn(&s, o, 100, k, 2, v1, 1);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(tA));   // A 预写但未提交，持锁
    // A 自身重复 prewrite 同键不冲突（except 排除自身 start_ts）
    // B 预写同键：他人 commit_ts==0 预写存在 → 锁冲突
    pcol_txn_t *tB = mkTxn(&s, o, 150, k, 2, v2, 1);
    EXPECT_EQ(PCOL_ERR_LOCK_CONFLICT, pcol_prewrite(tB));
    pcol_txn_free(tB);
    pcol_txn_free(tA);

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 多键原子：第二键冲突则整体回滚（首键不遗留可见写）
TEST(Percolator, MultiKeyConflictRollsBackAll) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t ka[] = "a", kb[] = "b";
    const uint8_t vx[] = "x";
    // txnA 预提交键 b（st=100, commit=300）
    pcol_txn_t *tA = mkTxn(&s, o, 100, kb, 1, vx, 1);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(tA));
    ASSERT_EQ(PCOL_OK, pcol_commit(tA, 300));
    pcol_txn_free(tA);

    // txnB(st=200) 同时写 a 与 b：b 有晚于 start 的已提交版本 → WRITE_CONFLICT
    pcol_txn_t *tB = pcol_txn_begin(&s, o, 200);
    pcol_write_t w1 = {ka, 1, vx, 1, false};
    pcol_write_t w2 = {kb, 1, vx, 1, false};
    pcol_txn_add_write(tB, &w1);
    pcol_txn_add_write(tB, &w2);
    EXPECT_EQ(PCOL_ERR_WRITE_CONFLICT, pcol_prewrite(tB));
    pcol_txn_free(tB);

    // 首键 a 的预写被整体回滚：无可见版本
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(-1, ts_store_get(&s, ka, 1, 1000, nullptr, 0, &out));

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 删除写（tombstone）：提交后对足够新 read_ts 命中 -2
TEST(Percolator, DeleteWriteAfterCommit) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "k";
    const uint8_t v[] = "v";
    // 先写一条已提交值
    pcol_txn_t *t1 = mkTxn(&s, o, 100, k, 1, v, 1);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(t1));
    ASSERT_EQ(PCOL_OK, pcol_commit(t1, 200));
    pcol_txn_free(t1);

    // 删除写 st=250
    pcol_txn_t *tDel = pcol_txn_begin(&s, o, 250);
    pcol_write_t w = {k, 1, nullptr, 0, true};
    pcol_txn_add_write(tDel, &w);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(tDel));
    ASSERT_EQ(PCOL_OK, pcol_commit(tDel, 300));
    pcol_txn_free(tDel);

    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(-2, ts_store_get(&s, k, 1, 400, nullptr, 0, &out));

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// I-2 回归：tombstone 也是写——事务 A(start=100) 对 K 写删除并提交(commit=200) 后，
// 并发事务 B(start=150 < 200) 对同一 K 的普通写必须在 prewrite 阶段报 WRITE_CONFLICT。
// 修复前 ww_conflict 对可见 tombstone(rc=-2) 一律放行，删除被静默复活（SI 丢失更新）。
TEST(Percolator, TombstoneWriteWriteConflict) {
    ts_store_t s;
    ts_store_init(&s);
    tso_oracle_t *o = nullptr;
    ASSERT_EQ(0, tso_oracle_init(&o));

    const uint8_t k[] = "pk", v[] = "v";
    // 事务 A：对 K 写 tombstone，prewrite + commit(200)
    pcol_txn_t *tA = pcol_txn_begin(&s, o, 100);
    pcol_write_t wdel = {k, 2, nullptr, 0, true};
    pcol_txn_add_write(tA, &wdel);
    ASSERT_EQ(PCOL_OK, pcol_prewrite(tA));
    ASSERT_EQ(PCOL_OK, pcol_commit(tA, 200));
    pcol_txn_free(tA);

    // 事务 B(start=150 < 200)：对同一 K 写普通值 → 必须写写冲突（删除提交晚于 B 的开始）
    pcol_txn_t *tB = mkTxn(&s, o, 150, k, 2, v, 1);
    EXPECT_EQ(PCOL_ERR_WRITE_CONFLICT, pcol_prewrite(tB));
    pcol_txn_free(tB);

    // 删除语义未被破坏：K 在新快照下仍为 tombstone（未被 B 复活/覆盖）
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(-2, ts_store_get(&s, k, 2, 300, nullptr, 0, &out));
    // B 冲突回滚后无残留预写锁
    EXPECT_EQ(0, ts_store_has_pending_write(&s, k, 2, -1));

    ts_store_destroy(&s);
    tso_oracle_destroy(o);
}

// 错误串非空
TEST(Percolator, ErrorStringNonNull) {
    EXPECT_NE(pcol_error_string(PCOL_OK), nullptr);
    EXPECT_NE(pcol_error_string(PCOL_ERR_ALREADY_COMMITTED), nullptr);
}

// ---------- InDoubt 恢复（Task 7） ----------

// 关键：prewrite 后 primary 只被 ts_store_promote 提权（模拟"commit 写了 primary 才崩溃"）
TEST(PercolatorRecover, PrimaryCommittedThenSecondariesCommitted) {
    ts_store_t s; ts_store_init(&s);
    tso_oracle_t *o = nullptr; EXPECT_EQ(0, tso_oracle_init(&o));
    pcol_lock_table_t lt; EXPECT_EQ(0, pcol_lock_table_init(&lt));
    uint8_t pk[] = {'p'}, sk[] = {'s'}, v[] = {'v'};
    pcol_txn_t *t = pcol_txn_begin(&s, o, 100);
    pcol_txn_set_lock_table(t, &lt);
    pcol_write_t w1 = {pk, 1, v, 1, false}, w2 = {sk, 1, v, 1, false};
    pcol_txn_add_write(t, &w1); pcol_txn_add_write(t, &w2);
    EXPECT_EQ(PCOL_OK, pcol_prewrite(t));   /* 两键挂预写锁 + 登记 two locks */
    /* 模拟宕机撕裂提交：只提权 primary，secondary 保持预写 */
    EXPECT_EQ(0, ts_store_promote(&s, pk, 1, 100, 500));
    pcol_txn_free(t);
    ASSERT_EQ(2u, lt.n);
    int c = 0, r = 0;
    EXPECT_EQ(0, pcol_recover_txn(&s, &lt, pk, 1, 100, &c, &r));
    EXPECT_EQ(1, c); EXPECT_EQ(0, r);       /* 裁定提交并补提交 secondary */
    EXPECT_EQ(0u, lt.n);                    /* 锁被移除 */
    ts_version_t out;
    EXPECT_EQ(0, ts_store_get(&s, sk, 1, 600, nullptr, 0, &out)); /* secondary 现可见 */
    ts_version_free(&out);
    pcol_lock_table_destroy(&lt); tso_oracle_destroy(o); ts_store_destroy(&s);
}

// primary 未提交 → 整体回滚，锁与预写均被清
TEST(PercolatorRecover, PrimaryNotCommittedRollsBack) {
    ts_store_t s; ts_store_init(&s);
    tso_oracle_t *o = nullptr; EXPECT_EQ(0, tso_oracle_init(&o));
    pcol_lock_table_t lt; EXPECT_EQ(0, pcol_lock_table_init(&lt));
    uint8_t pk[] = {'P'}, sk[] = {'S'}, v[] = {'v'};
    pcol_txn_t *t = pcol_txn_begin(&s, o, 200);
    pcol_txn_set_lock_table(t, &lt);
    pcol_write_t w1 = {pk, 1, v, 1, false}, w2 = {sk, 1, v, 1, false};
    pcol_txn_add_write(t, &w1); pcol_txn_add_write(t, &w2);
    EXPECT_EQ(PCOL_OK, pcol_prewrite(t));
    pcol_txn_free(t);                        /* 崩溃：从不 commit */
    int c = 1, r = 0;
    EXPECT_EQ(0, pcol_recover_txn(&s, &lt, pk, 1, 200, &c, &r));
    EXPECT_EQ(0, c); EXPECT_EQ(1, r);
    EXPECT_EQ(0, ts_store_has_pending_write(&s, sk, 1, -1)); /* secondary 锁已清 */
    EXPECT_EQ(0, ts_store_has_pending_write(&s, pk, 1, -1)); /* primary 锁已清 */
    EXPECT_EQ(0u, lt.n);
    ts_version_t out;
    EXPECT_NE(0, ts_store_get(&s, sk, 1, 600, nullptr, 0, &out)); /* 无可见值 */
    pcol_lock_table_destroy(&lt); tso_oracle_destroy(o); ts_store_destroy(&s);
}

// 超期锁：未提交事务被回滚；撕裂提交事务被补提交。now_ms 用极大值使全部锁过期。
TEST(PercolatorGc, StaleLocksRecoveredOrRolledBack) {
    ts_store_t s; ts_store_init(&s);
    tso_oracle_t *o = nullptr; EXPECT_EQ(0, tso_oracle_init(&o));
    pcol_lock_table_t lt; EXPECT_EQ(0, pcol_lock_table_init(&lt));
    uint8_t x[] = {'X'}, w[] = {'W'}, y[] = {'Y'}, z[] = {'Z'}, v[] = {'v'};

    /* 事务 A：prewrite-only 永不提交（将回滚） */
    pcol_txn_t *ta = pcol_txn_begin(&s, o, 500); pcol_txn_set_lock_table(ta, &lt);
    pcol_write_t wa1 = {x, 1, v, 1, false}, wa2 = {w, 1, v, 1, false};
    pcol_txn_add_write(ta, &wa1); pcol_txn_add_write(ta, &wa2);
    EXPECT_EQ(PCOL_OK, pcol_prewrite(ta)); pcol_txn_free(ta);

    /* 事务 B：撕裂提交（只提权 primary Y） */
    pcol_txn_t *tb = pcol_txn_begin(&s, o, 600); pcol_txn_set_lock_table(tb, &lt);
    pcol_write_t wb1 = {y, 1, v, 1, false}, wb2 = {z, 1, v, 1, false};
    pcol_txn_add_write(tb, &wb1); pcol_txn_add_write(tb, &wb2);
    EXPECT_EQ(PCOL_OK, pcol_prewrite(tb));
    EXPECT_EQ(0, ts_store_promote(&s, y, 1, 600, 700)); pcol_txn_free(tb);

    ASSERT_EQ(4u, lt.n);
    int nc = 0, nr = 0;
    EXPECT_EQ(0, pcol_gc_stale_locks(&s, &lt, INT64_MAX / 2, &nc, &nr));
    EXPECT_EQ(1, nc); EXPECT_EQ(1, nr);       /* B 补提交、A 回滚 */
    EXPECT_EQ(0u, lt.n);

    ts_version_t out;
    EXPECT_EQ(0, ts_store_get(&s, z, 1, 800, nullptr, 0, &out));  /* B.secondary 可见 */
    ts_version_free(&out);
    EXPECT_NE(0, ts_store_get(&s, w, 1, 800, nullptr, 0, &out));  /* A 无可见值 */
    EXPECT_EQ(0, ts_store_has_pending_write(&s, x, 1, -1));       /* A 锁已清 */
    pcol_lock_table_destroy(&lt); tso_oracle_destroy(o); ts_store_destroy(&s);
}

// ---------- 跨分片协调（Task 9：链路打通） ----------
//
// 分片路由用 pcol_shard_hash（FNV-1a mod 2）：键 'a'/'c' 落分片 0、'b'/'d' 落分片 1。
// 断言一律以"路由到的分片"为准（用 shard_of 取出对应 store），不硬编码具体下标。

namespace {

// 取键实际路由到的分片 store
ts_store_t *shard_of(ts_store_t **stores, const uint8_t *k, size_t klen, int n) {
    return stores[pcol_shard_hash(k, klen, n)];
}

}  // namespace

// 跨两分片原子提交：共享 commit_ts 下两分片均可见；小于该 ts 的快照两分片均不可见
TEST(PercolatorCross, TwoShardsAtomicCommit) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);

    const uint8_t k1[] = {'a'}, k2[] = {'b'}, v[] = {'x'};
    // 前提：两个键确实落在不同分片，本用例才真正跨分片
    ASSERT_NE(pcol_shard_hash(k1, 1, 2), pcol_shard_hash(k2, 1, 2));
    ts_store_t *sa = shard_of(stores, k1, 1, 2);
    ts_store_t *sb = shard_of(stores, k2, 1, 2);

    pcol_write_t ws[2] = {{k1, 1, v, 1, false}, {k2, 1, v, 1, false}};
    EXPECT_EQ(PCOL_OK, pcol_commit_cross(ctx, pcol_shard_hash, ws, 2, 100, 500, nullptr));

    ts_version_t out;
    memset(&out, 0, sizeof(out));
    // commit_ts=500 之后：两分片都可见
    EXPECT_EQ(0, ts_store_get(sa, k1, 1, 600, nullptr, 0, &out)); ts_version_free(&out);
    EXPECT_EQ(0, ts_store_get(sb, k2, 1, 600, nullptr, 0, &out)); ts_version_free(&out);
    // 原子性：commit_ts=500 之前，两分片都不可见（不存在"一半可见"的中间态）
    EXPECT_NE(0, ts_store_get(sa, k1, 1, 400, nullptr, 0, &out));
    EXPECT_NE(0, ts_store_get(sb, k2, 1, 400, nullptr, 0, &out));
    // 预写锁在提交后收敛
    EXPECT_EQ(0, ts_store_has_pending_write(sa, k1, 1, -1));
    EXPECT_EQ(0, ts_store_has_pending_write(sb, k2, 1, -1));

    pcol_context_free(ctx); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}

// 某分片 prewrite 冲突 → 全部分片回滚，无部分可见；释放持有者锁后可重试成功
TEST(PercolatorCross, ConflictRollsBackAllShards) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);

    const uint8_t k1[] = {'a'}, k2[] = {'b'}, v[] = {'x'};
    ts_store_t *sa = shard_of(stores, k1, 1, 2);
    ts_store_t *sb = shard_of(stores, k2, 1, 2);

    // 造锁冲突基线：裸预写（commit_ts=0）令 start_ts=60 持有 b 所在分片的预写锁
    ASSERT_EQ(0, ts_store_put(sb, k2, 1, 60, 0, v, 1));

    pcol_write_t ws[2] = {{k1, 1, v, 1, false}, {k2, 1, v, 1, false}};
    EXPECT_EQ(PCOL_ERR_LOCK_CONFLICT,
              pcol_commit_cross(ctx, pcol_shard_hash, ws, 2, 100, 200, nullptr));

    // 原子回滚：另一分片（a）上本事务的预写也被清干净，且无任何可见版本
    EXPECT_EQ(0, ts_store_has_pending_write(sa, k1, 1, -1));
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_NE(0, ts_store_get(sa, k1, 1, 1000, nullptr, 0, &out));
    EXPECT_NE(0, ts_store_get(sb, k2, 1, 1000, nullptr, 0, &out));

    // 持有者释放锁后重试：跨分片提交成功，两分片同时可见
    ts_store_discard_pending(sb, k2, 1, 60);
    EXPECT_EQ(PCOL_OK, pcol_commit_cross(ctx, pcol_shard_hash, ws, 2, 100, 200, nullptr));
    EXPECT_EQ(0, ts_store_get(sa, k1, 1, 300, nullptr, 0, &out)); ts_version_free(&out);
    EXPECT_EQ(0, ts_store_get(sb, k2, 1, 300, nullptr, 0, &out)); ts_version_free(&out);

    pcol_context_free(ctx); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}

// 协调器意图重放：撕裂状态（只提交了 primary 分片）→ recover 依据 primary 补提交其余分片；
// primary 未提交的意图 → recover 对全部分片回滚。
TEST(PercolatorCross, CoordinatorRecoverReplaysIntent) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);
    pcol_intent_log_clear();

    const uint8_t k1[] = {'a'}, k2[] = {'b'}, v[] = {'x'};
    const int sh1 = pcol_shard_hash(k1, 1, 2), sh2 = pcol_shard_hash(k2, 1, 2);
    ASSERT_NE(sh1, sh2);

    // 撕裂状态（最稳构造）：裸预写两分片，只把 primary 分片的键提权为已提交
    ASSERT_EQ(0, ts_store_put(stores[sh1], k1, 1, 700, 0, v, 1));
    ASSERT_EQ(0, ts_store_put(stores[sh2], k2, 1, 700, 0, v, 1));
    ASSERT_EQ(0, ts_store_promote(stores[sh1], k1, 1, 700, 800));  // primary 分片已提交

    // 意图日志：键缓冲布局 = [uint32 klen][key bytes]
    uint8_t bufa[5], bufb[5];
    uint32_t one = 1;
    memcpy(bufa, &one, 4); bufa[4] = 'a';
    memcpy(bufb, &one, 4); bufb[4] = 'b';
    uint8_t *keys[2]; size_t klens[2]; int kns[2];
    keys[sh1] = bufa; klens[sh1] = 5; kns[sh1] = 1;
    keys[sh2] = bufb; klens[sh2] = 5; kns[sh2] = 1;

    pcol_intent_t it;
    memset(&it, 0, sizeof(it));
    it.start_ts = 700; it.commit_ts = 800; it.shard_count = 2;
    it.primary_shard = sh1;
    it.shard_keys = keys; it.shard_keys_len = klens; it.shard_keys_n = kns;
    ASSERT_EQ(0, pcol_intent_log_add(&it));

    const pcol_intent_t *got = nullptr;
    EXPECT_EQ(0, pcol_intent_log_get(700, &got));
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(800, got->commit_ts);

    // 崩溃重放：primary 已提交 → 另一分片被补提交
    EXPECT_EQ(PCOL_OK, pcol_cross_recover(ctx, 700));
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    ASSERT_EQ(0, ts_store_get(stores[sh2], k2, 1, 900, nullptr, 0, &out));
    EXPECT_EQ(800, out.commit_ts);
    ts_version_free(&out);
    EXPECT_EQ(0, ts_store_has_pending_write(stores[sh2], k2, 1, -1));  // 锁已收敛

    // 反向分支：primary 未提交的意图 → 全分片回滚
    const uint8_t k3[] = {'c'}, k4[] = {'d'};
    const int sh3 = pcol_shard_hash(k3, 1, 2), sh4 = pcol_shard_hash(k4, 1, 2);
    ASSERT_NE(sh3, sh4);
    ASSERT_EQ(0, ts_store_put(stores[sh3], k3, 1, 900, 0, v, 1));
    ASSERT_EQ(0, ts_store_put(stores[sh4], k4, 1, 900, 0, v, 1));
    uint8_t bufc[5], bufd[5];
    memcpy(bufc, &one, 4); bufc[4] = 'c';
    memcpy(bufd, &one, 4); bufd[4] = 'd';
    keys[sh3] = bufc; klens[sh3] = 5; kns[sh3] = 1;
    keys[sh4] = bufd; klens[sh4] = 5; kns[sh4] = 1;
    it.start_ts = 900; it.commit_ts = 1000; it.primary_shard = sh3;
    ASSERT_EQ(0, pcol_intent_log_add(&it));

    EXPECT_EQ(PCOL_OK, pcol_cross_recover(ctx, 900));
    EXPECT_EQ(0, ts_store_has_pending_write(stores[sh3], k3, 1, -1));  // 全分片锁清空
    EXPECT_EQ(0, ts_store_has_pending_write(stores[sh4], k4, 1, -1));
    EXPECT_NE(0, ts_store_get(stores[sh4], k4, 1, 2000, nullptr, 0, &out));

    pcol_intent_log_clear();
    pcol_context_free(ctx); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}

// 锁冲突可观测 + wfg 死锁防御：能定位持有者并登记 wait-edge；构成环时报 DEADLOCK
TEST(PercolatorCross, LockConflictReportsHolderAndWfg) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);
    wfg_t *g = wfg_new();
    ASSERT_NE(nullptr, g);

    const uint8_t k2[] = {'b'}, v[] = {'x'};
    ts_store_t *sb = shard_of(stores, k2, 1, 2);
    // 持有者 start_ts=60 在 b 所在分片持预写锁（裸预写构造，生命周期最简单）
    ASSERT_EQ(0, ts_store_put(sb, k2, 1, 60, 0, v, 1));

    // 主事务 start=100 想写 b → LOCK_CONFLICT，且 wait-edge(100→60) 被登记进 wfg
    pcol_write_t ws[1] = {{k2, 1, v, 1, false}};
    EXPECT_EQ(PCOL_ERR_LOCK_CONFLICT,
              pcol_commit_cross(ctx, pcol_shard_hash, ws, 1, 100, 200, g));

    // 持有者可观测：ts_store_pending_holder 能报出真实持有者 start_ts
    int64_t holder = 0;
    EXPECT_EQ(0, ts_store_pending_holder(sb, k2, 1, /*except*/100, &holder));
    EXPECT_EQ(60LL, holder);

    // 此时只有单向边 100→60，无环
    int cyc_n = -1;
    int64_t *cyc = wfg_detect_cycles(g, &cyc_n);
    EXPECT_EQ(nullptr, cyc);
    EXPECT_EQ(0, cyc_n);
    free(cyc);

    // 补上反向边 60→100 构成环：再冲突一次即应判为 DEADLOCK（防御生效）
    ASSERT_EQ(0, wfg_add_edge(g, 60, 100, k2, 1, 1));
    EXPECT_EQ(PCOL_ERR_DEADLOCK,
              pcol_commit_cross(ctx, pcol_shard_hash, ws, 1, 100, 200, g));
    EXPECT_STRNE("unknown", pcol_error_string(PCOL_ERR_DEADLOCK));

    pcol_context_free(ctx); wfg_free(g); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}

// 意图日志自动接线：一次成功的 pcol_commit_cross 应由库自身把意图记入日志，
// 无需调用方手工 pcol_intent_log_add——崩溃重放（pcol_cross_recover）由此端到端打通。
TEST(PercolatorCross, CommitCrossAutoRecordsIntent) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);
    pcol_intent_log_clear();   // 静态意图表跨用例隔离

    const uint8_t k1[] = {'a'}, k2[] = {'b'}, v[] = {'x'};
    const int sh1 = pcol_shard_hash(k1, 1, 2);
    const int sh2 = pcol_shard_hash(k2, 1, 2);
    ASSERT_NE(sh1, sh2);   // 前提：两键真落不同分片

    pcol_write_t ws[2] = {{k1, 1, v, 1, false}, {k2, 1, v, 1, false}};
    ASSERT_EQ(PCOL_OK, pcol_commit_cross(ctx, pcol_shard_hash, ws, 2, 100, 500, nullptr));

    // 库已自动记录意图：get 命中且字段与本次提交一致（无任何手工 log_add）
    const pcol_intent_t *got = nullptr;
    EXPECT_EQ(0, pcol_intent_log_get(100, &got));
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(500, got->commit_ts);
    EXPECT_EQ(2, got->shard_count);
    EXPECT_EQ(sh1, got->primary_shard);          // primary = 整体首写所在分片
    EXPECT_EQ(1, got->shard_keys_n[0]);          // 每分片各一键
    EXPECT_EQ(1, got->shard_keys_n[1]);

    // 自动记录的意图须能被 recover 正确解析：模拟"commit-all 中途崩溃"撕裂态——
    // 次分片已提交版本丢失、只剩预写锁（教学模型：重建空 store 后裸预写），
    // primary 分片保持已提交；recover 应据自动记录的意图把次分片补提交到 500。
    // 次分片即 k2('b') 所在分片（primary 键恒为 writes[0]=k1），与其下标无关。
    ts_store_t *sec = stores[sh2];
    const uint8_t *skey = k2;
    ts_store_destroy(sec);
    ts_store_init(sec);
    ASSERT_EQ(0, ts_store_put(sec, skey, 1, 100, 0, v, 1));  // 次分片残留预写锁
    EXPECT_EQ(PCOL_OK, pcol_cross_recover(ctx, 100));
    ts_version_t out;
    memset(&out, 0, sizeof(out));
    EXPECT_EQ(0, ts_store_get(sec, skey, 1, 600, nullptr, 0, &out));
    EXPECT_EQ(500, out.commit_ts);               // recover 据意图补提交，键布局解析正确
    ts_version_free(&out);

    pcol_intent_log_clear();
    pcol_context_free(ctx); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}

// I-1 回归：primary 落非 0 分片（writes[0]='b'→shard1）时，commit-all 必须 primary 先提交；
// 且"只提权 primary"的可达撕裂态在非 0 primary 下经 pcol_cross_recover 必须收敛为全体提交。
// 修复前 commit-all 按下标顺序提交：primary 在 shard1 时会先提交 shard0 的 secondary，
// 中途崩溃后 recover 据未提交的 primary 裁定回滚，已提交的 secondary 永久可见 → 部分提交。
TEST(PercolatorCross, PrimaryOnNonZeroShardCommitsFirstAndRecovers) {
    ts_store_t s1, s2; ts_store_init(&s1); ts_store_init(&s2);
    ts_store_t *stores[2] = {&s1, &s2};
    tso_oracle_t *o = nullptr; ASSERT_EQ(0, tso_oracle_init(&o));
    pcol_context_t *ctx = pcol_context_new(stores, 2, o);
    ASSERT_NE(nullptr, ctx);
    pcol_intent_log_clear();   // 静态意图表跨用例隔离

    const uint8_t ka[] = {'a'}, kb[] = {'b'}, v[] = {'x'};
    const int sa = pcol_shard_hash(ka, 1, 2);   // 'a' → shard0
    const int sb = pcol_shard_hash(kb, 1, 2);   // 'b' → shard1
    ASSERT_EQ(0, sa);
    ASSERT_EQ(1, sb);                           // 前提：primary(writes[0]='b') 落非 0 分片

    // (a) 正常路径：writes[0]='b'（primary→shard1）、writes[1]='a'（secondary→shard0）
    pcol_write_t ws[2] = {{kb, 1, v, 1, false}, {ka, 1, v, 1, false}};
    EXPECT_EQ(PCOL_OK, pcol_commit_cross(ctx, pcol_shard_hash, ws, 2, 100, 500, nullptr));

    ts_version_t out;
    memset(&out, 0, sizeof(out));
    // 两分片在 commit_ts=500 均可见
    EXPECT_EQ(0, ts_store_get(stores[sa], ka, 1, 600, nullptr, 0, &out));
    EXPECT_EQ(500, out.commit_ts); ts_version_free(&out);
    EXPECT_EQ(0, ts_store_get(stores[sb], kb, 1, 600, nullptr, 0, &out));
    EXPECT_EQ(500, out.commit_ts); ts_version_free(&out);
    // 自动记录的意图中 primary_shard 应为非 0 的 1
    const pcol_intent_t *got = nullptr;
    EXPECT_EQ(0, pcol_intent_log_get(100, &got));
    ASSERT_NE(nullptr, got);
    EXPECT_EQ(1, got->primary_shard);

    // (b) 可达撕裂态：两分片裸预写（commit=0，同一 start），只提权 primary（shard1 的 'b'），
    //     记录 primary_shard=1 的意图后 pcol_cross_recover —— secondary 必须被补提交。
    pcol_intent_log_clear();
    const int64_t st = 700, ct = 800;
    ASSERT_EQ(0, ts_store_put(stores[sa], ka, 1, st, 0, v, 1));   // secondary 预写
    ASSERT_EQ(0, ts_store_put(stores[sb], kb, 1, st, 0, v, 1));   // primary 预写
    ASSERT_EQ(0, ts_store_promote(stores[sb], kb, 1, st, ct));    // 崩溃点：只 primary 提交

    // 意图：键缓冲布局 [uint32 klen][key]（与库内 intent_key_at 同源），shard_keys 按下标摆放
    uint8_t bufa[5], bufb[5];
    uint32_t one = 1;
    memcpy(bufa, &one, 4); bufa[4] = 'a';
    memcpy(bufb, &one, 4); bufb[4] = 'b';
    uint8_t *keys[2]; size_t klens[2]; int kns[2];
    memset(keys, 0, sizeof(keys)); memset(klens, 0, sizeof(klens)); memset(kns, 0, sizeof(kns));
    keys[sa] = bufa; klens[sa] = 5; kns[sa] = 1;
    keys[sb] = bufb; klens[sb] = 5; kns[sb] = 1;
    pcol_intent_t it;
    memset(&it, 0, sizeof(it));
    it.start_ts = st; it.commit_ts = ct; it.shard_count = 2;
    it.primary_shard = sb;                  // primary 落 shard1（非 0）
    it.shard_keys = keys; it.shard_keys_len = klens; it.shard_keys_n = kns;
    ASSERT_EQ(0, pcol_intent_log_add(&it));

    EXPECT_EQ(PCOL_OK, pcol_cross_recover(ctx, st));
    // secondary（shard0 的 'a'）被补提交可见，commit_ts 与 primary 一致
    ASSERT_EQ(0, ts_store_get(stores[sa], ka, 1, 900, nullptr, 0, &out));
    EXPECT_EQ(ct, out.commit_ts);
    ts_version_free(&out);
    // primary（shard1 的 'b'）保持可见
    EXPECT_EQ(0, ts_store_get(stores[sb], kb, 1, 900, nullptr, 0, &out));
    ts_version_free(&out);
    // 两分片均无残留预写锁
    EXPECT_EQ(0, ts_store_has_pending_write(stores[sa], ka, 1, -1));
    EXPECT_EQ(0, ts_store_has_pending_write(stores[sb], kb, 1, -1));

    pcol_intent_log_clear();
    pcol_context_free(ctx); tso_oracle_destroy(o);
    ts_store_destroy(&s1); ts_store_destroy(&s2);
}