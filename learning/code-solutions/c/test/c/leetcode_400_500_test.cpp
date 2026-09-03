#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode400To500CTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 thirdMax 函数 (LeetCode 414)
TEST_F(LeetCode400To500CTest, thirdMax_EmptyArray) {
    EXPECT_EQ(thirdMax(NULL, 0), INT_MIN);
}

TEST_F(LeetCode400To500CTest, thirdMax_SingleElement) {
    int nums[] = {5};
    EXPECT_EQ(thirdMax(nums, 1), 5);
}

TEST_F(LeetCode400To500CTest, thirdMax_TwoElements) {
    int nums[] = {3, 2};
    EXPECT_EQ(thirdMax(nums, 2), 3); // 返回最大值
}

TEST_F(LeetCode400To500CTest, thirdMax_ThreeElements) {
    int nums[] = {3, 2, 1};
    EXPECT_EQ(thirdMax(nums, 3), 1);

    int nums2[] = {1, 2, 3};
    EXPECT_EQ(thirdMax(nums2, 3), 1);

    int nums3[] = {1, 3, 2};
    EXPECT_EQ(thirdMax(nums3, 3), 1);
}

TEST_F(LeetCode400To500CTest, thirdMax_MoreThanThreeElements) {
    int nums[] = {3, 2, 1, 4};
    EXPECT_EQ(thirdMax(nums, 4), 2);

    int nums2[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(thirdMax(nums2, 5), 3);

    int nums3[] = {5, 5, 4, 3, 2, 1};
    EXPECT_EQ(thirdMax(nums3, 6), 3);
}

TEST_F(LeetCode400To500CTest, thirdMax_WithDuplicates) {
    int nums[] = {2, 2, 3, 1};
    EXPECT_EQ(thirdMax(nums, 4), 1);

    int nums2[] = {1, 1, 1, 1};
    EXPECT_EQ(thirdMax(nums2, 4), 1);

    int nums3[] = {3, 3, 3, 2, 2, 1};
    EXPECT_EQ(thirdMax(nums3, 6), 1);
}

TEST_F(LeetCode400To500CTest, thirdMax_NegativeNumbers) {
    int nums[] = {-1, -2, -3};
    EXPECT_EQ(thirdMax(nums, 3), -3);

    int nums2[] = {-5, -1, -3, -2};
    EXPECT_EQ(thirdMax(nums2, 4), -3);
}

TEST_F(LeetCode400To500CTest, thirdMax_LargeNumbers) {
    int nums[] = {INT_MAX, INT_MAX - 1, INT_MAX - 2};
    EXPECT_EQ(thirdMax(nums, 3), INT_MAX - 2);

    int nums2[] = {INT_MIN, INT_MIN + 1, INT_MIN + 2};
    EXPECT_EQ(thirdMax(nums2, 3), INT_MIN);
}