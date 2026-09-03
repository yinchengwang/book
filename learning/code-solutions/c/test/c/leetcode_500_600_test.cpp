#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode500To600CTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 findUnsortedSubarray 函数 (LeetCode 581)
TEST_F(LeetCode500To600CTest, findUnsortedSubarray_EmptyArray) {
    EXPECT_EQ(findUnsortedSubarray(NULL, 0), 0);
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_SingleElement) {
    int nums[] = {1};
    EXPECT_EQ(findUnsortedSubarray(nums, 1), 0);
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_TwoElements) {
    int nums[] = {1, 2};
    EXPECT_EQ(findUnsortedSubarray(nums, 2), 0);

    int nums2[] = {2, 1};
    EXPECT_EQ(findUnsortedSubarray(nums2, 2), 2);
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_AlreadySorted) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(findUnsortedSubarray(nums, 5), 0);

    int nums2[] = {1, 1, 1, 1, 1};
    EXPECT_EQ(findUnsortedSubarray(nums2, 5), 0);
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_ReverseSorted) {
    int nums[] = {5, 4, 3, 2, 1};
    EXPECT_EQ(findUnsortedSubarray(nums, 5), 5);
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_MiddleUnsorted) {
    int nums[] = {2, 6, 4, 8, 10, 9, 15};
    EXPECT_EQ(findUnsortedSubarray(nums, 7), 5); // [6,4,8,10,9]

    int nums2[] = {1, 3, 2, 4, 5};
    EXPECT_EQ(findUnsortedSubarray(nums2, 5), 2); // [3,2]
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_StartUnsorted) {
    int nums[] = {3, 2, 1, 4, 5};
    EXPECT_EQ(findUnsortedSubarray(nums, 5), 3); // [3,2,1]

    int nums2[] = {2, 1, 3, 4, 5};
    EXPECT_EQ(findUnsortedSubarray(nums2, 5), 2); // [2,1]
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_EndUnsorted) {
    int nums[] = {1, 2, 3, 5, 4};
    EXPECT_EQ(findUnsortedSubarray(nums, 5), 2); // [5,4]

    int nums2[] = {1, 2, 5, 3, 4};
    EXPECT_EQ(findUnsortedSubarray(nums2, 5), 3); // [5,3,4]
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_ComplexCase) {
    int nums[] = {1, 2, 4, 5, 3, 6, 7};
    EXPECT_EQ(findUnsortedSubarray(nums, 7), 3); // [4,5,3]

    int nums2[] = {1, 3, 5, 2, 4, 6, 7};
    EXPECT_EQ(findUnsortedSubarray(nums2, 7), 4); // [3,5,2,4]
}

TEST_F(LeetCode500To600CTest, findUnsortedSubarray_WithDuplicates) {
    int nums[] = {1, 2, 3, 2, 4, 5};
    EXPECT_EQ(findUnsortedSubarray(nums, 6), 2); // [3,2]

    int nums2[] = {2, 3, 3, 2, 4};
    EXPECT_EQ(findUnsortedSubarray(nums2, 5), 3); // [3,3,2]
}