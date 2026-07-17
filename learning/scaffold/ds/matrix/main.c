/**
 * @file main.c
 * @brief 矩阵运算：矩阵乘法、矩阵快速幂、稀疏矩阵压缩
 *
 * 演示内容：
 * 1. 矩阵乘法（三层循环 + 缓存友好分块）
 * 2. 矩阵快速幂（求斐波那契数列）
 * 3. 稀疏矩阵压缩存储 (CSR/CSC)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 * 稠密矩阵定义与基本操作
 * ══════════════════════════════════════════════════════════════════════ */

typedef struct {
    int rows;       /* 行数 */
    int cols;       /* 列数 */
    int **data;     /* 二维数组，按行存储 */
} Matrix;

/** 创建矩阵 */
static Matrix *matrix_create(int rows, int cols)
{
    Matrix *m = (Matrix *)malloc(sizeof(Matrix));
    m->rows = rows;
    m->cols = cols;
    m->data = (int **)malloc((size_t)rows * sizeof(int *));
    for (int i = 0; i < rows; i++) {
        m->data[i] = (int *)calloc((size_t)cols, sizeof(int));
    }
    return m;
}

/** 释放矩阵 */
static void matrix_free(Matrix *m)
{
    if (!m) return;
    for (int i = 0; i < m->rows; i++) {
        free(m->data[i]);
    }
    free(m->data);
    free(m);
}

/** 创建单位矩阵 */
static Matrix *matrix_identity(int n)
{
    Matrix *m = matrix_create(n, n);
    for (int i = 0; i < n; i++) {
        m->data[i][i] = 1;
    }
    return m;
}

/** 打印矩阵 */
static void matrix_print(const Matrix *m)
{
    for (int i = 0; i < m->rows; i++) {
        printf("    [");
        for (int j = 0; j < m->cols; j++) {
            printf("%5d", m->data[i][j]);
            if (j < m->cols - 1) printf(" ");
        }
        printf("]\n");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 矩阵乘法
 * ══════════════════════════════════════════════════════════════════════ */

/**
 * 标准矩阵乘法 C = A × B
 * 时间复杂度: O(n³)
 * 空间复杂度: O(1) 额外空间（原地计算）
 */
static Matrix *matrix_multiply(const Matrix *A, const Matrix *B)
{
    if (A->cols != B->rows) {
        printf("    错误: 矩阵维度不匹配!\n");
        return NULL;
    }

    Matrix *C = matrix_create(A->rows, B->cols);

    for (int i = 0; i < A->rows; i++) {
        for (int j = 0; j < B->cols; j++) {
            int sum = 0;
            for (int k = 0; k < A->cols; k++) {
                sum += A->data[i][k] * B->data[k][j];
            }
            C->data[i][j] = sum;
        }
    }

    return C;
}

/**
 * 矩阵乘法（缓存友好优化版，分块计算）
 * 将大矩阵分成 64×64 小块，提高 CPU 缓存命中率
 */
static Matrix *matrix_multiply_blocked(const Matrix *A, const Matrix *B, int block_size)
{
    if (A->cols != B->rows) {
        printf("    错误: 矩阵维度不匹配!\n");
        return NULL;
    }

    Matrix *C = matrix_create(A->rows, B->cols);

    for (int ii = 0; ii < A->rows; ii += block_size) {
        for (int jj = 0; jj < B->cols; jj += block_size) {
            for (int kk = 0; kk < A->cols; kk += block_size) {

                /* 处理每个小块 */
                int i_max = (ii + block_size < A->rows) ? ii + block_size : A->rows;
                int j_max = (jj + block_size < B->cols) ? jj + block_size : B->cols;
                int k_max = (kk + block_size < A->cols) ? kk + block_size : A->cols;

                for (int i = ii; i < i_max; i++) {
                    for (int j = jj; j < j_max; j++) {
                        int sum = 0;
                        for (int k = kk; k < k_max; k++) {
                            sum += A->data[i][k] * B->data[k][j];
                        }
                        C->data[i][j] += sum;
                    }
                }
            }
        }
    }

    return C;
}

/* ══════════════════════════════════════════════════════════════════════
 * 矩阵快速幂
 * ══════════════════════════════════════════════════════════════════════ */

/** 矩阵乘法（2×2 整数版，用于斐波那契） */
static void mat2x2_multiply(int a[2][2], int b[2][2], int c[2][2])
{
    c[0][0] = a[0][0]*b[0][0] + a[0][1]*b[1][0];
    c[0][1] = a[0][0]*b[0][1] + a[0][1]*b[1][1];
    c[1][0] = a[1][0]*b[0][0] + a[1][1]*b[1][0];
    c[1][1] = a[1][0]*b[0][1] + a[1][1]*b[1][1];
}

/** 矩阵快速幂（2×2） */
static void mat2x2_power(int m[2][2], int n, int result[2][2])
{
    /* 初始化为单位矩阵 */
    result[0][0] = 1; result[0][1] = 0;
    result[1][0] = 0; result[1][1] = 1;

    int base[2][2];
    memcpy(base, m, sizeof(base));

    while (n > 0) {
        if (n & 1) {
            int temp[2][2];
            mat2x2_multiply(result, base, temp);
            memcpy(result, temp, sizeof(temp));
        }
        n >>= 1;
        if (n > 0) {
            int temp[2][2];
            mat2x2_multiply(base, base, temp);
            memcpy(base, temp, sizeof(base));
        }
    }
}

/**
 * 用矩阵快速幂求斐波那契数列第 n 项
 * 原理: [F(n), F(n-1)]^T = A^{n-1} × [F(1), F(0)]^T
 * 其中 A = [[1,1], [1,0]]
 */
static long long fibonacci_fast(int n)
{
    if (n == 0) return 0;
    if (n == 1) return 1;

    int A[2][2] = {{1, 1}, {1, 0}};
    int R[2][2];

    mat2x2_power(A, n - 1, R);
    return R[0][0];  /* F(n) = R[0][0] */
}

/* ══════════════════════════════════════════════════════════════════════
 * 稀疏矩阵 CSR 压缩
 * ══════════════════════════════════════════════════════════════════════ */

/** CSR 格式稀疏矩阵 */
typedef struct {
    int rows;           /* 行数 */
    int cols;           /* 列数 */
    int nnz;            /* 非零元素个数 */
    double *values;     /* 非零值数组 */
    int *col_indices;    /* 列索引数组 */
    int *row_ptr;       /* 行指针数组 (长度 rows+1) */
} SparseMatrix;

/** 从稠密矩阵创建 CSR 格式 */
static SparseMatrix *sparse_from_dense(const Matrix *dense)
{
    SparseMatrix *sp = (SparseMatrix *)malloc(sizeof(SparseMatrix));
    sp->rows = dense->rows;
    sp->cols = dense->cols;

    /* 先统计非零元素个数 */
    int nnz = 0;
    for (int i = 0; i < dense->rows; i++) {
        for (int j = 0; j < dense->cols; j++) {
            if (dense->data[i][j] != 0) nnz++;
        }
    }
    sp->nnz = nnz;

    /* 分配内存 */
    sp->values = (double *)malloc((size_t)nnz * sizeof(double));
    sp->col_indices = (int *)malloc((size_t)nnz * sizeof(int));
    sp->row_ptr = (int *)malloc((size_t)(dense->rows + 1) * sizeof(int));

    /* 填充 CSR 数据 */
    int idx = 0;
    sp->row_ptr[0] = 0;
    for (int i = 0; i < dense->rows; i++) {
        for (int j = 0; j < dense->cols; j++) {
            if (dense->data[i][j] != 0) {
                sp->values[idx] = (double)dense->data[i][j];
                sp->col_indices[idx] = j;
                idx++;
            }
        }
        sp->row_ptr[i + 1] = idx;
    }

    return sp;
}

/** 释放稀疏矩阵 */
static void sparse_free(SparseMatrix *sp)
{
    if (!sp) return;
    free(sp->values);
    free(sp->col_indices);
    free(sp->row_ptr);
    free(sp);
}

/** 打印稀疏矩阵 (CSR 格式) */
static void sparse_print(const SparseMatrix *sp)
{
    printf("    CSR 稀疏矩阵 (%d×%d, %d 个非零元素):\n",
           sp->rows, sp->cols, sp->nnz);
    printf("    values:     [");
    for (int i = 0; i < sp->nnz; i++) {
        printf("%.0f", sp->values[i]);
        if (i < sp->nnz - 1) printf(", ");
    }
    printf("]\n");

    printf("    col_indices: [");
    for (int i = 0; i < sp->nnz; i++) {
        printf("%d", sp->col_indices[i]);
        if (i < sp->nnz - 1) printf(", ");
    }
    printf("]\n");

    printf("    row_ptr:     [");
    for (int i = 0; i <= sp->rows; i++) {
        printf("%d", sp->row_ptr[i]);
        if (i < sp->rows) printf(", ");
    }
    printf("]\n");
}

/* ══════════════════════════════════════════════════════════════════════
 * 演示函数
 * ══════════════════════════════════════════════════════════════════════ */

static void demo_matrix_multiply(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 矩阵乘法\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 创建测试矩阵 */
    Matrix *A = matrix_create(3, 3);
    Matrix *B = matrix_create(3, 3);

    /* 填充数据: A = 1,2,3 / 4,5,6 / 7,8,9 */
    int val = 1;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            A->data[i][j] = val;
            B->data[i][j] = val++;
        }
    }

    printf("\n  矩阵 A:\n");
    matrix_print(A);
    printf("\n  矩阵 B:\n");
    matrix_print(B);

    Matrix *C = matrix_multiply(A, B);
    if (C) {
        printf("\n  A × B (标准算法):\n");
        matrix_print(C);
        matrix_free(C);
    }

    Matrix *D = matrix_multiply_blocked(A, B, 2);
    if (D) {
        printf("\n  A × B (分块优化):\n");
        matrix_print(D);
        matrix_free(D);
    }

    matrix_free(A);
    matrix_free(B);
}

static void demo_fibonacci_power(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: 矩阵快速幂求斐波那契\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    printf("\n  斐波那契数列 (矩阵快速幂 O(log n)):\n");
    printf("    n:    ");
    for (int n = 0; n <= 15; n++) {
        printf("%4d ", n);
    }
    printf("\n    F(n): ");
    for (int n = 0; n <= 15; n++) {
        printf("%4lld ", fibonacci_fast(n));
    }
    printf("\n");

    /* 大数斐波那契 */
    printf("\n  较大项斐波那契:\n");
    printf("    F(30) = %lld\n", fibonacci_fast(30));
    printf("    F(40) = %lld\n", fibonacci_fast(40));
    printf("    F(50) = %lld\n", fibonacci_fast(50));
    printf("    F(60) = %lld\n", fibonacci_fast(60));
}

static void demo_sparse_matrix(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 稀疏矩阵 CSR 压缩\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 创建稀疏矩阵示例:
     * [1 0 0 2]
     * [0 0 3 0]
     * [0 4 0 5]
     */
    Matrix *dense = matrix_create(3, 4);
    dense->data[0][0] = 1;  dense->data[0][3] = 2;
    dense->data[1][2] = 3;
    dense->data[2][1] = 4;  dense->data[2][3] = 5;

    printf("\n  稠密矩阵 (3×4):\n");
    matrix_print(dense);

    SparseMatrix *sp = sparse_from_dense(dense);
    sparse_print(sp);

    /* 空间对比 */
    size_t dense_size = (size_t)dense->rows * dense->cols * sizeof(int);
    size_t sparse_size = (size_t)sp->nnz * sizeof(double)
                       + (size_t)sp->nnz * sizeof(int)
                       + (size_t)(sp->rows + 1) * sizeof(int);
    printf("\n  空间对比:\n");
    printf("    稠密格式: %zu 字节\n", dense_size);
    printf("    CSR 格式: %zu 字节\n", sparse_size);
    printf("    压缩比:   %.1f%%\n", 100.0 * sparse_size / dense_size);

    sparse_free(sp);
    matrix_free(dense);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║           矩阵运算: 乘法 / 快速幂 / 稀疏矩阵                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_matrix_multiply();
    demo_fibonacci_power();
    demo_sparse_matrix();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  核心要点:\n");
    printf("    矩阵乘法: O(n³) — 分块优化提高缓存命中率\n");
    printf("    矩阵幂:   O(n³ log k) — 二分幂加速\n");
    printf("    稀疏矩阵: CSR = values + col_indices + row_ptr\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
