/**
 * @file test_kv.cpp
 * @brief KV API 测试
 */

#include <gtest/gtest.h>
#include "db/kv.h"
#include <cstdio>
#include <cstring>

// 每个测试用不同的数据库文件
const char* test_file = "test_kv.db";

TEST(KVTest, SimplePutGet) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const char *key = "hello";
    const char *val = "world";
    EXPECT_EQ(KV_OK, kv_put(db, key, strlen(key), val, strlen(val)));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, key, strlen(key), &out_value, &out_len));
    EXPECT_EQ(strlen(val), out_len);
    EXPECT_EQ(0, memcmp(val, out_value, out_len));
    free(out_value);

    kv_close(db);
}

TEST(KVTest, Scan) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "a", 1, "1", 1);
    kv_put(db, "b", 1, "2", 1);
    kv_put(db, "c", 1, "3", 1);

    kv_iter_t *iter = kv_scan(db, NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);

    int count = 0;
    while (kv_iter_next(iter) == KV_OK) {
        count++;
    }
    kv_iter_free(iter);

    EXPECT_EQ(3, count);
    kv_close(db);
}

TEST(KVTest, Delete) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const char *key = "to_delete";
    kv_put(db, key, strlen(key), "data", 4);

    EXPECT_TRUE(kv_exists(db, key, strlen(key)));
    kv_delete(db, key, strlen(key));
    EXPECT_FALSE(kv_exists(db, key, strlen(key)));

    kv_close(db);
}

// ========================================================================
// C3-5 T22：CAS（Compare-And-Swap）测试
// ========================================================================

TEST(KVTest, CAS_Success) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    /* 先写入初始值 */
    EXPECT_EQ(KV_OK, kv_put(db, "count", 5, "10", 2));

    /* CAS 旧值匹配，替换成功 */
    EXPECT_EQ(KV_OK, kv_cas(db, "count", 5, "10", 2, "20", 2));

    /* 验证新值 */
    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "count", 5, &out_value, &out_len));
    EXPECT_EQ(2u, out_len);
    EXPECT_EQ(0, memcmp("20", out_value, 2));
    free(out_value);

    kv_close(db);
}

TEST(KVTest, CAS_Conflict) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    /* 先写入初始值 */
    EXPECT_EQ(KV_OK, kv_put(db, "count", 5, "10", 2));

    /* CAS 旧值不匹配，应返回 KV_CONFLICT */
    EXPECT_EQ(KV_CONFLICT, kv_cas(db, "count", 5, "99", 2, "20", 2));

    /* 原值未被修改 */
    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "count", 5, &out_value, &out_len));
    EXPECT_EQ(2u, out_len);
    EXPECT_EQ(0, memcmp("10", out_value, 2));
    free(out_value);

    kv_close(db);
}

TEST(KVTest, CAS_NewKey) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    /* 对不存在的键，expected_old=NULL 应成功插入 */
    EXPECT_EQ(KV_OK, kv_cas(db, "new_key", 7, NULL, 0, "new_val", 7));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "new_key", 7, &out_value, &out_len));
    EXPECT_EQ(7u, out_len);
    EXPECT_EQ(0, memcmp("new_val", out_value, 7));
    free(out_value);

    kv_close(db);
}

// ========================================================================
// C3-5 T22：Watch 通知测试
// ========================================================================

struct watch_ctx {
    std::string key;
    std::string old_value;
    std::string new_value;
    int call_count;
};

static void watch_callback(void *user_data,
                           const char *key, size_t key_len,
                           const void *old_value, size_t old_len,
                           const void *new_value, size_t new_len) {
    watch_ctx *ctx = (watch_ctx *)user_data;
    ctx->key = std::string((const char *)key, key_len);
    ctx->old_value.clear();
    ctx->new_value.clear();
    if (old_value && old_len > 0)
        ctx->old_value = std::string((const char *)old_value, old_len);
    if (new_value && new_len > 0)
        ctx->new_value = std::string((const char *)new_value, new_len);
    ctx->call_count++;
}

TEST(KVTest, Watch_TriggerOnPut) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    watch_ctx ctx;
    ctx.call_count = 0;

    kv_watch_t *watch = kv_watch(db, "counter", 7, watch_callback, &ctx);
    ASSERT_NE(nullptr, watch);

    /* 写入新键，触发 watch */
    EXPECT_EQ(KV_OK, kv_put(db, "counter", 7, "1", 1));
    EXPECT_EQ(1, ctx.call_count);
    EXPECT_EQ("counter", ctx.key);
    EXPECT_EQ("", ctx.old_value);  /* 新键无旧值 */
    EXPECT_EQ("1", ctx.new_value);

    /* 更新已有键，触发 watch */
    EXPECT_EQ(KV_OK, kv_put(db, "counter", 7, "2", 1));
    EXPECT_EQ(2, ctx.call_count);
    EXPECT_EQ("1", ctx.old_value);
    EXPECT_EQ("2", ctx.new_value);

    kv_unwatch(db, watch);
    kv_close(db);
}

TEST(KVTest, Watch_GlobalSubscribe) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    int call_count = 0;
    kv_watch_t *watch = kv_watch(db, NULL, 0,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            int *cnt = (int *)ud;
            (*cnt)++;
        }, &call_count);
    ASSERT_NE(nullptr, watch);

    kv_put(db, "a", 1, "1", 1);
    kv_put(db, "b", 1, "2", 2);
    EXPECT_EQ(2, call_count);

    kv_unwatch(db, watch);
    kv_close(db);
}

TEST(KVTest, Watch_TriggerOnDelete) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    watch_ctx ctx;
    ctx.call_count = 0;

    kv_watch_t *watch = kv_watch(db, "item", 4, watch_callback, &ctx);
    ASSERT_NE(nullptr, watch);

    kv_put(db, "item", 4, "data", 4);
    EXPECT_EQ(1, ctx.call_count);

    /* 删除触发 watch，new_value 应为 NULL */
    EXPECT_EQ(KV_OK, kv_delete(db, "item", 4));
    EXPECT_EQ(2, ctx.call_count);
    EXPECT_EQ("data", ctx.old_value);
    EXPECT_EQ("", ctx.new_value);

    kv_unwatch(db, watch);
    kv_close(db);
}

// ========================================================================
// C3-5 T22：Multi 批量操作测试
// ========================================================================

TEST(KVTest, MultiGet_Set) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    /* 先写入一些数据 */
    kv_put(db, "k1", 2, "v1", 2);
    kv_put(db, "k2", 2, "v2", 2);
    kv_put(db, "k3", 2, "v3", 2);

    /* 批量读取 */
    kv_multi_entry_t entries[] = {
        { (void *)"k1", 2, nullptr, 0, false },
        { (void *)"k2", 2, nullptr, 0, false },
        { (void *)"k4", 2, nullptr, 0, false },  /* 不存在 */
    };
    EXPECT_EQ(KV_OK, kv_multi_get(db, entries, 3));
    EXPECT_EQ(true, entries[0].is_set);
    EXPECT_EQ(2u, entries[0].value_len);
    EXPECT_EQ(0, memcmp("v1", entries[0].value, 2));
    EXPECT_EQ(true, entries[1].is_set);
    EXPECT_EQ(2u, entries[1].value_len);
    EXPECT_EQ(0, memcmp("v2", entries[1].value, 2));
    EXPECT_EQ(false, entries[2].is_set);  /* 不存在 */

    /* 释放 */
    if (entries[0].value) free(entries[0].value);
    if (entries[1].value) free(entries[1].value);

    /* 批量写入 */
    kv_multi_entry_t writes[] = {
        { (void *)"m1", 2, (void *)"mv1", 3, true },
        { (void *)"m2", 2, (void *)"mv2", 3, true },
    };
    EXPECT_EQ(KV_OK, kv_multi_set(db, writes, 2));

    /* 验证写入 */
    void *val = nullptr;
    size_t vlen = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "m1", 2, &val, &vlen));
    EXPECT_EQ(3u, vlen);
    EXPECT_EQ(0, memcmp("mv1", val, 3));
    free(val);
    EXPECT_EQ(KV_OK, kv_get(db, "m2", 2, &val, &vlen));
    EXPECT_EQ(3u, vlen);
    EXPECT_EQ(0, memcmp("mv2", val, 3));
    free(val);

    kv_close(db);
}

TEST(KVTest, MultiDel) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "del1", 4, "x", 1);
    kv_put(db, "del2", 4, "y", 1);

    kv_multi_entry_t del_entries[] = {
        { (void *)"del1", 4, nullptr, 0, false },
        { (void *)"del2", 4, nullptr, 0, false },
    };
    EXPECT_EQ(KV_OK, kv_multi_del(db, del_entries, 2));

    EXPECT_FALSE(kv_exists(db, "del1", 4));
    EXPECT_FALSE(kv_exists(db, "del2", 4));

    kv_close(db);
}

TEST(KVTest, MultiMixGetSet) {
    std::remove(test_file);

    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "existing", 8, "old_val", 7);

    /* 先用 multi_set 写入新值 */
    kv_multi_entry_t set_entries[] = {
        { (void *)"existing", 8, (void *)"new_val", 7, true },
        { (void *)"new_key",  7, (void *)"new_val", 7, true },
    };
    EXPECT_EQ(KV_OK, kv_multi_set(db, set_entries, 2));

    /* 再用 multi_get 验证读取结果 */
    kv_multi_entry_t get_entries[] = {
        { (void *)"existing", 8, nullptr, 0, false },
        { (void *)"new_key",  7, nullptr, 0, false },
        { (void *)"missing",  7, nullptr, 0, false },
    };
    EXPECT_EQ(KV_OK, kv_multi_get(db, get_entries, 3));

    /* existing 和 new_key 应读到新值 */
    EXPECT_EQ(true, get_entries[0].is_set);
    EXPECT_EQ(7u, get_entries[0].value_len);
    EXPECT_EQ(0, memcmp("new_val", get_entries[0].value, 7));
    free(get_entries[0].value);

    EXPECT_EQ(true, get_entries[1].is_set);
    EXPECT_EQ(7u, get_entries[1].value_len);
    EXPECT_EQ(0, memcmp("new_val", get_entries[1].value, 7));
    free(get_entries[1].value);

    /* missing 应读不到 */
    EXPECT_EQ(false, get_entries[2].is_set);

    kv_close(db);
}
