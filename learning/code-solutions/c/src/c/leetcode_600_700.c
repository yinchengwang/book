#include "common.h"
#include "uthash/uthash.h"

#include <stdbool.h>
#include <math.h>

// 605
bool canPlaceFlowers(int *flowerbed, int flowerbedSize, int n)
{
    int len = 1;
    int res = 0;
    for (int i = 0; i < flowerbedSize; i++) {
        if (flowerbed[i] == 1) {
            res += (len - 1) / 2;
            len = 0;
        } else {
            len++;
        }
    }

    res += len / 2;
    return res >= n;
}

// 628
int maximumProduct(int* nums, int numsSize)
{
    if (nums == NULL || numsSize < 3) {
        return 0;
    }

    qsort(nums, numsSize, sizeof(int), int_cmp);

    int result = nums[numsSize - 1] * nums[numsSize - 2] * nums[numsSize - 3];
    if (nums[1] < 0) {
        int tmp_max = nums[0] * nums[1] * nums[numsSize - 1];
        result = tmp_max > result ? tmp_max : result;
    }

    return result;
}

// 643
double findMaxAverage(int* nums, int numsSize, int k)
{
    if (nums == NULL || numsSize == 0 || k <= 0 || k > numsSize) {
        return NAN;
    }

    int sum = 0;
    for (int i = 0; i < k; i++) {
        sum += nums[i];
    }

    int max_sum = sum;
    for (int i = k; i < numsSize; i++) {
        sum = sum + nums[i] - nums[i - k];
        max_sum = fmax(max_sum, sum);
    }

    return (double)(max_sum) / k;
}

// 674
int findLengthOfLCIS(int* nums, int numsSize)
{
    int ans = 0;
    int start = 0;
    for (int i = 0; i < numsSize; i++) {
        if (i > 0 && nums[i] <= nums[i - 1]) {
            start = i;
        }
        ans = fmax(ans, i - start + 1);
    }
    return ans;
}

// 697
typedef struct hash_table {
    int key;
    int num;
    int start;
    int end;
    UT_hash_handle hh;
} hash_table_697_t;

int findShortestSubArray(int* nums, int numsSize)
{
    if (nums == NULL || numsSize == 0) {
        return 0;
    }

    hash_table_697_t *table = NULL;
    for (int i = 0; i < numsSize; i++) {
        hash_table_697_t *entry = NULL;
        HASH_FIND_INT(table, &nums[i], entry);
        if (entry != NULL) {
            entry->num++;
            entry->end = i;
        } else {
            entry = (hash_table_697_t *)malloc(sizeof(hash_table_697_t));
            entry->key = nums[i];
            entry->num = 1;
            entry->start = i;
            entry->end = i;
            HASH_ADD_INT(table, key, entry);
        }
    }

    int max_num = 0;
    int min_len = 0;
    hash_table_697_t *entry;
    hash_table_697_t *tmp;
    HASH_ITER(hh, table, entry, tmp) {
        if (max_num < entry->num) {
            max_num = entry->num;
            min_len = entry->end - entry->start + 1;
        } else if (max_num == entry->num) {
            if (min_len > entry->end - entry->start + 1) {
                min_len = entry->end - entry->start + 1;
            }
        }
    }

    return min_len;
}