#include "leetcode/leetcode_cpp.h"

// leet_code 2234
string LeetCode_Solution::largestWordCount(vector<string>& messages, vector<string>& senders)
{
    unordered_map<string, int> map;
    for (int i = 0; i < messages.size(); i++) {
        map[senders[i]] += count(messages[i].begin(), messages[i].end(), ' ') + 1;
    }

    int maxCnt = 0;
    string res;
    for (auto &element : map) {
        if (element.second > maxCnt) {
            maxCnt = element.second;
            res = element.first;
        } else if (element.second == maxCnt) {
            if (res < element.first) {
                res = element.first;
            }
        }
    }

    return res;
}