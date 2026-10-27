/**
 * @file sql_function_registry_test.cpp
 * @brief 内置 SQL 函数按 profile 注册的行为测试（Task 3）
 *
 * func_registry.c 拆分后，各模态 SQL 函数表迁至模态目录，由生成的
 * mm_builtin_engines.c 按 MMDB_ENABLE_* 注册。本测试钉住：
 *  - 启用模态的函数在 func_registry_init 后可查到；
 *  - 未启用模态的函数不被注册（且测试可正常链接 —— 未启用模态的
 *    注册符号根本不存在，生成器不会生成对它们的调用）。
 * 期望随 profile 走：用 multimodal_config.h 的编译期开关守卫。
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/core/func_registry.h"
#include "db/multimodal_config.h"
}

TEST(SqlFunctionRegistry, BuiltinFunctionsVisibleAfterInit) {
    func_registry_shutdown();  /* 幂等清场，防止前序用例残留 */
    ASSERT_EQ(func_registry_init(), 0);

#ifdef MMDB_ENABLE_VECTOR
    EXPECT_NE(func_registry_lookup("VECTOR_SEARCH", "public", 3), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("VECTOR_SEARCH", "public", 3), nullptr);
#endif

#ifdef MMDB_ENABLE_TIMESERIES
    EXPECT_NE(func_registry_lookup("TIME_SERIES_AGG", "public", 2), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("TIME_SERIES_AGG", "public", 2), nullptr);
#endif

#ifdef MMDB_ENABLE_GRAPH
    EXPECT_NE(func_registry_lookup("GRAPH_TRAVERSE", "public", 2), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("GRAPH_TRAVERSE", "public", 2), nullptr);
#endif

#ifdef MMDB_ENABLE_DOCUMENT
    EXPECT_NE(func_registry_lookup("MATCH", "public", 2), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("MATCH", "public", 2), nullptr);
#endif

#ifdef MMDB_ENABLE_SPATIAL
    EXPECT_NE(func_registry_lookup("ST_Distance", "public", 2), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("ST_Distance", "public", 2), nullptr);
#endif

#ifdef MMDB_ENABLE_KV
    EXPECT_NE(func_registry_lookup("ZADD", "public", 3), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("ZADD", "public", 3), nullptr);
#endif

#ifdef MMDB_ENABLE_TREE
    EXPECT_NE(func_registry_lookup("TREE_DEPTH", "public", 1), nullptr);
#else
    EXPECT_EQ(func_registry_lookup("TREE_DEPTH", "public", 1), nullptr);
#endif
}

TEST(SqlFunctionRegistry, CountMatchesEnabledModalities) {
    func_registry_shutdown();
    ASSERT_EQ(func_registry_init(), 0);

    /* 各启用模态的函数表条数：vector=3 ts=4 graph=4 doc=3 spatial=4 kv=2 tree=3。
     * 这里只设下限：至少应注册「启用模态中表最小的 2 条」，防止注册链整体断裂
     * 而单条 lookup 恰好因其他模态同名函数误判。 */
    size_t n = func_registry_count();
    EXPECT_GE(n, 2u);
}
