#ifndef LEET_CODE_100_H
#define LEET_CODE_100_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 56
extern int **merge(int **intervals, int intervalsSize, int *intervalsColSize, int *returnSize, int **returnColumnSizes);

// 75
extern void sortColors_qsort(int *nums, int numsSize);
extern void sortColors_single_pointer(int *nums, int numsSize);
extern void sortColors_double_pointer(int *nums, int numsSize);

// 118
extern int **generate(int numRows, int *returnSize, int **returnColumnSizes);

// 231
extern bool isPowerOfTwo(int n);

// 414
extern int thirdMax(int* nums, int numsSize);

// 581
extern int findUnsortedSubarray(int* nums, int numsSize);

// 605
extern bool canPlaceFlowers(int *flowerbed, int flowerbedSize, int n);

// 628
extern int maximumProduct(int* nums, int numsSize);

// 643
extern double findMaxAverage(int* nums, int numsSize, int k);

// 674
extern int findLengthOfLCIS(int* nums, int numsSize);

// 697
extern int findShortestSubArray(int* nums, int numsSize);

// 1323
extern int maximum69Number (int num);

// 2016
extern int maximum_difference(int *nums, int numsSize);

// 2034 helper
extern bool isBalance(int x);

// 2348
extern long long zeroFilledSubarray(int *nums, int numsSize);

// 2048
extern int nextBeautifulNumber(int n);

// 3452
extern int sumOfGoodNumbers(int* nums, int numsSize, int k);

// 3169
extern int countDays(int days, int** meetings, int meetingsSize, int* meetingsColSize);


#ifdef __cplusplus
}
#endif // extern "C"

#endif // LEET_CODE_100_H