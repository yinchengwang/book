/* undefined_behavior scaffold — 演示 5 类常见 UB 与 UBSAN 拦截 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* 各 demo 用 if(0) {...} 默认跳过 UB，避免误触发；改为 if(1) 才能触发 */

static void demo_signed_overflow(void) {
    /* UB #1: signed integer overflow */
    int a = INT_MAX;
    int b = a + 1;
    (void)b; /* undefined */
    fprintf(stderr, "[UB-1] INT_MAX + 1 = %d (undefined value, may wrap or erase bits)\n", b);
}

static void demo_null_deref(void) {
    /* UB #2: null pointer dereference */
    int *p = NULL;
    int v = *p;
    (void)v;
    fprintf(stderr, "[UB-2] *NULL segfault, but UB-before-crash: read attempted\n");
}

static void demo_oob(void) {
    /* UB #3: out of bounds (stack array) */
    int arr[4] = {1, 2, 3, 4};
    int v = arr[5]; /* index out of bound */
    (void)v;
    fprintf(stderr, "[UB-3] arr[5] out of bounds (may read adjacent stack)\n");
}

static void demo_uaf(void) {
    /* UB #4: use-after-free */
    int *p = malloc(sizeof(int));
    *p = 7;
    free(p);
    fprintf(stderr, "[UB-4] *p after free (heap metadata may be reused)\n");
}

static void demo_sequence_point(void) {
    /* UB #5: undefined order of side effects */
    int i = 5;
    int v = i++ + i++;
    (void)v;
    fprintf(stderr, "[UB-5] i++ + i++ sequence point violation\n");
}

int main(int argc, char **argv) {
    printf("[UB] 编译此文件时加 -fsanitize=undefined,address 可拦截以下 UB\n");
    printf("[UB] 默认跳过 UB 执行，需传 argv[1]=run 才能触发：\n\n");

    int run_all = (argc >= 2 && argv[1][0] == 'r');

    /* 各 demo 单独包函数，main 用 if 控制触发 */
    if (run_all) {
        demo_signed_overflow();
        demo_null_deref();   /* 会段错误，下一行的 printf 不打印 */
    } else {
        printf("[safe] 全 safe 模式：UB 未触发；传 `undefined_behavior run` 真触发\n");
    }
    return 0;
}
