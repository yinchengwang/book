#include "gtest/gtest.h"

#include "leetcode/leetcode.h"

// 定义测试类
class LeetCode100UnitsTest : public ::testing::Test {
    protected:
        // 每个测试用例执行前调用
        void SetUp() override {
            // 初始化代码
            printf("Setting up for LeetCode100UnitsTest unit test case\n");
        }

        // 每个测试用例执行后调用
        void TearDown() override {
            // 清理代码
            printf("Tearing down after LeetCode100UnitsTest unit test case\n");
        }

        // 整个测试类执行前调用
        static void SetUpTestCase() {
            // 初始化代码
            printf("Setting up for the LeetCode100UnitsTest entire test class\n");
        }

        // 整个测试类执行后调用
        static void TearDownTestCase() {
            // 清理代码
            printf("Tearing down after the LeetCode100UnitsTest entire test class\n");
        }
    };


TEST_F(LeetCode100UnitsTest, 75_sortColors)
{
    int ans1[] = { 0, 0, 1, 1, 2, 2 };
    int ans2[] = { 0, 1, 2 };
    // 1.直接使用qsort函数
    int nums1[] = { 2, 0, 2, 1, 1, 0 };
    int nums2[] = { 2, 0, 1 };
    sortColors_qsort(nums1, sizeof(nums1) / sizeof(nums1[0]));
    for (uint32_t i = 0; i < sizeof(nums1) / sizeof(nums1[0]); i++) {
        EXPECT_EQ(nums1[i], ans1[i]);
    }
    sortColors_qsort(nums2, sizeof(nums2) / sizeof(nums2[0]));
    for (uint32_t i = 0; i < sizeof(nums2) / sizeof(nums2[0]); i++) {
        EXPECT_EQ(nums2[i], ans2[i]);
    }

    // 2.单指针
    int nums11[] = { 2, 0, 2, 1, 1, 0 };
    int nums22[] = { 2, 0, 1 };
    sortColors_single_pointer(nums11, sizeof(nums11) / sizeof(nums11[0]));
    for (uint32_t i = 0; i < sizeof(nums11) / sizeof(nums11[0]); i++) {
        EXPECT_EQ(nums11[i], ans1[i]);
    }
    sortColors_single_pointer(nums22, sizeof(nums22) / sizeof(nums22[0]));
    for (uint32_t i = 0; i < sizeof(nums22) / sizeof(nums22[0]); i++) {
        EXPECT_EQ(nums22[i], ans2[i]);
    }

    // 3.双指针
    int nums111[] = { 2, 0, 2, 1, 1, 0 };
    int nums222[] = { 2, 0, 1 };
    sortColors_double_pointer(nums111, sizeof(nums111) / sizeof(nums111[0]));
    for (uint32_t i = 0; i < sizeof(nums111) / sizeof(nums111[0]); i++) {
        EXPECT_EQ(nums111[i], ans1[i]);
    }
    sortColors_double_pointer(nums222, sizeof(nums222) / sizeof(nums222[0]));
    for (uint32_t i = 0; i < sizeof(nums222) / sizeof(nums222[0]); i++) {
        EXPECT_EQ(nums222[i], ans2[i]);
    }
}