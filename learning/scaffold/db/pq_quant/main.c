/**
 * @file main.c
 * @brief PQ（乘积量化）原理演示程序
 *
 * 本程序演示 PQ 的核心概念：
 * - 子空间划分（Sub-space Partitioning）
 * - 码本训练（Codebook Training）
 * - 编码（Encoding）
 * - 查表距离计算（Lookup Table Distance）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define DIM             8       /* 向量维度 */
#define SUB_DIMS        4       /* 子空间维度（2 个子空间） */
#define NUM_SUBQUANTIZERS   (DIM / SUB_DIMS)  /* 子量化器数量：2 */
#define CODEBOOK_SIZE   4       /* 码本大小（每个子空间 4 个码字） */
#define NUM_VECTORS     6       /* 演示用向量数量 */

/* ============================================================
 * 数据结构定义
 * ============================================================ */

/**
 * @brief 向量结构
 */
typedef struct {
    float data[DIM];            /* 原始向量 */
    int   code[NUM_SUBQUANTIZERS]; /* 编码结果（每个子空间一个码字索引） */
} Vector;

/**
 * @brief 码本结构（每个子空间一个）
 */
typedef struct {
    float centroids[CODEBOOK_SIZE][SUB_DIMS];  /* 码字集合 */
} Codebook;

/**
 * @brief PQ 量化器
 */
typedef struct {
    Codebook codebooks[NUM_SUBQUANTIZERS];  /* 多个子码本 */
    float   lookup_table[NUM_SUBQUANTIZERS][CODEBOOK_SIZE]; /* 距离查表 */
} PQQuantizer;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 计算两个向量的欧氏距离
 */
static float euclidean_distance(const float *a, const float *b, int dim) {
    float dist = 0.0f;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return sqrtf(dist);
}

/**
 * 打印向量
 */
static void print_vector(const char *name, const float *vec, int dim) {
    printf("%s: [", name);
    for (int i = 0; i < dim; i++) {
        printf("%6.2f%s", vec[i], (i < dim - 1) ? ", " : "");
    }
    printf("]\n");
}

/**
 * 打印码本
 */
static void print_codebook(const Codebook *cb, int sub_idx) {
    printf("  子空间 %d 码本（%d 个码字）：\n", sub_idx, CODEBOOK_SIZE);
    for (int i = 0; i < CODEBOOK_SIZE; i++) {
        printf("    码字 %d: [", i);
        for (int j = 0; j < SUB_DIMS; j++) {
            printf("%6.2f%s", cb->centroids[i][j], (j < SUB_DIMS - 1) ? ", " : "");
        }
        printf("]\n");
    }
}

/* ============================================================
 * PQ 核心操作
 * ============================================================ */

/**
 * 训练阶段：使用简化的 K-Means 构建码本
 * 实际实现参考 engineering/src/algo-prod/quantization/pq.c
 */
static void train_codebook(PQQuantizer *pq, Vector *vectors, int num_vecs) {
    printf("\n========== 1. 码本训练（训练阶段）==========\n\n");

    /* 简化版 K-Means 训练：对每个子空间独立训练 */
    for (int sub = 0; sub < NUM_SUBQUANTIZERS; sub++) {
        printf("训练子空间 %d...\n", sub);

        /* 提取子向量并统计范围 */
        float subvecs[NUM_VECTORS][SUB_DIMS];
        for (int v = 0; v < num_vecs; v++) {
            for (int d = 0; d < SUB_DIMS; d++) {
                subvecs[v][d] = vectors[v].data[sub * SUB_DIMS + d];
            }
        }

        /* 简化训练：直接取训练样本作为初始码字 */
        /* 实际 pq.c 中使用 KMeans 算法，params.n_init=8, max_iter=100 */
        for (int c = 0; c < CODEBOOK_SIZE && c < num_vecs; c++) {
            int sample_idx = (c * num_vecs) / CODEBOOK_SIZE;
            for (int d = 0; d < SUB_DIMS; d++) {
                pq->codebooks[sub].centroids[c][d] = subvecs[sample_idx][d];
            }
        }

        print_codebook(&pq->codebooks[sub], sub);
    }
}

/**
 * 编码阶段：为每个向量分配码字
 */
static void encode_vectors(PQQuantizer *pq, Vector *vectors, int num_vecs) {
    printf("\n========== 2. 编码（Encode）==========\n\n");

    for (int v = 0; v < num_vecs; v++) {
        printf("向量 %d: ", v);
        print_vector("原始", vectors[v].data, DIM);

        /* 对每个子空间找最近码字 */
        for (int sub = 0; sub < NUM_SUBQUANTIZERS; sub++) {
            float min_dist = euclidean_distance(
                &vectors[v].data[sub * SUB_DIMS],
                pq->codebooks[sub].centroids[0],
                SUB_DIMS
            );
            int best_code = 0;

            for (int c = 1; c < CODEBOOK_SIZE; c++) {
                float dist = euclidean_distance(
                    &vectors[v].data[sub * SUB_DIMS],
                    pq->codebooks[sub].centroids[c],
                    SUB_DIMS
                );
                if (dist < min_dist) {
                    min_dist = dist;
                    best_code = c;
                }
            }
            vectors[v].code[sub] = best_code;
            printf("  子空间 %d -> 码字 %d（距离 %.2f）\n", sub, best_code, min_dist);
        }
        printf("  编码结果: [%d, %d]\n\n", vectors[v].code[0], vectors[v].code[1]);
    }
}

/**
 * 构建查表：预计算所有子空间中向量与码字的距离
 */
static void build_lookup_table(PQQuantizer *pq, const float *query) {
    printf("========== 3. 构建距离查表 ==========\n\n");

    /* 对每个子空间，计算查询向量子向量到所有码字的距离 */
    for (int sub = 0; sub < NUM_SUBQUANTIZERS; sub++) {
        printf("子空间 %d 查表（查询子向量）:\n", sub);
        for (int c = 0; c < CODEBOOK_SIZE; c++) {
            float dist = euclidean_distance(
                &query[sub * SUB_DIMS],
                pq->codebooks[sub].centroids[c],
                SUB_DIMS
            );
            pq->lookup_table[sub][c] = dist;
            printf("  与码字 %d 距离: %.2f\n", c, dist);
        }
    }
}

/**
 * 查表计算两个编码向量的距离（无需解码）
 */
static float lookup_distance(PQQuantizer *pq, const Vector *v1, const Vector *v2) {
    float total_dist = 0.0f;

    /* 每个子空间：直接查表获取距离，然后求和 */
    for (int sub = 0; sub < NUM_SUBQUANTIZERS; sub++) {
        int code1 = v1->code[sub];
        int code2 = v2->code[sub];
        float sub_dist = pq->lookup_table[sub][code1] + pq->lookup_table[sub][code2];
        /* 实际使用平方距离和：d² = (a-c)² + (b-d)² */
        /* 这里简化为加和：d ≈ d(a,c) + d(b,d) */
        total_dist += sub_dist;
    }

    return total_dist;
}

/**
 * 演示搜索：查找最相似的向量
 */
static void demo_search(PQQuantizer *pq, Vector *vectors, int num_vecs, const float *query) {
    printf("\n========== 4. 近似最近邻搜索 ==========\n\n");

    print_vector("查询向量", query, DIM);

    /* 构建查表 */
    build_lookup_table(pq, query);

    /* 查表距离计算 */
    printf("\n查表距离计算结果：\n");
    float min_dist = lookup_distance(pq, &vectors[0], &vectors[0]);
    int best_idx = 0;

    for (int i = 0; i < num_vecs; i++) {
        float dist = lookup_distance(pq, &vectors[i], &vectors[0]); /* 简化：与向量0比较 */
        printf("  向量 %d 距离: %.2f\n", i, dist);

        if (dist < min_dist) {
            min_dist = dist;
            best_idx = i;
        }
    }

    printf("\n最近似向量: 向量 %d（查表距离 %.2f）\n", best_idx, min_dist);
}

/* ============================================================
 * 主函数：演示 PQ 全流程
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("      PQ（乘积量化）原理演示\n");
    printf("========================================\n");

    /* 初始化 PQ 量化器 */
    PQQuantizer pq;
    memset(&pq, 0, sizeof(pq));

    /* 准备演示向量 */
    Vector vectors[NUM_VECTORS] = {
        {{10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f}, {0, 0}},
        {{12.0f, 22.0f, 32.0f, 42.0f, 52.0f, 62.0f, 72.0f, 82.0f}, {0, 0}},
        {{11.0f, 21.0f, 31.0f, 41.0f, 51.0f, 61.0f, 71.0f, 81.0f}, {0, 0}},
        {{ 8.0f, 18.0f, 28.0f, 38.0f, 48.0f, 58.0f, 68.0f, 78.0f}, {0, 0}},
        {{15.0f, 25.0f, 35.0f, 45.0f, 55.0f, 65.0f, 75.0f, 85.0f}, {0, 0}},
        {{ 9.0f, 19.0f, 29.0f, 39.0f, 49.0f, 59.0f, 69.0f, 79.0f}, {0, 0}},
    };

    printf("\n原始向量集合（%d 个 %d 维向量）：\n", NUM_VECTORS, DIM);
    for (int i = 0; i < NUM_VECTORS; i++) {
        printf("向量 %d: ", i);
        print_vector("data", vectors[i].data, DIM);
    }

    /* 训练码本 */
    train_codebook(&pq, vectors, NUM_VECTORS);

    /* 编码向量 */
    encode_vectors(&pq, vectors, NUM_VECTORS);

    /* 演示搜索 */
    float query[DIM] = {11.0f, 21.0f, 31.0f, 41.0f, 51.0f, 61.0f, 71.0f, 81.0f};
    demo_search(&pq, vectors, NUM_VECTORS, query);

    printf("\n========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");

    return 0;
}
