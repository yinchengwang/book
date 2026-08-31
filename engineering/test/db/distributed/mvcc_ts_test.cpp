// mvcc_ts_test.cpp —— 时间戳版本链快照存储（ts_store_t）SI 读取面测试
#include <gtest/gtest.h>

#include <string>
#include <vector>

extern "C" {
#include "distributed/mvcc_ts.h"
}

namespace {

using std::string;
using std::vector;

// 取出副本的 value 转字符串
string val_of(ts_version_t *v) {
    return v->value_len ? string(static_cast<char *>(v->value), v->value_len) : string();
}

// klen 便捷取整（ASCII 测试键）
uint32_t KL(const char *k) { return static_cast<uint32_t>(strlen(k)); }

}  // namespace

// 快照读取：可见已提交版本；未提交(prewrite)与早于快照的版本不可见
TEST(TsStore, GetBasicSnapshot) {
    ts_store_t s;
    ts_store_init(&s);
    const char *k = "k1";

    ts_store_put(&s, k, KL(k), 5, 0, "uncommitted", 11);   // prewrite(commit_ts=0)
    ts_store_put(&s, k, KL(k), 5, 200, "v200", 4);         // 提交于 200

    ts_version_t out;
    memset(&out, 0, sizeof(out));

    // read_ts >= 400：看到 commit_ts=200 的版本
    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 400, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "v200");
    EXPECT_EQ(out.commit_ts, 200);
    EXPECT_EQ(out.start_ts, 5);
    EXPECT_NE(out.key, (void *)nullptr);
    ts_version_free(&out);

    // read_ts < 200：已提交版本尚未提交；prewrite 不可见 -> -1
    EXPECT_EQ(-1, ts_store_get(&s, k, KL(k), 100, nullptr, 0, &out));
    // read_ts=0：没有满足 commit_ts>0 的版本 -> -1
    EXPECT_EQ(-1, ts_store_get(&s, k, KL(k), 0, nullptr, 0, &out));

    ts_store_destroy(&s);
}

// 键不存在 -> -1
TEST(TsStore, GetMissingKey) {
    ts_store_t s;
    ts_store_init(&s);
    ts_version_t out;
    EXPECT_EQ(-1, ts_store_get(&s, "nope", KL("nope"), 100, nullptr, 0, &out));
    ts_store_destroy(&s);
}

// active 隐藏集：已提交但 start_ts 在 active 内的版本不可见
TEST(TsStore, GetActiveHidesTransaction) {
    ts_store_t s;
    ts_store_init(&s);
    const char *k = "k";

    ts_store_put(&s, k, KL(k), 7, 100, "a", 1);   // tx7 提交于 100

    int64_t active[] = {7};
    ts_version_t out;

    // 不隐藏时可见
    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 200, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "a");
    ts_version_free(&out);

    // 隐藏 tx7 后：其版本视为不可见 -> -1
    EXPECT_EQ(-1, ts_store_get(&s, k, KL(k), 200, active, 1, &out));

    ts_store_destroy(&s);
}

// 多版本：读取应取 commit_ts<=read_ts 且最大的可见版本（与插入顺序无关）
TEST(TsStore, GetPicksNewestVisibleIgnoringInsertOrder) {
    ts_store_t s;
    ts_store_init(&s);
    const char *k = "k";

    // 故意乱序提交
    ts_store_put(&s, k, KL(k), 1, 300, "new@300", 7);
    ts_store_put(&s, k, KL(k), 2, 100, "mid@100", 7);
    ts_store_put(&s, k, KL(k), 3, 200, "old@200", 7);

    ts_version_t out;
    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 250, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "old@200");
    EXPECT_EQ(out.commit_ts, 200);
    ts_version_free(&out);

    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 150, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "mid@100");
    ts_version_free(&out);

    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 500, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "new@300");
    ts_version_free(&out);

    ts_store_destroy(&s);
}

// tombstone：可见则 -2；删除前的快照仍读到旧值；删除后又有新写则读到新写
TEST(TsStore, TombstoneSemantics) {
    ts_store_t s;
    ts_store_init(&s);
    const char *k = "k";

    ts_store_put(&s, k, KL(k), 1, 100, "v1", 2);
    ts_store_put_delete(&s, k, KL(k), 2, 200);   // 于 200 删除

    ts_version_t out;
    // 删除前快照：看到 v1
    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 150, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "v1");
    ts_version_free(&out);

    // 删除后快照：-2
    EXPECT_EQ(-2, ts_store_get(&s, k, KL(k), 250, nullptr, 0, &out));

    // 删除后又新写：读到新写
    ts_store_put(&s, k, KL(k), 3, 300, "v2", 2);
    ASSERT_EQ(0, ts_store_get(&s, k, KL(k), 350, nullptr, 0, &out));
    EXPECT_EQ(val_of(&out), "v2");
    ts_version_free(&out);

    ts_store_destroy(&s);
}

// 扫描：按 key 字节序升序输出所有可见非删版本；不可见/已删键被跳过
TEST(TsStore, ScanOrderAndFiltering) {
    ts_store_t s;
    ts_store_init(&s);

    // 键：a, b10, b2, c(已删), d(被active隐藏)。字节序：a < b1... < b2... < c < d
    ts_store_put(&s, "a", 1, 1, 100, "A", 1);
    ts_store_put(&s, "b10", 3, 2, 100, "B10", 3);
    ts_store_put(&s, "b2", 2, 3, 100, "B2", 2);
    ts_store_put(&s, "c", 1, 4, 100, "C", 1);
    ts_store_put_delete(&s, "c", 1, 5, 200);               // c 已删
    ts_store_put(&s, "d", 1, 6, 100, "D", 1);

    int64_t active[] = {6};   // 隐藏 d

    ts_iter_t it;
    ts_store_scan(&s, nullptr, 0, 400, active, 1, &it);

    vector<string> got, keys;
    ts_version_t out;
    int rc;
    while ((rc = ts_iter_next(&it, &out)) == 0) {
        keys.push_back(string(out.key, out.klen));
        got.push_back(val_of(&out));
        ts_version_free(&out);
    }
    EXPECT_EQ(rc, 1);

    // 期望只产出 a、b10、b2（c 已删跳过、d 被隐藏跳过），且按字节序
    vector<string> want_keys = {"a", "b10", "b2"};
    vector<string> want_vals = {"A", "B10", "B2"};
    EXPECT_EQ(keys, want_keys);
    EXPECT_EQ(got, want_vals);

    ts_store_destroy(&s);
}

// 扫描 start 键含起：只产出 start 及之后
TEST(TsStore, ScanFromStartInclusive) {
    ts_store_t s;
    ts_store_init(&s);

    ts_store_put(&s, "a", 1, 1, 100, "A", 1);
    ts_store_put(&s, "b", 1, 2, 100, "B", 1);
    ts_store_put(&s, "c", 1, 3, 100, "C", 1);

    ts_iter_t it;
    ts_store_scan(&s, "b", KL("b"), 200, nullptr, 0, &it);

    vector<string> keys;
    ts_version_t out;
    int rc;
    while ((rc = ts_iter_next(&it, &out)) == 0) {
        keys.push_back(string(out.key, out.klen));
        ts_version_free(&out);
    }
    EXPECT_EQ(rc, 1);
    vector<string> want = {"b", "c"};
    EXPECT_EQ(keys, want);

    ts_store_destroy(&s);
}

// 空库扫描立即结束
TEST(TsStore, ScanEmpty) {
    ts_store_t s;
    ts_store_init(&s);
    ts_iter_t it;
    ts_store_scan(&s, nullptr, 0, 100, nullptr, 0, &it);
    ts_version_t out;
    EXPECT_EQ(1, ts_iter_next(&it, &out));
    ts_store_destroy(&s);
}