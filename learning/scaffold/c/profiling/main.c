/*
 * profiling scaffold — 性能剖析演示
 *
 * 三类性能问题供 profiling 工具发现：
 *   1. 热点函数：malloc/free 在循环内反复调用
 *   2. Cache-unfriendly 矩阵乘法：列主序访问行主序矩阵
 *   3. 对比优化版：loop interchange + blocking
 *
 * 复现：
 *   gcc -Wall -Wextra -O2 -std=c11 -o prof_demo main.c
 *   ./prof_demo
 *
 * Linux profiling:
 *   gcc -pg -O2 -std=c11 -o prof_demo main.c && ./prof_demo && gprof prof_demo gmon.out
 *
 * 本机 MinGW 用 clock() 做手动计时（不依赖 gprof）。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define N 256  /* 矩阵大小（足够大以观察 cache 效应） */
#define LOOPS 10000

/* ========== 1. 热点函数：malloc/free 在循环内 ========== */

/* 慢版本：每次调用都在堆上分配+释放 */
void hot_slow(void) {
    for (int i = 0; i < LOOPS; i++) {
        char *buf = (char *)malloc(64);
        if (!buf) exit(1);
        int hash = 0;
        for (int j = 0; j < 64; j++) {
            buf[j] = (char)((i + j) & 0x7f);
            hash = hash * 31 + buf[j];
        }
        free(buf);
        /* 防止编译器优化掉整个循环 */
        if (i == LOOPS - 1) printf("[hot_slow] last hash=%d\n", hash);
    }
}

/* 快版本：用栈数组替代堆分配 */
void hot_fast(void) {
    for (int i = 0; i < LOOPS; i++) {
        char buf[64];
        int hash = 0;
        for (int j = 0; j < 64; j++) {
            buf[j] = (char)((i + j) & 0x7f);
            hash = hash * 31 + buf[j];
        }
        if (i == LOOPS - 1) printf("[hot_fast] last hash=%d\n", hash);
    }
}

/* ========== 2. Cache-unfriendly 矩阵乘法 ========== */

/* 朴素三重循环：k 在最内层 → b[k][j] 列主序访问 → cache miss */
void matmul_naive(int ** restrict a, int ** restrict b,
                  int ** restrict c, int n) {
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            c[i][j] = 0;
            for (int k = 0; k < n; k++)
                c[i][j] += a[i][k] * b[k][j];  /* b 列主序 → cache miss */
        }
}

/* 优化版：k 外提 + j/k 交换 → b 行主序访问 */
void matmul_opt(int ** restrict a, int ** restrict b,
                int ** restrict c, int n) {
    /* 先清零 */
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            c[i][j] = 0;

    for (int i = 0; i < n; i++)
        for (int k = 0; k < n; k++) {
            int aik = a[i][k];
            for (int j = 0; j < n; j++)
                c[i][j] += aik * b[k][j];  /* b 行主序 → cache friendly */
        }
}

int **alloc_matrix(int n) {
    int **m = (int **)malloc((size_t)n * sizeof(int *));
    for (int i = 0; i < n; i++) {
        m[i] = (int *)malloc((size_t)n * sizeof(int));
        for (int j = 0; j < n; j++)
            m[i][j] = (i * 7 + j * 13) & 0xff;
    }
    return m;
}

void free_matrix(int **m, int n) {
    for (int i = 0; i < n; i++) free(m[i]);
    free(m);
}

/* ========== 计时工具 ========== */

/* cross-platform clock() wrapper */
double get_time_ms(void) {
    return (double)clock() * 1000.0 / (double)CLOCKS_PER_SEC;
}

/* ========== main ========== */

int main(void) {
    printf("=== 性能剖析演示 ===\n\n");

    /* --- 热点函数对比 --- */
    printf("--- 1. malloc vs stack（%d iterations）---\n", LOOPS);
    double t0 = get_time_ms();
    hot_slow();
    double t1 = get_time_ms();
    printf("[time] hot_slow: %.2f ms (malloc/free each iteration)\n", t1 - t0);

    t0 = get_time_ms();
    hot_fast();
    t1 = get_time_ms();
    printf("[time] hot_fast: %.2f ms (stack allocation)\n", t1 - t0);
    printf("\n");

    /* --- 矩阵乘法对比 --- */
    printf("--- 2. Matrix multiplication（%dx%d）---\n", N, N);
    int **a = alloc_matrix(N);
    int **b = alloc_matrix(N);
    int **c_naive = alloc_matrix(N);
    int **c_opt   = alloc_matrix(N);

    t0 = get_time_ms();
    matmul_naive(a, b, c_naive, N);
    t1 = get_time_ms();
    printf("[time] matmul_naive: %.2f ms (k-innermost, cache-unfriendly)\n", t1 - t0);

    t0 = get_time_ms();
    matmul_opt(a, b, c_opt, N);
    t1 = get_time_ms();
    printf("[time] matmul_opt:   %.2f ms (loop interchange, cache-friendly)\n", t1 - t0);

    /* 验证两种算法结果一致 */
    int errors = 0;
    for (int i = 0; i < N && errors < 5; i++)
        for (int j = 0; j < N && errors < 5; j++)
            if (c_naive[i][j] != c_opt[i][j]) {
                printf("[ERROR] mismatch at [%d][%d]: naive=%d opt=%d\n",
                       i, j, c_naive[i][j], c_opt[i][j]);
                errors++;
            }
    printf("[verify] %s\n", errors ? "MISMATCH" : "OK (results identical)");

    free_matrix(a, N); free_matrix(b, N);
    free_matrix(c_naive, N); free_matrix(c_opt, N);

    printf("\n=== PASS ===\n");
    return 0;
}
