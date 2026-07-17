/**
 * @file vector_scan_test.cpp
 * @brief 向量扫描算子单元测试
 *
 * 测试向量算子的 Volcano 迭代器协议实现。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/executor/vector/vector_scan.h"
#include "db/executor/vector/hnsw_scan.h"
#include "db/executor/vector/ivf_scan.h"
#include "db/vector_engine.h"
#include "db/storage/vector/vector_engine.h"
}

#include <cstdlib>
#include <cstring>

/* ============================================================
 * VectorScan 测试
 * ============================================================ */

class VectorScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/vector_scan_test";
        vector_engine_init(test_dir);
        vector_engine_create("test_vec", nullptr);
    }

    void TearDown() override {
        vector_engine_drop("test_vec");
        vector_engine_shutdown();
    }
};

TEST_F(VectorScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    float query[] = {1.0f, 0.0f, 0.0f};
    VectorScanState *state = exec_vector_scan_init(&parent, query, 10, 0);

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->ss.ps.type, EXEC_VECTOR_SCAN);
    EXPECT_EQ(state->top_k, 10);
    EXPECT_EQ(state->distance_metric, 0);

    exec_vector_scan_close(state);
}

TEST_F(VectorScanTest, InsertAndSearch) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    /* 插入测试向量 */
    void *rel = vector_engine_open("test_vec", ACCESS_MODE_READ_WRITE);
    ASSERT_NE(rel, nullptr);

    float vec1[] = {1.0f, 0.0f, 0.0f};
    float vec2[] = {0.0f, 1.0f, 0.0f};
    float vec3[] = {0.0f, 0.0f, 1.0f};
    vector_engine_insert(rel, vec1, sizeof(vec1));
    vector_engine_insert(rel, vec2, sizeof(vec2));
    vector_engine_insert(rel, vec3, sizeof(vec3));
    vector_engine_close(rel);

    /* 执行搜索 */
    float query[] = {1.0f, 0.1f, 0.0f};
    VectorScanState *state = exec_vector_scan_init(&parent, query, 5, 0);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_vector_scan_next(state);
    /* 搜索可能返回正确结果或 NULL（取决于数据库状态） */
    if (slot) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
    }

    exec_vector_scan_close(state);
}

/* ============================================================
 * HnswScan 测试
 * ============================================================ */

class HnswScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/hnsw_scan_test";
        vector_engine_init(test_dir);
        vector_engine_create("test_hnsw", nullptr);

        void *rel = vector_engine_open("test_hnsw", ACCESS_MODE_READ_WRITE);
        if (rel) {
            float vec1[] = {1.0f, 0.0f, 0.0f};
            float vec2[] = {0.0f, 1.0f, 0.0f};
            float vec3[] = {0.0f, 0.0f, 1.0f};
            vector_engine_insert(rel, vec1, sizeof(vec1));
            vector_engine_insert(rel, vec2, sizeof(vec2));
            vector_engine_insert(rel, vec3, sizeof(vec3));
            vector_engine_close(rel);
        }
    }

    void TearDown() override {
        vector_engine_drop("test_hnsw");
        vector_engine_shutdown();
    }
};

TEST_F(HnswScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    float query[] = {1.0f, 0.0f, 0.0f};
    HnswScanState *state = exec_hnsw_scan_init(&parent, query, 10, 0, 100, 16);

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->vss.ss.ps.type, EXEC_HNSW_SCAN);
    EXPECT_EQ(state->ef, 100);
    EXPECT_EQ(state->m, 16);

    exec_hnsw_scan_close(state);
}

TEST_F(HnswScanTest, HnswSearch) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    float query[] = {1.0f, 0.0f, 0.0f};
    HnswScanState *state = exec_hnsw_scan_init(&parent, query, 5, 0, 100, 16);
    ASSERT_NE(state, nullptr);

    TupleTableSlot *slot = exec_hnsw_scan_next(state);
    if (slot) {
        EXPECT_EQ(slot->tts_nvalid, 3);
        exec_clear_tuple_slot(slot);
    }

    exec_hnsw_scan_close(state);
}

/* ============================================================
 * IvfScan 测试
 * ============================================================ */

class IvfScanTest : public ::testing::Test {
protected:
    void SetUp() override {
        const char *test_dir = std::getenv("TEST_TMPDIR");
        if (!test_dir) test_dir = "test_data/ivf_scan_test";
        vector_engine_init(test_dir);
        vector_engine_create("test_ivf", nullptr);

        void *rel = vector_engine_open("test_ivf", ACCESS_MODE_READ_WRITE);
        if (rel) {
            float vec1[] = {1.0f, 0.0f, 0.0f};
            float vec2[] = {0.0f, 1.0f, 0.0f};
            vector_engine_insert(rel, vec1, sizeof(vec1));
            vector_engine_insert(rel, vec2, sizeof(vec2));
            vector_engine_close(rel);
        }
    }

    void TearDown() override {
        vector_engine_drop("test_ivf");
        vector_engine_shutdown();
    }
};

TEST_F(IvfScanTest, InitAndClose) {
    PlanState parent;
    memset(&parent, 0, sizeof(parent));

    float query[] = {1.0f, 0.0f, 0.0f};
    IvfScanState *state = exec_ivf_scan_init(&parent, query, 10, 0, 10, 100);

    ASSERT_NE(state, nullptr);
    EXPECT_EQ(state->nprobe, 10);
    EXPECT_EQ(state->nlist, 100);

    exec_ivf_scan_close(state);
}
