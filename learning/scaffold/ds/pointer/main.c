/**
 * @file main.c
 * @brief 数据结构指针学习卡片 - 演示指针基础与函数指针
 *
 * 涵盖内容：
 * - 指针基础：地址、取值、解引用
 * - 指针运算：++, --, +, - 与数组下标的等价关系
 * - 函数指针：声明、使用、回调
 * - 指针与数组的关系：数组名作为指针、指针算术运算
 * - const 指针模式：const 指针、指向 const 的指针
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 指针基础演示 ==================== */

/**
 * 指针基础演示
 * 指针是一个变量，其值为另一个变量的地址
 */
static void demo_pointer_basics(void)
{
    printf("[pointer] 指针基础演示\n");

    int value = 42;           /* 普通整型变量 */
    int *ptr = &value;        /* 指针：指向 value 的地址 */

    /* & 是取地址运算符 */
    printf("  value 的值: %d\n", value);
    printf("  value 的地址: %p\n", (void *)&value);
    printf("  ptr 存储的地址: %p\n", (void *)ptr);
    printf("  ptr 与 &value 相等: %s\n", (ptr == &value) ? "是" : "否");

    /* * 是解引用运算符（取值） */
    printf("  *ptr 的值: %d\n", *ptr);
    printf("  *ptr == value: %s\n", (*ptr == value) ? "是" : "否");

    /* 通过指针修改原始值 */
    *ptr = 100;
    printf("  通过 *ptr = 100 修改后，value = %d\n", value);

    /* 指针大小（在不同架构上可能不同） */
    printf("  指针大小: %zu 字节\n", sizeof(int *));
    printf("\n");
}

/* ==================== 指针运算演示 ==================== */

/**
 * 打印数组（使用指针算术而非下标）
 */
static void print_array_with_pointer(const int *arr, size_t size)
{
    printf("  数组内容 (用指针遍历): [");
    const int *p = arr;
    const int *end = arr + size;

    while (p < end) {
        printf("%d%s", *p, (p < end - 1) ? ", " : "");
        p++;
    }
    printf("]\n");
}

/**
 * 指针运算演示
 * 指针加减 n = 地址前进/后退 n * sizeof(元素类型)
 */
static void demo_pointer_arithmetic(void)
{
    printf("[pointer] 指针运算演示\n");

    int arr[] = {10, 20, 30, 40, 50};
    size_t size = sizeof(arr) / sizeof(arr[0]);

    printf("  数组: int arr[] = {10, 20, 30, 40, 50}\n");
    print_array_with_pointer(arr, size);

    int *p = arr;       /* arr 等价于 &arr[0] */
    int *p2 = arr + 2;  /* 指向 arr[2] = 30 */

    printf("  p = arr (指向 arr[0] = %d)\n", *p);
    printf("  p + 1 (指向 arr[1] = %d)\n", *(p + 1));
    printf("  p + 2 (指向 arr[2] = %d)\n", *(p + 2));
    printf("  p2 = arr + 2 (指向 arr[2] = %d)\n", *p2);
    printf("  p2 - p = %td (两个指针的距离，单位是元素个数)\n", p2 - p);

    /* 指针比较 */
    printf("  p < p2: %s\n", (p < p2) ? "是" : "否");

    /* 指针递增递减 */
    p++;
    printf("  p++ 后，*p = %d\n", *p);
    p--;
    printf("  p-- 后，*p = %d\n", *p);

    printf("  arr[i] 等价于 *(arr + i) 和 *(p + i)\n");
    printf("\n");
}

/* ==================== 函数指针演示 ==================== */

/**
 * 简单的数学运算函数
 */
static int add(int a, int b)
{
    return a + b;
}

static int multiply(int a, int b)
{
    return a * b;
}

static int max_int(int a, int b)
{
    return (a > b) ? a : b;
}

/**
 * 使用函数指针的通用操作
 * @param a        操作数1
 * @param b        操作数2
 * @param func     函数指针
 * @return         func(a, b) 的结果
 */
static int apply_operation(int a, int b, int (*func)(int, int))
{
    return func(a, b);
}

/**
 * 函数指针演示
 */
static void demo_function_pointer(void)
{
    printf("[pointer] 函数指针演示\n");

    /* 直接声明函数指针 */
    int (*op)(int, int);

    /* 赋值：函数名就是函数的地址 */
    op = &add;           /* & 可以省略 */
    op = add;            /* 等价于上面的写法 */
    printf("  op = add; op(3, 4) = %d\n", op(3, 4));

    /* 使用函数指针数组实现策略模式 */
    typedef int (*Operation)(int, int);
    Operation operations[] = {add, multiply, max_int};
    const char *names[] = {"add", "multiply", "max"};

    printf("  策略模式示例:\n");
    for (size_t i = 0; i < 3; i++) {
        int result = operations[i](10, 5);
        printf("    %s(10, 5) = %d\n", names[i], result);
    }

    /* 回调函数示例 */
    printf("  回调函数示例:\n");
    int a = 8, b = 3;
    printf("    apply_operation(%d, %d, add) = %d\n", a, b, apply_operation(a, b, add));
    printf("    apply_operation(%d, %d, multiply) = %d\n", a, b, apply_operation(a, b, multiply));
    printf("    apply_operation(%d, %d, max) = %d\n", a, b, apply_operation(a, b, max_int));

    printf("\n");
}

/* ==================== const 指针模式演示 ==================== */

/**
 * 指向 const 的指针：不能通过指针修改所指对象
 */
static void demo_pointer_to_const(void)
{
    printf("[pointer] const 指针模式演示\n");

    int value = 42;
    const int *p = &value;  /* 指向 const 的指针 */

    printf("  const int *p = &value;\n");
    printf("  *p = 100;  // 编译错误，不能修改\n");
    printf("  value = 100; // 可以直接修改 value\n");
    printf("  *p 的值: %d (value 被修改后)\n", *p);
}

/**
 * const 指针：指针本身是 const（不能指向其他地址）
 */
static void demo_const_pointer(void)
{
    printf("\n  int * const p = &value; // const 指针\n");
    printf("  p = &other; // 编译错误，不能指向其他地址\n");
    printf("  *p = 100;   // 可以修改所指对象\n");
}

/**
 * 指向 const 的 const 指针：都不能修改
 */
static void demo_const_ptr_to_const(void)
{
    printf("\n  const int * const p = &value; // const 指针指向 const\n");
    printf("  既不能修改 p，也不能通过 p 修改 *p\n");
}

/**
 * const 指针模式演示
 */
static void demo_pointer_patterns(void)
{
    printf("[pointer] const 指针模式\n");
    demo_pointer_to_const();
    demo_const_pointer();
    demo_const_ptr_to_const();
    printf("\n");
}

/* ==================== 数组与指针关系演示 ==================== */

/**
 * 数组名作为指针的演示
 */
static void demo_array_decay(void)
{
    printf("[pointer] 数组与指针的关系\n");

    int arr[] = {1, 2, 3, 4, 5};
    int *ptr = arr;  /* 数组名退化为指向首元素的指针 */

    printf("  int arr[] = {1, 2, 3, 4, 5};\n");
    printf("  int *ptr = arr;\n");
    printf("  sizeof(arr) = %zu (整个数组大小)\n", sizeof(arr));
    printf("  sizeof(ptr) = %zu (指针大小)\n", sizeof(ptr));
    printf("  arr == &arr[0]: %s\n", (arr == &arr[0]) ? "是" : "否");
    printf("  &arr + 1 vs arr + 1:\n");
    printf("    &arr 是数组的地址，加1跳过一个数组\n");
    printf("    arr 是首元素地址，加1跳过一个元素\n");
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 指针 ===\n\n");

    /* 指针基础演示 */
    demo_pointer_basics();

    /* 指针运算演示 */
    demo_pointer_arithmetic();

    /* 函数指针演示 */
    demo_function_pointer();

    /* const 指针模式演示 */
    demo_pointer_patterns();

    /* 数组与指针关系演示 */
    demo_array_decay();

    printf("=== PASS ===\n");
    return 0;
}
