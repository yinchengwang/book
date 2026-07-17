/**
 * @file kv_ttl_test.cpp
 * @brief KV TTL 功能测试（简化版）
 */

#include "db/storage/kv/kv_ttl.h"
#include "db/core/log.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>

class KVTTLTest : public ::testing::Test {
protected:
    void SetUp() override {
        remove("test_ttl.db");
        remove("test_ttl.db.ttl");
    }

    void TearDown() override {
        remove("test_ttl.db");
        remove("test_ttl.db.ttl");
    }
};

/**
 * @brief 测试 TTL 管理器创建和销毁
 */
TEST_F(KVTTLTest, CreateAndDestroy) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试基本 TTL 设置和获取
 */
TEST_F(KVTTLTest, BasicTTLSetAndGet) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    const char *key = "test_key";
    int64_t ttl_ms = 5000;  // 5 秒

    // 设置 TTL
    int ret = kv_ttl_set(mgr, key, strlen(key), ttl_ms);
    EXPECT_EQ(ret, 0);

    // 获取剩余 TTL
    int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
    EXPECT_GT(remaining, 0);
    EXPECT_LE(remaining, ttl_ms);

    // 检查是否过期
    EXPECT_FALSE(kv_ttl_is_expired(mgr, key, strlen(key)));

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试 TTL 过期
 */
TEST_F(KVTTLTest, TTLExpiration) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    const char *key = "expire_key";
    int64_t ttl_ms = 50;  // 50 毫秒

    kv_ttl_set(mgr, key, strlen(key), ttl_ms);

    // 立即检查应该未过期
    EXPECT_FALSE(kv_ttl_is_expired(mgr, key, strlen(key)));

    // 等待过期
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 过期后检查
    EXPECT_TRUE(kv_ttl_is_expired(mgr, key, strlen(key)));

    // 剩余时间应该 <= 0
    int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
    EXPECT_LE(remaining, 0);

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试 persist 功能
 */
TEST_F(KVTTLTest, PersistKey) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    const char *key = "persist_key";

    // 设置 TTL
    kv_ttl_set(mgr, key, strlen(key), 1000);

    // 移除 TTL（持久化）
    int ret = kv_ttl_persist(mgr, key, strlen(key));
    EXPECT_EQ(ret, 0);

    // TTL 应该变为永不过期（返回 -2）
    int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
    EXPECT_EQ(remaining, -2);

    // 即使等待也不应该过期
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(kv_ttl_is_expired(mgr, key, strlen(key)));

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试 TTL 删除
 */
TEST_F(KVTTLTest, TTLDelete) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    const char *key = "delete_key";

    kv_ttl_set(mgr, key, strlen(key), 5000);

    // 删除 TTL 条目
    int ret = kv_ttl_delete(mgr, key, strlen(key));
    EXPECT_EQ(ret, 0);

    // 再次获取应该返回 -1（不存在）
    int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
    EXPECT_EQ(remaining, -1);

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试 TTL 持久化到文件
 */
TEST_F(KVTTLTest, TTLPersistence) {
    const char *key = "persist_key";

    {
        // 第一次创建并设置 TTL
        kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
        ASSERT_NE(mgr, nullptr);

        kv_ttl_set(mgr, key, strlen(key), 10000);
        kv_ttl_mgr_destroy(mgr);
    }

    {
        // 第二次打开，TTL 应该仍然存在
        kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
        ASSERT_NE(mgr, nullptr);

        int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
        EXPECT_GT(remaining, 0);
        EXPECT_LE(remaining, 10000);

        kv_ttl_mgr_destroy(mgr);
    }
}

/**
 * @brief 测试 TTL 清理过期条目
 */
TEST_F(KVTTLTest, TTLCleanup) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    // 设置一个会过期的键
    const char *key = "cleanup_key";
    kv_ttl_set(mgr, key, strlen(key), 50);

    // 等待过期
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 清理过期条目
    size_t cleaned = kv_ttl_cleanup_expired(mgr);
    EXPECT_EQ(cleaned, 1);

    // 键应该不再存在
    int64_t remaining = kv_ttl_get_remaining(mgr, key, strlen(key));
    EXPECT_EQ(remaining, -1);

    kv_ttl_mgr_destroy(mgr);
}

/**
 * @brief 测试 TTL 统计信息
 */
TEST_F(KVTTLTest, TTLStats) {
    kv_ttl_mgr_t *mgr = kv_ttl_mgr_create("test_ttl.db");
    ASSERT_NE(mgr, nullptr);

    // 添加几个 TTL 条目
    kv_ttl_set(mgr, "key1", 4, 5000);
    kv_ttl_set(mgr, "key2", 4, 10000);
    kv_ttl_set(mgr, "key3", 4, 0);  // 永不过期

    kv_ttl_stats_t stats;
    kv_ttl_get_stats(mgr, &stats);

    EXPECT_EQ(stats.total_entries, 3);
    EXPECT_EQ(stats.persistent_keys, 1);  // key3 永不过期
    EXPECT_GT(stats.next_expire_time, 0);

    kv_ttl_mgr_destroy(mgr);
}
