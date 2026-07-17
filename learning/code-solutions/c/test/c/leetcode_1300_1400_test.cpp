#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <string>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode1300To1400CTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
    }
    void TearDown() override
    {
    }
};

// 测试 maximum69Number 函数 (LeetCode 1323)
TEST_F(LeetCode1300To1400CTest, maximum69Number_SingleDigit) {
    EXPECT_EQ(maximum69Number(6), 9);
    EXPECT_EQ(maximum69Number(9), 9);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_TwoDigits) {
    EXPECT_EQ(maximum69Number(66), 96);
    EXPECT_EQ(maximum69Number(69), 99);
    EXPECT_EQ(maximum69Number(96), 99);
    EXPECT_EQ(maximum69Number(99), 99);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_ThreeDigits) {
    EXPECT_EQ(maximum69Number(666), 966);
    EXPECT_EQ(maximum69Number(969), 999);
    EXPECT_EQ(maximum69Number(996), 999);
    EXPECT_EQ(maximum69Number(999), 999);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_FourDigits) {
    EXPECT_EQ(maximum69Number(6699), 9699);
    EXPECT_EQ(maximum69Number(6969), 9969);
    EXPECT_EQ(maximum69Number(9696), 9996);
    EXPECT_EQ(maximum69Number(9966), 9996);
    EXPECT_EQ(maximum69Number(9999), 9999);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_LeftmostSix) {
    EXPECT_EQ(maximum69Number(6999), 9999);
    EXPECT_EQ(maximum69Number(6969), 9969);
    EXPECT_EQ(maximum69Number(6699), 9699);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_AllSixes) {
    EXPECT_EQ(maximum69Number(6666), 9666);
}

TEST_F(LeetCode1300To1400CTest, maximum69Number_EdgeCases) {
    EXPECT_EQ(maximum69Number(0), 0); // 虽然题目可能不考虑0，但测试一下
    EXPECT_EQ(maximum69Number(9996), 9999);
    EXPECT_EQ(maximum69Number(6996), 9996);
}