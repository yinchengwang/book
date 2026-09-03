#include <stdio.h>
#include <stdlib.h>

int cmp(const void *a, const void *b)
{
    return (*(int *)a - *(int *)b);
}

int majorityElement(int* nums, int numsSize)
{
    qsort(nums, numsSize, sizeof(int), cmp);

    int count = 0;
    int middle = numsSize / 2;
    for (int i = 0; i < numsSize; i++) {
        if (nums[i] == nums[middle]) {
            count++;
        }
    }

    return count > middle ? nums[middle] : -1;
}

int main()
{
    int nums[] = {1, 2, 5, 9, 5, 9, 5, 5, 5};
    int major = majorityElement(nums, sizeof(nums) / sizeof(int));

    printf("major is: %d.\n", major);

    return 0;
}