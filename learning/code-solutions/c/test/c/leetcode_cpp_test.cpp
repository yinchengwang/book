#include "gtest/gtest.h"

#include "leetcode/leetcode_cpp.h"

// 定义测试类
class LeetCodeCPPUnitsTest : public ::testing::Test {
    protected:
        // 每个测试用例执行前调用
        void SetUp() override {
            // 初始化代码
            printf("Setting up for LeetCodeCPPUnitsTest unit test case\n");
        }

        // 每个测试用例执行后调用
        void TearDown() override {
            // 清理代码
            printf("Tearing down after LeetCodeCPPUnitsTest unit test case\n");
        }

        // 整个测试类执行前调用
        static void SetUpTestCase() {
            // 初始化代码
            printf("Setting up for the LeetCodeCPPUnitsTest entire test class\n");
        }

        // 整个测试类执行后调用
        static void TearDownTestCase() {
            // 清理代码
            printf("Tearing down after the LeetCodeCPPUnitsTest entire test class\n");
        }
    };


TEST_F(LeetCodeCPPUnitsTest, 2131_longestPalindrome)
{
    LeetCode_Solution solution = LeetCode_Solution();

    vector<string> words_1 = { "lc", "cl", "gg" };
    int cnt = solution.longestPalindrome(words_1);
    EXPECT_EQ(cnt, 6);

    vector<string> words_2 = { "ab", "ty", "yt", "lc", "cl", "ab" };
    cnt = solution.longestPalindrome(words_2);
    EXPECT_EQ(cnt, 8);

    vector<string> words_3 = { "cc", "ll", "xx" };
    cnt = solution.longestPalindrome(words_3);
    EXPECT_EQ(cnt, 2);
}