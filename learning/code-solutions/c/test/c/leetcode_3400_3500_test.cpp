#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode3400To3500CTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 sumOfGoodNumbers 函数 (LeetCode 3452)
TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_EmptyArray) {
    EXPECT_EQ(sumOfGoodNumbers(NULL, 0, 1), 0);
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_SingleElement) {
    int nums[] = {5};
    EXPECT_EQ(sumOfGoodNumbers(nums, 1, 1), 5); // 没有左右邻居，认为是good
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_TwoElements) {
    int nums[] = {1, 2};
    EXPECT_EQ(sumOfGoodNumbers(nums, 2, 1), 2); // 只有2严格大于距离1的存在元素

    int nums2[] = {2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 2, 1), 2); // 只有2严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_KEquals1) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(sumOfGoodNumbers(nums, 5, 1), 5); // 只有5严格大于距离1的存在元素

    int nums2[] = {5, 4, 3, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 5, 1), 5); // 只有5严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_KEquals2) {
    int nums[] = {1, 2, 3, 4, 5, 6};
    EXPECT_EQ(sumOfGoodNumbers(nums, 6, 2), 5 + 6); // 5和6严格大于距离2的存在元素

    int nums2[] = {6, 5, 4, 3, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 6, 2), 6 + 5); // 6和5严格大于距离2的存在元素
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_AllGood) {
    int nums[] = {5, 1, 5, 1, 5};
    EXPECT_EQ(sumOfGoodNumbers(nums, 5, 1), 5 + 5 + 5); // 三个5严格大于距离1的存在元素

    int nums2[] = {1, 5, 1, 5, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 5, 1), 5 + 5); // 两个5严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_AllBad) {
    int nums[] = {1, 2, 2, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums, 5, 1), 0); // 没有元素比左右邻居都大

    int nums2[] = {3, 3, 3, 3, 3};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 5, 1), 0); // 所有元素都等于邻居
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_WithDuplicates) {
    int nums[] = {1, 2, 2, 3, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums, 6, 1), 3); // 只有3比左右邻居都大

    int nums2[] = {2, 1, 2, 1, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 6, 1), 2 + 2 + 2); // 三个2都比邻居大
}

TEST_F(LeetCode3400To3500CTest, sumOfGoodNumbers_LargeK) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(sumOfGoodNumbers(nums, 5, 3), 3 + 4 + 5); // 3、4、5严格大于距离3的存在元素

    int nums2[] = {5, 4, 3, 2, 1};
    EXPECT_EQ(sumOfGoodNumbers(nums2, 5, 3), 5 + 4 + 3); // 5、4、3严格大于距离3的存在元素
}