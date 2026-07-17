/**
 * @file main.c
 * @brief 数据结构函数指针学习卡片 - 演示回调与策略模式
 *
 * 涵盖内容：
 * - 函数指针 typedef：简化函数指针声明
 * - 回调函数：作为参数传递给其他函数
 * - 策略模式：运行时选择不同的算法
 * - qsort 自定义比较器：标准库排序的原理
 * - 实用示例：条件筛选、数据转换
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== typedef 函数指针 ==================== */

/**
 * 简化函数指针声明的 typedef
 * 使用 typedef 给函数指针类型起别名，使代码更易读
 */
typedef int (*CompareFunc)(const void *, const void *);  /* 比较函数类型 */
typedef int (*TransformFunc)(int);                        /* 转换函数类型 */
typedef int (*PredicateFunc)(int);                        /* 谓词函数类型 */

/* 具体函数实现 */
static int cmp_int_asc(const void *a, const void *b)
{
    int ia = *(const int *)a;  /* 先强制转换，再解引用 */
    int ib = *(const int *)b;
    return ia - ib;            /* 升序：a小返回负数 */
}

static int cmp_int_desc(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return ib - ia;            /* 降序：a小返回正数 */
}

static int abs_value(int x)
{
    return (x < 0) ? -x : x;
}

static int is_even(int x)
{
    return (x % 2) == 0;
}

/**
 * typedef 函数指针演示
 */
static void demo_typedef_function_pointer(void)
{
    printf("[function_pointer] typedef 函数指针演示\n");

    /* 使用 typedef 后的类型别名声明变量 */
    CompareFunc cmp = cmp_int_asc;
    TransformFunc transform = abs_value;
    PredicateFunc predicate = is_even;

    printf("  CompareFunc cmp = cmp_int_asc;\n");
    printf("  TransformFunc transform = abs_value;\n");
    printf("  PredicateFunc predicate = is_even;\n");

    printf("  调用: cmp(&a, &b) = %d\n", cmp((void *)(int[]){5}, (void *)(int[]){3}));
    printf("  调用: transform(-10) = %d\n", transform(-10));
    printf("  调用: predicate(7) = %d\n", predicate(7));

    printf("\n");
}

/* ==================== 回调函数演示 ==================== */

/**
 * 使用回调函数实现通用数组遍历
 * @param arr      数组指针
 * @param n        数组元素个数
 * @param transform 转换函数（回调）
 */
static void array_transform(int *arr, size_t n, TransformFunc transform)
{
    for (size_t i = 0; i < n; i++) {
        arr[i] = transform(arr[i]);
    }
}

/**
 * 使用回调函数实现条件筛选
 * @param src      源数组
 * @param n        源数组元素个数
 * @param dst      目标数组（输出）
 * @param pred     谓词函数（回调）
 * @return         筛选后元素个数
 */
static size_t array_filter(const int *src, size_t n, int *dst, PredicateFunc pred)
{
    size_t count = 0;
    for (size_t i = 0; i < n; i++) {
        if (pred(src[i])) {
            dst[count++] = src[i];
        }
    }
    return count;
}

/**
 * 打印数组
 */
static void print_array(const char *label, const int *arr, size_t n)
{
    printf("  %s: [", label);
    for (size_t i = 0; i < n; i++) {
        printf("%d%s", arr[i], (i < n - 1) ? ", " : "");
    }
    printf("]\n");
}

/**
 * 回调函数演示
 */
static void demo_callback(void)
{
    printf("[function_pointer] 回调函数演示\n");

    int data[] = {-3, 5, -8, 2, -1, 7};
    size_t n = sizeof(data) / sizeof(data[0]);

    printf("  原始数组:\n");
    print_array("  原始", data, n);

    /* 使用回调实现数组转换（取绝对值） */
    int transformed[6];
    memcpy(transformed, data, sizeof(data));
    array_transform(transformed, n, abs_value);
    print_array("  绝对值", transformed, n);

    /* 使用回调实现条件筛选（筛选偶数） */
    int filtered[6];
    size_t filtered_count = array_filter(data, n, filtered, is_even);
    print_array("  筛选偶数", filtered, filtered_count);

    printf("\n");
}

/* ==================== 策略模式演示 ==================== */

/**
 * 策略上下文结构
 * 包含一个指向策略函数的指针
 */
typedef struct {
    const char *name;
    TransformFunc strategy;  /* 策略函数指针 */
} TransformContext;

/* 不同策略实现 */
static int strategy_double(int x) { return x * 2; }
static int strategy_square(int x) { return x * x; }
static int strategy_negate(int x) { return -x; }

/**
 * 执行策略
 */
static int execute_strategy(TransformContext *ctx, int value)
{
    printf("  策略 '%s' 对 %d 应用: ", ctx->name, value);
    int result = ctx->strategy(value);  /* 通过函数指针调用策略 */
    printf("结果 = %d\n", result);
    return result;
}

/**
 * 策略模式演示
 */
static void demo_strategy_pattern(void)
{
    printf("[function_pointer] 策略模式演示\n");

    /* 定义不同策略 */
    TransformContext contexts[] = {
        {"double",  strategy_double},
        {"square",  strategy_square},
        {"negate",  strategy_negate}
    };
    size_t num_strategies = sizeof(contexts) / sizeof(contexts[0]);

    int value = 5;
    printf("  输入值: %d\n", value);

    /* 运行时选择策略 */
    for (size_t i = 0; i < num_strategies; i++) {
        execute_strategy(&contexts[i], value);
    }

    printf("\n");
}

/* ==================== qsort 比较器演示 ==================== */

/**
 * 学生结构体
 */
typedef struct {
    char name[32];
    int age;
    double score;
} Student;

/**
 * 按年龄比较学生
 */
static int cmp_student_by_age(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return sa->age - sb->age;
}

/**
 * 按分数比较学生（降序）
 */
static int cmp_student_by_score_desc(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    if (sb->score > sa->score) return 1;
    if (sb->score < sa->score) return -1;
    return 0;
}

/**
 * 按姓名比较学生
 */
static int cmp_student_by_name(const void *a, const void *b)
{
    const Student *sa = (const Student *)a;
    const Student *sb = (const Student *)b;
    return strcmp(sa->name, sb->name);
}

/**
 * 打印学生数组
 */
static void print_students(const char *label, Student *students, size_t n)
{
    printf("  %s:\n", label);
    for (size_t i = 0; i < n; i++) {
        printf("    [%zu] %s, age=%d, score=%.1f\n",
               i, students[i].name, students[i].age, students[i].score);
    }
}

/**
 * qsort 比较器演示
 */
static void demo_qsort_comparator(void)
{
    printf("[function_pointer] qsort 比较器演示\n");

    Student students[] = {
        {"Alice", 22, 95.5},
        {"Bob",   20, 88.0},
        {"Charlie", 23, 92.0},
        {"Diana",  21, 90.5}
    };
    size_t n = sizeof(students) / sizeof(students[0]);

    print_students("原始顺序", students, n);

    /* 使用不同的比较器进行排序 */
    printf("\n  按年龄升序排序:\n");
    qsort(students, n, sizeof(Student), cmp_student_by_age);
    print_students("排序后", students, n);

    printf("\n  按分数降序排序:\n");
    qsort(students, n, sizeof(Student), cmp_student_by_score_desc);
    print_students("排序后", students, n);

    printf("\n  按姓名排序:\n");
    qsort(students, n, sizeof(Student), cmp_student_by_name);
    print_students("排序后", students, n);

    printf("\n");
}

/* ==================== 函数指针在排序中的实际应用 ==================== */

/**
 * 通用排序结果打印
 */
static void print_sort_result(const char *label, int *arr, size_t n)
{
    printf("  %s:", label);
    for (size_t i = 0; i < n; i++) {
        printf(" %d", arr[i]);
    }
    printf("\n");
}

/**
 * 函数指针在排序中的实际应用
 */
static void demo_sort_usage(void)
{
    printf("[function_pointer] 排序中的函数指针应用\n");

    int data1[] = {3, 1, 4, 1, 5, 9, 2, 6};
    int data2[] = {3, 1, 4, 1, 5, 9, 2, 6};
    size_t n = sizeof(data1) / sizeof(data1[0]);

    printf("  原始数组:");
    print_sort_result("", data1, n);

    /* 升序排序 */
    qsort(data1, n, sizeof(int), cmp_int_asc);
    print_sort_result("升序", data1, n);

    /* 降序排序 */
    qsort(data2, n, sizeof(int), cmp_int_desc);
    print_sort_result("降序", data2, n);

    /* 二分查找（也需要比较器） */
    int key = 5;
    int *found = bsearch(&key, data1, n, sizeof(int), cmp_int_asc);
    printf("  二分查找 key=%d: %s\n", key, found ? "找到" : "未找到");

    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    printf("=== 数据结构: 函数指针 ===\n\n");

    /* typedef 函数指针演示 */
    demo_typedef_function_pointer();

    /* 回调函数演示 */
    demo_callback();

    /* 策略模式演示 */
    demo_strategy_pattern();

    /* qsort 比较器演示 */
    demo_qsort_comparator();

    /* 排序中的函数指针应用 */
    demo_sort_usage();

    printf("=== PASS ===\n");
    return 0;
}
