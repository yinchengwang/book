#include "gtest/gtest.h"
#include "leetcode/leetcode_cpp.h"

// 定义测试类
class LeetCode2000To2100CPPTest : public ::testing::Test
{
protected:
    void SetUp() override {
        printf("Setting up for LeetCode2000To2100CPPTest unit test case\n");
    }

    void TearDown() override {
        printf("Tearing down after LeetCode2000To2100CPPTest unit test case\n");
    }

    static void SetUpTestCase() {
        printf("Setting up for the LeetCode2000To2100CPPTest entire test class\n");
    }

    static void TearDownTestCase() {
        printf("Tearing down after the LeetCode2000To2100CPPTest entire test class\n");
    }
};

// 测试 maximumDifference 函数 (LeetCode 2016)
TEST_F(LeetCode2000To2100CPPTest, maximumDifference_EmptyArray) {
    LeetCode_Solution solution;
    vector<int> nums = {};
    EXPECT_EQ(solution.maximumDifference(nums), -1);
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_SingleElement) {
    LeetCode_Solution solution;
    vector<int> nums = {5};
    EXPECT_EQ(solution.maximumDifference(nums), -1);
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_TwoElements) {
    LeetCode_Solution solution;
    vector<int> nums1 = {1, 5};
    EXPECT_EQ(solution.maximumDifference(nums1), 4);

    vector<int> nums2 = {5, 1};
    EXPECT_EQ(solution.maximumDifference(nums2), -1);
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_Increasing) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 3, 4, 5};
    EXPECT_EQ(solution.maximumDifference(nums), 4); // 5-1

    vector<int> nums2 = {7, 1, 5, 4};
    EXPECT_EQ(solution.maximumDifference(nums2), 4); // 5-1
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_Decreasing) {
    LeetCode_Solution solution;
    vector<int> nums = {5, 4, 3, 2, 1};
    EXPECT_EQ(solution.maximumDifference(nums), -1);
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_WithDuplicates) {
    LeetCode_Solution solution;
    vector<int> nums = {1, 2, 2, 3, 1};
    EXPECT_EQ(solution.maximumDifference(nums), 2); // 3-1

    vector<int> nums2 = {7, 1, 5, 3, 6, 4};
    EXPECT_EQ(solution.maximumDifference(nums2), 5); // 6-1
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_NegativeNumbers) {
    LeetCode_Solution solution;
    vector<int> nums = {-3, -1, -2, 0};
    EXPECT_EQ(solution.maximumDifference(nums), 3); // 0-(-3)

    vector<int> nums2 = {-5, -4, -3, -2, -1};
    EXPECT_EQ(solution.maximumDifference(nums2), 4); // -1-(-5)
}

TEST_F(LeetCode2000To2100CPPTest, maximumDifference_LargeNumbers) {
    LeetCode_Solution solution;
    vector<int> nums = {1000000, 1, 500000, 2000000};
    EXPECT_EQ(solution.maximumDifference(nums), 1999999); // 2000000-1
}

// 测试 StockPrice 类 (LeetCode 2034)
TEST_F(LeetCode2000To2100CPPTest, StockPrice_BasicOperations) {
    StockPrice stockPrice;

    // 更新价格
    stockPrice.update(1, 10);
    EXPECT_EQ(stockPrice.current(), 10);
    EXPECT_EQ(stockPrice.maximum(), 10);
    EXPECT_EQ(stockPrice.minimum(), 10);

    // 更新同一时间戳的价格
    stockPrice.update(1, 15);
    EXPECT_EQ(stockPrice.current(), 15);
    EXPECT_EQ(stockPrice.maximum(), 15);
    EXPECT_EQ(stockPrice.minimum(), 15);

    // 添加新时间戳
    stockPrice.update(2, 5);
    EXPECT_EQ(stockPrice.current(), 5);
    EXPECT_EQ(stockPrice.maximum(), 15);
    EXPECT_EQ(stockPrice.minimum(), 5);

    stockPrice.update(3, 20);
    EXPECT_EQ(stockPrice.current(), 20);
    EXPECT_EQ(stockPrice.maximum(), 20);
    EXPECT_EQ(stockPrice.minimum(), 5);
}

TEST_F(LeetCode2000To2100CPPTest, StockPrice_OutOfOrderUpdates) {
    StockPrice stockPrice;

    // 非顺序更新时间戳
    stockPrice.update(10, 100);
    stockPrice.update(5, 50);
    stockPrice.update(15, 150);

    EXPECT_EQ(stockPrice.current(), 150); // 最新的时间戳
    EXPECT_EQ(stockPrice.maximum(), 150);
    EXPECT_EQ(stockPrice.minimum(), 50);
}

TEST_F(LeetCode2000To2100CPPTest, StockPrice_UpdateExistingTimestamp) {
    StockPrice stockPrice;

    stockPrice.update(1, 10);
    stockPrice.update(2, 20);
    stockPrice.update(3, 30);

    // 更新中间时间戳的价格
    stockPrice.update(2, 25);
    EXPECT_EQ(stockPrice.current(), 30); // 当前仍然是最新时间戳
    EXPECT_EQ(stockPrice.maximum(), 30);
    EXPECT_EQ(stockPrice.minimum(), 10);
}

TEST_F(LeetCode2000To2100CPPTest, StockPrice_AllSamePrices) {
    StockPrice stockPrice;

    stockPrice.update(1, 100);
    stockPrice.update(2, 100);
    stockPrice.update(3, 100);

    EXPECT_EQ(stockPrice.current(), 100);
    EXPECT_EQ(stockPrice.maximum(), 100);
    EXPECT_EQ(stockPrice.minimum(), 100);
}

TEST_F(LeetCode2000To2100CPPTest, StockPrice_NegativePrices) {
    StockPrice stockPrice;

    stockPrice.update(1, -10);
    stockPrice.update(2, -5);
    stockPrice.update(3, -15);

    EXPECT_EQ(stockPrice.current(), -15);
    EXPECT_EQ(stockPrice.maximum(), -5);
    EXPECT_EQ(stockPrice.minimum(), -15);
}