#include <stdbool.h>


// 有序数组查找元素
int orderSearch(int *nums, int numsLen, int target)
{
    int left = 0;
    int right = numsLen;
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] == target) {
            return mid;
        } else if (nums[mid] > target) {
            right = mid;
        } else {
            left = mid + 1;
        }
    }

    return -1;
}


// 二维数组中的查找
bool array2dSearch(int target, int **array, int arrayRowLen, int *arrayColLen)
{
    for (int i = 0; i < arrayRowLen; i++) {
        if (orderSearch(array[i], *arrayColLen, target) >= 0) {
            return true;
        }
    }

    return false;
}


// 寻找峰值
int findPeakElement(int *nums, int numsLen)
{
    int left = 0;
    int right = numsLen - 1;
    while (left < right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] < nums[right]) {
            left = mid + 1;
        } else {
            right = mid;
        }
    }

    return right;
}