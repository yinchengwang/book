/**
 * @file kv_concurrency_test.cpp
 * @brief KV 并发和高级功能测试
 *
 * 测试覆盖：
 * 1. CAS 操作边缘情况
 * 2. WATCH 多场景
 * 3. MULTI 批量操作边缘情况
 * 4. 并发安全（多线程）
 * 5. 错误处理
 * 6. 统计和刷新操作
 * 7. WAL 恢复
 */

#include <gtest/gtest.h>
#include "db/kv.h"
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>

// 测试数据库文件
const char* test_file = "test_kv_concurrency.db";

class KVConcurrencyTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(test_file);
        std::remove((std::string(test_file) + ".wal").c_str());
        std::remove((std::string(test_file) + ".ttl").c_str());
    }

    void TearDown() override {
        std::remove(test_file);
        std::remove((std::string(test_file) + ".wal").c_str());
        std::remove((std::string(test_file) + ".ttl").c_str());
    }
};

// ========================================================================
// CAS 操作边缘情况测试
// ========================================================================

/**
 * @brief CAS 使用相同值（应该成功，因为值没变）
 */
TEST_F(KVConcurrencyTest, CAS_SameValue) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 先写入初始值
    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "value", 5));

    // CAS 使用相同的旧值和新值（无变化）
    EXPECT_EQ(KV_OK, kv_cas(db, "key", 3, "value", 5, "value", 5));

    // 验证值没变
    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "key", 3, &out_value, &out_len));
    EXPECT_EQ(5u, out_len);
    EXPECT_EQ(0, memcmp("value", out_value, 5));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 值长度变化（从短变长）
 */
TEST_F(KVConcurrencyTest, CAS_ValueLengthIncrease) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "x", 1));

    // CAS 将值从 "x" 变为 "longer_value"
    EXPECT_EQ(KV_OK, kv_cas(db, "key", 3, "x", 1, "longer_value", 11));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "key", 3, &out_value, &out_len));
    EXPECT_EQ(11u, out_len);
    EXPECT_EQ(0, memcmp("longer_value", out_value, 11));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 值长度变化（从长变短）
 */
TEST_F(KVConcurrencyTest, CAS_ValueLengthDecrease) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "long_value", 9));

    // CAS 将值从 "long_value" 变为 "x"
    EXPECT_EQ(KV_OK, kv_cas(db, "key", 3, "long_value", 9, "x", 1));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "key", 3, &out_value, &out_len));
    EXPECT_EQ(1u, out_len);
    EXPECT_EQ(0, memcmp("x", out_value, 1));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 部分匹配（旧值部分匹配但不完全相等）
 */
TEST_F(KVConcurrencyTest, CAS_PartialMatch) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "hello", 5));

    // 旧值部分匹配（"hell" vs "hello"）
    EXPECT_EQ(KV_CONFLICT, kv_cas(db, "key", 3, "hell", 4, "world", 5));

    // 验证原值未变
    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "key", 3, &out_value, &out_len));
    EXPECT_EQ(0, memcmp("hello", out_value, 5));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 空数据库中对不存在的键使用 NULL old（应成功插入）
 */
TEST_F(KVConcurrencyTest, CAS_NewKeyOnEmptyDB) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 对不存在的键，expected_old=NULL 应成功插入
    EXPECT_EQ(KV_OK, kv_cas(db, "new_key", 7, NULL, 0, "new_val", 7));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "new_key", 7, &out_value, &out_len));
    EXPECT_EQ(7u, out_len);
    EXPECT_EQ(0, memcmp("new_val", out_value, 7));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 旧值为空字符串（不同于 NULL）
 */
TEST_F(KVConcurrencyTest, CAS_OldValueEmptyString) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 写入空字符串值
    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "", 0));

    // CAS 用非空值替换空字符串
    EXPECT_EQ(KV_OK, kv_cas(db, "key", 3, "", 0, "nonempty", 8));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "key", 3, &out_value, &out_len));
    EXPECT_EQ(8u, out_len);
    EXPECT_EQ(0, memcmp("nonempty", out_value, 8));
    free(out_value);

    kv_close(db);
}

/**
 * @brief CAS 连续多次成功
 */
TEST_F(KVConcurrencyTest, CAS_MultipleSuccessive) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    EXPECT_EQ(KV_OK, kv_put(db, "counter", 7, "1", 1));

    // 连续多次 CAS 递增
    EXPECT_EQ(KV_OK, kv_cas(db, "counter", 7, "1", 1, "2", 1));
    EXPECT_EQ(KV_OK, kv_cas(db, "counter", 7, "2", 1, "3", 1));
    EXPECT_EQ(KV_OK, kv_cas(db, "counter", 7, "3", 1, "4", 1));
    EXPECT_EQ(KV_OK, kv_cas(db, "counter", 7, "4", 1, "5", 1));

    void *out_value = nullptr;
    size_t out_len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "counter", 7, &out_value, &out_len));
    EXPECT_EQ(0, memcmp("5", out_value, 1));
    free(out_value);

    kv_close(db);
}

// ========================================================================
// WATCH 多场景测试
// ========================================================================

/**
 * @brief 同一键的多个 watch 回调
 */
TEST_F(KVConcurrencyTest, Watch_MultipleWatchesOnSameKey) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};

    kv_watch_t *watch1 = kv_watch(db, "key", 3,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &callback1_count);

    kv_watch_t *watch2 = kv_watch(db, "key", 3,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &callback2_count);

    ASSERT_NE(nullptr, watch1);
    ASSERT_NE(nullptr, watch2);

    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "value1", 6));

    // 两个回调都应该被触发
    EXPECT_EQ(1, callback1_count.load());
    EXPECT_EQ(1, callback2_count.load());

    kv_unwatch(db, watch1);
    kv_unwatch(db, watch2);
    kv_close(db);
}

/**
 * @brief 全局 watch 和特定键 watch 共存
 */
TEST_F(KVConcurrencyTest, Watch_GlobalAndSpecificCoexist) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    std::atomic<int> global_count{0};
    std::atomic<int> specific_count{0};

    kv_watch_t *global_watch = kv_watch(db, NULL, 0,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &global_count);

    kv_watch_t *specific_watch = kv_watch(db, "target", 6,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &specific_count);

    // 写入 target 键 - 两者都应触发
    EXPECT_EQ(KV_OK, kv_put(db, "target", 6, "v1", 2));
    EXPECT_EQ(1, global_count.load());
    EXPECT_EQ(1, specific_count.load());

    // 写入 other 键 - 只有全局触发
    EXPECT_EQ(KV_OK, kv_put(db, "other", 5, "v2", 2));
    EXPECT_EQ(2, global_count.load());
    EXPECT_EQ(1, specific_count.load());

    kv_unwatch(db, global_watch);
    kv_unwatch(db, specific_watch);
    kv_close(db);
}

/**
 * @brief 删除已存在键触发 watch
 */
TEST_F(KVConcurrencyTest, Watch_DeleteExistingKey) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    std::string caught_old_value;
    std::string caught_new_value;

    kv_watch_t *watch = kv_watch(db, "key", 3,
        [&caught_old_value, &caught_new_value](void*, const char*, size_t,
                                              const void* old_val, size_t old_len,
                                              const void* new_val, size_t new_len) {
            if (old_val && old_len > 0) caught_old_value = std::string((const char*)old_val, old_len);
            if (new_val && new_len > 0) caught_new_value = std::string((const char*)new_val, new_len);
        }, nullptr);

    ASSERT_NE(nullptr, watch);

    kv_put(db, "key", 3, "value", 5);
    EXPECT_EQ(KV_OK, kv_delete(db, "key", 3));

    // 删除操作 new_value 应为空
    EXPECT_EQ("value", caught_old_value);
    EXPECT_EQ("", caught_new_value);

    kv_unwatch(db, watch);
    kv_close(db);
}

/**
 * @brief 取消特定 watch，不影响其他的
 */
TEST_F(KVConcurrencyTest, Watch_UnwatchSpecific) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    std::atomic<int> count1{0};
    std::atomic<int> count2{0};

    kv_watch_t *watch1 = kv_watch(db, "key", 3,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &count1);

    kv_watch_t *watch2 = kv_watch(db, "key", 3,
        [](void *ud, const char*, size_t, const void*, size_t, const void*, size_t) {
            (*(std::atomic<int>*)ud)++;
        }, &count2);

    ASSERT_NE(nullptr, watch1);
    ASSERT_NE(nullptr, watch2);

    // 写入触发两个 watch
    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "v1", 2));
    EXPECT_EQ(1, count1.load());
    EXPECT_EQ(1, count2.load());

    // 取消 watch1
    kv_unwatch(db, watch1);

    // 再次写入只触发 watch2
    EXPECT_EQ(KV_OK, kv_put(db, "key", 3, "v2", 2));
    EXPECT_EQ(1, count1.load());  // 未增加
    EXPECT_EQ(2, count2.load());  // 增加

    kv_unwatch(db, watch2);
    kv_close(db);
}

/**
 * @brief CAS 操作触发 watch
 */
TEST_F(KVConcurrencyTest, Watch_CASTrigger) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    std::atomic<int> watch_count{0};

    kv_watch_t *watch = kv_watch(db, "counter", 7,
        [&watch_count](void*, const char*, size_t, const void*, size_t, const void*, size_t) {
            watch_count++;
        }, nullptr);

    ASSERT_NE(nullptr, watch);

    kv_put(db, "counter", 7, "1", 1);
    EXPECT_EQ(1, watch_count.load());

    // CAS 成功触发 watch
    EXPECT_EQ(KV_OK, kv_cas(db, "counter", 7, "1", 1, "2", 1));
    EXPECT_EQ(2, watch_count.load());

    // CAS 失败也触发 watch（因为实际发生了比较操作）
    EXPECT_EQ(KV_CONFLICT, kv_cas(db, "counter", 7, "99", 1, "3", 1));
    // 注意：CAS 冲突时不会触发 watch，因为值没变

    kv_unwatch(db, watch);
    kv_close(db);
}

// ========================================================================
// MULTI 批量操作边缘情况测试
// ========================================================================

/**
 * @brief MultiGet 包含不存在的键
 */
TEST_F(KVConcurrencyTest, MultiGet_WithNonExistentKeys) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "k1", 2, "v1", 2);
    kv_put(db, "k2", 2, "v2", 2);

    kv_multi_entry_t entries[] = {
        { (void *)"k1", 2, nullptr, 0, false },
        { (void *)"k3", 2, nullptr, 0, false },  // 不存在
        { (void *)"k2", 2, nullptr, 0, false },
    };
    EXPECT_EQ(KV_OK, kv_multi_get(db, entries, 3));

    EXPECT_EQ(true, entries[0].is_set);
    EXPECT_EQ(2u, entries[0].value_len);
    EXPECT_EQ(0, memcmp("v1", entries[0].value, 2));
    free(entries[0].value);

    EXPECT_EQ(false, entries[1].is_set);  // 不存在

    EXPECT_EQ(true, entries[2].is_set);
    EXPECT_EQ(2u, entries[2].value_len);
    EXPECT_EQ(0, memcmp("v2", entries[2].value, 2));
    free(entries[2].value);

    kv_close(db);
}

/**
 * @brief MultiSet 混合新键和已存在键
 */
TEST_F(KVConcurrencyTest, MultiSet_MixedNewAndExisting) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "existing", 8, "old_val", 7);

    kv_multi_entry_t entries[] = {
        { (void *)"existing", 8, (void *)"new_val1", 8, true },
        { (void *)"brand_new", 8, (void *)"new_val2", 8, true },
    };
    EXPECT_EQ(KV_OK, kv_multi_set(db, entries, 2));

    // 验证 existing 被更新
    void *val = nullptr;
    size_t vlen = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "existing", 8, &val, &vlen));
    EXPECT_EQ(0, memcmp("new_val1", val, 8));
    free(val);

    // 验证 brand_new 被创建
    EXPECT_EQ(KV_OK, kv_get(db, "brand_new", 8, &val, &vlen));
    EXPECT_EQ(0, memcmp("new_val2", val, 8));
    free(val);

    kv_close(db);
}

/**
 * @brief MultiDel 包含不存在的键
 */
TEST_F(KVConcurrencyTest, MultiDel_WithNonExistentKeys) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "existent1", 9, "x", 1);
    kv_put(db, "existent2", 9, "y", 1);

    kv_multi_entry_t entries[] = {
        { (void *)"existent1", 9, nullptr, 0, false },
        { (void *)"nonexistent", 10, nullptr, 0, false },
        { (void *)"existent2", 9, nullptr, 0, false },
    };
    EXPECT_EQ(KV_OK, kv_multi_del(db, entries, 3));

    // 已存在的键应该被删除
    EXPECT_FALSE(kv_exists(db, "existent1", 9));
    EXPECT_FALSE(kv_exists(db, "existent2", 9));

    kv_close(db);
}

/**
 * @brief MultiDel 空数组
 */
TEST_F(KVConcurrencyTest, MultiDel_EmptyArray) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "key", 3, "value", 5);

    kv_multi_entry_t entries[] = {};
    EXPECT_EQ(KV_OK, kv_multi_del(db, entries, 0));

    // 原键应该还在
    EXPECT_TRUE(kv_exists(db, "key", 3));

    kv_close(db);
}

/**
 * @brief MultiSet 大量键
 */
TEST_F(KVConcurrencyTest, MultiSet_LargeBatch) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const size_t count = 100;
    kv_multi_entry_t entries[100];

    for (size_t i = 0; i < count; i++) {
        char key[32];
        char value[32];
        snprintf(key, sizeof(key), "key%zu", i);
        snprintf(value, sizeof(value), "value%zu", i);
        entries[i].key = strdup(key);
        entries[i].key_len = strlen(key);
        entries[i].value = strdup(value);
        entries[i].value_len = strlen(value);
        entries[i].is_set = true;
    }

    EXPECT_EQ(KV_OK, kv_multi_set(db, entries, count));

    // 验证所有键都存在
    for (size_t i = 0; i < count; i++) {
        EXPECT_TRUE(kv_exists(db, entries[i].key, entries[i].key_len));
        free(entries[i].key);
        free(entries[i].value);
    }

    kv_close(db);
}

// ========================================================================
// 错误处理测试
// ========================================================================

/**
 * @brief 无效参数测试 - kv_put
 */
TEST(KVErrorTest, PutInvalidParams) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空键
    EXPECT_EQ(KV_INVALID, kv_put(db, NULL, 0, "value", 5));
    EXPECT_EQ(KV_INVALID, kv_put(db, "", 0, "value", 5));

    // 键过长
    char long_key[8193];
    memset(long_key, 'a', sizeof(long_key));
    long_key[sizeof(long_key) - 1] = '\0';
    EXPECT_EQ(KV_INVALID, kv_put(db, long_key, sizeof(long_key) - 1, "value", 5));

    // 空值
    EXPECT_EQ(KV_INVALID, kv_put(db, "key", 3, NULL, 0));

    kv_close(db);
}

/**
 * @brief 无效参数测试 - kv_get
 */
TEST(KVErrorTest, GetInvalidParams) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空键
    EXPECT_EQ(KV_INVALID, kv_get(db, NULL, 0, NULL, NULL));
    EXPECT_EQ(KV_INVALID, kv_get(db, "", 0, NULL, NULL));

    kv_close(db);
}

/**
 * @brief 无效参数测试 - kv_delete
 */
TEST(KVErrorTest, DeleteInvalidParams) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空键
    EXPECT_EQ(KV_INVALID, kv_delete(db, NULL, 0));
    EXPECT_EQ(KV_INVALID, kv_delete(db, "", 0));

    kv_close(db);
}

/**
 * @brief 无效参数测试 - kv_cas
 */
TEST(KVErrorTest, CASInvalidParams) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空键
    EXPECT_EQ(KV_INVALID, kv_cas(db, NULL, 0, NULL, 0, "new", 3));
    EXPECT_EQ(KV_INVALID, kv_cas(db, "", 0, NULL, 0, "new", 3));

    // 键过长
    char long_key[8193];
    memset(long_key, 'a', sizeof(long_key));
    long_key[sizeof(long_key) - 1] = '\0';
    EXPECT_EQ(KV_INVALID, kv_cas(db, long_key, sizeof(long_key) - 1, NULL, 0, "new", 3));

    // new_value 为空
    EXPECT_EQ(KV_INVALID, kv_cas(db, "key", 3, NULL, 0, NULL, 0));

    kv_close(db);
}

/**
 * @brief KV_NOT_FOUND 测试
 */
TEST(KVErrorTest, NotFound) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 获取不存在的键
    void *val = nullptr;
    size_t len = 0;
    EXPECT_EQ(KV_NOT_FOUND, kv_get(db, "nonexistent", 12, &val, &len));

    // 删除不存在的键
    EXPECT_EQ(KV_NOT_FOUND, kv_delete(db, "nonexistent", 12));

    kv_close(db);
}

/**
 * @brief 双重删除返回 NOT_FOUND
 */
TEST(KVErrorTest, DoubleDelete) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "key", 3, "value", 5);
    EXPECT_EQ(KV_OK, kv_delete(db, "key", 3));
    EXPECT_EQ(KV_NOT_FOUND, kv_delete(db, "key", 3));

    kv_close(db);
}

/**
 * @brief 空数据库上的操作
 */
TEST(KVErrorTest, EmptyDatabase) {
    std::remove(test_file);
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 扫描空数据库
    kv_iter_t *iter = kv_scan(db, NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);
    EXPECT_EQ(KV_NOT_FOUND, kv_iter_next(iter));
    kv_iter_free(iter);

    // 统计空数据库
    kv_stats_t stats;
    EXPECT_EQ(KV_OK, kv_stats(db, &stats));
    EXPECT_EQ(0u, stats.num_keys);

    kv_close(db);
}

// ========================================================================
// 统计和刷新操作测试
// ========================================================================

/**
 * @brief kv_stats 基本测试
 */
TEST_F(KVConcurrencyTest, Stats_Basic) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空数据库统计
    kv_stats_t stats;
    EXPECT_EQ(KV_OK, kv_stats(db, &stats));
    EXPECT_EQ(0u, stats.num_keys);

    // 添加一些数据
    kv_put(db, "k1", 2, "v1", 2);
    kv_put(db, "k2", 2, "v2", 2);
    kv_put(db, "k3", 2, "v3", 2);

    EXPECT_EQ(KV_OK, kv_stats(db, &stats));
    EXPECT_EQ(3u, stats.num_keys);

    kv_close(db);
}

/**
 * @brief kv_flush 测试
 */
TEST_F(KVConcurrencyTest, Flush_Basic) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "key", 3, "value", 5);

    // 刷新应该成功
    EXPECT_EQ(KV_OK, kv_flush(db));

    kv_close(db);
}

/**
 * @brief 关闭再打开后数据仍然存在
 */
TEST_F(KVConcurrencyTest, DataPersistenceAfterClose) {
    {
        kv_t *db = kv_open(test_file);
        ASSERT_NE(nullptr, db);

        kv_put(db, "persistent", 9, "data", 4);
        kv_close(db);
    }

    {
        kv_t *db = kv_open(test_file);
        ASSERT_NE(nullptr, db);

        void *val = nullptr;
        size_t len = 0;
        EXPECT_EQ(KV_OK, kv_get(db, "persistent", 9, &val, &len));
        EXPECT_EQ(4u, len);
        EXPECT_EQ(0, memcmp("data", val, 4));
        free(val);

        kv_close(db);
    }
}

/**
 * @brief 空键测试
 */
TEST_F(KVConcurrencyTest, EmptyKey) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 空键应该可以存储
    EXPECT_EQ(KV_OK, kv_put(db, "", 0, "value", 5));

    void *val = nullptr;
    size_t len = 0;
    EXPECT_EQ(KV_OK, kv_get(db, "", 0, &val, &len));
    EXPECT_EQ(5u, len);
    EXPECT_EQ(0, memcmp("value", val, 5));
    free(val);

    kv_close(db);
}

// ========================================================================
// 并发安全测试
// ========================================================================

/**
 * @brief 多线程并发写入
 */
TEST_F(KVConcurrencyTest, Concurrent_Put) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    const int num_threads = 4;
    const int ops_per_thread = 100;
    std::atomic<int> success_count{0};

    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; t++) {
        threads.emplace_back([db, t, &success_count, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; i++) {
                char key[32];
                char value[32];
                snprintf(key, sizeof(key), "thread%d_key%d", t, i);
                snprintf(value, sizeof(value), "value%d", i);
                if (kv_put(db, key, strlen(key), value, strlen(value)) == KV_OK) {
                    success_count++;
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 验证所有操作都成功
    EXPECT_EQ(num_threads * ops_per_thread, success_count.load());

    // 验证数据完整性
    kv_stats_t stats;
    kv_stats(db, &stats);
    EXPECT_EQ((size_t)(num_threads * ops_per_thread), stats.num_keys);

    kv_close(db);
}

/**
 * @brief 多线程并发读取
 */
TEST_F(KVConcurrencyTest, Concurrent_Get) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 先写入数据
    const int num_keys = 100;
    for (int i = 0; i < num_keys; i++) {
        char key[32];
        char value[32];
        snprintf(key, sizeof(key), "key%d", i);
        snprintf(value, sizeof(value), "value%d", i);
        kv_put(db, key, strlen(key), value, strlen(value));
    }

    // 多线程并发读取
    std::atomic<int> read_count{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([db, &read_count, num_keys]() {
            for (int i = 0; i < num_keys * 10; i++) {
                int key_id = i % num_keys;
                char key[32];
                snprintf(key, sizeof(key), "key%d", key_id);

                void *val = nullptr;
                size_t len = 0;
                if (kv_get(db, key, strlen(key), &val, &len) == KV_OK) {
                    read_count++;
                    free(val);
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(4 * num_keys * 10, read_count.load());

    kv_close(db);
}

/**
 * @brief 多线程并发删除
 */
TEST_F(KVConcurrencyTest, Concurrent_Delete) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 先写入数据
    const int num_keys = 50;
    for (int i = 0; i < num_keys; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        kv_put(db, key, strlen(key), "value", 5);
    }

    // 多线程并发删除
    std::atomic<int> delete_count{0};
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([db, &delete_count, num_keys]() {
            for (int i = 0; i < num_keys; i++) {
                char key[32];
                snprintf(key, sizeof(key), "key%d", i);
                if (kv_delete(db, key, strlen(key)) == KV_OK) {
                    delete_count++;
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 验证所有键都被删除
    kv_stats_t stats;
    kv_stats(db, &stats);
    EXPECT_EQ(0u, stats.num_keys);

    kv_close(db);
}

/**
 * @brief 多线程混合读写操作
 */
TEST_F(KVConcurrencyTest, Concurrent_MixedOperations) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 预写入一些数据
    for (int i = 0; i < 20; i++) {
        char key[32];
        snprintf(key, sizeof(key), "key%d", i);
        kv_put(db, key, strlen(key), "initial", 8);
    }

    std::atomic<int> op_count{0};
    std::vector<std::thread> threads;

    // 4 个线程混合读写
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([db, &op_count, t]() {
            for (int i = 0; i < 50; i++) {
                int op = i % 3;
                char key[32];
                snprintf(key, sizeof(key), "key%d", (i + t * 10) % 20);

                if (op == 0) {
                    // 写入
                    char value[32];
                    snprintf(value, sizeof(value), "thread%d_op%d", t, i);
                    kv_put(db, key, strlen(key), value, strlen(value));
                } else if (op == 1) {
                    // 读取
                    void *val = nullptr;
                    size_t len = 0;
                    kv_get(db, key, strlen(key), &val, &len);
                    if (val) free(val);
                } else {
                    // 删除
                    kv_delete(db, key, strlen(key));
                }
                op_count++;
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    EXPECT_EQ(200, op_count.load());

    kv_close(db);
}

/**
 * @brief 并发 CAS 操作
 */
TEST_F(KVConcurrencyTest, Concurrent_CAS) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    // 初始化计数器
    kv_put(db, "counter", 7, "0", 1);

    std::atomic<int> success_count{0};
    std::vector<std::thread> threads;

    // 多个线程尝试 CAS 递增
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([db, &success_count]() {
            for (int i = 0; i < 10; i++) {
                // 简单的乐观锁 CAS 循环
                for (int retry = 0; retry < 100; retry++) {
                    void *old_val = nullptr;
                    size_t old_len = 0;
                    if (kv_get(db, "counter", 7, &old_val, &old_len) != KV_OK) {
                        break;
                    }

                    int old_num = atoi((char*)old_val);
                    free(old_val);

                    char new_val[32];
                    snprintf(new_val, sizeof(new_val), "%d", old_num + 1);

                    if (kv_cas(db, "counter", 7, old_val, old_len, new_val, strlen(new_val)) == KV_OK) {
                        success_count++;
                        break;
                    }
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    // 最终值应该是 40
    void *val = nullptr;
    size_t len = 0;
    kv_get(db, "counter", 7, &val, &len);
    EXPECT_EQ(0, memcmp("40", val, 2));
    free(val);

    kv_close(db);
}

// ========================================================================
// 扫描操作测试
// ========================================================================

/**
 * @brief 扫描空数据库
 */
TEST_F(KVConcurrencyTest, Scan_Empty) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_iter_t *iter = kv_scan(db, NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);
    EXPECT_EQ(KV_NOT_FOUND, kv_iter_next(iter));
    kv_iter_free(iter);

    kv_close(db);
}

/**
 * @brief 扫描边界测试 - start_key 和 end_key
 */
TEST_F(KVConcurrencyTest, Scan_WithBoundaries) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "a", 1, "1", 1);
    kv_put(db, "b", 1, "2", 1);
    kv_put(db, "c", 1, "3", 1);
    kv_put(db, "d", 1, "4", 1);
    kv_put(db, "e", 1, "5", 1);

    // 扫描 b 到 d（不包含 d）
    kv_iter_t *iter = kv_scan(db, "b", 1, "d", 1);
    ASSERT_NE(nullptr, iter);

    int count = 0;
    while (kv_iter_next(iter) == KV_OK) {
        count++;
    }
    EXPECT_EQ(2, count);  // b 和 c
    kv_iter_free(iter);

    kv_close(db);
}

/**
 * @brief 扫描后修改数据
 */
TEST_F(KVConcurrencyTest, ScanThenModify) {
    kv_t *db = kv_open(test_file);
    ASSERT_NE(nullptr, db);

    kv_put(db, "a", 1, "1", 1);
    kv_put(db, "b", 1, "2", 1);

    kv_iter_t *iter = kv_scan(db, NULL, 0, NULL, 0);
    ASSERT_NE(nullptr, iter);

    // 扫描过程中修改数据
    kv_put(db, "c", 1, "3", 1);

    // 继续扫描应该能看到所有 3 个键
    int count = 0;
    while (kv_iter_next(iter) == KV_OK) {
        count++;
    }
    EXPECT_EQ(3, count);
    kv_iter_free(iter);

    kv_close(db);
}
