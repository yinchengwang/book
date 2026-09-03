#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

bool isPossibleToSplit(int* nums, int numsSize)
{
    int map[101] = {0};
    for (int i = 0; i < numsSize; i++) {
        if (++map[nums[i]] > 2) {
            return false;
        }
    }

    return true;
}

int main()
{
    int nums[] = {1,1,1,1};
    bool flag = isPossibleToSplit(nums, sizeof(nums) / sizeof(int));
    printf("flag: %d\n", flag);

    int nums1[] = {1,1,2,2,3,4};
    flag = isPossibleToSplit(nums1, sizeof(nums1) / sizeof(int));
    printf("flag: %d\n", flag);

    return 0;
}