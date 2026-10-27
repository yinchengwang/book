/**
 * @file engine_register_isolation_test.cpp
 * @brief 单模态构建只注册已启用引擎的隔离测试
 *
 * Task 2 注册表反转的行为钉子：
 *  - 全量构建下注册数 == profile 声明的模态数（MMDB_EXPECTED_ENGINE_COUNT
 *    由 CMake 按 MMDB_ENABLE_* 就地计算注入，不依赖 Task 5 的 profile 机制）；
 *  - 10 号保留空槽不是模型：storage_model_name 对它返回 "unknown"。
 *
 * 若未来某个模态的注册函数漏接生成器、或生成器多调了未启用模态，
 * 注册数就会偏离注入值，本测试立即失败。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/engine_registry.h"
#include "db/storage_engine.h"
}

TEST(EngineRegisterIsolation, OnlyEnabledModalitiesRegistered) {
    /* storage_get_engine 只读全局注册表，需要先触发注册。
     * mm_register_builtin_engines 幂等，重复调用安全。 */
    engine_registry_init();

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != NULL) {
            ++n;
        }
    }
    /* 全量构建下应等于 profile 声明的模态数；kv profile 下应为 1。
     * 期望值由 CMake 通过 MMDB_EXPECTED_ENGINE_COUNT 注入。 */
    EXPECT_EQ(n, MMDB_EXPECTED_ENGINE_COUNT);
}

TEST(EngineRegisterIsolation, EmptySlotIsNotAModel) {
    /* 10 号槽是保留空槽：storage_model_name 对它返回 "unknown"，
     * 而不是 NULL —— 该函数对越界返回 "unknown"，对空槽同样返回 "unknown"。 */
    EXPECT_STREQ(storage_model_name((DataModel)10), "unknown");
    /* 空槽上不应有任何引擎被注册 */
    EXPECT_EQ(storage_get_engine((DataModel)10), nullptr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
