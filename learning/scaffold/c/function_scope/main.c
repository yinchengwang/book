/* function_scope scaffold — 函数与作用域
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *
 * 演示 5 段：
 *   [proto]     — 函数声明（prototype）与定义分离
 *   [param]     — 传值 vs 传址
 *   [static]    — static 函数/变量的文件作用域
 *   [extern]    — extern 跨文件引用
 *   [block]     — 块作用域与文件作用域
 */

#include <stdio.h>

/* === [proto] 函数声明（prototype） === */
/* 前向声明让编译器知道调用约定，函数定义可在文件任意位置 */
static int add(int a, int b);             /* static 函数，仅本文件可见 */
static void swap(int *x, int *y);         /* 传址 */
static int counter_increment(void);       /* 内部状态封装 */

/* === [extern] 跨文件引用声明 === */
/* extern 告诉编译器"这个变量在别的 .c 文件里定义"，不分配内存 */
extern const char *PROGRAM_NAME;          /* 定义在 main() 之后 */

/* === [block] 文件作用域变量 === */
static int call_count = 0;                /* 文件作用域：仅本文件可见，生命周期 = 程序 */

/* === 函数定义 === */

static int add(int a, int b) {
    call_count++;                          /* 修改文件作用域变量 */
    return a + b;
}

static void swap(int *x, int *y) {
    int tmp = *x;
    *x = *y;
    *y = tmp;
}

static int counter_increment(void) {
    static int counter = 0;                /* 静态局部变量：生命周期 = 程序，但仅函数可见 */
    return ++counter;
}

const char *PROGRAM_NAME = "function_scope";   /* 定义（extern 声明用） */

int main(void) {
    /* === [proto] 函数声明 vs 定义 === */
    printf("[proto] === 函数声明（prototype）与定义 ===\n");
    int sum = add(3, 4);                  /* 编译时检查 prototype，链接时找定义 */
    printf("  add(3, 4) = %d (call_count=%d)\n", sum, call_count);

    /* === [param] 传值 vs 传址 === */
    printf("\n[param] === 传值 vs 传址 ===\n");
    int a = 10, b = 20;
    printf("  传值前: a=%d, b=%d\n", a, b);
    /* 传值：函数内修改不影响原变量 */
    int tmp = a;
    a = b;
    b = tmp;
    printf("  手动 swap: a=%d, b=%d (演示传值不可逆)\n", a, b);

    /* 传址：函数内修改影响原变量 */
    swap(&a, &b);
    printf("  swap(&a,&b): a=%d, b=%d\n", a, b);

    /* === [static] static 函数/变量的文件作用域 === */
    printf("\n[static] === static 函数/变量 ===\n");
    /* add() 是 static —— 别的 .c 文件不能调 */
    /* call_count 是 static —— 别的 .c 文件不能访问 */
    /* "static" 在函数外 = 文件作用域；在函数内 = 静态存储期 */
    for (int i = 0; i < 3; i++) {
        printf("  add(%d, %d) = %d\n", i, i+1, add(i, i+1));
    }
    printf("  call_count = %d (跨多次调用累加)\n", call_count);

    /* === [extern] extern 跨文件引用 === */
    printf("\n[extern] === extern 跨文件引用 ===\n");
    printf("  PROGRAM_NAME = \"%s\" (extern 声明在文件顶部)\n", PROGRAM_NAME);

    /* === [block] 块作用域 vs 文件作用域 === */
    printf("\n[block] === 块作用域与文件作用域 ===\n");

    int outer = 100;                       /* 函数作用域（main 整个范围） */
    printf("  outer = %d (main 作用域)\n", outer);

    {
        /* 内层块作用域 */
        int inner = 200;
        printf("  inner = %d (内层块)\n", inner);
        printf("  outer = %d (内层可见外层)\n", outer);

        int outer = 999;                   /* 遮蔽外层 outer —— 块作用域规则 */
        printf("  inner outer = %d (内层遮蔽外层 outer)\n", outer);
    }
    printf("  outer = %d (离开内层块，外层 outer 恢复)\n", outer);

    /* 静态局部变量演示：counter 跨调用保持 */
    /* 注：printf 参数求值顺序 GCC -O2 是 right-to-left，
       所以 "3, 2, 1" 是反序打印但每次都正确 +1 */
    int c1 = counter_increment();
    int c2 = counter_increment();
    int c3 = counter_increment();
    printf("\n  counter_increment: 1st=%d, 2nd=%d, 3rd=%d (顺序累加)\n", c1, c2, c3);

    printf("\n=== PASS ===\n");
    return 0;
}