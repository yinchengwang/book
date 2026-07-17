#include <stdlib.h>
#include <math.h>

#include "common.h"

// leetcode 56

int **merge(int **intervals, int intervalsSize, int *intervalsColSize, int *returnSize, int **returnColumnSizes)
{
    (void)intervalsColSize; // Suppress unused parameter warning

    int **ans = malloc(sizeof(int *) * intervalsSize);
    *returnColumnSizes = malloc(sizeof(int) * intervalsSize);

    for (int i = 0; i < intervalsSize; i++) {
        ans[i] = (int *)malloc(sizeof(int) * 2);
    }

    qsort(intervals, intervalsSize, sizeof(int *), int_cmp);

    int count = 0;
    for (int i = 0; i < intervalsSize; i++) {
        int L = intervals[i][0], R = intervals[i][1];
        if (count == 0 || ans[count - 1][1] < L) {
            returnColumnSizes[0][count] = 2;
            ans[count][0] = L;
            ans[count][1] = R;
            count++;
        } else {
            ans[count - 1][1] = fmax(ans[count - 1][1], R);
        }
    }

    *returnSize = count;
    return ans;
}

// leetcode 75
void sortColors_qsort(int *nums, int numsSize)
{
    qsort(nums, numsSize, sizeof(int), int_cmp);
}

void sortColors_single_pointer(int *nums, int numsSize)
{
    int idx = 0;
    // 遇到0则把0换到前面
    for (int i = 0; i < numsSize; i++) {
        if (nums[i] == 0) {
            int_swap(&nums[i], &nums[idx]);
            ++idx;
        }
    }
    // 遇到1把1换到0后面部分
    for (int i = idx; i < numsSize; i++) {
        if (nums[i] == 1) {
            int_swap(&nums[i], &nums[idx]);
            ++idx;
        }
    }
}

void sortColors_double_pointer(int *nums, int numsSize)
{
    int p0 = 0, p1 = 0;
    for (int i = 0; i < numsSize; i++) {
        if (nums[i] == 1) {
            int_swap(&nums[i], &nums[p1]);
            ++p1;
        } else if (nums[i] == 0) {
            int_swap(&nums[i], &nums[p0]);
            if (p0 < p1) {
                // 把1再换到后面
                int_swap(&nums[i], &nums[p1]);
            }
            ++p1;
            ++p0;
        }
    }
}