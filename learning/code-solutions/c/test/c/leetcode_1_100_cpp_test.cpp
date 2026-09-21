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

// ---------------- 链表辅助函数（LeetCode 2 测试用） ----------------
static ListNode* makeList(initializer_list<int> vals) {
    ListNode dummy(0);
    ListNode* tail = &dummy;
    for (int v : vals) {
        tail->next = new ListNode(v);
        tail = tail->next;
    }
    return dummy.next;
}

static vector<int> toVector(ListNode* head) {
    vector<int> res;
    while (head != NULL) {
        res.push_back(head->val);
        head = head->next;
    }
    return res;
}

static void freeList(ListNode* head) {
    while (head != NULL) {
        ListNode* next = head->next;
        delete head;
        head = next;
    }
}

// 测试 addTwoNumbers 函数 (LeetCode 2)
TEST_F(LeetCode1To100CPPTest, addTwoNumbers_BasicCase) {
    LeetCode_Solution solution;
    // 342 + 465 = 807
    ListNode* l1 = makeList({2, 4, 3});
    ListNode* l2 = makeList({5, 6, 4});

    ListNode* result = solution.addTwoNumbers(l1, l2);
    EXPECT_EQ(toVector(result), vector<int>({7, 0, 8}));

    freeList(l1);
    freeList(l2);
    freeList(result);
}

TEST_F(LeetCode1To100CPPTest, addTwoNumbers_CarryChain) {
    LeetCode_Solution solution;
    // 999 + 1 = 1000，进位传播到最后产生新节点
    ListNode* l1 = makeList({9, 9, 9});
    ListNode* l2 = makeList({1});

    ListNode* result = solution.addTwoNumbers(l1, l2);
    EXPECT_EQ(toVector(result), vector<int>({0, 0, 0, 1}));

    freeList(l1);
    freeList(l2);
    freeList(result);
}

TEST_F(LeetCode1To100CPPTest, addTwoNumbers_BothZero) {
    LeetCode_Solution solution;
    ListNode* l1 = makeList({0});
    ListNode* l2 = makeList({0});

    ListNode* result = solution.addTwoNumbers(l1, l2);
    EXPECT_EQ(toVector(result), vector<int>({0}));

    freeList(l1);
    freeList(l2);
    freeList(result);
}

// 测试 lengthOfLongestSubstring 函数 (LeetCode 3)
TEST_F(LeetCode1To100CPPTest, lengthOfLongestSubstring_LeetCodeCases) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.lengthOfLongestSubstring("abcabcbb"), 3);
    EXPECT_EQ(solution.lengthOfLongestSubstring("bbbbb"), 1);
    EXPECT_EQ(solution.lengthOfLongestSubstring("pwwkew"), 3);
}

TEST_F(LeetCode1To100CPPTest, lengthOfLongestSubstring_EdgeCases) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.lengthOfLongestSubstring(""), 0);
    EXPECT_EQ(solution.lengthOfLongestSubstring("a"), 1);
    // 重复字符出现在窗口左端点之前时不应收缩窗口
    EXPECT_EQ(solution.lengthOfLongestSubstring("abba"), 2);
}

// 测试 findMedianSortedArrays 函数 (LeetCode 4)
TEST_F(LeetCode1To100CPPTest, findMedianSortedArrays_OddTotal) {
    LeetCode_Solution solution;
    vector<int> nums1 = {1, 3};
    vector<int> nums2 = {2};

    EXPECT_DOUBLE_EQ(solution.findMedianSortedArrays(nums1, nums2), 2.0);
}

TEST_F(LeetCode1To100CPPTest, findMedianSortedArrays_EvenTotal) {
    LeetCode_Solution solution;
    vector<int> nums1 = {1, 2};
    vector<int> nums2 = {3, 4};

    EXPECT_DOUBLE_EQ(solution.findMedianSortedArrays(nums1, nums2), 2.5);
}

TEST_F(LeetCode1To100CPPTest, findMedianSortedArrays_OneEmpty) {
    LeetCode_Solution solution;
    vector<int> nums1 = {};
    vector<int> nums2 = {1};

    EXPECT_DOUBLE_EQ(solution.findMedianSortedArrays(nums1, nums2), 1.0);
}

// 测试 longestPalindrome 函数 (LeetCode 5)
TEST_F(LeetCode1To100CPPTest, longestPalindrome_LeetCodeCases) {
    LeetCode_Solution solution;

    string res1 = solution.longestPalindrome("babad");
    EXPECT_TRUE(res1 == "bab" || res1 == "aba");
    EXPECT_EQ(solution.longestPalindrome("cbbd"), "bb");
}

TEST_F(LeetCode1To100CPPTest, longestPalindrome_EdgeCases) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.longestPalindrome("a"), "a");
    EXPECT_EQ(solution.longestPalindrome("ac"), string(1, solution.longestPalindrome("ac")[0]));
    EXPECT_EQ(solution.longestPalindrome("racecar"), "racecar");
}

// 测试 convert 函数 (LeetCode 6)
TEST_F(LeetCode1To100CPPTest, convert_LeetCodeCases) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.convert("PAYPALISHIRING", 3), "PAHNAPLSIIGYIR");
    EXPECT_EQ(solution.convert("PAYPALISHIRING", 4), "PINALSIGYAHRPI");
}

TEST_F(LeetCode1To100CPPTest, convert_EdgeCases) {
    LeetCode_Solution solution;

    // 单行时原样返回
    EXPECT_EQ(solution.convert("AB", 1), "AB");
    // 行数不小于串长时原样返回
    EXPECT_EQ(solution.convert("AB", 3), "AB");
}

// 测试 myAtoi 函数 (LeetCode 8)
TEST_F(LeetCode1To100CPPTest, myAtoi_LeetCodeCases) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.myAtoi("42"), 42);
    EXPECT_EQ(solution.myAtoi("   -42"), -42);
    EXPECT_EQ(solution.myAtoi("4193 with words"), 4193);
    EXPECT_EQ(solution.myAtoi("words and 987"), 0);
}

TEST_F(LeetCode1To100CPPTest, myAtoi_Overflow) {
    LeetCode_Solution solution;

    EXPECT_EQ(solution.myAtoi("2147483648"), INT_MAX);
    EXPECT_EQ(solution.myAtoi("-2147483649"), INT_MIN);
    EXPECT_EQ(solution.myAtoi("2147483647"), INT_MAX);
    EXPECT_EQ(solution.myAtoi("-2147483648"), INT_MIN);
}

// 测试 isPalindrome 函数 (LeetCode 9)
TEST_F(LeetCode1To100CPPTest, isPalindrome_LeetCodeCases) {
    LeetCode_Solution solution;

    EXPECT_TRUE(solution.isPalindrome(121));
    EXPECT_FALSE(solution.isPalindrome(-121));
    EXPECT_FALSE(solution.isPalindrome(10));
}

TEST_F(LeetCode1To100CPPTest, isPalindrome_EdgeCases) {
    LeetCode_Solution solution;

    EXPECT_TRUE(solution.isPalindrome(0));
    EXPECT_TRUE(solution.isPalindrome(1221));   // 偶数位回文
    EXPECT_TRUE(solution.isPalindrome(12321));  // 奇数位回文
    EXPECT_FALSE(solution.isPalindrome(1234));
}

// 测试 isMatch 函数 (LeetCode 10)
TEST_F(LeetCode1To100CPPTest, isMatch_LeetCodeCases) {
    LeetCode_Solution solution;

    EXPECT_FALSE(solution.isMatch("aa", "a"));
    EXPECT_TRUE(solution.isMatch("aa", "a*"));
    EXPECT_TRUE(solution.isMatch("ab", ".*"));
    EXPECT_TRUE(solution.isMatch("aab", "c*a*b"));
    EXPECT_FALSE(solution.isMatch("mississippi", "mis*is*p*."));
}

TEST_F(LeetCode1To100CPPTest, isMatch_EdgeCases) {
    LeetCode_Solution solution;

    EXPECT_TRUE(solution.isMatch("", ""));
    EXPECT_TRUE(solution.isMatch("", "a*b*c*"));  // 星号模式匹配空串
    EXPECT_FALSE(solution.isMatch("a", ""));
    EXPECT_TRUE(solution.isMatch("aaa", "a*a"));
}
