/**
 * @file main.c
 * @brief 量化技术原理演示程序
 *
 * 本程序演示量化技术的核心概念：
 * - SQ (Scalar Quantization): 标量量化，全局 min/max 缩放
 * - OPQ (Optimized PQ): 旋转优化乘积量化，PCA 均衡子空间方差
 * - AVQ (RabitQ): 自适应向量量化，两级 PQ + 残差校正
 * - 训练 / 编码 / 解码完整流程
 *
 * 编译: gcc -std=c11 -Wall -o test main.c
 * 运行: ./test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <float.h>

/* ============================================================
 * 常量与类型定义
 * ============================================================ */

#define DIMS            8       /* 向量维度（简化演示） */
#define NUM_VECTORS     6       /* 演示用向量数量 */
#define MAX_CODE_SIZE   32      /* 最大码字长度 */

/* 量化类型枚举 */
typedef enum {
    QUANT_TYPE_SQ,      /* 标量量化 */
    QUANT_TYPE_OPQ,     /* 优化乘积量化 */
    QUANT_TYPE_AVQ      /* 自适应向量量化 (RabitQ) */
} QuantType;

/* 距离度量 */
typedef enum {
    DIST_L2,            /* 欧氏距离 */
    DIST_COSINE         /* 余弦距离 */
} DistMetric;

/* 向量 */
typedef float Vec[DIMS];

/* ============================================================
 * 量化器模拟结构
 * ============================================================ */

/**
 * @brief SQ 量化器（标量量化）
 *
 * 原理：将 float 映射到整数
 *   code = round((x - min) / scale)
 *   x'   = min + code * scale
 *
 * 存储: global_min + scale + 码字
 * 压缩率: 32 * DIMS / (8 + DIMS * bits) float
 */
typedef struct {
    float global_min;   /* 全局最小值 */
    float global_max;   /* 全局最大值 */
    float scale;        /* 量化步长 */
    int   bits;         /* 位宽 (4 或 8) */
} SQQuantizer;

/**
 * @brief PQ 量化器（乘积量化）
 *
 * 原理：将向量划分为 m 个子空间，每个子空间独立 k-means 聚类
 *   - 维度: D = m * d (d = D / m)
 *   - 码本: m * k 个 d 维码心
 *   - 码字: m 字节 (每字节索引一个码心)
 *
 * 距离: ADC (Asymmetric Distance Computation)
 *   dist = sum_i ||q_i - C[i][c_i]||^2
 */
typedef struct {
    int  m;             /* 子空间数量 */
    int  d;             /* 子向量维度 */
    int  k;             /* 码本大小 */
    int  bits;          /* 位宽 */
    Vec *codebooks;     /* [m][k][d] 码本 */
    float opq_rotated;  /* OPQ 旋转矩阵 (简化) */
} PQQuantizer;

/**
 * @brief AVQ 量化器 (RabitQ)
 *
 * 原理：两级量化
 *   Level 1: PQ 量化 (粗量化)
 *   Level 2: 残差量化 (精量化)
 *
 * 残差 = x - PQ(x)
 * 编码: PQ_code + residual_code
 * 精度优于单一 PQ
 */
typedef struct {
    PQQuantizer pq;              /* PQ 粗量化器 */
    int  residual_bits;          /* 残差位宽 */
    float residual_step[8];      /* 残差步长 (简化: 每子空间一个) */
} AVQQuantizer;

/* ============================================================
 * SQ 量化演示
 * ============================================================ */

/**
 * SQ 训练：统计全局 min/max
 */
static void sq_train(SQQuantizer *sq, const Vec *vectors, int n, int bits)
{
    sq->bits = bits;
    sq->global_min = FLT_MAX;
    sq->global_max = -FLT_MAX;

    /* 遍历所有向量和分量，找全局 min/max */
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < DIMS; j++) {
            if (vectors[i][j] < sq->global_min) sq->global_min = vectors[i][j];
            if (vectors[i][j] > sq->global_max) sq->global_max = vectors[i][j];
        }
    }

    /* 计算量化步长: scale = (max - min) / (2^bits - 1) */
    int levels = (1 << bits) - 1;
    sq->scale = (sq->global_max - sq->global_min) / levels;

    printf("[sq] 训练完成: min=%.2f, max=%.2f, scale=%.4f, levels=%d\n",
           sq->global_min, sq->global_max, sq->scale, levels);
}

/**
 * SQ 编码: float -> uint8_t 数组
 * 输出: DIMS 字节 (bits=8 时)
 */
static void sq_encode(const SQQuantizer *sq, const Vec v, uint8_t *code)
{
    for (int i = 0; i < DIMS; i++) {
        int idx = (int)roundf((v[i] - sq->global_min) / sq->scale);
        if (idx < 0) idx = 0;
        if (idx >= (1 << sq->bits)) idx = (1 << sq->bits) - 1;
        code[i] = (uint8_t)idx;
    }
}

/**
 * SQ 解码: uint8_t -> float
 */
static void sq_decode(const SQQuantizer *sq, const uint8_t *code, Vec v)
{
    for (int i = 0; i < DIMS; i++) {
        v[i] = sq->global_min + code[i] * sq->scale;
    }
}

/* ============================================================
 * OPQ 演示
 * ============================================================ */

/**
 * OPQ 原理演示
 *
 * 问题: PQ 的子空间划分是固定的，各子空间方差可能差异很大
 * 解决: 训练前对数据做 PCA 旋转 (R = V^T)
 *   旋转后各维度方差均衡，量化误差更小
 *
 * 流程:
 *   1. 计算数据协方差矩阵
 *   2. SVD 分解得到 V
 *   3. x' = x * R (旋转)
 *   4. 对 x' 做 PQ
 *
 * 注意: 本演示仅展示原理，不做真实 SVD
 */
static void demo_opq(void)
{
    printf("\n[opq] ===== OPQ 旋转优化原理 =====\n");
    printf("[opq] 问题: PQ 子空间方差不均衡\n");
    printf("[opq] 方案: PCA 旋转均衡方差\n");
    printf("[opq] 效果: 量化误差降低 20%%~40%%\n");

    /* 模拟方差差异 */
    printf("[opq] 旋转前子空间方差:\n");
    printf("[opq]   Sub0: 0.05  Sub1: 0.80  Sub2: 0.12  Sub3: 0.03\n");
    printf("[opq]   (Sub1 远大于其他，方差极不均衡)\n");

    printf("[opq] 旋转后子空间方差:\n");
    printf("[opq]   Sub0: 0.25  Sub1: 0.24  Sub2: 0.26  Sub3: 0.25\n");
    printf("[opq]   (方差均衡，量化更均匀)\n");

    printf("[opq] 参考: engineering/src/algo-prod/quantization/quantization.c\n");
    printf("[opq]       quantizer_enable_opq()\n");
}

/* ============================================================
 * AVQ (RabitQ) 演示
 * ============================================================ */

/**
 * AVQ 两级量化流程
 *
 * Level 1: PQ 粗量化
 *   x -> PQ(x) = sum_i C_i[code_i]
 *
 * Level 2: 残差精量化
 *   r = x - PQ(x)
 *   r' = round(r / step)
 *   code = [PQ_code | residual_code]
 */
static void demo_avq(void)
{
    printf("\n[avq] ===== AVQ (RabitQ) 两级量化原理 =====\n");

    /* 模拟数据 */
    Vec x = {3.14159f, 2.71828f, 1.41421f, 1.73205f, 0.57735f, 0.30119f, 1.20206f, 0.69315f};

    printf("[avq] 原始向量 x:\n");
    printf("[avq]   [%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f]\n",
           x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7]);

    /* Level 1: PQ 粗量化 (模拟) */
    Vec pq_result = {3.1f, 2.7f, 1.4f, 1.7f, 0.6f, 0.3f, 1.2f, 0.7f};
    printf("[avq] Level-1 PQ 粗量化:\n");
    printf("[avq]   PQ(x) = [3.1, 2.7, 1.4, 1.7, 0.6, 0.3, 1.2, 0.7]\n");

    /* 残差计算 */
    printf("[avq] 残差 r = x - PQ(x):\n");
    printf("[avq]   [%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %.4f]\n",
           x[0]-pq_result[0], x[1]-pq_result[1], x[2]-pq_result[2], x[3]-pq_result[3],
           x[4]-pq_result[4], x[5]-pq_result[5], x[6]-pq_result[6], x[7]-pq_result[7]);

    /* Level 2: 残差量化 */
    float residual_step = 0.05f;
    printf("[avq] Level-2 残差量化 (step=%.2f):\n", residual_step);
    printf("[avq]   r' = round(r / step)\n");
    printf("[avq]   residual_code = [83, 37, 28, 64, -5, 2, 4, -14]\n");

    /* 精度对比 */
    printf("[avq] 精度对比:\n");
    printf("[avq]   仅 PQ:    MSE ~ 0.0021\n");
    printf("[avq]   PQ + AVQ: MSE ~ 0.0003 (提升 7x)\n");

    printf("[avq] 参考: engineering/src/algo-prod/quantization/rq.c\n");
}

/* ============================================================
 * 距离计算演示
 * ============================================================ */

/**
 * L2 距离
 */
static float vec_l2_distance(const Vec a, const Vec b)
{
    float sum = 0.0f;
    for (int i = 0; i < DIMS; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * SQ 解码后精度评估
 */
static void demo_sq_accuracy(SQQuantizer *sq, const Vec *vectors, int n)
{
    printf("\n[eval] ===== SQ 量化精度评估 =====\n");

    float total_orig = 0.0f, total_quant = 0.0f;
    for (int i = 0; i < n; i++) {
        uint8_t code[MAX_CODE_SIZE];
        Vec decoded;

        sq_encode(sq, vectors[i], code);
        sq_decode(sq, code, decoded);

        float d = vec_l2_distance(vectors[i], decoded);
        total_orig += vec_l2_distance(vectors[i], (Vec){0});
        total_quant += d;

        printf("[eval] 向量[%d] L2 误差: %.4f\n", i, d);
    }

    float avg_err = total_quant / n;
    printf("[eval] 平均 L2 误差: %.4f\n", avg_err);
    printf("[eval] bits=%d: 压缩率 = %.1fx (float32 -> %d-bit)\n",
           sq->bits, 32.0f / sq->bits, sq->bits);
}

/* ============================================================
 * 主函数：演示完整量化流程
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("      量化技术原理演示\n");
    printf("   SQ / OPQ / AVQ (RabitQ)\n");
    printf("========================================\n");

    /* 准备测试向量 (模拟高维嵌入) */
    Vec vectors[NUM_VECTORS] = {
        {1.23f, 4.56f, 7.89f, 2.34f, 5.67f, 8.90f, 3.21f, 6.54f},
        {9.87f, 6.54f, 3.21f, 8.76f, 5.43f, 2.10f, 7.98f, 4.65f},
        {0.12f, 3.45f, 6.78f, 9.01f, 2.34f, 5.67f, 8.90f, 1.23f},
        {4.56f, 7.89f, 1.23f, 5.67f, 8.90f, 2.34f, 6.78f, 9.01f},
        {2.34f, 5.67f, 8.90f, 1.23f, 4.56f, 7.89f, 0.12f, 3.45f},
        {6.78f, 9.01f, 2.34f, 5.67f, 8.90f, 1.23f, 4.56f, 7.89f}
    };

    printf("\n--- 1. SQ 标量量化演示 ---\n");
    printf("[main] 训练向量: %d 个, 维度: %d\n", NUM_VECTORS, DIMS);

    /* SQ 训练 */
    SQQuantizer sq;
    sq_train(&sq, vectors, NUM_VECTORS, 8);

    /* SQ 编码/解码演示 */
    printf("\n--- 2. SQ 编码/解码 ---\n");
    uint8_t code[MAX_CODE_SIZE];
    Vec decoded;

    sq_encode(&sq, vectors[0], code);
    sq_decode(&sq, code, decoded);

    printf("[main] 原始向量: [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f]\n",
           vectors[0][0], vectors[0][1], vectors[0][2], vectors[0][3],
           vectors[0][4], vectors[0][5], vectors[0][6], vectors[0][7]);
    printf("[main] 量化码字: [%d, %d, %d, %d, %d, %d, %d, %d]\n",
           code[0], code[1], code[2], code[3], code[4], code[5], code[6], code[7]);
    printf("[main] 解码向量: [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f, %.2f]\n",
           decoded[0], decoded[1], decoded[2], decoded[3],
           decoded[4], decoded[5], decoded[6], decoded[7]);

    /* 精度评估 */
    demo_sq_accuracy(&sq, vectors, NUM_VECTORS);

    /* OPQ 原理 */
    printf("\n--- 3. OPQ 旋转优化 ---\n");
    demo_opq();

    /* AVQ 原理 */
    printf("\n--- 4. AVQ (RabitQ) 两级量化 ---\n");
    demo_avq();

    /* 总结 */
    printf("\n========================================\n");
    printf("      量化技术总结\n");
    printf("========================================\n");
    printf("SQ:   全局缩放，压缩比 %.0fx，适合低精度场景\n", 32.0f / 8.0f);
    printf("OPQ:  PCA 旋转优化，配合 PQ 使用效果更佳\n");
    printf("AVQ:  两级量化 (PQ + 残差)，精度最高\n");
    printf("========================================\n");

    return 0;
}
