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