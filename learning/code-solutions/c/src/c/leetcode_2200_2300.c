#include "leetcode/leetcode.h"

// leetcode 2348
long long zeroFilledSubarray(int *nums, int numsSize)
{
    long long res = 0;
    int curr = 0;

    for (int i = 0; i < numsSize; i++) {
        curr = nums[i] == 0 ? curr + 1 : 0;
        res += curr;
    }

    return res;
}