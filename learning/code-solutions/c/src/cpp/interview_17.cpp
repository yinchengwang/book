#include <algorithm>

#include "leetcode/leetcode_cpp.h"

// 17.14
vector<int> LeetCode_Solution::smallestK(vector<int>& arr, int k)
{
    vector<int> res;

    sort(arr.begin(), arr.end());

    for (int i = 0; i < k; i++) {
        res.push_back(arr[i]);
    }

    return res;
}