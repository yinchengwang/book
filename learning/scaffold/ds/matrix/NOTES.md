# 工程对照说明

## 与工程模块的关联

### 工程矩阵计算库

`engineering/src/algo/matrix.c` 提供了完整的矩阵运算库：

```c
/* 矩阵乘法 (优化版，使用 SIMD) */
void matrix_multiply_simd(const matrix_t *A, const matrix_t *B, matrix_t *C) {
    for (int i = 0; i < A->rows; i++) {
        for (int k = 0; k < A->cols; k++) {
            /* 使用 SIMD 同时计算多个元素 */
            __m256 a_vec = _mm256_set1_ps(A->data[i][k]);
            for (int j = 0; j < B->cols; j += 8) {
                __m256 b_vec = _mm256_loadu_ps(&B->data[k][j]);
                __m256 c_vec = _mm256_loadu_ps(&C->data[i][j]);
                c_vec = _mm256_fmadd_ps(a_vec, b_vec, c_vec);
                _mm256_storeu_ps(&C->data[i][j], c_vec);
            }
        }
    }
}
```

### 图算法的矩阵表示

图论算法中，矩阵是核心数据结构：

```c
/* 邻接矩阵的幂表示路径数 */
void count_paths_matrix(int **adj, int n, int **path_count) {
    /* path_count[i][j][k] = i 到 j 经过 k 条边的路径数 */
    for (int k = 2; k <= MAX_K; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                for (int m = 0; m < n; m++) {
                    path_count[i][j][k] += path_count[i][m][k-1] * adj[m][j];
                }
            }
        }
    }
}
```

### PageRank 算法中的矩阵运算

PageRank 使用幂迭代法计算网页排名：
```c
/* PageRank 迭代: PR = d * M^T * PR + (1-d)/n * 1 */
void pagerank_iteration(double *pr, const sparse_matrix_t *M,
                        double damping, int n) {
    double *new_pr = calloc(n, sizeof(double));

    for (int j = 0; j < n; j++) {
        for (int i = M->row_ptr[j]; i < M->row_ptr[j+1]; i++) {
            int row = M->col_indices[i];
            new_pr[row] += damping * M->values[i] * pr[j];
        }
    }

    /* 加上跳转概率 */
    for (int i = 0; i < n; i++) {
        new_pr[i] += (1.0 - damping) / n;
    }

    memcpy(pr, new_pr, n * sizeof(double));
    free(new_pr);
}
```

### 稀疏矩阵格式对比

| 格式 | 适用场景 | 访问模式 |
|------|----------|----------|
| COO | 构建阶段 | 双数组遍历 |
| CSR | 按行访问 | 稀疏矩阵乘法 |
| CSC | 按列访问 | 稀疏矩阵转置 |
| ELL | 固定稀疏度 | GPU 高效 |

### 矩阵分块与缓存优化

`engineering/src/algo/matrix_block.c` 实现分块乘法：

```c
/* 分块矩阵乘法 - 优化缓存命中率 */
void matrix_multiply_blocked(const matrix_t *A, const matrix_t *B,
                             matrix_t *C, int block_size) {
    for (int ii = 0; ii < n; ii += block_size) {
        for (int jj = 0; jj < n; jj += block_size) {
            for (int kk = 0; kk < n; kk += block_size) {
                /* 处理 B×B 小块，全部加载到 L1 cache */
                int i_max = min(ii + block_size, n);
                int j_max = min(jj + block_size, n);
                int k_max = min(kk + block_size, n);

                for (int i = ii; i < i_max; i++) {
                    for (int k = kk; k < k_max; k++) {
                        float a_ik = A->data[i][k];
                        for (int j = jj; j < j_max; j++) {
                            C->data[i][j] += a_ik * B->data[k][j];
                        }
                    }
                }
            }
        }
    }
}
```

### 数据库索引中的矩阵运算

向量索引（如 HNSW）的距离计算：
```c
/* 内积距离计算（矩阵运算的特例） */
float cosine_distance(const float *a, const float *b, int dim) {
    float dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    return 1.0f - dot / (sqrtf(norm_a) * sqrtf(norm_b));
}
```

本模块的矩阵乘法是上述向量运算的二维推广。
