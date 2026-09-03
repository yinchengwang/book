/**
 * @file main.c
 * @brief 排序算法实战 - 合并区间 + 快速选择 + 桶排序
 * @author yinchengwang
 * @version 1.0
 * @date 2026-07-12
 * @note C11 标准实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>

// =============================================================================
// 第一部分：合并区间（排序后合并）
// =============================================================================

/**
 * @brief 区间结构
 */
typedef struct {
    int start;
    int end;
} Interval;

/**
 * @brief 比较函数（用于 qsort）
 */
int compare_interval(const void* a, const void* b) {
    Interval* ia = (Interval*)a;
    Interval* ib = (Interval*)b;
    if (ia->start != ib->start) {
        return ia->start - ib->start;
    }
    return ia->end - ib->end;
}

/**
 * @brief 合并重叠区间
 * @param intervals 输入区间数组
 * @param intervals_size 区间数量
 * @param result_size 输出区间数量（通过指针返回）
 * @return 合并后的区间数组（需要调用者 free）
 */
Interval* merge_intervals(Interval* intervals, int intervals_size, int* result_size) {
    if (intervals_size <= 0) {
        *result_size = 0;
        return NULL;
    }

    // 按起始位置排序
    qsort(intervals, intervals_size, sizeof(Interval), compare_interval);

    // 分配最大可能的输出空间
    Interval* result = (Interval*)malloc(intervals_size * sizeof(Interval));
    int count = 0;

    // 逐个合并
    int current_start = intervals[0].start;
    int current_end = intervals[0].end;

    for (int i = 1; i < intervals_size; i++) {
        if (intervals[i].start <= current_end) {
            // 有重叠，扩展当前区间
            current_end = (intervals[i].end > current_end) ? intervals[i].end : current_end;
        } else {
            // 无重叠，保存当前区间，开始新区间
            result[count].start = current_start;
            result[count].end = current_end;
            count++;
            current_start = intervals[i].start;
            current_end = intervals[i].end;
        }
    }
    // 保存最后一个区间
    result[count].start = current_start;
    result[count].end = current_end;
    count++;

    *result_size = count;
    return result;
}

// =============================================================================
// 第二部分：快速选择（找第 K 大元素）
// =============================================================================

/**
 * @brief 分区函数（荷兰国旗问题）
 * @param arr 数组
 * @param left 左边界
 * @param right 右边界
 * @param pivot_idx 基准值索引
 * @return 基准值最终位置
 */
int partition(int* arr, int left, int right, int pivot_idx) {
    int pivot = arr[pivot_idx];
    // 将基准值移到末尾
    int temp = arr[pivot_idx];
    arr[pivot_idx] = arr[right];
    arr[right] = temp;

    int store_idx = left;

    for (int i = left; i < right; i++) {
        if (arr[i] > pivot) {  // 降序排列，第 K 大意味着大于基准的个数
            temp = arr[store_idx];
            arr[store_idx] = arr[i];
            arr[i] = temp;
            store_idx++;
        }
    }

    // 将基准值放回正确位置
    temp = arr[store_idx];
    arr[store_idx] = arr[right];
    arr[right] = temp;

    return store_idx;
}

/**
 * @brief 快速选择算法 - 找第 K 大的元素
 * @param arr 数组
 * @param left 左边界
 * @param right 右边界
 * @param k 第 K 大（1 <= k <= right-left+1）
 * @return 第 K 大的值
 */
int quick_select(int* arr, int left, int right, int k) {
    if (left == right) {
        return arr[left];
    }

    // 随机选择基准
    int pivot_idx = left + rand() % (right - left + 1);
    pivot_idx = partition(arr, left, right, pivot_idx);

    // 计算有多少元素大于基准（即大于等于 pivot_idx 位置的值）
    int num_greater = pivot_idx - left + 1;

    if (k == num_greater) {
        return arr[pivot_idx];
    } else if (k < num_greater) {
        return quick_select(arr, left, pivot_idx - 1, k);
    } else {
        return quick_select(arr, pivot_idx + 1, right, k - num_greater);
    }
}

// =============================================================================
// 第三部分：桶排序
// =============================================================================

/**
 * @brief 桶排序
 * @param arr 输入数组
 * @param arr_size 数组大小
 * @return 排序后的数组（需要调用者 free）
 */
int* bucket_sort(int* arr, int arr_size) {
    if (arr_size <= 0) return NULL;

    // 找到最大值和最小值
    int min_val = arr[0];
    int max_val = arr[0];
    for (int i = 1; i < arr_size; i++) {
        if (arr[i] < min_val) min_val = arr[i];
        if (arr[i] > max_val) max_val = arr[i];
    }

    if (min_val == max_val) {
        int* result = (int*)malloc(arr_size * sizeof(int));
        for (int i = 0; i < arr_size; i++) {
            result[i] = arr[i];
        }
        return result;
    }

    // 创建桶
    int bucket_count = arr_size;
    int** buckets = (int**)calloc(bucket_count, sizeof(int*));
    int* bucket_sizes = (int*)calloc(bucket_count, sizeof(int));

    // 计算桶范围
    double range = (double)(max_val - min_val) / bucket_count + 1;

    // 分配元素到桶
    for (int i = 0; i < arr_size; i++) {
        int bucket_idx = (int)((arr[i] - min_val) / range);
        buckets[bucket_idx] = (int*)realloc(buckets[bucket_idx],
                                            (bucket_sizes[bucket_idx] + 1) * sizeof(int));
        buckets[bucket_idx][bucket_sizes[bucket_idx]] = arr[i];
        bucket_sizes[bucket_idx]++;
    }

    // 对每个桶进行排序并收集结果
    int* result = (int*)malloc(arr_size * sizeof(int));
    int result_idx = 0;

    for (int i = 0; i < bucket_count; i++) {
        // 对桶内元素排序（这里使用简单的插入排序）
        for (int j = 1; j < bucket_sizes[i]; j++) {
            int key = buckets[i][j];
            int k = j - 1;
            while (k >= 0 && buckets[i][k] > key) {
                buckets[i][k + 1] = buckets[i][k];
                k--;
            }
            buckets[i][k + 1] = key;
        }

        // 将排序后的元素复制到结果
        for (int j = 0; j < bucket_sizes[i]; j++) {
            result[result_idx++] = buckets[i][j];
        }
        free(buckets[i]);
    }

    free(buckets);
    free(bucket_sizes);

    return result;
}

// =============================================================================
// 主函数：测试所有功能
// =============================================================================

void print_intervals(Interval* intervals, int size) {
    printf("[");
    for (int i = 0; i < size; i++) {
        printf("[%d,%d]", intervals[i].start, intervals[i].end);
        if (i < size - 1) printf(", ");
    }
    printf("]\n");
}

int main(void) {
    printf("=== 排序算法实战 ===\n\n");

    // 测试合并区间
    printf("--- 合并区间测试 ---\n");
    Interval intervals[] = {{1,3},{2,6},{8,10},{15,18}};
    int intervals_size = sizeof(intervals) / sizeof(intervals[0]);
    printf("输入: ");
    print_intervals(intervals, intervals_size);

    int result_size;
    Interval* merged = merge_intervals(intervals, intervals_size, &result_size);
    printf("输出: ");
    print_intervals(merged, result_size);
    free(merged);

    // 测试快速选择
    printf("\n--- 快速选择测试（找第 K 大） ---\n");
    int arr[] = {3, 2, 1, 5, 6, 4};
    int arr_size = sizeof(arr) / sizeof(arr[0]);
    printf("数组: ");
    for (int i = 0; i < arr_size; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    int k = 2;
    int arr_copy[6];
    memcpy(arr_copy, arr, sizeof(arr));
    int kth_largest = quick_select(arr_copy, 0, arr_size - 1, k);
    printf("第 %d 大的元素是: %d\n", k, kth_largest);

    k = 4;
    memcpy(arr_copy, arr, sizeof(arr));
    kth_largest = quick_select(arr_copy, 0, arr_size - 1, k);
    printf("第 %d 大的元素是: %d\n", k, kth_largest);

    // 测试桶排序
    printf("\n--- 桶排序测试 ---\n");
    int arr_to_sort[] = {42, 32, 33, 12, 22, 11, 90};
    arr_size = sizeof(arr_to_sort) / sizeof(arr_to_sort[0]);
    printf("输入: ");
    for (int i = 0; i < arr_size; i++) {
        printf("%d ", arr_to_sort[i]);
    }
    printf("\n");

    int* sorted = bucket_sort(arr_to_sort, arr_size);
    printf("输出: ");
    for (int i = 0; i < arr_size; i++) {
        printf("%d ", sorted[i]);
    }
    printf("\n");
    free(sorted);

    return 0;
}
