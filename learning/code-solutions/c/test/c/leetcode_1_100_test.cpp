#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include "common.h"
#include "leetcode/leetcode.h"

// 测试辅助函数
bool isSortedColors(const int* nums, int numsSize) {
    for (int i = 0; i < numsSize - 1; i++) {
        if (nums[i] > nums[i + 1]) return false;
    }
    return true;
}

bool hasOnly012(const int* nums, int numsSize) {
    for (int i = 0; i < numsSize; i++) {
        if (nums[i] < 0 || nums[i] > 2) return false;
    }
    return true;
}

class LeetCode1To100CTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 sortColors_qsort 函数
TEST_F(LeetCode1To100CTest, sortColors_qsort_EmptyArray) {
    sortColors_qsort(NULL, 0);
    EXPECT_TRUE(isSortedColors(NULL, 0));
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_SingleElement) {
    int nums[] = {1};
    int expected[] = {1};

    sortColors_qsort(nums, 1);

    EXPECT_TRUE(isSortedColors(nums, 1));
    EXPECT_EQ(nums[0], expected[0]);
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_AllZeros) {
    int nums[] = {0, 0, 0, 0};
    int expected[] = {0, 0, 0, 0};

    sortColors_qsort(nums, 4);

    EXPECT_TRUE(isSortedColors(nums, 4));
    for (int i = 0; i < 4; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_AllOnes) {
    int nums[] = {1, 1, 1};
    int expected[] = {1, 1, 1};

    sortColors_qsort(nums, 3);

    EXPECT_TRUE(isSortedColors(nums, 3));
    for (int i = 0; i < 3; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_AllTwos) {
    int nums[] = {2, 2, 2, 2, 2};
    int expected[] = {2, 2, 2, 2, 2};

    sortColors_qsort(nums, 5);

    EXPECT_TRUE(isSortedColors(nums, 5));
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_MixedColors) {
    int nums[] = {2, 0, 2, 1, 1, 0};
    int expected[] = {0, 0, 1, 1, 2, 2};

    sortColors_qsort(nums, 6);

    EXPECT_TRUE(isSortedColors(nums, 6));
    EXPECT_TRUE(hasOnly012(nums, 6));
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_qsort_RandomOrder) {
    int nums[] = {1, 2, 0, 1, 2, 0, 1, 0, 2};
    int expected[] = {0, 0, 0, 1, 1, 1, 2, 2, 2};

    sortColors_qsort(nums, 9);

    EXPECT_TRUE(isSortedColors(nums, 9));
    EXPECT_TRUE(hasOnly012(nums, 9));
    for (int i = 0; i < 9; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

// 测试 sortColors_single_pointer 函数
TEST_F(LeetCode1To100CTest, sortColors_single_pointer_EmptyArray) {
    sortColors_single_pointer(NULL, 0);
    EXPECT_TRUE(isSortedColors(NULL, 0));
}

TEST_F(LeetCode1To100CTest, sortColors_single_pointer_MixedColors) {
    int nums[] = {2, 0, 2, 1, 1, 0};
    int expected[] = {0, 0, 1, 1, 2, 2};

    sortColors_single_pointer(nums, 6);

    EXPECT_TRUE(isSortedColors(nums, 6));
    EXPECT_TRUE(hasOnly012(nums, 6));
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_single_pointer_AllZerosAndOnes) {
    int nums[] = {0, 1, 0, 1, 0, 1};
    int expected[] = {0, 0, 0, 1, 1, 1};

    sortColors_single_pointer(nums, 6);

    EXPECT_TRUE(isSortedColors(nums, 6));
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

// 测试 sortColors_double_pointer 函数
TEST_F(LeetCode1To100CTest, sortColors_double_pointer_EmptyArray) {
    sortColors_double_pointer(NULL, 0);
    EXPECT_TRUE(isSortedColors(NULL, 0));
}

TEST_F(LeetCode1To100CTest, sortColors_double_pointer_MixedColors) {
    int nums[] = {2, 0, 2, 1, 1, 0};
    int expected[] = {0, 0, 1, 1, 2, 2};

    sortColors_double_pointer(nums, 6);

    EXPECT_TRUE(isSortedColors(nums, 6));
    EXPECT_TRUE(hasOnly012(nums, 6));
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

TEST_F(LeetCode1To100CTest, sortColors_double_pointer_ComplexCase) {
    int nums[] = {2, 0, 1, 2, 0, 1, 2, 0, 1, 0};
    int expected[] = {0, 0, 0, 0, 1, 1, 1, 2, 2, 2};

    sortColors_double_pointer(nums, 10);

    EXPECT_TRUE(isSortedColors(nums, 10));
    EXPECT_TRUE(hasOnly012(nums, 10));
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(nums[i], expected[i]);
    }
}

// 比较三种算法的结果一致性
TEST_F(LeetCode1To100CTest, sortColors_AlgorithmsConsistency) {
    int original[] = {2, 0, 2, 1, 1, 0, 2, 0, 1};
    int nums1[9], nums2[9], nums3[9];

    memcpy(nums1, original, sizeof(original));
    memcpy(nums2, original, sizeof(original));
    memcpy(nums3, original, sizeof(original));

    sortColors_qsort(nums1, 9);
    sortColors_single_pointer(nums2, 9);
    sortColors_double_pointer(nums3, 9);

    for (int i = 0; i < 9; i++) {
        EXPECT_EQ(nums1[i], nums2[i]);
        EXPECT_EQ(nums2[i], nums3[i]);
        EXPECT_EQ(nums1[i], nums3[i]);
    }
}