/**
 * @file arrays/main.c
 * @brief 数组高频算法题演示
 *
 * 包含:
 * 1. 两数之和 (哈希表法)
 * 2. 旋转数组 (二分查找)
 * 3. 滑动窗口 (最大子数组和)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// 1. 两数之和 - 哈希表法
// ============================================================================

/**
 * 两数之和 (LeetCode #1)
 * 给定数组和目标值，返回两数之和为目标值的索引
 *
 * 方法: 哈希表
 * 时间: O(n)
 * 空间: O(n)
 *
 * @param nums   输入数组
 * @param numsSize 数组长度
 * @param target 目标值
 * @param returnSize 返回索引数组大小
 * @return 索引数组 [index1, index2]，需调用者 free
 */
int *two_sum(int *nums, int numsSize, int target, int *returnSize) {
    // 简单哈希表实现: 值 -> 索引
    // 由于 C 没有内置哈希表，用数组模拟 (假设值范围 [-1024, 1023])
    int hash[2048] = {0};  // 偏移 1024 使负数可索引
    int offset = 1024;

    for (int i = 0; i < numsSize; i++) {
        int complement = target - nums[i];
        // 检查补数是否在哈希表中
        if (complement >= -1024 && complement <= 1023) {
            int idx = complement + offset;
            if (hash[idx] != 0) {
                int *result = (int *)malloc(2 * sizeof(int));
                result[0] = hash[idx] - 1;  // 存储的是 i+1，所以 -1
                result[1] = i;
                *returnSize = 2;
                return result;
            }
        }
        // 将当前数存入哈希表 (存储 i+1 区分 0 值)
        int idx = nums[i] + offset;
        if (idx >= 0 && idx < 2048) {
            hash[idx] = i + 1;
        }
    }
    *returnSize = 0;
    return NULL;
}

// ============================================================================
// 2. 旋转数组 - 二分查找
// ============================================================================

/**
 * 搜索旋转排序数组 (LeetCode #33)
 * 数组由升序数组旋转得到，找出目标值的位置
 *
 * 方法: 二分查找变体
 * 时间: O(log n)
 * 空间: O(1)
 *
 * @param nums   旋转后的升序数组
 * @param numsSize 数组长度
 * @param target 目标值
 * @return 目标值索引，未找到返回 -1
 */
int search_rotated(int *nums, int numsSize, int target) {
    if (numsSize == 0) return -1;

    int left = 0, right = numsSize - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;

        if (nums[mid] == target) return mid;

        // 判断哪半边是有序的
        if (nums[left] <= nums[mid]) {
            // 左半边有序
            if (nums[left] <= target && target < nums[mid]) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        } else {
            // 右半边有序
            if (nums[mid] < target && target <= nums[right]) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
    }
    return -1;
}

// ============================================================================
// 3. 滑动窗口 - 最大子数组和
// ============================================================================

/**
 * 最大子数组和 (LeetCode #53)
 * 找出连续子数组的最大和
 *
 * 方法: 滑动窗口 / Kadane 算法
 * 时间: O(n)
 * 空间: O(1)
 *
 * @param nums   输入数组
 * @param numsSize 数组长度
 * @return 最大子数组和
 */
int max_subarray_sum(int *nums, int numsSize) {
    if (numsSize == 0) return 0;

    int max_sum = nums[0];      // 全局最大和
    int current_sum = nums[0];  // 当前窗口和

    for (int i = 1; i < numsSize; i++) {
        // 当前元素要么扩展已有窗口，要么重新开始
        current_sum = (current_sum > 0) ? current_sum + nums[i] : nums[i];
        // 更新全局最大
        if (current_sum > max_sum) {
            max_sum = current_sum;
        }
    }
    return max_sum;
}

// ============================================================================
// 辅助函数
// ============================================================================

void print_array(int *arr, int size, const char *name) {
    printf("%s: [", name);
    for (int i = 0; i < size; i++) {
        printf("%d", arr[i]);
        if (i < size - 1) printf(", ");
    }
    printf("]\n");
}

// ============================================================================
// 主函数
// ============================================================================

int main(void) {
    printf("=== 数组高频算法题演示 ===\n\n");

    // ------------------------------------------------------------
    // 1. 两数之和测试
    // ------------------------------------------------------------
    printf("--- 1. 两数之和 (哈希表法 O(n)) ---\n");
    int nums1[] = {2, 7, 11, 15};
    int size1 = 4;
    int target1 = 9;
    int returnSize;

    print_array(nums1, size1, "输入数组");
    printf("目标值: %d\n", target1);

    int *result1 = two_sum(nums1, size1, target1, &returnSize);
    if (result1 && returnSize == 2) {
        printf("结果: 索引 [%d, %d] -> 值 [%d, %d]\n",
               result1[0], result1[1], nums1[result1[0]], nums1[result1[1]]);
        free(result1);
    }

    // ------------------------------------------------------------
    // 2. 旋转数组测试
    // ------------------------------------------------------------
    printf("\n--- 2. 旋转数组 (二分查找 O(log n)) ---\n");
    int nums2[] = {4, 5, 6, 7, 0, 1, 2};
    int size2 = 7;
    int target2 = 0;

    print_array(nums2, size2, "旋转数组");
    printf("目标值: %d\n", target2);

    int idx = search_rotated(nums2, size2, target2);
    printf("结果: 索引 %d\n", idx);

    // ------------------------------------------------------------
    // 3. 滑动窗口测试
    // ------------------------------------------------------------
    printf("\n--- 3. 最大子数组和 (滑动窗口 O(n)) ---\n");
    int nums3[] = {-2, 1, -3, 4, -1, 2, 1, -5, 4};
    int size3 = 9;

    print_array(nums3, size3, "输入数组");

    int max_sum = max_subarray_sum(nums3, size3);
    printf("最大子数组和: %d\n", max_sum);

    printf("\n=== 演示完成 ===\n");
    return 0;
}
