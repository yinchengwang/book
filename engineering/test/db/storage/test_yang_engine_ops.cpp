/**
 * @file test_yang_engine_ops.cpp
 * @brief yang_engine storage_ops_t 接入测试（Task 2.15）
 *
 * 验证 yang_engine 通过 storage_register_engine 注册到全局引擎表。
 */

#include <gtest/gtest.h>
#include <cstring>

extern "C" {
#include "db/storage/yang/yang_engine.h"
#include "db/storage_engine.h"
}

namespace {

class YangEngineOpsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        yang_engine_shutdown();
    }
};

TEST_F(YangEngineOpsTest, RegisterStorageOps) {
    int rc = yang_engine_init("./test_data/yang_engine_ops");
    EXPECT_EQ(rc, 0) << "yang_engine_init 应返回 0";

    const storage_ops_t *ops = storage_get_engine(MODEL_TREE);
    EXPECT_NE(ops, nullptr) << "storage_get_engine(MODEL_TREE) 应返回非 NULL";

    EXPECT_NE(ops->name, nullptr);
    EXPECT_EQ(ops->model, MODEL_TREE);

    EXPECT_NE(ops->init, nullptr);
    EXPECT_NE(ops->shutdown, nullptr);
    EXPECT_NE(ops->table_create, nullptr);
    EXPECT_NE(ops->table_open, nullptr);
    EXPECT_NE(ops->table_close, nullptr);
    EXPECT_NE(ops->tuple_insert, nullptr);
}

TEST_F(YangEngineOpsTest, GetOpsDirect) {
    const storage_ops_t *ops = yang_engine_get_ops();
    EXPECT_NE(ops, nullptr);
    EXPECT_EQ(ops->model, MODEL_TREE);
}

}  /* namespace */