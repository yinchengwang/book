/**
 * @file main.c
 * @brief 标量量化（Scalar Quantization）演示程序
 *
 * 演示内容：
 * - int8/float16 量化原理
 * - 缩放因子（scale factor）计算
 * - 溢出处理与饱和截断
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define DIMS 8               /* 向量维度 */
#define MAX_VAL 100.0f       /* 测试数据最大值 */

/* ============================================================
 * 量化配置
 * ============================================================ */

typedef enum {
    SQ_INT8,    /* int8 量化：256 个离散值 */
    SQ_FP16,    /* float16 量化：16 位浮点 */
    SQ_UINT8    /* uint8 量化：无符号整数 */
} SqType;

typedef struct {
    SqType  type;           /* 量化类型 */
    float   scale;          /* 缩放因子 */
    float   zero_point;     /* 零点偏移 */
    float   min_val;        /* 最小值 */
    float   max_val;        /* 最大值 */
} SqConfig;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 计算 int8 量化的缩放因子和零点
 * @param data     输入数据
 * @param n        数据数量
 * @param scale    输出缩放因子
 * @param zero_pt  输出零点
 */
void calc_int8_params(const float *data, int n, float *scale, float *zero_pt) {
    float min_val = data[0];
    float max_val = data[0];

    /* 统计 min/max */
    for (int i = 1; i < n; i++) {
        if (data[i] < min_val) min_val = data[i];
        if (data[i] > max_val) max_val = data[i];
    }

    /* int8 范围：-128 ~ 127 */
    float qmin = -128.0f;
    float qmax = 127.0f;

    *scale = (max_val - min_val) / (qmax - qmin);
    *zero_pt = qmin - min_val / *scale;

    printf("[config] 数据范围: [%.2f, %.2f]\n", min_val, max_val);
    printf("[config] 缩放因子: %.6f\n", *scale);
    printf("[config] 零点偏移: %.2f\n", *zero_pt);
}

/**
 * int8 量化：将 float 映射到 int8
 */
int8_t quantize_int8(float val, float scale, float zero_pt) {
    /* 计算量化值 */
    float qval = val / scale + zero_pt;

    /* 饱和截断：处理溢出 */
    if (qval < -128.0f) {
        printf("[overflow] 值 %.2f 截断为 -128\n", val);
        return -128;
    }
    if (qval > 127.0f) {
        printf("[overflow] 值 %.2f 截断为 127\n", val);
        return 127;
    }

    return (int8_t)roundf(qval);
}

/**
 * int8 反量化：从 int8 恢复 float
 */
float dequantize_int8(int8_t qval, float scale, float zero_pt) {
    return (qval - zero_pt) * scale;
}

/**
 * float16 量化（简化演示）
 * 实际使用需包含 <半精度浮点库>
 */
typedef struct {
    uint16_t bits;  /* 16 位存储 */
} Float16;

Float16 quantize_fp16(float val) {
    Float16 result;
    /* 简化实现：实际需要 IEEE-754 转换逻辑 */
    /* 这里用截断演示原理 */
    int sign = (val < 0) ? 1 : 0;
    val = fabsf(val);

    /* 简化为 10 位尾数 + 5 位指数 */
    int exp = 0;
    while (val >= 2.0f && exp < 15) { val /= 2.0f; exp++; }
    while (val < 1.0f && exp > -16) { val *= 2.0f; exp--; }

    int mantissa = (int)((val - 1.0f) * 1024.0f);
    result.bits = (sign << 15) | ((exp + 15) << 10) | mantissa;

    return result;
}

float dequantize_fp16(Float16 f16) {
    int sign = (f16.bits >> 15) & 1;
    int exp = ((f16.bits >> 10) & 31) - 15;
    int mantissa = f16.bits & 1023;

    float val = (1.0f + mantissa / 1024.0f) * powf(2.0f, exp);
    return sign ? -val : val;
}

/**
 * 打印向量
 */
void print_vector(const char *label, const float *vec, int n) {
    printf("%s: [", label);
    for (int i = 0; i < n; i++) {
        printf("%6.2f", vec[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

/**
 * 打印 int8 向量
 */
void print_int8_vector(const char *label, const int8_t *vec, int n) {
    printf("%s: [", label);
    for (int i = 0; i < n; i++) {
        printf("%4d", vec[i]);
        if (i < n - 1) printf(", ");
    }
    printf("]\n");
}

/* ============================================================
 * 演示程序
 * ============================================================ */

int main(void) {
    printf("=== 标量量化（Scalar Quantization）演示 ===\n\n");

    /* 准备测试数据 */
    float original[DIMS] = {
        -45.5f, -20.3f, 0.0f, 15.7f,
        33.2f, 58.9f, 75.0f, 99.8f
    };

    printf("--- 演示 1: int8 量化 ---\n\n");
    print_vector("原始数据", original, DIMS);

    /* 计算量化参数 */
    float scale, zero_pt;
    calc_int8_params(original, DIMS, &scale, &zero_pt);
    printf("\n");

    /* 执行量化 */
    int8_t quantized[DIMS];
    for (int i = 0; i < DIMS; i++) {
        quantized[i] = quantize_int8(original[i], scale, zero_pt);
    }
    print_int8_vector("量化结果", quantized, DIMS);

    /* 执行反量化 */
    float restored[DIMS];
    for (int i = 0; i < DIMS; i++) {
        restored[i] = dequantize_int8(quantized[i], scale, zero_pt);
    }
    print_vector("反量化数据", restored, DIMS);

    /* 计算量化误差 */
    printf("\n量化误差分析:\n");
    float total_err = 0.0f;
    for (int i = 0; i < DIMS; i++) {
        float err = fabsf(original[i] - restored[i]);
        float rel_err = err / (fabsf(original[i]) + 1e-6f) * 100.0f;
        printf("  [%d] 原始=%6.2f, 恢复=%6.2f, 误差=%5.2f (%.2f%%)\n",
               i, original[i], restored[i], err, rel_err);
        total_err += err;
    }
    printf("  平均绝对误差: %.4f\n", total_err / DIMS);

    /* 演示溢出处理 */
    printf("\n--- 演示 2: 溢出处理 ---\n\n");
    float overflow_test[3] = {500.0f, -300.0f, 50.0f};
    printf("测试溢出数据: [%.1f, %.1f, %.1f]\n",
           overflow_test[0], overflow_test[1], overflow_test[2]);

    int8_t overflow_q[3];
    for (int i = 0; i < 3; i++) {
        overflow_q[i] = quantize_int8(overflow_test[i], scale, zero_pt);
    }
    printf("溢出量化结果: [%d, %d, %d] (有效范围: -128 ~ 127)\n",
           overflow_q[0], overflow_q[1], overflow_q[2]);

    /* 演示 float16 量化 */
    printf("\n--- 演示 3: float16 量化 ---\n\n");
    Float16 f16_vec[DIMS];
    float f16_restored[DIMS];

    for (int i = 0; i < DIMS; i++) {
        f16_vec[i] = quantize_fp16(original[i]);
        f16_restored[i] = dequantize_fp16(f16_vec[i]);
    }

    printf("原始数据:  ");
    for (int i = 0; i < DIMS; i++) printf("%6.2f ", original[i]);
    printf("\n");

    printf("FP16 位:   ");
    for (int i = 0; i < DIMS; i++) printf("0x%04X ", f16_vec[i].bits);
    printf("\n");

    printf("恢复数据:  ");
    for (int i = 0; i < DIMS; i++) printf("%6.2f ", f16_restored[i]);
    printf("\n");

    /* 存储空间对比 */
    printf("\n--- 演示 4: 存储空间对比 ---\n\n");
    printf("| 数据类型  | 每维度比特数 | 压缩比 |\n");
    printf("|-----------|-------------|--------|\n");
    printf("| float32   | 32 bit      | 1.0x   |\n");
    printf("| float16   | 16 bit      | 2.0x   |\n");
    printf("| int8      | 8 bit       | 4.0x   |\n");
    printf("| uint8     | 8 bit       | 4.0x   |\n");

    printf("\n=== 演示完成 ===\n");
    return 0;
}
