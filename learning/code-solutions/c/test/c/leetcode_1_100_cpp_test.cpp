#include "gtest/gtest.h"
#include "leetcode/leetcode_cpp.h"

// 定义测试类
class LeetCode1To100CPPTest : public ::testing::Test {
protected:
    void SetUp() override {
        printf("Setting up for LeetCode1To100CPPTest unit test case\n");
    }

    void TearDown() override {
        printf("Tearing down after LeetCode1To100CPPTest unit test case\n");
    }

    static void SetUpTestCase() {
        printf("Setting up for the LeetCode1To100CPPTest entire test class\n");
    }

    static void TearDownTestCase() {
        printf("Tearing down after the LeetCode1To100CPPTest entire test class\n");
    }
};

// 测试 twoSumEnum 函数 (LeetCode 1 - 枚举法)
TEST_F(LeetCode1To100CPPTest, twoSumEnum_BasicCase) {
    LeetCode_Solution solution;
    vector<int> nums = {2, 7, 11, 15};
    int target = 9;
    vector<int> result = solution.twoSumEnum(nums, target);

    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE((result[0] == 0 && result[1] == 1) || (result[0] == 1 && result[1] == 0));
}

TEST_F(LeetCode1To100CPPTest, twoSumEnum_NoSolution) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3, 4};
    int target = 10;
    vector<int> result = solution.twoSumEnum(nums, target);

    EXPECT_TRUE(result.empty());
}

TEST_F(LeetCode1To100CPPTest, twoSumEnum_Duplicates) {
    LeetCode_Solution solution;
    vector<int> nums = {3, 3};
    int target = 6;
    vector<int> result = solution.twoSumEnum(nums, target);

    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE((result[0] == 0 && result[1] == 1) || (result[0] == 1 && result[1] == 0));
}

TEST_F(LeetCode1To100CPPTest, twoSumEnum_NegativeNumbers) {
    LeetCode_Solution solution;
    vector<int> nums = {-1, -2, -3, -4, -5};
    int target = -8;
    vector<int> result = solution.twoSumEnum(nums, target);

    EXPECT_EQ(result.size(), 2);
    // 应该返回 -3 和 -5 的索引
    EXPECT_TRUE((nums[result[0]] + nums[result[1]] == target));
}

// 测试 twoSumHash 函数 (LeetCode 1 - 哈希表法)
TEST_F(LeetCode1To100CPPTest, twoSumHash_BasicCase) {
    LeetCode_Solution solution;
    vector<int> nums = {2, 7, 11, 15};
    int target = 9;
    vector<int> result = solution.twoSumHash(nums, target);

    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE((result[0] == 0 && result[1] == 1) || (result[0] == 1 && result[1] == 0));
}

TEST_F(LeetCode1To100CPPTest, twoSumHash_NoSolution) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3, 4};
    int target = 10;
    vector<int> result = solution.twoSumHash(nums, target);

    EXPECT_TRUE(result.empty());
}

TEST_F(LeetCode1To100CPPTest, twoSumHash_Duplicates) {
    LeetCode_Solution solution;
    vector<int> nums = {3, 3};
    int target = 6;
    vector<int> result = solution.twoSumHash(nums, target);

    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE((result[0] == 0 && result[1] == 1) || (result[0] == 1 && result[1] == 0));
}

TEST_F(LeetCode1To100CPPTest, twoSumHash_LargeArray) {
    LeetCode_Solution solution;
    vector<int> nums(1000);
    for (int i = 0; i < 1000; i++) {
        nums[i] = i;
    }
    // target=1997: 998+999=1997，两个不同索引且都在范围内
    int target = 1997;
    vector<int> result = solution.twoSumHash(nums, target);

    EXPECT_EQ(result.size(), 2);
    EXPECT_TRUE((nums[result[0]] + nums[result[1]] == target));
}

// 测试 reverse 函数 (LeetCode 7)
TEST_F(LeetCode1To100CPPTest, reverse_PositiveNumber) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.reverse(123), 321);
    EXPECT_EQ(solution.reverse(120), 21);
    EXPECT_EQ(solution.reverse(1), 1);
}

TEST_F(LeetCode1To100CPPTest, reverse_NegativeNumber) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.reverse(-123), -321);
    EXPECT_EQ(solution.reverse(-120), -21);
    EXPECT_EQ(solution.reverse(-1), -1);
}

TEST_F(LeetCode1To100CPPTest, reverse_Zero) {
    LeetCode_Solution solution;
    EXPECT_EQ(solution.reverse(0), 0);
}

TEST_F(LeetCode1To100CPPTest, reverse_Overflow) {
    LeetCode_Solution solution;

    // INT_MAX = 2147483647, 反转后会溢出
    EXPECT_EQ(solution.reverse(2147483647), 0);
    // INT_MIN = -2147483648, 反转后会溢出
    EXPECT_EQ(solution.reverse(-2147483648), 0);
}

TEST_F(LeetCode1To100CPPTest, reverse_LeadingZeros) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.reverse(100), 1);
    EXPECT_EQ(solution.reverse(-100), -1);
}

// 比较两种twoSum算法的结果一致性
TEST_F(LeetCode1To100CPPTest, twoSum_AlgorithmsConsistency) {
    LeetCode_Solution solution;
    vector<int> nums = {2, 7, 11, 15, 3, 6};
    int target = 9;

    vector<int> result1 = solution.twoSumEnum(nums, target);
    vector<int> result2 = solution.twoSumHash(nums, target);

    // 结果应该包含相同的元素（可能顺序不同）
    sort(result1.begin(), result1.end());
    sort(result2.begin(), result2.end());
    EXPECT_EQ(result1, result2);
}
