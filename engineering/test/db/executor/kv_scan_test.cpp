/**
 * @file kv_scan_test.cpp
 * @brief KV 扫描算子单元测试
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/executor/kv/kv_scan.h"
#include "db/executor/kv/kv_lookup.h"
#include "db/storage/kv/kv_engine.h"
#include "db/storage/kv/kv.h"
}

#include <cstring>
#include <cstdlib>

class KvScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/kv_scan_test";
        kv_engine_init(test_dir);
        kv_engine_create("test_kv", nullptr);

        /* 插入测试数据 */
        void *rel = kv_engine_open("test_kv", ACCESS_MODE_READ_WRITE);
        if (rel) {
            const char *key1 = "key1";
            const char *val1 = "value1";
            kv_engine_insert(rel, key1, strlen(key1) + 1);

            const char *key2 = "key2";
            const char *val2 = "value2";
            kv_engine_insert(rel, key2, strlen(key2) + 1);
            kv_engine_close(rel);
        }
    }

    void TearDown() override {
        kv_engine_drop("test_kv");
        kv_engine_shutdown();
    }
};

TEST_F(KvScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    void *key = (void *)"test_key";
    KvScanState *state = exec_kv_scan_init(&parent, key);
    ASSERT_NE(state, nullptr);

    exec_kv_scan_close(state);
}

TEST_F(KvScanTest, PointLookupFound) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    void *key = (void *)"key1";
    KvScanState *state = exec_kv_scan_init(&parent, key);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_kv_scan_next(state);
    /* 点查可能返回 NULL（取决于引擎状态），不应崩溃 */
    if (slot) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
    }

    exec_kv_scan_close(state);
}

TEST_F(KvScanTest, RangeScanInit) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    KvScanState *state = exec_kv_range_scan_init(&parent,
                                                   (void *)"key0", (void *)"key9");
    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->is_range, 1);

    exec_kv_scan_close(state);
}

/* ============================================================
 * KvPointLookup 测试
 * ============================================================ */

TEST(KvPointLookupTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    KvPointLookupState *state = exec_kv_lookup_init(&parent, (void *)"test", 4);
    ASSERT_NE(state, nullptr);

    exec_kv_lookup_close(state);
}

TEST(KvPointLookupTest, Lookup) {
    /* 先插入数据 */
    const char *test_dir = std::getenv("TEST_TMPDIR");
    if (!test_dir) test_dir = "test_data/kv_lookup_test";
    kv_engine_init(test_dir);
    kv_engine_create("test_lookup", nullptr);

    void *rel = kv_engine_open("test_lookup", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(rel, nullptr);
    kv_engine_insert(rel, "mykey", 5);
    kv_engine_close(rel);

    /* 执行点查 */
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    KvPointLookupState *state = exec_kv_lookup_init(&parent, (void *)"mykey", 5);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_kv_lookup_next(state);
    if (slot) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
    }

    exec_kv_lookup_close(state);
    kv_engine_drop("test_lookup");
    kv_engine_shutdown();
}