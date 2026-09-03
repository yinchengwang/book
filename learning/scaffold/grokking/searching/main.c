/**
 * searching/main.c —— 搜索技巧：二分查找及其变体
 *
 * 功能：
 *   1. 标准二分查找
 *   2. 旋转排序数组搜索
 *   3. 搜索左右边界
 *
 * 编译：make all
 * 运行：make run
 */

#include <stdio.h>
#include <stdlib.h>

/* ========== 1. 标准二分查找 ========== */

/**
 * binary_search - 在有序数组 nums[0..size-1] 中查找 target
 * @nums:   升序数组
 * @size:   数组长度
 * @target: 目标值
 *
 * 返回：目标下标，未找到返回 -1
 * 时间复杂度 O(log n)，空间复杂度 O(1)
 */
int binary_search(const int *nums, int size, int target) {
    int left  = 0;
    int right = size - 1;  /* 闭区间 [left, right] */

    while (left <= right) {
        int mid = left + (right - left) / 2;  /* 防溢出 */
        if (nums[mid] == target)
            return mid;
        else if (nums[mid] < target)
            left  = mid + 1;
        else
            right = mid - 1;
    }
    return -1;
}

/* ========== 2. 旋转排序数组搜索 ========== */

/**
 * search_rotated - 在旋转排序数组中搜索 target
 * @nums:   旋转后的数组（例：[4,5,6,7,0,1,2]）
 * @size:   数组长度
 * @target: 目标值
 *
 * 思路：二分查找，每次判断 mid 落在左/右哪个有序区间内
 * 返回：目标下标，未找到返回 -1
 * 时间复杂度 O(log n)，空间复杂度 O(1)
 */
int search_rotated(const int *nums, int size, int target) {
    int left  = 0;
    int right = size - 1;

    while (left <= right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] == target)
            return mid;

        /* 左半有序 */
        if (nums[left] <= nums[mid]) {
            if (nums[left] <= target && target < nums[mid])
                right = mid - 1;
            else
                left  = mid + 1;
        }
        /* 右半有序 */
        else {
            if (nums[mid] < target && target <= nums[right])
                left  = mid + 1;
            else
                right = mid - 1;
        }
    }
    return -1;
}

/* ========== 3. 搜索左右边界 ========== */

/**
 * search_left_bound - 查找 target 第一次出现的位置（左边界）
 * @nums:   升序数组（可含重复值）
 * @size:   数组长度
 * @target: 目标值
 *
 * 返回：最左下标，未找到返回 -1
 */
int search_left_bound(const int *nums, int size, int target) {
    int left  = 0;
    int right = size;  /* 左闭右开 [left, right) */

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] >= target)
            right = mid;
        else
            left  = mid + 1;
    }
    if (left < size && nums[left] == target)
        return left;
    return -1;
}

/**
 * search_right_bound - 查找 target 最后一次出现的位置（右边界）
 * @nums:   升序数组（可含重复值）
 * @size:   数组长度
 * @target: 目标值
 *
 * 返回：最右下标，未找到返回 -1
 */
int search_right_bound(const int *nums, int size, int target) {
    int left  = 0;
    int right = size;  /* 左闭右开 [left, right) */

    while (left < right) {
        int mid = left + (right - left) / 2;
        if (nums[mid] <= target)
            left = mid + 1;
        else
            right = mid;
    }
    if (left > 0 && nums[left - 1] == target)
        return left - 1;
    return -1;
}

/* ========== 测试 ========== */

static void test_binary_search(void) {
    int nums[] = {1, 3, 5, 7, 9, 11, 13};
    int size   = sizeof(nums) / sizeof(nums[0]);

    printf("【二分查找】数组: [1,3,5,7,9,11,13]\n");
    printf("  查找 7 → 下标 %d\n", binary_search(nums, size, 7));
    printf("  查找 1 → 下标 %d\n", binary_search(nums, size, 1));
    printf("  查找 13 → 下标 %d\n", binary_search(nums, size, 13));
    printf("  查找 4 → 下标 %d\n", binary_search(nums, size, 4));
}

static void test_search_rotated(void) {
    int nums[] = {4, 5, 6, 7, 0, 1, 2};
    int size   = sizeof(nums) / sizeof(nums[0]);

    printf("【旋转数组搜索】数组: [4,5,6,7,0,1,2]\n");
    printf("  查找 0 → 下标 %d\n", search_rotated(nums, size, 0));
    printf("  查找 4 → 下标 %d\n", search_rotated(nums, size, 4));
    printf("  查找 3 → 下标 %d\n", search_rotated(nums, size, 3));
}

static void test_bound_search(void) {
    int nums[] = {1, 2, 2, 2, 3, 4, 5};
    int size   = sizeof(nums) / sizeof(nums[0]);

    printf("【边界搜索】数组: [1,2,2,2,3,4,5]\n");
    printf("  左边界 2 → %d\n", search_left_bound(nums, size, 2));
    printf("  右边界 2 → %d\n", search_right_bound(nums, size, 2));
    printf("  左边界 1 → %d\n", search_left_bound(nums, size, 1));
    printf("  右边界 5 → %d\n", search_right_bound(nums, size, 5));
    printf("  左边界 6 → %d\n", search_left_bound(nums, size, 6));
}

int main(void) {
    printf("========== 搜索技巧演示 ==========\n\n");
    test_binary_search();
    printf("\n");
    test_search_rotated();
    printf("\n");
    test_bound_search();
    printf("\n========== 演示结束 ==========\n");
    return 0;
}
