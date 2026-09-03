#include "common.h"

#include <limits.h>
#include <stddef.h>

// 414
int thirdMax(int* nums, int numsSize)
{
    if (nums == NULL || numsSize == 0) {
        return INT_MIN;
    }

    long long first_num = LLONG_MIN;
    long long second_num = LLONG_MIN;
    long long third_num = LLONG_MIN;

    for (int i = 0; i < numsSize; i++) {
        if (nums[i] == first_num || nums[i] == second_num || nums[i] == third_num) {
            continue;
        }

        if (nums[i] > first_num) {
            third_num = second_num;
            second_num = first_num;
            first_num = nums[i];
        } else if (nums[i] > second_num && nums[i] < first_num) {
            third_num = second_num;
            second_num = nums[i];
        } else if (nums[i] > third_num && nums[i] < second_num) {
            third_num = nums[i];
        }
    }

    return third_num == LLONG_MIN ? (int)first_num : (int)third_num;
}