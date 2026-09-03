/*
 * static_analysis scaffold — 静态分析工具演示
 *
 * 本文件故意包含多种 GCC -Wall -Wextra 能检测到的代码问题。
 * 每个问题都可被静态分析工具（GCC Warnings / cppcheck / clang-tidy）发现。
 *
 * 复现（编译并观察警告）：
 *   gcc -Wall -Wextra -Wpedantic -Wconversion -std=c11 -c main.c
 *
 * 预期：至少 4 条编译警告
 *
 * 注：本文件本身可以编译通过——它只是触发 warning，不是 error。
 *     用 gcc -Werror 可以把 warning 升级为 error（CI gate 常见做法）。
 */

#include <stdio.h>
#include <stdlib.h>

/* --- 问题 1: -Wunused-function --- */
/* 静态函数未被调用——编译器警告"定义了但未使用" */
static int unused_helper(int x) {
    return x * x + 1;
}

/* --- 问题 2: -Wmaybe-uninitialized --- */
/* 当 flag==0 时 val 未初始化就返回 */
int maybe_uninit(int flag) {
    int val;                    /* 未初始化 */
    if (flag) {
        val = 42;
    }
    return val;                 /* flag==0 时返回未初始化值 */
}

/* --- 问题 3: -Wsign-compare --- */
/* 有符号和无符号比较——语义隐晦 */
int sign_compare(unsigned int u, int s) {
    return u < (unsigned int)s;
}

/* --- 问题 4: 死代码（逻辑错误） --- */
/* 第二个条件永远不会为真（被第一个条件覆盖） */
int dead_code(int x) {
    if (x > 0) {
        return 1;
    } else if (x > 100) {      /* 死代码：x>0 已经拦截了所有 >100 的情况 */
        return 2;
    }
    return 0;
}

/* --- 问题 5: -Wconversion 隐式类型转换 --- */
/* size_t → int 可能截断 */
int implicit_convert(size_t n) {
    return (int)(n * 2);       /* n*2 可能溢出 int */
}

/* --- 问题 6: 资源泄露（逻辑层面） --- */
/* 部分路径不释放 malloc 的内存——cppcheck 可检测 */
int *resource_leak(int flag) {
    int *p = (int *)malloc(sizeof(int) * 100);
    if (p == NULL) return NULL;
    if (flag) {
        return p;              /* OK：调用者负责 free */
    }
    /* BUG: flag==0 时 p 泄露（函数返回但不 free p） */
    return NULL;
}

/* --- main: 调用所有函数以消除 unused warning（但不运行，仅编译检查） --- */
int main(void) {
    printf("=== 静态分析演示 ===\n");
    printf("本文件用于触发静态分析工具——用 'gcc -Wall -Wextra -c main.c' 编译观察警告。\n");
    printf("也可以用 cppcheck/flawfinder 做更深层检查（本机不可用则 skip）。\n\n");

    /* 运行几个函数观察行为 */
    printf("[maybe_uninit] flag=1 → %d\n", maybe_uninit(1));
    printf("[sign_compare] u=5 s=-1 → %d (signed cmp!)\n", sign_compare(5, -1));
    printf("[dead_code] x=200 → %d\n", dead_code(200));
    printf("[implicit_convert] n=1<<30 → %d\n", implicit_convert((size_t)1 << 30));

    int *p = resource_leak(1);
    if (p) free(p);

    printf("\n=== PASS ===\n");
    return 0;
}
