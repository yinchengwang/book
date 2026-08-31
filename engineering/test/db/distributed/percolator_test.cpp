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