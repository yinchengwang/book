#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include <cmath>
#include "common.h"
#include "leetcode/leetcode.h"
#include "leetcode/leetcode_cpp.h"

class LeetCode600To700CTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试 canPlaceFlowers 函数 (LeetCode 605)
TEST_F(LeetCode600To700CTest, canPlaceFlowers_EmptyFlowerbed) {
    EXPECT_TRUE(canPlaceFlowers(NULL, 0, 0));
    EXPECT_FALSE(canPlaceFlowers(NULL, 0, 1));
}

TEST_F(LeetCode600To700CTest, canPlaceFlowers_SinglePlot) {
    int flowerbed[] = {0};
    EXPECT_TRUE(canPlaceFlowers(flowerbed, 1, 0));
    EXPECT_TRUE(canPlaceFlowers(flowerbed, 1, 1));
    EXPECT_FALSE(canPlaceFlowers(flowerbed, 1, 2));
}

TEST_F(LeetCode600To700CTest, canPlaceFlowers_SinglePlotWithFlower) {
    int flowerbed[] = {1};
    EXPECT_TRUE(canPlaceFlowers(flowerbed, 1, 0));
    EXPECT_FALSE(canPlaceFlowers(flowerbed, 1, 1));
}

TEST_F(LeetCode600To700CTest, canPlaceFlowers_MultiplePlots) {
    int flowerbed1[] = {1, 0, 0, 0, 1};
    EXPECT_TRUE(canPlaceFlowers(flowerbed1, 5, 1));
    EXPECT_FALSE(canPlaceFlowers(flowerbed1, 5, 2));
    EXPECT_FALSE(canPlaceFlowers(flowerbed1, 5, 3));

    int flowerbed2[] = {0, 0, 1, 0, 0};
    EXPECT_TRUE(canPlaceFlowers(flowerbed2, 5, 2));
    EXPECT_FALSE(canPlaceFlowers(flowerbed2, 5, 3));
}

TEST_F(LeetCode600To700CTest, canPlaceFlowers_AllZeros) {
    int flowerbed[] = {0, 0, 0, 0, 0, 0};
    EXPECT_TRUE(canPlaceFlowers(flowerbed, 6, 3));
    EXPECT_FALSE(canPlaceFlowers(flowerbed, 6, 4));
}

TEST_F(LeetCode600To700CTest, canPlaceFlowers_EdgeCase) {
    int flowerbed1[] = {0, 1, 0, 1, 0, 1, 0};
    EXPECT_FALSE(canPlaceFlowers(flowerbed1, 7, 1));
    EXPECT_FALSE(canPlaceFlowers(flowerbed1, 7, 2));

    int flowerbed2[] = {1, 0, 1, 0, 1, 0, 1};
    EXPECT_TRUE(canPlaceFlowers(flowerbed2, 7, 0));
    EXPECT_FALSE(canPlaceFlowers(flowerbed2, 7, 1));
}

// 测试 maximumProduct 函数 (LeetCode 628)
TEST_F(LeetCode600To700CTest, maximumProduct_EmptyArray) {
    EXPECT_EQ(maximumProduct(NULL, 0), 0);
}

TEST_F(LeetCode600To700CTest, maximumProduct_SingleElement) {
    int nums[] = {5};
    EXPECT_EQ(maximumProduct(nums, 1), 0); // 需要至少3个元素
}

TEST_F(LeetCode600To700CTest, maximumProduct_TwoElements) {
    int nums[] = {2, 3};
    EXPECT_EQ(maximumProduct(nums, 2), 0); // 需要至少3个元素
}

TEST_F(LeetCode600To700CTest, maximumProduct_ThreeElements) {
    int nums[] = {1, 2, 3};
    EXPECT_EQ(maximumProduct(nums, 3), 6);

    int nums2[] = {-1, -2, -3};
    EXPECT_EQ(maximumProduct(nums2, 3), -6);
}

TEST_F(LeetCode600To700CTest, maximumProduct_AllPositive) {
    int nums[] = {1, 2, 3, 4};
    EXPECT_EQ(maximumProduct(nums, 4), 24); // 2*3*4

    int nums2[] = {5, 1, 3, 2, 4};
    EXPECT_EQ(maximumProduct(nums2, 5), 60); // 2*3*5
}

TEST_F(LeetCode600To700CTest, maximumProduct_WithNegatives) {
    int nums[] = {-4, -3, -2, -1};
    EXPECT_EQ(maximumProduct(nums, 4), -6); // (-1)*(-2)*(-3)

    int nums2[] = {-10, -3, 5, 6, -2};
    EXPECT_EQ(maximumProduct(nums2, 5), 180); // (-10)*(-3)*6
}

TEST_F(LeetCode600To700CTest, maximumProduct_ZeroIncluded) {
    int nums[] = {0, 2, 3, 4};
    EXPECT_EQ(maximumProduct(nums, 4), 24); // 2*3*4

    int nums2[] = {-1, 0, 2, 3};
    EXPECT_EQ(maximumProduct(nums2, 4), 0); // 包含0，最大乘积为0
}

// 测试 findMaxAverage 函数 (LeetCode 643)
TEST_F(LeetCode600To700CTest, findMaxAverage_EmptyArray) {
    EXPECT_TRUE(std::isnan(findMaxAverage(NULL, 0, 1)));
}

TEST_F(LeetCode600To700CTest, findMaxAverage_SingleElement) {
    int nums[] = {5};
    EXPECT_DOUBLE_EQ(findMaxAverage(nums, 1, 1), 5.0);
}

TEST_F(LeetCode600To700CTest, findMaxAverage_KGreaterThanSize) {
    int nums[] = {1, 2, 3};
    EXPECT_TRUE(std::isnan(findMaxAverage(nums, 3, 4)));
}

TEST_F(LeetCode600To700CTest, findMaxAverage_BasicCase) {
    int nums[] = {1, 12, -5, -6, 50, 3};
    EXPECT_DOUBLE_EQ(findMaxAverage(nums, 6, 4), 12.75); // (12-5-6+50)/4 = 51/4

    int nums2[] = {5};
    EXPECT_DOUBLE_EQ(findMaxAverage(nums2, 1, 1), 5.0);
}

TEST_F(LeetCode600To700CTest, findMaxAverage_AllNegative) {
    int nums[] = {-1, -2, -3, -4};
    EXPECT_DOUBLE_EQ(findMaxAverage(nums, 4, 2), -1.5); // (-1+-2)/2
}

TEST_F(LeetCode600To700CTest, findMaxAverage_WithZeros) {
    int nums[] = {0, 4, 0, 3, 2};
    EXPECT_DOUBLE_EQ(findMaxAverage(nums, 5, 1), 4.0);
    EXPECT_DOUBLE_EQ(findMaxAverage(nums, 5, 3), 7.0/3.0); // (4+0+3)/3
}

// 测试 findLengthOfLCIS 函数 (LeetCode 674)
TEST_F(LeetCode600To700CTest, findLengthOfLCIS_EmptyArray) {
    EXPECT_EQ(findLengthOfLCIS(NULL, 0), 0);
}

TEST_F(LeetCode600To700CTest, findLengthOfLCIS_SingleElement) {
    int nums[] = {5};
    EXPECT_EQ(findLengthOfLCIS(nums, 1), 1);
}

TEST_F(LeetCode600To700CTest, findLengthOfLCIS_AllIncreasing) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(findLengthOfLCIS(nums, 5), 5);

    int nums2[] = {1, 3, 5, 7};
    EXPECT_EQ(findLengthOfLCIS(nums2, 4), 4);
}

TEST_F(LeetCode600To700CTest, findLengthOfLCIS_AllDecreasing) {
    int nums[] = {5, 4, 3, 2, 1};
    EXPECT_EQ(findLengthOfLCIS(nums, 5), 1);
}

TEST_F(LeetCode600To700CTest, findLengthOfLCIS_Mixed) {
    int nums[] = {1, 3, 5, 4, 7};
    EXPECT_EQ(findLengthOfLCIS(nums, 5), 3); // [3,5,7] or [4,7]

    int nums2[] = {2, 2, 2, 2, 2};
    EXPECT_EQ(findLengthOfLCIS(nums2, 5), 1);

    int nums3[] = {1, 2, 2, 3, 4, 4, 5};
    EXPECT_EQ(findLengthOfLCIS(nums3, 7), 3); // [2,3,4] or [3,4,5]
}

// 测试 findShortestSubArray 函数 (LeetCode 697)
TEST_F(LeetCode600To700CTest, findShortestSubArray_EmptyArray) {
    EXPECT_EQ(findShortestSubArray(NULL, 0), 0);
}

TEST_F(LeetCode600To700CTest, findShortestSubArray_SingleElement) {
    int nums[] = {1};
    EXPECT_EQ(findShortestSubArray(nums, 1), 1);
}

TEST_F(LeetCode600To700CTest, findShortestSubArray_AllUnique) {
    int nums[] = {1, 2, 3, 4, 5};
    EXPECT_EQ(findShortestSubArray(nums, 5), 1);
}

TEST_F(LeetCode600To700CTest, findShortestSubArray_AllSame) {
    int nums[] = {1, 1, 1, 1, 1};
    EXPECT_EQ(findShortestSubArray(nums, 5), 5);
}

TEST_F(LeetCode600To700CTest, findShortestSubArray_BasicCase) {
    int nums[] = {1, 2, 2, 3, 1};
    EXPECT_EQ(findShortestSubArray(nums, 5), 2); // [1,2] or [2,3] or [3,1]

    int nums2[] = {1, 2, 2, 3, 1, 4, 2};
    EXPECT_EQ(findShortestSubArray(nums2, 7), 6); // [2,2,3,1,4,2] 包含两个2
}

TEST_F(LeetCode600To700CTest, findShortestSubArray_ComplexCase) {
    int nums[] = {1, 2, 2, 3, 1, 4, 2, 5, 2};
    EXPECT_EQ(findShortestSubArray(nums, 9), 8); // [2,2,3,1,4,2,5,2] 包含数组度数所需的四个2
}