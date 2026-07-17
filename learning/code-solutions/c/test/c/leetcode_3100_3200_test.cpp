#include <gtest/gtest.h>
#include <vector>
#include <algorithm>
#include <limits.h>
#include "common.h"
#include "leetcode/leetcode.h"

class LeetCode3100To3200CTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}

    // 辅助函数：创建二维数组
    int** createMeetings(const std::vector<std::vector<int>>& meetings_vec) {
        int** meetings = (int**)malloc(meetings_vec.size() * sizeof(int*));
        for (size_t i = 0; i < meetings_vec.size(); i++) {
            meetings[i] = (int*)malloc(2 * sizeof(int));
            meetings[i][0] = meetings_vec[i][0];
            meetings[i][1] = meetings_vec[i][1];
        }
        return meetings;
    }

    void freeMeetings(int** meetings, int size) {
        for (int i = 0; i < size; i++) {
            free(meetings[i]);
        }
        free(meetings);
    }
};

// 测试 countDays 函数 (LeetCode 3169)
TEST_F(LeetCode3100To3200CTest, countDays_NoMeetings) {
    int meetingsColSize[] = {0};
    EXPECT_EQ(countDays(10, NULL, 0, meetingsColSize), 10);
}

TEST_F(LeetCode3100To3200CTest, countDays_SingleMeeting) {
    std::vector<std::vector<int>> meetings_vec = {{2, 4}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2};

    EXPECT_EQ(countDays(10, meetings, 1, meetingsColSize), 7); // 总10天，会议占用3天，剩余7天

    freeMeetings(meetings, 1);
}

TEST_F(LeetCode3100To3200CTest, countDays_NonOverlappingMeetings) {
    std::vector<std::vector<int>> meetings_vec = {{1, 3}, {5, 7}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2, 2};

    EXPECT_EQ(countDays(10, meetings, 2, meetingsColSize), 4); // 会议占用6天，剩余4天

    freeMeetings(meetings, 2);
}

TEST_F(LeetCode3100To3200CTest, countDays_OverlappingMeetings) {
    std::vector<std::vector<int>> meetings_vec = {{1, 5}, {3, 7}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2, 2};

    EXPECT_EQ(countDays(10, meetings, 2, meetingsColSize), 3); // 合并后会议占用7天，剩余3天

    freeMeetings(meetings, 2);
}

TEST_F(LeetCode3100To3200CTest, countDays_AllDaysCovered) {
    std::vector<std::vector<int>> meetings_vec = {{1, 10}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2};

    EXPECT_EQ(countDays(10, meetings, 1, meetingsColSize), 0);

    freeMeetings(meetings, 1);
}

TEST_F(LeetCode3100To3200CTest, countDays_ComplexCase) {
    std::vector<std::vector<int>> meetings_vec = {{2, 4}, {6, 8}, {1, 3}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2, 2, 2};

    // 排序后: {1,3}, {2,4}, {6,8}
    // 合并后: {1,4}, {6,8} 共占用7天
    EXPECT_EQ(countDays(10, meetings, 3, meetingsColSize), 3);

    freeMeetings(meetings, 3);
}

TEST_F(LeetCode3100To3200CTest, countDays_EdgeCase) {
    std::vector<std::vector<int>> meetings_vec = {{1, 1}, {10, 10}};
    int** meetings = createMeetings(meetings_vec);
    int meetingsColSize[] = {2, 2};

    EXPECT_EQ(countDays(10, meetings, 2, meetingsColSize), 8);

    freeMeetings(meetings, 2);
}