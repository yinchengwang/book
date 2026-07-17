#include "leetcode/leetcode_cpp.h"

// leet_code 2131
int LeetCode_Solution::longestPalindrome(vector<string>& words) {
    unordered_map<string, int> freq;
    for (const string& word: words) {
        ++freq[word];
    }

    int res = 0;
    bool mid = false;

    for (const auto& [word, cnt]: freq) {
        // 反转后的单词
        string rev = string(1, word[1]) + word[0];
        if (rev == word) {
            // 存在中心对称的单词
            if (cnt % 2 == 1) {
                mid = true;
            }
            // 计算长度
            res += 2 * (cnt / 2 * 2);
        } else if (word > rev) {
            // 避免重复计算 ab ba只计算一次
            res += 4 * min(freq[word], freq[rev]);
        }
    }

    if (mid) {
        res += 2;
    }

    return res;
}
