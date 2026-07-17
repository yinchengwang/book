#include "leetcode/leetcode_cpp.h"
#include "common.h"

// leetcode 3452
int LeetCode_Solution::sumOfGoodNumbers(vector<int>& nums, int k)
{
    int res = 0;

    for (int i = 0; i < (int)nums.size(); i++) {
        int left = 1;
        int right = 1;

        if (i - k >= 0 && nums[i] <= nums[i - k]) {
            left = 0;
        }

        if (i + k < (int)nums.size() && nums[i] <= nums[i + k]) {
            right = 0;
        }

        if (left && right) {
            res += nums[i];
        }
    }

    return res;
}
