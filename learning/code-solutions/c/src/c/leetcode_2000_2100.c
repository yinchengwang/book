#include "common.h"
#include "leetcode/leetcode.h"

#include <stddef.h>

// leet_code 2016
int maximum_difference(int *nums, int numsSize)
{
    if (nums == NULL || numsSize < 2) {
        return -1;
    }

    int res = -1;
    int min_num = nums[0];

    for (int i = 0; i < numsSize; i++) {
        if (nums[i] > min_num) {
            res = MAX(res, nums[i] - min_num);
        } else {
            min_num = nums[i];
        }
    }
 
    return res;
}

// leet_code 2034


// leetcode 2048
bool isBalance(int x)
{
    if (x <= 0) {
        return false;
    }

    int count[10] = { 0 };
    while (x > 0) {
        if (x % 10 == 0) {
            return false;
        }
        count[x % 10]++;
        x /= 10;
    }

    for (int i = 1; i < 10; i++) {
        if (count[i] > 0 && count[i] != i) {
            return false;
        }
    }

    return true;
}

int nextBeautifulNumber(int n)
{
    for (int i = n + 1; i <= 1224444; i++) {
        if (isBalance(i)) {
            return i;
        }
    }

    return -1;
}