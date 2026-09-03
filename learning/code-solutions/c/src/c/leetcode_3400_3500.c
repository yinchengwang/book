#include "common.h"

// 3452
int sumOfGoodNumbers(int* nums, int numsSize, int k)
{
    int res = 0;

    for (int i = 0; i < numsSize; i++) {
        int left = 1;
        int right = 1;

        if (i - k >= 0 && nums[i] <= nums[i - k]) {
            left = 0;
        }

        if (i + k < numsSize && nums[i] <= nums[i + k]) {
            right = 0;
        }

        if (left && right) {
            res += nums[i];
        }
    }

    return res;
}