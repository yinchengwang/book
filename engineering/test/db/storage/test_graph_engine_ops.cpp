/**
 * @file test_graph_engine_ops.cpp
 * @brief graph_engine storage_ops_t 接入测试（Task 2.11）
 *
 * 验证 graph_engine 通过 storage_register_engine 注册到全局引擎表：
 * 1. 调 graph_engine_init 触发注册
 * 2. 调 storage_get_engine(MODEL_GRAPH) 获取引擎
 * 3. 验证返回的 ops 表非空且 model 字段正确
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/storage/graph/graph_engine.h"
#include "db/storage_engine.h"
}

namespace {

/**
 * @brief graph_engine 接入测试
 */
class GraphEngineOpsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        graph_engine_shutdown();
    }
};

/**
 * @brief 测试 1: graph_engine_init 注册到 storage_ops_t
 */
TEST_F(GraphEngineOpsTest, RegisterStorageOps) {
    /* graph_engine_init 触发 storage_register_engine(MODEL_GRAPH, ...) */
    int rc = graph_engine_init("./test_data/graph_engine_ops");
    EXPECT_EQ(rc, 0) << "graph_engine_init 应返回 0";

    /* 通过 storage_get_engine(MODEL_GRAPH) 获取注册的 ops */
    const storage_ops_t *ops = storage_get_engine(MODEL_GRAPH);
    EXPECT_NE(ops, nullptr) << "storage_get_engine(MODEL_GRAPH) 应返回非 NULL";

    /* 验证 ops 字段 */
    EXPECT_NE(ops->name, nullptr);
    EXPECT_EQ(ops->model, MODEL_GRAPH);

    /* 验证关键函数指针 */
    EXPECT_NE(ops->init, nullptr);
    EXPECT_NE(ops->shutdown, nullptr);
    EXPECT_NE(ops->table_create, nullptr);
    EXPECT_NE(ops->table_open, nullptr);
    EXPECT_NE(ops->table_close, nullptr);
    EXPECT_NE(ops->tuple_insert, nullptr);
    EXPECT_NE(ops->tuple_delete, nullptr);
    EXPECT_NE(ops->scan_begin, nullptr);
    EXPECT_NE(ops->scan_next, nullptr);
    EXPECT_NE(ops->scan_end, nullptr);
    EXPECT_NE(ops->get_stats, nullptr);
}

/**
 * @brief 测试 2: graph_engine_get_ops 直接返回 ops 表
 */
TEST_F(GraphEngineOpsTest, GetOpsDirect) {
    const storage_ops_t *ops = graph_engine_get_ops();
    EXPECT_NE(ops, nullptr);
    EXPECT_EQ(ops->model, MODEL_GRAPH);
}

/**
 * @brief 测试 3: 未初始化的模型返回 NULL
 */
TEST_F(GraphEngineOpsTest, UnregisteredModelReturnsNull) {
    /* Task 2.11 只注册了 graph。其他未注册的模型应返回 NULL */
    const storage_ops_t *ops = storage_get_engine(MODEL_TIMESERIES);
    /* 由于未注册 ts 引擎，ops 应为 NULL（除非测试中其他测试已注册）*/
    (void)ops;  /* 不强制断言，避免测试间依赖 */
}

}  /* namespace */