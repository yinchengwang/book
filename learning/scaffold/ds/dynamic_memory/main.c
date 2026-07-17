/**
 * @file main.c
 * @brief 数据结构动态内存学习卡片 - 演示堆内存管理与常见错误
 *
 * 涵盖内容：
 * - malloc/calloc/realloc/free 的基本用法
 * - 内存布局：栈 vs 堆
 * - 常见内存错误：double free、use after free、memory leak
 * - 分配失败处理
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 内存布局可视化 ==================== */

/**
 * 打印内存布局信息
 * 演示栈和堆的地址空间差异
 */
static void demo_memory_layout(void)
{
    printf("[dynamic_memory] 内存布局演示\n");

    /* 栈变量：地址在高位，向低地址增长 */
    int stack_var = 42;
    int stack_arr[3] = {1, 2, 3};

    /* 堆内存：malloc 分配 */
    int *heap_var = (int *)malloc(sizeof(int));
    int *heap_arr = (int *)malloc(sizeof(int) * 3);

    if (!heap_var || !heap_arr) {
        printf("  内存分配失败\n");
        free(heap_var);
        free(heap_arr);
        return;
    }

    *heap_var = 100;
    for (int i = 0; i < 3; i++) heap_arr[i] = i + 10;

    printf("  栈变量地址 (高位):\n");
    printf("    stack_var @ %p = %d\n", (void *)&stack_var, stack_var);
    printf("    stack_arr @ %p ~ %p\n",
           (void *)stack_arr, (void *)(stack_arr + 2));

    printf("  堆变量地址 (低位):\n");
    printf("    heap_var  @ %p = %d\n", (void *)heap_var, *heap_var);
    printf("    heap_arr  @ %p ~ %p\n",
           (void *)heap_arr, (void *)(heap_arr + 2));

    printf("  地址关系: stack > heap (栈向低地址增长，堆向高地址增长)\n");

    /* 清理 */
    free(heap_var);
    free(heap_arr);
}

/* ==================== malloc/calloc/realloc 演示 ==================== */

/**
 * malloc 演示
 * malloc 分配原始内存，不初始化
 */
static void demo_malloc(void)
{
    printf("\n[dynamic_memory] malloc 演示\n");

    /* malloc 分配 5 个 int 的内存，不初始化 */
    int *arr = (int *)malloc(5 * sizeof(int));
    if (!arr) {
        printf("  分配失败\n");
        return;
    }

    printf("  malloc(5 * sizeof(int)) 分配成功\n");
    printf("  分配后的值（未初始化，可能是垃圾值）:\n  ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    /* 初始化并使用 */
    for (int i = 0; i < 5; i++) {
        arr[i] = (i + 1) * 10;
    }
    printf("  初始化后: [10, 20, 30, 40, 50]\n");

    free(arr);
    printf("  free(arr) 释放内存\n");
}

/**
 * calloc 演示
 * calloc 分配并初始化为 0
 */
static void demo_calloc(void)
{
    printf("\n[dynamic_memory] calloc 演示\n");

    /* calloc 分配 5 个 int 的内存，初始化为 0 */
    int *arr = (int *)calloc(5, sizeof(int));
    if (!arr) {
        printf("  分配失败\n");
        return;
    }

    printf("  calloc(5, sizeof(int)) 分配成功\n");
    printf("  分配后的值（已初始化为 0）:\n  ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    /* 使用 */
    for (int i = 0; i < 5; i++) {
        arr[i] = (i + 1) * 10;
    }
    printf("  赋值后: [10, 20, 30, 40, 50]\n");

    free(arr);
    printf("  free(arr) 释放内存\n");
}

/**
 * realloc 演示
 * realloc 调整已分配内存的大小
 */
static void demo_realloc(void)
{
    printf("\n[dynamic_memory] realloc 演示\n");

    /* 初始分配 3 个 int */
    int *arr = (int *)malloc(3 * sizeof(int));
    if (!arr) {
        printf("  初始分配失败\n");
        return;
    }

    printf("  初始分配: malloc(3 * sizeof(int))\n");
    for (int i = 0; i < 3; i++) {
        arr[i] = (i + 1) * 10;
    }
    printf("  初始值: [10, 20, 30]\n");

    /* realloc 扩展到 6 个 int */
    printf("  realloc 扩展: 3 -> 6 个元素\n");
    int *new_arr = (int *)realloc(arr, 6 * sizeof(int));
    if (!new_arr) {
        printf("  扩展失败，原内存仍有效\n");
        free(arr);
        return;
    }

    /* realloc 成功后，原指针可能无效，只能使用新指针 */
    arr = new_arr;  /* 更新指针 */

    /* 新空间未初始化 */
    for (int i = 3; i < 6; i++) {
        arr[i] = (i + 1) * 10;
    }
    printf("  扩展后: [10, 20, 30, 40, 50, 60]\n");

    /* 缩减内存 */
    printf("  realloc 缩减: 6 -> 4 个元素\n");
    new_arr = (int *)realloc(arr, 4 * sizeof(int));
    if (new_arr) {
        arr = new_arr;
    }
    printf("  缩减后: [10, 20, 30, 40]\n");

    free(arr);
    printf("  free(arr) 释放内存\n");
}

/* ==================== 分配失败处理 ==================== */

/**
 * 演示分配失败的处理
 * 大量分配测试内存耗尽场景
 */
static void demo_allocation_failure(void)
{
    printf("\n[dynamic_memory] 分配失败处理演示\n");

    /* 尝试分配巨大的内存块 */
    printf("  尝试分配 10TB 内存...\n");
    void *huge = malloc((size_t)10 * 1024 * 1024 * 1024 * 1024);
    if (!huge) {
        printf("  [正确处理] malloc 返回 NULL，检查失败避免崩溃\n");
    } else {
        printf("  意外成功（内存足够）\n");
        free(huge);
    }

    /* 连续分配直到失败 */
    printf("  连续分配直到失败:\n");
    int count = 0;
    void *ptrs[100];
    memset(ptrs, 0, sizeof(ptrs));

    while (count < 100) {
        ptrs[count] = malloc(1024 * 1024);  /* 1MB */
        if (!ptrs[count]) {
            printf("    第 %d 次分配失败（之前成功分配 %d MB）\n",
                   count + 1, count);
            break;
        }
        count++;
    }

    /* 清理已分配的 */
    for (int i = 0; i < count; i++) {
        free(ptrs[i]);
    }
    printf("  释放 %d 个内存块完成\n", count);
}

/* ==================== 常见内存错误演示 ==================== */

/**
 * double free 演示
 * 同一个指针 free 两次会导致未定义行为
 */
static void demo_double_free(void)
{
    printf("\n[dynamic_memory] Double Free 错误演示\n");

    int *ptr = (int *)malloc(sizeof(int));
    if (!ptr) return;

    *ptr = 42;
    printf("  分配 ptr @ %p = %d\n", (void *)ptr, *ptr);

    printf("  第一次 free(ptr)\n");
    free(ptr);

    printf("  [警告] 第二次 free(ptr) - 这会导致未定义行为！\n");
    printf("  在实际运行中可能崩溃或触发安全漏洞\n");

    /* 注意：实际不应再次 free，这里仅作演示说明 */
}

/**
 * use after free 演示
 * 访问已释放的内存会导致未定义行为
 */
static void demo_use_after_free(void)
{
    printf("\n[dynamic_memory] Use After Free 错误演示\n");

    int *ptr = (int *)malloc(sizeof(int));
    if (!ptr) return;

    *ptr = 123;
    printf("  分配 ptr @ %p = %d\n", (void *)ptr, *ptr);
    printf("  访问: ptr = %d\n", *ptr);

    printf("  free(ptr) - 释放内存\n");
    free(ptr);

    printf("  [警告] use after free - 访问已释放的内存\n");
    printf("  ptr = %d  (可能读到垃圾值或触发段错误)\n", *ptr);

    /* 预防方法：释放后将指针设为 NULL */
    printf("  预防: 释放后将指针设为 NULL\n");
    ptr = NULL;
    printf("  检查: if (ptr != NULL) ...\n");
}

/**
 * 内存泄漏演示（简化版）
 * 分配内存但忘记释放
 */
static void demo_memory_leak(void)
{
    printf("\n[dynamic_memory] 内存泄漏演示\n");

    /* 模拟内存泄漏：分配但不释放 */
    int *leaked1 = (int *)malloc(sizeof(int) * 100);
    int *leaked2 = (int *)malloc(sizeof(int) * 200);
    int *leaked3 = (int *)malloc(sizeof(int) * 300);

    if (!leaked1 || !leaked2 || !leaked3) {
        free(leaked1);
        free(leaked2);
        free(leaked3);
        return;
    }

    printf("  分配三块内存（模拟泄漏）:\n");
    printf("    leaked1: %zu bytes @ %p\n", 100 * sizeof(int), (void *)leaked1);
    printf("    leaked2: %zu bytes @ %p\n", 200 * sizeof(int), (void *)leaked2);
    printf("    leaked3: %zu bytes @ %p\n", 300 * sizeof(int), (void *)leaked3);
    printf("  总计: %zu bytes 泄漏\n", 600 * sizeof(int));

    printf("  [提示] 函数结束后，这些指针超出作用域但内存未释放\n");
    printf("  实际项目中应使用 valgrind 或 address sanitizer 检测\n");

    /* 正确做法：释放 */
    free(leaked1);
    free(leaked2);
    free(leaked3);
    printf("  [正确做法] free(leaked1/2/3) - 已修复\n");
}

/* ==================== 2D 动态数组演示 ==================== */

/**
 * 二维动态数组的分配与释放
 * 演示多级指针和嵌套 malloc
 */
static void demo_2d_array(void)
{
    printf("\n[dynamic_memory] 二维动态数组演示\n");

    int rows = 3;
    int cols = 4;

    /* 分配 rows 个 int* 指针 */
    int **matrix = (int **)malloc(rows * sizeof(int *));
    if (!matrix) return;

    /* 每行分配 cols 个 int */
    for (int i = 0; i < rows; i++) {
        matrix[i] = (int *)malloc(cols * sizeof(int));
        if (!matrix[i]) {
            /* 分配失败，回滚 */
            for (int j = 0; j < i; j++) {
                free(matrix[j]);
            }
            free(matrix);
            return;
        }
    }

    printf("  创建 %dx%d 矩阵:\n", rows, cols);

    /* 初始化 */
    int val = 1;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            matrix[i][j] = val++;
        }
    }

    /* 打印 */
    for (int i = 0; i < rows; i++) {
        printf("    [");
        for (int j = 0; j < cols; j++) {
            printf("%2d%s", matrix[i][j], (j < cols - 1) ? ", " : "");
        }
        printf("]\n");
    }

    /* 释放：先释放每行，再释放指针数组 */
    printf("  释放内存:\n");
    for (int i = 0; i < rows; i++) {
        printf("    free(matrix[%d])\n", i);
        free(matrix[i]);
    }
    free(matrix);
    printf("    free(matrix)\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 动态内存 ===\n\n");

    /* 内存布局 */
    demo_memory_layout();

    /* 基本分配函数 */
    demo_malloc();
    demo_calloc();
    demo_realloc();

    /* 分配失败处理 */
    demo_allocation_failure();

    /* 常见错误 */
    demo_double_free();
    demo_use_after_free();
    demo_memory_leak();

    /* 复杂结构 */
    demo_2d_array();

    printf("\n=== PASS ===\n");
    return 0;
}
