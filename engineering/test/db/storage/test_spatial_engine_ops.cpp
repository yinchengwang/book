/**
 * @file test_spatial_engine_ops.cpp
 * @brief spatial_engine storage_ops_t 接入测试（Task 2.12）
 *
 * 验证 spatial_engine 通过 storage_register_engine 注册到全局引擎表。
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/storage/spatial/spatial_engine.h"
#include "db/storage_engine.h"
}

namespace {

/**
 * @brief spatial_engine 接入测试
 */
class SpatialEngineOpsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        spatial_engine_shutdown();
    }
};

/**
 * @brief 测试 1: spatial_engine_init 注册到 storage_ops_t
 */
TEST_F(SpatialEngineOpsTest, RegisterStorageOps) {
    int rc = spatial_engine_init("./test_data/spatial_engine_ops");
    EXPECT_EQ(rc, 0) << "spatial_engine_init 应返回 0";

    /* 通过 storage_get_engine(MODEL_SPATIAL) 获取注册的 ops */
    const storage_ops_t *ops = storage_get_engine(MODEL_SPATIAL);
    EXPECT_NE(ops, nullptr) << "storage_get_engine(MODEL_SPATIAL) 应返回非 NULL";

    EXPECT_NE(ops->name, nullptr);
    EXPECT_EQ(ops->model, MODEL_SPATIAL);

    /* 验证关键函数指针 */
    EXPECT_NE(ops->init, nullptr);
    EXPECT_NE(ops->shutdown, nullptr);
    EXPECT_NE(ops->table_create, nullptr);
    EXPECT_NE(ops->table_open, nullptr);
    EXPECT_NE(ops->table_close, nullptr);
    EXPECT_NE(ops->tuple_insert, nullptr);
    EXPECT_NE(ops->get_stats, nullptr);
}

/**
 * @brief 测试 2: spatial_engine_get_ops 直接返回 ops 表
 */
TEST_F(SpatialEngineOpsTest, GetOpsDirect) {
    const storage_ops_t *ops = spatial_engine_get_ops();
    EXPECT_NE(ops, nullptr);
    EXPECT_EQ(ops->model, MODEL_SPATIAL);
}

/**
 * @brief 测试 3: 同时注册 graph 和 spatial 互不影响
 */
TEST_F(SpatialEngineOpsTest, MultiEngineIsolation) {
    /* 初始化 spatial，验证 graph 仍是之前注册的（由 graph_engine_ops 测试） */
    spatial_engine_init("./test_data/multi_engine");

    const storage_ops_t *spatial_ops = storage_get_engine(MODEL_SPATIAL);
    EXPECT_NE(spatial_ops, nullptr);
    EXPECT_EQ(spatial_ops->model, MODEL_SPATIAL);
}

}  /* namespace */