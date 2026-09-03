#include "gtest/gtest.h"
#include "leetcode/leetcode_cpp.h"

// 定义测试类
class LeetCode3100To3200CPPTest : public ::testing::Test
{
protected:
    void SetUp() override {
        printf("Setting up for LeetCode3100To3200CPPTest unit test case\n");
    }

    void TearDown() override {
        printf("Tearing down after LeetCode3100To3200CPPTest unit test case\n");
    }

    static void SetUpTestCase() {
        printf("Setting up for the LeetCode3100To3200CPPTest entire test class\n");
    }

    static void TearDownTestCase() {
        printf("Tearing down after the LeetCode3100To3200CPPTest entire test class\n");
    }
};

// 测试 countDays 函数 (LeetCode 3169)
TEST_F(LeetCode3100To3200CPPTest, countDays_BasicCase) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 3}, {4, 6}, {7, 9}};
    EXPECT_EQ(solution.countDays(10, meetings), 1); // 第10天没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_NoMeetings) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {};
    EXPECT_EQ(solution.countDays(5, meetings), 5); // 所有天都没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_AllDaysCovered) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 5}};
    EXPECT_EQ(solution.countDays(5, meetings), 0); // 所有天都有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_OverlappingMeetings) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 3}, {2, 4}, {3, 5}};
    EXPECT_EQ(solution.countDays(5, meetings), 0); // 所有天都被覆盖
}

TEST_F(LeetCode3100To3200CPPTest, countDays_GapsBetweenMeetings) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 2}, {4, 5}, {7, 8}};
    EXPECT_EQ(solution.countDays(10, meetings), 4); // 天数3,6,9,10没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_SingleDayMeetings) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 1}, {3, 3}, {5, 5}};
    EXPECT_EQ(solution.countDays(7, meetings), 4); // 天数2,4,6,7没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_AdjacentMeetings) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 2}, {3, 4}, {5, 6}};
    EXPECT_EQ(solution.countDays(7, meetings), 1); // 第7天没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_LargeRange) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 100}, {200, 300}};
    EXPECT_EQ(solution.countDays(400, meetings), 199); // 天数101-199, 301-400没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_SingleMeeting) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{5, 8}};
    EXPECT_EQ(solution.countDays(10, meetings), 6); // 天数1-4,9-10没有会议
}

TEST_F(LeetCode3100To3200CPPTest, countDays_EdgeCase) {
    LeetCode_Solution solution;
    vector<vector<int>> meetings = {{1, 1}};
    EXPECT_EQ(solution.countDays(1, meetings), 0); // 只有1天，被会议覆盖

    vector<vector<int>> meetings2 = {{1, 1}};
    EXPECT_EQ(solution.countDays(2, meetings2), 1); // 第2天没有会议
}