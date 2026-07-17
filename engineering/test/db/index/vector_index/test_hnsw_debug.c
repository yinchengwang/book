/*
 * test_hnsw_debug.c
 *
 * 使用小数据集测试 HNSW 图构建过程，验证邻居选择逻辑
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 简化的 HNSW 结构用于调试 */
#define TEST_DIMS 4
#define TEST_M 4
#define TEST_N_VECTORS 10

static float test_vectors[TEST_N_VECTORS][TEST_DIMS] = {
    {0.0, 0.0, 0.0, 0.0},  /* 原点 */
    {1.0, 0.0, 0.0, 0.0},  /* x 轴 */
    {0.0, 1.0, 0.0, 0.0},  /* y 轴 */
    {0.0, 0.0, 1.0, 0.0},  /* z 轴 */
    {0.0, 0.0, 0.0, 1.0},  /* w 轴 */
    {0.5, 0.5, 0.0, 0.0},  /* xy 平面 */
    {0.5, 0.0, 0.5, 0.0},  /* xz 平面 */
    {0.0, 0.5, 0.5, 0.0},  /* yz 平面 */
    {0.5, 0.5, 0.5, 0.5},  /* 中心偏移 */
    {1.0, 1.0, 1.0, 1.0},  /* 对角 */
};

static float compute_distance(const float *a, const float *b, int dims) {
    float sum = 0.0f;
    for (int i = 0; i < dims; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/* 测试 shrink_neighbor_list 的行为 */
static void test_shrink_behavior(void) {
    printf("\n=== 测试 shrink_neighbor_list 行为 ===\n");

    /* 模拟场景：
     * 查询点 Q 在原点
     * 候选 v1 在 (0.5, 0, 0, 0)，距离 Q 为 0.5
     * 已选 v2 在 (0.4, 0, 0, 0)，距离 Q 为 0.4
     * v1-v2 距离 = 0.1
     */

    float Q[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float v1[4] = {0.5f, 0.0f, 0.0f, 0.0f};
    float v2[4] = {0.4f, 0.0f, 0.0f, 0.0f};

    float dist_v1_q = compute_distance(v1, Q, 4);  /* 0.5 */
    float dist_v2_q = compute_distance(v2, Q, 4);  /* 0.4 */
    float dist_v1_v2 = compute_distance(v1, v2, 4);  /* 0.1 */

    printf("查询点 Q: (%.1f, %.1f, %.1f, %.1f)\n", Q[0], Q[1], Q[2], Q[3]);
    printf("候选 v1: (%.1f, %.1f, %.1f, %.1f), dist(v1, Q) = %.3f\n",
           v1[0], v1[1], v1[2], v1[3], dist_v1_q);
    printf("已选 v2: (%.1f, %.1f, %.1f, %.1f), dist(v2, Q) = %.3f\n",
           v2[0], v2[1], v2[2], v2[3], dist_v2_q);
    printf("v1-v2 距离: %.3f\n", dist_v1_v2);

    printf("\n当前实现逻辑: if (dist_v1_v2 < dist_v1_q) -> 丢弃 v1\n");
    printf("  dist_v1_v2 (%.3f) < dist_v1_q (%.3f)? %s\n",
           dist_v1_v2, dist_v1_q,
           dist_v1_v2 < dist_v1_q ? "是 -> 丢弃 v1" : "否 -> 保留 v1");

    printf("\n问题分析:\n");
    printf("  v2 比 Q 更靠近 v1，但这不代表 v1 没有导航价值！\n");
    printf("  v1 可能是通往其他区域的重要桥梁节点。\n");

    printf("\n正确逻辑 (alpha=1.2):\n");
    float alpha = 1.2f;
    printf("  if (alpha * dist_v1_v2 < dist_v1_q) -> 丢弃 v1\n");
    printf("  %.1f * %.3f = %.3f < %.3f? %s\n",
           alpha, dist_v1_v2, alpha * dist_v1_v2, dist_v1_q,
           alpha * dist_v1_v2 < dist_v1_q ? "是 -> 丢弃" : "否 -> 保留");
}

/* 测试图连通性 */
static void test_graph_connectivity(void) {
    printf("\n=== 测试图连通性 ===\n");

    printf("\n使用 %d 个测试向量:\n", TEST_N_VECTORS);
    for (int i = 0; i < TEST_N_VECTORS; i++) {
        printf("  v%d: (%.1f, %.1f, %.1f, %.1f)\n", i,
               test_vectors[i][0], test_vectors[i][1],
               test_vectors[i][2], test_vectors[i][3]);
    }

    printf("\n计算距离矩阵:\n");
    printf("     ");
    for (int i = 0; i < TEST_N_VECTORS; i++) {
        printf("  v%d  ", i);
    }
    printf("\n");

    for (int i = 0; i < TEST_N_VECTORS; i++) {
        printf("v%d: ", i);
        for (int j = 0; j < TEST_N_VECTORS; j++) {
            float dist = compute_distance(test_vectors[i], test_vectors[j], TEST_DIMS);
            printf("%5.2f ", dist);
        }
        printf("\n");
    }

    printf("\n理想的邻居选择 (M=%d):\n", TEST_M);
    for (int i = 0; i < TEST_N_VECTORS; i++) {
        printf("  v%d 的理想邻居: ", i);

        /* 计算到其他所有点的距离 */
        float dists[TEST_N_VECTORS];
        int ids[TEST_N_VECTORS];
        for (int j = 0; j < TEST_N_VECTORS; j++) {
            dists[j] = compute_distance(test_vectors[i], test_vectors[j], TEST_DIMS);
            ids[j] = j;
        }

        /* 简单排序找最近的 M 个 */
        for (int k = 0; k < TEST_M + 1 && k < TEST_N_VECTORS; k++) {
            for (int l = k + 1; l < TEST_N_VECTORS; l++) {
                if (dists[l] < dists[k]) {
                    float tmp_d = dists[k];
                    dists[k] = dists[l];
                    dists[l] = tmp_d;
                    int tmp_id = ids[k];
                    ids[k] = ids[l];
                    ids[l] = tmp_id;
                }
            }
        }

        for (int k = 1; k <= TEST_M && k < TEST_N_VECTORS; k++) {  /* 跳过自己 */
            printf("v%d(%.2f) ", ids[k], dists[k]);
        }
        printf("\n");
    }
}

int main(void) {
    printf("HNSW 邻居选择逻辑调试\n");
    printf("====================\n");

    test_shrink_behavior();
    test_graph_connectivity();

    printf("\n=== 结论 ===\n");
    printf("shrink_neighbor_list 的当前剪枝逻辑过于激进，\n");
    printf("导致许多有导航价值的邻居被错误丢弃。\n");
    printf("建议引入 alpha 参数（默认 1.2）来放宽剪枝条件。\n");

    return 0;
}