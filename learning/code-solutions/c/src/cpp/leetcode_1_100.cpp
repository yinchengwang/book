#include "leetcode/leetcode_cpp.h"

// leetcode 1 枚举法
vector<int> LeetCode_Solution::twoSumEnum(vector<int>& nums, int target) {
    vector<int> res;
    for (int i = 0; i < nums.size(); i++) {
        for (int j = i + 1; j < nums.size(); j++) {
            if (nums[i] + nums[j] == target) {
                res.push_back(i);
                res.push_back(j);
                return res;
            }
        }
    }

    return res;
}

// leetcode 1 hash map
vector<int> LeetCode_Solution::twoSumHash(vector<int>& nums, int target)
{
    unordered_map<int, int> hash_map;

    int len = nums.size();
    for (int i = 0; i < len; i++) {
        int val = nums[i];
        int pre = target - val;
        if (hash_map.count(pre)) {
            return {i, hash_map[pre]};
        }

        hash_map[val] = i;
    }

    return {};
}

// leetcode 7
int LeetCode_Solution::reverse(int x) {
    int res = 0;
    while (x) {
        if (res < INT_MIN / 10 || res > INT_MAX / 10) {
            return 0;
        }

        int v = x % 10;
        res = res * 10 + v;
        x /= 10;
    }

    return res;
}

// leetcode 8
int LeetCode_Solution::myAtoi(string s) {
    int sign = 1;
    int i = 0;
    long result = 0;


    // 去除空格
    while (i < s.size() && s[i] == ' ') {
        i++;
    }

    // 判断符号
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) {
        sign = s[i] == '-' ? -1 : 1;
        i++;
    }

    // 读取数字
    while (i < s.size() && isdigit(s[i])) {
        int digit = s[i] - '0';
        // 检查是否会溢出
        if (result > INT_MAX / 10 || (result == INT_MAX / 10 && digit > INT_MAX % 10)) {
            return (sign == 1) ? INT_MAX : INT_MIN;
        }
        result = result * 10 + digit;
        i++;
    }

    return sign * result;
}

// leetcode 2
ListNode* LeetCode_Solution::addTwoNumbers(ListNode* l1, ListNode* l2) {
    ListNode dummy(0);
    ListNode* tail = &dummy;
    int carry = 0;

    while (l1 != NULL || l2 != NULL || carry != 0) {
        int sum = carry;
        if (l1 != NULL) {
            sum += l1->val;
            l1 = l1->next;
        }
        if (l2 != NULL) {
            sum += l2->val;
            l2 = l2->next;
        }

        tail->next = new ListNode(sum % 10);
        carry = sum / 10;
        tail = tail->next;
    }

    return dummy.next;
}

// leetcode 3 滑动窗口 + 最近出现位置表
int LeetCode_Solution::lengthOfLongestSubstring(string s) {
    int last[256];
    fill(begin(last), end(last), -1);

    int best = 0;
    int start = 0;  // 当前无重复窗口的左端点
    for (int i = 0; i < (int)s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (last[c] >= start) {
            start = last[c] + 1;
        }
        last[c] = i;
        best = max(best, i - start + 1);
    }

    return best;
}

// leetcode 4 在较短数组上二分划分
double LeetCode_Solution::findMedianSortedArrays(vector<int>& nums1, vector<int>& nums2) {
    // 保证 nums1 是较短的数组
    if (nums1.size() > nums2.size()) {
        return findMedianSortedArrays(nums2, nums1);
    }

    int m = (int)nums1.size();
    int n = (int)nums2.size();
    int lo = 0, hi = m;
    while (lo <= hi) {
        int i = (lo + hi) / 2;          // nums1 左半部分元素个数
        int j = (m + n + 1) / 2 - i;    // nums2 左半部分元素个数

        int l1 = (i == 0) ? INT_MIN : nums1[i - 1];
        int r1 = (i == m) ? INT_MAX : nums1[i];
        int l2 = (j == 0) ? INT_MIN : nums2[j - 1];
        int r2 = (j == n) ? INT_MAX : nums2[j];

        if (l1 <= r2 && l2 <= r1) {
            if ((m + n) % 2 == 1) {
                return (double)max(l1, l2);
            }
            return (max(l1, l2) + min(r1, r2)) / 2.0;
        } else if (l1 > r2) {
            hi = i - 1;
        } else {
            lo = i + 1;
        }
    }

    return 0.0;
}

// leetcode 5 中心扩展
string LeetCode_Solution::longestPalindrome(string s) {
    if (s.empty()) {
        return "";
    }

    int bestStart = 0;
    int bestLen = 1;
    for (int i = 0; i < (int)s.size(); i++) {
        // 奇数长度：以 s[i] 为中心
        int l = i, r = i;
        while (l >= 0 && r < (int)s.size() && s[l] == s[r]) {
            l--;
            r++;
        }
        if (r - l - 1 > bestLen) {
            bestLen = r - l - 1;
            bestStart = l + 1;
        }

        // 偶数长度：以 s[i], s[i+1] 为中心
        l = i;
        r = i + 1;
        while (l >= 0 && r < (int)s.size() && s[l] == s[r]) {
            l--;
            r++;
        }
        if (r - l - 1 > bestLen) {
            bestLen = r - l - 1;
            bestStart = l + 1;
        }
    }

    return s.substr(bestStart, bestLen);
}

// leetcode 6 按行走访，方向在首末行折返
string LeetCode_Solution::convert(string s, int numRows) {
    if (numRows <= 1 || (int)s.size() <= numRows) {
        return s;
    }

    vector<string> rows(numRows);
    int r = 0;
    int step = 1;
    for (char c : s) {
        rows[r] += c;
        if (r == 0) {
            step = 1;
        } else if (r == numRows - 1) {
            step = -1;
        }
        r += step;
    }

    string res;
    for (const string& row : rows) {
        res += row;
    }

    return res;
}

// leetcode 9 只反转后半段，避免溢出
bool LeetCode_Solution::isPalindrome(int x) {
    // 负数和末位为 0 的非零数不可能是回文
    if (x < 0 || (x % 10 == 0 && x != 0)) {
        return false;
    }

    int reversed = 0;
    while (x > reversed) {
        reversed = reversed * 10 + x % 10;
        x /= 10;
    }

    // 奇数位时 reversed 多一位，除以 10 去掉中间位
    return x == reversed || x == reversed / 10;
}

// leetcode 10 动态规划
bool LeetCode_Solution::isMatch(string s, string p) {
    int m = (int)s.size();
    int n = (int)p.size();
    // dp[i][j]: s 前 i 个字符是否匹配 p 前 j 个字符
    vector<vector<bool>> dp(m + 1, vector<bool>(n + 1, false));
    dp[0][0] = true;

    // 形如 a*b*c* 的模式可以匹配空串
    for (int j = 2; j <= n; j += 2) {
        if (p[j - 1] == '*') {
            dp[0][j] = dp[0][j - 2];
        }
    }

    for (int i = 1; i <= m; i++) {
        for (int j = 1; j <= n; j++) {
            if (p[j - 1] == '*') {
                // 0 次：丢弃 x*；>=1 次：消耗 s[i-1]，要求 x 匹配它
                dp[i][j] = dp[i][j - 2] ||
                    ((p[j - 2] == '.' || p[j - 2] == s[i - 1]) && dp[i - 1][j]);
            } else {
                dp[i][j] = (p[j - 1] == '.' || p[j - 1] == s[i - 1]) &&
                    dp[i - 1][j - 1];
            }
        }
    }

    return dp[m][n];
}

// leetcode 56
vector<vector<int>> LeetCode_Solution::merge(vector<vector<int>>& intervals)
{
    if (intervals.size() == 0) {
        return {};
    }

    sort(intervals.begin(), intervals.end());
    vector<vector<int>> merged;
    for (int i = 0; i < intervals.size(); i++) {
        int L = intervals[i][0], R = intervals[i][1];
        if (!merged.size() || merged.back()[1] < L) {
            merged.push_back({L, R});
        } else {
            merged.back()[1] = max(merged.back()[1], R);
        }
    }

    return merged;
}