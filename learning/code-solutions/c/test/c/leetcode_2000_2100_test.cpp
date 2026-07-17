#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode2000To2100CTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 maxi_mum_difference 函数 (LeetCode 2016)
TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_EmptyArray) {
    EXPECT_EQ(maxi_mum_difference(NULL, 0), -1);
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_SingleElement) {
    int nums[] = {5};
    EXPECT_EQ(maxi_mum_difference(nums, 1), -1);
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_TwoElements) {
    int nums[] = {1, 5};
    EXPECT_EQ(maxi_mum_difference(nums, 2), 4);

    int nums2[] = {5, 1};
    EXPECT_EQ(maxi_mum_difference(nums2, 2), -1); // 没有递增对
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_Increasing) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(maxi_mum_difference(nums, 5), 4); // 5-1

    int nums2[] = {7, 1, 5, 4};
    EXPECT_EQ(maxi_mum_difference(nums2, 4), 4); // 5-1
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_Decreasing) {
    int nums[] = {5, 4, 3, 2, 1};
    EXPECT_EQ(maxi_mum_difference(nums, 5), -1);
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_WithDuplicates) {
    int nums[] = {1, 2, 2, 3, 1};
    EXPECT_EQ(maxi_mum_difference(nums, 5), 2); // 3-1

    int nums2[] = {7, 1, 5, 3, 6, 4};
    EXPECT_EQ(maxi_mum_difference(nums2, 6), 5); // 6-1
}

TEST_F(LeetCode2000To2100CTest, maxi_mum_difference_NegativeNumbers) {
    int nums[] = {-3, -1, -2, 0};
    EXPECT_EQ(maxi_mum_difference(nums, 4), 3); // 0-(-3)

    int nums2[] = {-5, -4, -3, -2, -1};
    EXPECT_EQ(maxi_mum_difference(nums2, 5), 4); // -1-(-5)
}

// 测试 isBalance 函数 (LeetCode 2048 辅助函数)
TEST_F(LeetCode2000To2100CTest, isBalance_SingleDigit) {
    EXPECT_TRUE(isBalance(1));
    EXPECT_FALSE(isBalance(2));
    EXPECT_FALSE(isBalance(3));
    EXPECT_FALSE(isBalance(4));
    EXPECT_FALSE(isBalance(5));
    EXPECT_FALSE(isBalance(6));
    EXPECT_FALSE(isBalance(7));
    EXPECT_FALSE(isBalance(8));
    EXPECT_FALSE(isBalance(9));
    EXPECT_FALSE(isBalance(0));
}

TEST_F(LeetCode2000To2100CTest, isBalance_TwoDigits) {
    EXPECT_FALSE(isBalance(10)); // 包含0，不是 numerically balanced number
    EXPECT_TRUE(isBalance(22)); // 2个2
    EXPECT_FALSE(isBalance(12)); // 1个1，1个2，但2的计数不等于2
    EXPECT_FALSE(isBalance(21)); // 1个2，1个1，但1的计数不等于1
}

TEST_F(LeetCode2000To2100CTest, isBalance_ThreeDigits) {
    EXPECT_FALSE(isBalance(121)); // 1出现2次，不满足 digit 1 must appear exactly once
    EXPECT_TRUE(isBalance(122)); // 1个1，2个2
    EXPECT_FALSE(isBalance(123)); // 1,1,1 但3的计数为1，不等于3
    EXPECT_FALSE(isBalance(112)); // 2个1，1个2，2的计数为1，不等于2
}

TEST_F(LeetCode2000To2100CTest, isBalance_FourDigits) {
    EXPECT_FALSE(isBalance(1221)); // 1出现2次，不满足 digit 1 must appear exactly once
    EXPECT_FALSE(isBalance(1223)); // 2个1，1个2，1个3，3的计数不等于3
}

TEST_F(LeetCode2000To2100CTest, isBalance_WithZero) {
    EXPECT_FALSE(isBalance(10)); // 包含0，不是 numerically balanced number
    EXPECT_FALSE(isBalance(120)); // 包含0，不是 numerically balanced number
    EXPECT_FALSE(isBalance(102)); // 1个1，1个0，1个2，2的计数不等于2
}

// 测试 nextBeautifulNumber 函数 (LeetCode 2048)
TEST_F(LeetCode2000To2100CTest, nextBeautifulNumber_BasicCases) {
    EXPECT_EQ(nextBeautifulNumber(0), 1);
    EXPECT_EQ(nextBeautifulNumber(1), 22); // 1之后第一个是22
    EXPECT_EQ(nextBeautifulNumber(21), 22);
    EXPECT_EQ(nextBeautifulNumber(22), 122); // 22之后是122
}

TEST_F(LeetCode2000To2100CTest, nextBeautifulNumber_SmallNumbers) {
    EXPECT_EQ(nextBeautifulNumber(2), 22);
    EXPECT_EQ(nextBeautifulNumber(10), 22);
    EXPECT_EQ(nextBeautifulNumber(20), 22);
    EXPECT_EQ(nextBeautifulNumber(21), 22);
}

TEST_F(LeetCode2000To2100CTest, nextBeautifulNumber_LargerNumbers) {
    EXPECT_EQ(nextBeautifulNumber(100), 122);
    EXPECT_EQ(nextBeautifulNumber(121), 122);
    EXPECT_EQ(nextBeautifulNumber(122), 212); // 122之后应该是212
}

TEST_F(LeetCode2000To2100CTest, nextBeautifulNumber_EdgeCases) {
    EXPECT_EQ(nextBeautifulNumber(1224443), 1224444);
    EXPECT_EQ(nextBeautifulNumber(1224444), -1); // 超过上限
}