#include "common.h"

#include <stddef.h>

// 581
int findUnsortedSubarray(int* nums, int numsSize)
{
    if (nums == NULL || numsSize <= 1) {
        return 0;
    }

    int low = 0;
    int max_num = nums[0];
    int high = numsSize - 1;
    int min_num = nums[high];

    for (int i = 0; i < numsSize; i++) {
        if (nums[i] < max_num) {
            low = i;
        } else {
            max_num = nums[i];
        }

        if (nums[numsSize - i - 1] > min_num) {
            high = numsSize - i - 1;
        } else {
            min_num = nums[numsSize - i - 1];
        }
    }

    return low > high ? low - high + 1 : 0;
}