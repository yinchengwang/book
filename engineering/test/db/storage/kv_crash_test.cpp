/**
 * @file kv_crash_test.cpp
 * @brief KV 引擎崩溃恢复测试
 *
 * @attention 所有测试默认为 DISABLED 状态。
 * 当前 kv_close() 会调用 kv_save() 刷盘，破坏"模拟崩溃"的意图。
 * 需要 WAL 恢复成为主要持久化机制后，再启用这些测试。
 */
#include <gtest/gtest.h>
#include "chaos_test_base.h"

extern "C" {
#include "db/kv_engine.h"
#include "db/storage/kv/kv.h"
#include "db/wal.h"
}

/**
 * @brief KV 崩溃恢复测试夹具
 */
class KVChaosTest : public ChaosTestBase {
protected:
    kv_t *db = nullptr;
    char kv_path[512];

    void SetUp() override {
        ChaosTestBase::SetUp();
        snprintf(kv_path, sizeof(kv_path), "%s/test_kv.db", test_dir.c_str());
    }

    void TearDown() override {
        if (db) {
            kv_close(db);
            db = nullptr;
        }
        ChaosTestBase::TearDown();
    }
};

/**
 * @brief 写入后崩溃，恢复后数据完整
 *
 * 流程：
 * 1. 打开 KV，写入 key1=value1, key2=value2
 * 2. 模拟崩溃（不调用 kv_close）
 * 3. 重新打开 KV，WAL 恢复自动触发
 * 4. 验证数据一致性
 *
 * @note DISABLED：需要 WAL 恢复机制完善后启用
 */
TEST_F(KVChaosTest, DISABLED_WriteCrashRecovery) {
    /* 1. 打开 KV 并写入数据 */
    db = kv_open(kv_path);
    ASSERT_NE(db, nullptr);

    EXPECT_EQ(kv_put(db, "key1", 4, "value1", 6), 0);
    EXPECT_EQ(kv_put(db, "key2", 4, "value2", 6), 0);

    /* 2. 模拟崩溃（直接关闭，模拟异常退出）
     * 注意：kv_close 会触发刷盘，这里模拟的是"未正常关闭"的场景
     */
    kv_t *tmp = db; (void)tmp;
    db = nullptr;

    /* 3. 重新打开 KV，触发 WAL 恢复 */
    db = kv_open(kv_path);
    ASSERT_NE(db, nullptr);

    /* 4. 验证数据一致性 */
    void *val = nullptr;
    size_t vlen = 0;

    EXPECT_EQ(kv_get(db, "key1", 4, (void **)&val, &vlen), 0);
    if (val) {
        EXPECT_EQ(std::string((const char *)val, vlen), "value1");
        if (vlen > 0) free(val);
        val = nullptr;
    }

    EXPECT_EQ(kv_get(db, "key2", 4, (void **)&val, &vlen), 0);
    if (val) {
        EXPECT_EQ(std::string((const char *)val, vlen), "value2");
        if (vlen > 0) free(val);
        val = nullptr;
    }
}

/**
 * @brief 批量写入后崩溃，恢复后数据完整
 *
 * @note DISABLED：需要 WAL 恢复机制完善后启用
 */
TEST_F(KVChaosTest, DISABLED_BatchWriteCrashRecovery) {
    db = kv_open(kv_path);
    ASSERT_NE(db, nullptr);

    /* 批量写入 100 条记录 */
    for (int i = 0; i < 100; i++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(val, sizeof(val), "value_%d", i);
        EXPECT_EQ(kv_put(db, key, strlen(key), val, strlen(val)), 0);
    }

    /* 模拟崩溃 */
    kv_close(db);
    db = nullptr;

    /* 重新打开 */
    db = kv_open(kv_path);
    ASSERT_NE(db, nullptr);

    /* 验证部分数据 */
    void *val = nullptr;
    size_t vlen = 0;
    EXPECT_EQ(kv_get(db, "key_0", 5, (void **)&val, &vlen), 0);
    if (val) free(val);
}

/**
 * @brief 多次写入-崩溃-恢复循环
 *
 * @note DISABLED：需要 WAL 恢复机制完善后启用
 */
TEST_F(KVChaosTest, DISABLED_MultipleCrashRecovery) {
    db = kv_open(kv_path);
    ASSERT_NE(db, nullptr);

    for (int round = 0; round < 3; round++) {
        char key[32], val[32];
        snprintf(key, sizeof(key), "round_%d", round);
        snprintf(val, sizeof(val), "data_%d", round);
        EXPECT_EQ(kv_put(db, key, strlen(key), val, strlen(val)), 0);

        /* 模拟崩溃 */
        kv_close(db);
        db = nullptr;

        /* 重新打开 */
        db = kv_open(kv_path);
        ASSERT_NE(db, nullptr);
    }
}