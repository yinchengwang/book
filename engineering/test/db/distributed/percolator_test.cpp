// percolator_test.cpp —— Percolator 2PC（Prewrite/Commit/回滚，集成 MVCC 行 + 锁）
// 核心断言：prewrite 后未提交不可见；commit 后对足够新 read_ts 可见、对过旧
// read_ts 不可见；提交幂等（二次提交返回 ALREADY_COMMITTED）；冲突检测正确。
#include <gtest/gtest.h>

#include <cstring>
#include <string>

extern "C" {
#include "distributed/tso.h"
#include "distributed/mvcc_ts.h"
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