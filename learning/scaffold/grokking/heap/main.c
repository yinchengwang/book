/**
 * @file main.c
 * @brief 堆算法练习 - Top-K、动态中位数、K 路合并
 *
 * 包含：
 *   1. Top-K 问题（最小堆）
 *   2. 动态中位数（两个堆）
 *   3. K 路合并（多堆合并）
 *
 * @author Claude
 * @date 2026-07-12
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h>

/*==============================
 * 堆结构定义
 *=============================*/

typedef struct MinHeap {
    int *data;     // 堆数据
    int size;      // 当前元素数
    int capacity;  // 容量
} MinHeap;

/**
 * @brief 创建最小堆
 */
MinHeap* heap_create(int capacity) {
    MinHeap *heap = (MinHeap *)malloc(sizeof(MinHeap));
    if (!heap) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    heap->data = (int *)malloc(sizeof(int) * (capacity + 1));
    heap->size = 0;
    heap->capacity = capacity;
    return heap;
}

/**
 * @brief 释放堆
 */
void heap_destroy(MinHeap *heap) {
    free(heap->data);
    free(heap);
}

/**
 * @brief 交换两元素
 */
static void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

/**
 * @brief 上浮调整（插入时）
 */
static void heapify_up(MinHeap *heap, int idx) {
    while (idx > 1) {
        int parent = idx / 2;
        if (heap->data[parent] <= heap->data[idx]) break;
        swap(&heap->data[parent], &heap->data[idx]);
        idx = parent;
    }
}

/**
 * @brief 下沉调整（删除时）
 */
static void heapify_down(MinHeap *heap, int idx) {
    while (idx * 2 <= heap->size) {
        int child = idx * 2;
        // 找较小的子节点
        if (child + 1 <= heap->size &&
            heap->data[child + 1] < heap->data[child]) {
            child++;
        }
        if (heap->data[idx] <= heap->data[child]) break;
        swap(&heap->data[idx], &heap->data[child]);
        idx = child;
    }
}

/**
 * @brief 插入元素到最小堆
 */
void heap_push(MinHeap *heap, int val) {
    if (heap->size >= heap->capacity) {
        printf("堆已满，无法插入 %d\n", val);
        return;
    }
    heap->data[++heap->size] = val;
    heapify_up(heap, heap->size);
}

/**
 * @brief 取出堆顶最小元素
 */
int heap_pop(MinHeap *heap) {
    if (heap->size <= 0) return INT_MAX;
    int min = heap->data[1];
    heap->data[1] = heap->data[heap->size--];
    heapify_down(heap, 1);
    return min;
}

/**
 * @brief 获取堆顶元素
 */
int heap_peek(MinHeap *heap) {
    if (heap->size <= 0) return INT_MAX;
    return heap->data[1];
}

/*==============================
 * 1. Top-K 问题（最小堆）
 *=============================*/

/**
 * @brief 从数组中找出最大的 K 个元素
 *
 * 思路：
 *   - 维护一个大小为 K 的最小堆
 *   - 遍历数组，如果元素大于堆顶，替换堆顶并调整
 *   - 最终堆中就是最大的 K 个元素（无序）
 *
 * 时间复杂度: O(n log k)，空间复杂度: O(k)
 */
void find_top_k(int *arr, int n, int k) {
    MinHeap *heap = heap_create(k);

    for (int i = 0; i < n; i++) {
        if (heap->size < k) {
            heap_push(heap, arr[i]);
        } else if (arr[i] > heap_peek(heap)) {
            heap_pop(heap);
            heap_push(heap, arr[i]);
        }
    }

    printf("Top-%d 元素: ", k);
    while (heap->size > 0) {
        printf("%d ", heap_pop(heap));
    }
    printf("\n");

    heap_destroy(heap);
}

/*==============================
 * 2. 动态中位数（两个堆）
 *=============================*/

/**
 * @brief 动态中位数数据结构
 *
 * 使用两个堆维护：
 *   - max_heap（用负数模拟）：存较小的一半，堆顶是较大元素中的最小
 *   - min_heap：存较大的一半，堆顶是较小元素中的最大
 *
 * 调整规则：
 *   - 两堆大小相等时，中位数是两堆顶平均值
 *   - 两堆大小不等时，中位数是元素多的那个堆顶
 */
typedef struct MedianFinder {
    MinHeap *max_heap;  // 存负数，模拟最大堆
    MinHeap *min_heap;   // 最小堆
} MedianFinder;

MedianFinder* median_create(void) {
    MedianFinder *mf = (MedianFinder *)malloc(sizeof(MedianFinder));
    mf->max_heap = heap_create(1000);
    mf->min_heap = heap_create(1000);
    return mf;
}

void median_destroy(MedianFinder *mf) {
    heap_destroy(mf->max_heap);
    heap_destroy(mf->min_heap);
    free(mf);
}

/**
 * @brief 添加数字到数据结构
 */
void median_add(MedianFinder *mf, int num) {
    // 插入到 max_heap（存负数）
    heap_push(mf->max_heap, -num);

    // 平衡：确保 max_heap 顶 <= min_heap 顶
    if (mf->min_heap->size > 0 && -mf->max_heap->data[1] > mf->min_heap->data[1]) {
        int val = -heap_pop(mf->max_heap);
        heap_push(mf->min_heap, val);
    }

    // 调整大小平衡
    if (mf->max_heap->size > mf->min_heap->size + 1) {
        int val = -heap_pop(mf->max_heap);
        heap_push(mf->min_heap, val);
    }
    if (mf->min_heap->size > mf->max_heap->size + 1) {
        int val = heap_pop(mf->min_heap);
        heap_push(mf->max_heap, -val);
    }
}

/**
 * @brief 获取当前中位数
 */
double median_get(MedianFinder *mf) {
    if (mf->max_heap->size > mf->min_heap->size) {
        return (double)-mf->max_heap->data[1];
    }
    if (mf->min_heap->size > mf->max_heap->size) {
        return (double)mf->min_heap->data[1];
    }
    return ((double)-mf->max_heap->data[1] + mf->min_heap->data[1]) / 2.0;
}

/*==============================
 * 3. K 路合并（多堆合并）
 *=============================*/

/**
 * @brief K 路有序数组合并
 *
 * 思路：
 *   - 将每个数组的第一个元素加入最小堆
 *   - 取出堆顶最小元素，将其所属数组的下一个元素入堆
 *   - 重复直到所有元素处理完毕
 *
 * 时间复杂度: O(n log k)，n 为总元素数
 */
void merge_k_sorted_arrays(int **arrays, int *sizes, int k) {
    // 数组元素结构：{值, 数组索引, 当前读取到的位置}
    typedef struct {
        int val;
        int arr_idx;
        int elem_idx;
    } Elem;

    Elem *heap_arr = (Elem *)malloc(sizeof(Elem) * (k + 1));
    int heap_size = 0;

    // 初始化：每个数组第一个元素入堆
    for (int i = 0; i < k; i++) {
        if (sizes[i] > 0) {
            heap_arr[++heap_size] = (Elem){arrays[i][0], i, 0};
            // 上浮调整
            int idx = heap_size;
            while (idx > 1 && heap_arr[idx].val < heap_arr[idx/2].val) {
                Elem temp = heap_arr[idx];
                heap_arr[idx] = heap_arr[idx/2];
                heap_arr[idx/2] = temp;
                idx /= 2;
            }
        }
    }

    printf("K 路合并结果: ");
    while (heap_size > 0) {
        // 取出堆顶（最小元素）
        Elem min = heap_arr[1];
        printf("%d ", min.val);

        // 从对应数组取下一个元素
        if (min.elem_idx + 1 < sizes[min.arr_idx]) {
            min.elem_idx++;
            min.val = arrays[min.arr_idx][min.elem_idx];
            heap_arr[1] = min;
            // 下沉调整
            int idx = 1;
            while (idx * 2 <= heap_size) {
                int child = idx * 2;
                if (child + 1 <= heap_size &&
                    heap_arr[child + 1].val < heap_arr[child].val) {
                    child++;
                }
                if (heap_arr[idx].val <= heap_arr[child].val) break;
                Elem temp = heap_arr[idx];
                heap_arr[idx] = heap_arr[child];
                heap_arr[child] = temp;
                idx = child;
            }
        } else {
            // 用最后一个元素替换堆顶并下沉
            heap_arr[1] = heap_arr[heap_size--];
            int idx = 1;
            while (idx * 2 <= heap_size) {
                int child = idx * 2;
                if (child + 1 <= heap_size &&
                    heap_arr[child + 1].val < heap_arr[child].val) {
                    child++;
                }
                if (heap_arr[idx].val <= heap_arr[child].val) break;
                Elem temp = heap_arr[idx];
                heap_arr[idx] = heap_arr[child];
                heap_arr[child] = temp;
                idx = child;
            }
        }
    }
    printf("\n");

    free(heap_arr);
}

/*==============================
 * 测试入口
 *=============================*/

int main(void) {
    printf("========== 堆算法练习 ==========\n\n");

    /* 测试1: Top-K */
    printf("【测试1】Top-K 问题\n");
    int arr[] = {3, 1, 5, 7, 2, 8, 6, 4};
    int n = sizeof(arr) / sizeof(arr[0]);
    printf("数组: ");
    for (int i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");
    find_top_k(arr, n, 3);
    printf("\n");

    /* 测试2: 动态中位数 */
    printf("【测试2】动态中位数\n");
    MedianFinder *mf = median_create();
    int nums[] = {2, 4, 1, 3, 5, 7, 6};
    printf("添加序列: ");
    for (int i = 0; i < 7; i++) {
        printf("%d ", nums[i]);
        median_add(mf, nums[i]);
    }
    printf("\n");
    printf("当前中位数: %.1f\n\n", median_get(mf));
    median_destroy(mf);

    /* 测试3: K 路合并 */
    printf("【测试3】K 路合并\n");
    int arr1[] = {1, 4, 7};
    int arr2[] = {2, 5, 8};
    int arr3[] = {3, 6, 9};
    int *arrays[] = {arr1, arr2, arr3};
    int sizes[] = {3, 3, 3};
    int k = 3;
    printf("待合并数组:\n");
    for (int i = 0; i < k; i++) {
        printf("  数组%d: ", i + 1);
        for (int j = 0; j < sizes[i]; j++) printf("%d ", arrays[i][j]);
        printf("\n");
    }
    merge_k_sorted_arrays(arrays, sizes, k);

    printf("\n========== 测试完成 ==========\n");
    return 0;
}
