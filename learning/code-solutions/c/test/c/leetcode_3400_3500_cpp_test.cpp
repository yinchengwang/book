#include "gtest/gtest.h"
#include "leetcode/leetcode_cpp.h"

// 定义测试类
class LeetCode3400To3500CPPTest : public ::testing::Test
{
protected:
    void SetUp() override {
        printf("Setting up for LeetCode3400To3500CPPTest unit test case\n");
    }

    void TearDown() override {
        printf("Tearing down after LeetCode3400To3500CPPTest unit test case\n");
    }

    static void SetUpTestCase() {
        printf("Setting up for the LeetCode3400To3500CPPTest entire test class\n");
    }

    static void TearDownTestCase() {
        printf("Tearing down after the LeetCode3400To3500CPPTest entire test class\n");
    }
};

// 测试 sumOfGoodNumbers 函数 (LeetCode 3452)
TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_EmptyArray) {
    LeetCode_Solution solution;
    vector<int> nums = {};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), 0);
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_SingleElement) {
    LeetCode_Solution solution;
    vector<int> nums = {5};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), 5); // 没有邻居，总是好的
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_TwoElements) {
    LeetCode_Solution solution;
    vector<int> nums1 = {1, 2};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums1, 1), 2); // 只有2严格大于距离1的存在元素

    vector<int> nums2 = {2, 1};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums2, 1), 2); // 只有2严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_BasicCase) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 3, 2, 1, 5, 1};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 2), 8); // 3和5严格大于距离2的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_AllGood) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3, 4, 5};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), 5); // 只有5严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_AllBad) {
    LeetCode_Solution solution;
    vector<int> nums = {5, 4, 3, 2, 1};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), 5); // 只有5严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_WithDuplicates) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 2, 3, 1};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), 3); // 只有3严格大于距离1的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_LargeK) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3, 4, 5};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 3), 12); // 3、4、5严格大于距离3的存在元素
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_NegativeNumbers) {
    LeetCode_Solution solution;
    vector<int> nums = {-1, -2, -3, 0, -1};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 1), -1); // -1和0是good，和为-1
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_ZeroK) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 0), 0); // k=0时每个元素都需要严格大于自身，不可能满足
}

TEST_F(LeetCode3400To3500CPPTest, sumOfGoodNumbers_ComplexCase) {
    LeetCode_Solution solution;
    vector<int> nums = {10, 5, 15, 8, 20, 12, 25};
    EXPECT_EQ(solution.sumOfGoodNumbers(nums, 2), 37); // 12和25严格大于距离2的存在元素
}