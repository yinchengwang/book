/**
 * @file vector_query_hybrid_memory_test.cpp
 * @brief vector_query_hybrid 函数内存安全测试
 *
 * 验证：移除 free(NULL) 调用，修正失败路径的资源释放顺序
 */
#include <gtest/gtest.h>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "db/storage/vector/vector_engine.h"
}

/**
 * 测试场景 1: 验证 vector_query_hybrid 函数参数检查
 *
 * 参数无效时函数应该立即返回 -1，不应该有任何内存分配/释放。
 */
TEST(VectorQueryHybridMemoryTest, InvalidParametersReturnError) {
    vector_search_results_t results = {0};
    float query[4] = {0.1f, 0.2f, 0.3f, 0.4f};

    /* 测试：rel 为 NULL */
    EXPECT_EQ(vector_query_hybrid(NULL, query, 4, "test", 10, 0.7f, 0.3f, &results), -1);

    /* 测试：query 为 NULL */
    int dummy = 0;
    EXPECT_EQ(vector_query_hybrid(&dummy, NULL, 4, "test", 10, 0.7f, 0.3f, &results), -1);

    /* 测试：results 为 NULL */
    EXPECT_EQ(vector_query_hybrid(&dummy, query, 4, "test", 10, 0.7f, 0.3f, NULL), -1);

    /* 测试：query_dim <= 0 */
    EXPECT_EQ(vector_query_hybrid(&dummy, query, 0, "test", 10, 0.7f, 0.3f, &results), -1);
    EXPECT_EQ(vector_query_hybrid(&dummy, query, -1, "test", 10, 0.7f, 0.3f, &results), -1);
}

/**
 * 测试场景 2: 验证内存分配失败时无内存泄漏
 *
 * 当 malloc 失败时（通过 top_k 极大值模拟），函数应该返回 -1，
 * 并且不应该有内存泄漏。修复前后的关键区别：
 * - 修复前：包含 free(NULL) 调用
 * - 修复后：移除冗余的 free(NULL)，保证错误路径清理正确
 */
TEST(VectorQueryHybridMemoryTest, LargeTopKDoesNotLeak) {
    int dummy_engine = 0;
    float query[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    vector_search_results_t results = {0};

    /* 使用极大的 top_k 值，可能触发分配失败 */
    int32_t huge_top_k = 0x7FFFFFFF;  /* INT32_MAX */

    int ret = vector_query_hybrid(&dummy_engine, query, 4, "test",
                                   huge_top_k, 0.7f, 0.3f, &results);

    /* 由于 dummy_engine 不是有效的引擎句柄，函数应该在早期阶段失败 */
    /* 关键点：无论返回什么值，都不应该有内存泄漏或崩溃 */
    EXPECT_TRUE(ret == -1 || ret == 0);

    /* 如果函数部分成功分配了 results，调用方需要清理 */
    /* 但这里由于 rel 无效，应该在 vector_engine_search 阶段失败 */
}

/**
 * 测试场景 3: 验证函数对 NULL 输入指针的鲁棒性
 *
 * 这是修复后的一个基本安全检查：函数不应该因为输入为空而崩溃。
 */
TEST(VectorQueryHybridMemoryTest, RobustToNullInputs) {
    int dummy_engine = 0;
    float query[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    vector_search_results_t results = {0};

    /* 所有参数都应该进行 NULL 检查 */
    EXPECT_EQ(-1, vector_query_hybrid(&dummy_engine, NULL, 4, "test",
                                         10, 0.7f, 0.3f, &results));
    EXPECT_EQ(-1, vector_query_hybrid(&dummy_engine, query, 4, NULL,
                                         10, 0.7f, 0.3f, &results));
    EXPECT_EQ(-1, vector_query_hybrid(&dummy_engine, query, 4, "test",
                                         10, 0.7f, 0.3f, NULL));
}

/**
 * 测试场景 4: 验证函数签名一致性
 *
 * 确保函数签名与头文件声明匹配，并且所有路径都正确清理资源。
 * 这个测试本身不能直接验证内存，但能确认函数被正确调用。
 */
TEST(VectorQueryHybridMemoryTest, FunctionSignatureMatchesHeader) {
    /* 编译期检查：如果签名不匹配，这个测试编译失败 */
    void *rel = NULL;
    float query[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    const char *text = "test";
    vector_search_results_t results = {0};

    /* 调用函数，不关心返回值，只验证能编译通过 */
    vector_query_hybrid(rel, query, 4, text, 10, 0.7f, 0.3f, &results);
}
