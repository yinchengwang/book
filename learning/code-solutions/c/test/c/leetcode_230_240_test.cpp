#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode230To240CTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

// 测试 isPowerOfTwo 函数 (LeetCode 231)
TEST_F(LeetCode230To240CTest, isPowerOfTwo_Zero) {
    EXPECT_FALSE(isPowerOfTwo(0));
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_One) {
    EXPECT_TRUE(isPowerOfTwo(1));
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_PowersOfTwo) {
    EXPECT_TRUE(isPowerOfTwo(2));
    EXPECT_TRUE(isPowerOfTwo(4));
    EXPECT_TRUE(isPowerOfTwo(8));
    EXPECT_TRUE(isPowerOfTwo(16));
    EXPECT_TRUE(isPowerOfTwo(32));
    EXPECT_TRUE(isPowerOfTwo(64));
    EXPECT_TRUE(isPowerOfTwo(128));
    EXPECT_TRUE(isPowerOfTwo(256));
    EXPECT_TRUE(isPowerOfTwo(512));
    EXPECT_TRUE(isPowerOfTwo(1024));
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_NotPowersOfTwo) {
    EXPECT_FALSE(isPowerOfTwo(3));
    EXPECT_FALSE(isPowerOfTwo(5));
    EXPECT_FALSE(isPowerOfTwo(6));
    EXPECT_FALSE(isPowerOfTwo(7));
    EXPECT_FALSE(isPowerOfTwo(9));
    EXPECT_FALSE(isPowerOfTwo(10));
    EXPECT_FALSE(isPowerOfTwo(11));
    EXPECT_FALSE(isPowerOfTwo(12));
    EXPECT_FALSE(isPowerOfTwo(13));
    EXPECT_FALSE(isPowerOfTwo(14));
    EXPECT_FALSE(isPowerOfTwo(15));
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_NegativeNumbers) {
    EXPECT_FALSE(isPowerOfTwo(-1));
    EXPECT_FALSE(isPowerOfTwo(-2));
    EXPECT_FALSE(isPowerOfTwo(-4));
    EXPECT_FALSE(isPowerOfTwo(-8));
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_LargeNumbers) {
    EXPECT_TRUE(isPowerOfTwo(1073741824)); // 2^30
    EXPECT_FALSE(isPowerOfTwo(1073741823)); // 2^30 - 1
    EXPECT_FALSE(isPowerOfTwo(1073741825)); // 2^30 + 1
}

TEST_F(LeetCode230To240CTest, isPowerOfTwo_EdgeCases) {
    EXPECT_TRUE(isPowerOfTwo(INT_MAX / 2 + 1)); // 2^31 - 1 + 1 = 2^31 (if INT_MAX is 2^31-1)
    EXPECT_FALSE(isPowerOfTwo(INT_MAX)); // INT_MAX is usually 2^31 - 1
}