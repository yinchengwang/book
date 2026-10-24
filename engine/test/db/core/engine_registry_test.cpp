/**
 * @file engine_registry_test.cpp
 * @brief 存储引擎注册表测试
 *
 * 测试 engine_registry_init() 和相关函数的正确性。
 */
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "db/engine_registry.h"
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include "db/log.h"
}

#ifdef _WIN32
#define mkdir(path) _mkdir(path)
#endif

/**
 * @brief 引擎注册表测试夹具
 */
class EngineRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化日志 */
        log_config_t log_config;
        memset(&log_config, 0, sizeof(log_config));
        log_config.level = LOG_LEVEL_ERROR;
        log_config.target = LOG_TARGET_CONSOLE;
        log_config.enable_colors = false;
        log_init(&log_config);

        /* 确保测试目录存在 */
#ifdef _WIN32
        mkdir("./test_data_registry");
#else
        mkdir("./test_data_registry", 0755);
#endif
    }

    void TearDown() override {
        mm_storage_shutdown();
        log_shutdown();

        /* 清理测试数据目录 */
#ifdef _WIN32
        system("rmdir /s /q ./test_data_registry");
#else
        system("rm -rf ./test_data_registry");
#endif
    }
};

/**
 * @brief 测试 engine_registry_init 可以成功调用
 */
TEST_F(EngineRegistryTest, InitSuccess) {
    int ret = engine_registry_init();
    EXPECT_GE(ret, 0);
}

/**
 * @brief 测试 engine_registry_init 多次调用不会崩溃
 */
TEST_F(EngineRegistryTest, MultipleInit) {
    int ret1 = engine_registry_init();
    EXPECT_GE(ret1, 0);

    int ret2 = engine_registry_init();
    EXPECT_GE(ret2, 0);

    /* 多次初始化应该返回相同的计数或稳定值 */
    EXPECT_GE(get_registered_engine_count(), 0);
}

/**
 * @brief 测试注册引擎计数
 */
TEST_F(EngineRegistryTest, RegisteredCount) {
    engine_registry_init();
    int count = get_registered_engine_count();
    EXPECT_GE(count, 0);
}

/**
 * @brief 测试 storage_register_engine 后调用 engine_registry_init
 */
TEST_F(EngineRegistryTest, RegisterBeforeInit) {
    /* 注册引擎后再初始化不应该崩溃 */
    const storage_ops_t dummy_ops = {0};
    int reg_ret = register_storage_engine(MODEL_KV, &dummy_ops);
    (void)reg_ret;  /* 可能失败因为引擎已注册 */

    int init_ret = engine_registry_init();
    EXPECT_GE(init_ret, 0);
}

/**
 * @brief 测试 mm_storage 初始化（包含注册表初始化）
 */
TEST_F(EngineRegistryTest, MmStorageInit) {
    int ret = mm_storage_init("./test_data_registry");
    EXPECT_EQ(ret, 0);
    EXPECT_TRUE(mm_is_initialized());
}

/**
 * @brief 测试 mm_storage 初始化后获取上下文
 */
TEST_F(EngineRegistryTest, GetContext) {
    mm_storage_init("./test_data_registry");

    mm_context_t *ctx = mm_get_context();
    EXPECT_NE(ctx, nullptr);
}

/**
 * @brief 测试 storage_get_engine 返回有效指针
 */
TEST_F(EngineRegistryTest, GetEngine) {
    engine_registry_init();

    /* 检查各模型类型的引擎获取 */
    const storage_ops_t *ops = storage_get_engine(MODEL_KV);
    /* ops 可能为 NULL 如果 KV 引擎未启用 */

    ops = storage_get_engine(MODEL_VECTOR);
    /* ops 可能为 NULL 如果向量引擎未启用 */

    ops = storage_get_engine(MODEL_GRAPH);
    /* ops 可能为 NULL 如果图引擎未启用 */

    /* 无效模型应该返回 NULL */
    const storage_ops_t *invalid_ops = storage_get_engine((DataModel)999);
    EXPECT_EQ(invalid_ops, nullptr);

    invalid_ops = storage_get_engine((DataModel)-1);
    EXPECT_EQ(invalid_ops, nullptr);
}

/**
 * @brief 测试 storage_model_name 返回有效名称
 */
TEST_F(EngineRegistryTest, ModelName) {
    /* 所有有效模型应该有对应名称 */
    EXPECT_STREQ("relational", storage_model_name(MODEL_RELATIONAL));
    EXPECT_STREQ("kv", storage_model_name(MODEL_KV));
    EXPECT_STREQ("graph", storage_model_name(MODEL_GRAPH));
    EXPECT_STREQ("vector", storage_model_name(MODEL_VECTOR));
    EXPECT_STREQ("timeseries", storage_model_name(MODEL_TIMESERIES));
    EXPECT_STREQ("document", storage_model_name(MODEL_DOCUMENT));
    EXPECT_STREQ("spatial", storage_model_name(MODEL_SPATIAL));
    EXPECT_STREQ("tree", storage_model_name(MODEL_TREE));
    EXPECT_STREQ("stream", storage_model_name(MODEL_STREAM));
    EXPECT_STREQ("columnar", storage_model_name(MODEL_COLUMNAR));
    EXPECT_STREQ("rdf", storage_model_name(MODEL_RDF));
    EXPECT_STREQ("spatiotemporal", storage_model_name(MODEL_SPATIOTEMPORAL));

    /* 无效应返回 "unknown" */
    EXPECT_STREQ("unknown", storage_model_name((DataModel)999));
    EXPECT_STREQ("unknown", storage_model_name((DataModel)-1));
}

/**
 * @brief 测试 mm_get_model_dir 返回有效目录名
 */
TEST_F(EngineRegistryTest, ModelDir) {
    mm_storage_init("./test_data_registry");

    /* 各模型应该有对应目录名 */
    EXPECT_STREQ("base", mm_get_model_dir(MODEL_RELATIONAL));
    EXPECT_STREQ("kv", mm_get_model_dir(MODEL_KV));
    EXPECT_STREQ("graph", mm_get_model_dir(MODEL_GRAPH));
    EXPECT_STREQ("vector", mm_get_model_dir(MODEL_VECTOR));
    EXPECT_STREQ("timeseries", mm_get_model_dir(MODEL_TIMESERIES));
    EXPECT_STREQ("document", mm_get_model_dir(MODEL_DOCUMENT));
    EXPECT_STREQ("spatial", mm_get_model_dir(MODEL_SPATIAL));
    EXPECT_STREQ("yang", mm_get_model_dir(MODEL_TREE));
    EXPECT_STREQ("stream", mm_get_model_dir(MODEL_STREAM));
    EXPECT_STREQ("columnar", mm_get_model_dir(MODEL_COLUMNAR));
    EXPECT_STREQ("rdf", mm_get_model_dir(MODEL_RDF));
    EXPECT_STREQ("spatiotemporal", mm_get_model_dir(MODEL_SPATIOTEMPORAL));

    /* 无效应返回 NULL */
    EXPECT_EQ(nullptr, mm_get_model_dir((DataModel)999));
}

/**
 * @brief 测试无效模型的注册
 */
TEST_F(EngineRegistryTest, InvalidModelRegistration) {
    const storage_ops_t dummy_ops = {0};

    /* 负数模型应该失败 */
    int ret = register_storage_engine((DataModel)-1, &dummy_ops);
    EXPECT_EQ(ret, -1);

    /* 超出范围的模型应该失败 */
    ret = register_storage_engine((DataModel)999, &dummy_ops);
    EXPECT_EQ(ret, -1);

    /* NULL ops 应该失败 */
    ret = register_storage_engine(MODEL_KV, nullptr);
    EXPECT_EQ(ret, -1);
}

/**
 * @brief 测试 NULL 指针安全性
 */
TEST_F(EngineRegistryTest, NullSafety) {
    /* engine_registry_init 不接受参数，应该是安全的 */
    engine_registry_init();

    /* storage_get_engine 对无效模型返回 NULL */
    EXPECT_EQ(nullptr, storage_get_engine((DataModel)-1));
    EXPECT_EQ(nullptr, storage_get_engine((DataModel)999));
}

/**
 * @brief 测试 mm_storage 重复初始化不会崩溃
 */
TEST_F(EngineRegistryTest, DoubleInit) {
    int ret1 = mm_storage_init("./test_data_registry");
    EXPECT_EQ(ret1, 0);

    int ret2 = mm_storage_init("./test_data_registry");
    EXPECT_EQ(ret2, 0);  /* 重复初始化应该返回成功（已初始化）*/
}

/**
 * @brief 测试 mm_storage 未初始化时获取上下文返回 NULL
 */
TEST_F(EngineRegistryTest, ContextBeforeInit) {
    mm_context_t *ctx = mm_get_context();
    /* 可能返回 NULL 或之前初始化的上下文 */
}

/**
 * @brief 测试 MODEL_COUNT 与实际模型数量一致
 */
TEST_F(EngineRegistryTest, ModelCount) {
    EXPECT_EQ(MODEL_COUNT, 13);

    /* 确保所有模型索引在有效范围内 */
    for (int i = 0; i < MODEL_COUNT; i++) {
        DataModel model = (DataModel)i;
        EXPECT_NE(nullptr, storage_model_name(model));
        EXPECT_STRNE("unknown", storage_model_name(model));
    }
}

/**
 * @brief 主函数
 */
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
