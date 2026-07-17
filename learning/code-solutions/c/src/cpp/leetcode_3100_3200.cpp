#include "leetcode/leetcode_cpp.h"
#include "common.h"

// leetcode 3169
int LeetCode_Solution::countDays(int days, vector<vector<int>>& meetings)
{
    sort(meetings.begin(), meetings.end());
    int l = 1;
    int r = 0;
    for (auto m : meetings) {
        if (m[0] > r) {
            days -= (r - l + 1);
            l = m[0];
        }

        r = max(r, m[1]);
    }

    days -= (r - l + 1);
    return days;
}