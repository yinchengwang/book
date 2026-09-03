#include "leetcode/leetcode_cpp.h"

// leet_code 2942
vector<int> LeetCode_Solution::findWordsContaining(vector<string>& words, char x)
{
    vector<int> res;
    for (int i = 0; i < words.size(); i++) {
        if (words[i].find(x) != string::npos) {
            res.push_back(i);
        }
    }

    return res;
}