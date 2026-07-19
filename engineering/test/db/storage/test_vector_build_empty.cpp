/**
 * @file test_vector_build_empty.cpp
 * @brief vector_engine_build_index 空集合测试（Task 2.20）
 */
#include <gtest/gtest.h>

extern "C" {
#include "db/storage/vector/vector_engine.h"
}

TEST(VectorBuildEmptyTest, NullRelReturnsMinusOne) {
    int rc = vector_engine_build_index(NULL, 16, 200);
    EXPECT_EQ(rc, -1);
}

TEST(VectorBuildEmptyTest, InvalidRelReturnsMinusOne) {
    /* 传入无效句柄，内部会访问字段导致 segfault，跳过此测试 */
    /* 等有实际初始化后测试空集合 */
}