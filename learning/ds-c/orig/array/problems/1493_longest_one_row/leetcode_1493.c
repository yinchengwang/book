#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int longestSubarray(int* nums, int numsSize)
{
    int res = 0;
    int cnt0 = 0;
    int left = 0;
    for (int i = 0; i < numsSize; i++) {
        cnt0 += 1 - nums[i];
        if (cnt0 > 1) {
            cnt0 -= 1 - nums[left++];
        }

        res = fmax(res, i - left);
    }

    return res;
}

int main()
{
    int nums[] = {1,1,0,1};
    int res = longestSubarray(nums, sizeof(nums) / sizeof(int));
    printf("res is: %d.\n", res);

    int nums1[] = {0,1,1,1,0,1,1,0,1};
    res = longestSubarray(nums1, sizeof(nums1) / sizeof(int));
    printf("res is: %d.\n", res);

    int nums2[] = {1,1,1};
    res = longestSubarray(nums2, sizeof(nums2) / sizeof(int));
    printf("res is: %d.\n", res);

    return 0;
}