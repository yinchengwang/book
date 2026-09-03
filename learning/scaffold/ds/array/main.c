/**
 * @file main.c
 * @brief 数据结构数组学习卡片 - 演示静态数组与动态数组
 *
 * 涵盖内容：
 * - 静态数组的创建和随机访问 O(1)
 * - 动态数组：malloc + 2x扩容 + resize
 * - 数组插入 O(n) 和删除 O(n)
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 工具函数 ==================== */

/**
 * 打印数组内容
 * @param arr  数组指针
 * @param size 元素个数
 */
static void print_array(const char *label, const int *arr, size_t size)
{
    printf("  %s: [", label);
    for (size_t i = 0; i < size; i++) {
        printf("%d%s", arr[i], (i < size - 1) ? ", " : "");
    }
    printf("]\n");
}

/**
 * 打印动态数组状态
 * @param capacity 当前容量
 * @param size     当前元素数
 */
static void print_dyn_status(size_t capacity, size_t size)
{
    printf("  状态: size=%zu, capacity=%zu\n", size, capacity);
}

/* ==================== 静态数组演示 ==================== */

/**
 * 静态数组演示
 * 特点：编译时确定大小，栈上分配，随机访问 O(1)
 */
static void demo_static_array(void)
{
    printf("[array] 静态数组演示\n");

    /* 静态数组：编译时确定大小，栈上分配 */
    int static_arr[8] = {10, 20, 30, 40, 50, 60, 70, 80};

    printf("  创建静态数组 int arr[8] = {...}\n");
    print_array("数组内容", static_arr, 8);

    /* 随机访问 O(1)：直接通过索引计算内存地址 */
    size_t idx = 3;
    printf("  访问 arr[%zu] = %d  (O(1) 时间复杂度)\n", idx, static_arr[idx]);

    /* 修改元素 */
    static_arr[5] = 65;
    printf("  修改 arr[5] = 65\n");
    print_array("修改后", static_arr, 8);

    printf("\n");
}

/* ==================== 动态数组演示 ==================== */

/**
 * 动态数组结构
 */
typedef struct {
    int *data;     /* 数据存储区 */
    size_t size;   /* 当前元素数 */
    size_t capacity; /* 容量 */
} DynArray;

/**
 * 创建动态数组
 */
static DynArray *dyn_array_create(size_t init_cap)
{
    DynArray *arr = (DynArray *)malloc(sizeof(DynArray));
    if (!arr) {
        return NULL;
    }
    arr->data = (int *)malloc(init_cap * sizeof(int));
    if (!arr->data) {
        free(arr);
        return NULL;
    }
    arr->size = 0;
    arr->capacity = init_cap;
    return arr;
}

/**
 * 释放动态数组
 */
static void dyn_array_free(DynArray *arr)
{
    if (arr) {
        free(arr->data);
        free(arr);
    }
}

/**
 * 动态数组扩容（2倍策略）
 */
static int dyn_array_resize(DynArray *arr)
{
    size_t new_cap = arr->capacity * 2;
    int *new_data = (int *)realloc(arr->data, new_cap * sizeof(int));
    if (!new_data) {
        return -1;
    }
    printf("  [resize] 容量从 %zu 扩展到 %zu\n", arr->capacity, new_cap);
    arr->data = new_data;
    arr->capacity = new_cap;
    return 0;
}

/**
 * 动态数组追加元素（尾部插入）
 */
static int dyn_array_push(DynArray *arr, int value)
{
    /* 需要扩容时，2倍扩展 */
    if (arr->size >= arr->capacity) {
        if (dyn_array_resize(arr) != 0) {
            return -1;
        }
    }
    arr->data[arr->size++] = value;
    return 0;
}

/**
 * 动态数组插入（指定位置）O(n)
 */
static int dyn_array_insert(DynArray *arr, size_t pos, int value)
{
    if (pos > arr->size) {
        return -1;
    }
    /* 需要扩容时，2倍扩展 */
    if (arr->size >= arr->capacity) {
        if (dyn_array_resize(arr) != 0) {
            return -1;
        }
    }
    /* 元素后移腾出位置 */
    for (size_t i = arr->size; i > pos; i--) {
        arr->data[i] = arr->data[i - 1];
    }
    arr->data[pos] = value;
    arr->size++;
    return 0;
}

/**
 * 动态数组删除（指定位置）O(n)
 */
static int dyn_array_delete(DynArray *arr, size_t pos)
{
    if (pos >= arr->size) {
        return -1;
    }
    /* 元素前移覆盖 */
    for (size_t i = pos; i < arr->size - 1; i++) {
        arr->data[i] = arr->data[i + 1];
    }
    arr->size--;
    return 0;
}

/**
 * 动态数组演示
 */
static void demo_dynamic_array(void)
{
    printf("[array] 动态数组演示\n");

    /* 创建初始容量为 4 的动态数组 */
    DynArray *arr = dyn_array_create(4);
    if (!arr) {
        fprintf(stderr, "创建动态数组失败\n");
        return;
    }
    printf("  创建动态数组，初始容量=4\n");
    print_dyn_status(arr->capacity, arr->size);

    /* 依次插入元素，触发扩容 */
    printf("  依次插入: 1, 2, 3, 4, 5\n");
    dyn_array_push(arr, 1);
    dyn_array_push(arr, 2);
    dyn_array_push(arr, 3);
    dyn_array_push(arr, 4);
    print_dyn_status(arr->capacity, arr->size);

    /* 第5个元素触发2倍扩容：4 -> 8 */
    dyn_array_push(arr, 5);
    print_dyn_status(arr->capacity, arr->size);
    print_array("数组内容", arr->data, arr->size);

    /* 指定位置插入 O(n) */
    printf("  在位置 2 插入 99 (O(n))\n");
    dyn_array_insert(arr, 2, 99);
    print_array("插入后", arr->data, arr->size);

    /* 指定位置删除 O(n) */
    printf("  删除位置 3 的元素 (O(n))\n");
    dyn_array_delete(arr, 3);
    print_array("删除后", arr->data, arr->size);

    /* 再触发一次扩容：8 -> 16 */
    printf("  继续插入: 10, 11, 12, 13, 14, 15\n");
    dyn_array_push(arr, 10);
    dyn_array_push(arr, 11);
    dyn_array_push(arr, 12);
    dyn_array_push(arr, 13);
    dyn_array_push(arr, 14);
    dyn_array_push(arr, 15);
    print_dyn_status(arr->capacity, arr->size);
    print_array("最终数组", arr->data, arr->size);

    dyn_array_free(arr);
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 数组 ===\n\n");

    /* 静态数组演示 */
    demo_static_array();

    /* 动态数组演示 */
    demo_dynamic_array();

    printf("=== PASS ===\n");
    return 0;
}
