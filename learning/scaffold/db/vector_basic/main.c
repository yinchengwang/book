/**
 * @file main.c
 * @brief 向量基础原理演示程序
 *
 * 本程序演示向量基础概念：
 * - 向量的内存表示（连续数组）
 * - 欧氏距离计算（L2 距离）
 * - 余弦距离计算
 * - 点积计算
 * - 向量归一化
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* ============================================================
 * 类型定义
 * ============================================================ */

/**
 * @brief 向量结构（动态数组实现）
 */
typedef struct {
    float *data;     /* 数据指针 */
    int    dim;      /* 维度 */
} Vector;

/* ============================================================
 * 向量基本操作
 * ============================================================ */

/**
 * 创建指定维度的向量
 */
static Vector *vector_create(int dim) {
    Vector *vec = (Vector *)malloc(sizeof(Vector));
    if (!vec) return NULL;

    vec->data = (float *)calloc(dim, sizeof(float));
    if (!vec->data) {
        free(vec);
        return NULL;
    }
    vec->dim = dim;
    return vec;
}

/**
 * 释放向量内存
 */
static void vector_destroy(Vector *vec) {
    if (vec) {
        free(vec->data);
        free(vec);
    }
}

/**
 * 打印向量内容
 */
static void vector_print(const char *name, const Vector *vec) {
    printf("[%s] dim=%d: [", name, vec->dim);
    for (int i = 0; i < vec->dim; i++) {
        printf("%.3f", vec->data[i]);
        if (i < vec->dim - 1) printf(", ");
    }
    printf("]\n");
}

/* ============================================================
 * 距离计算函数
 * ============================================================ */

/**
 * 计算欧氏距离（L2 距离）
 * d(a,b) = sqrt(sum((ai-bi)^2))
 *
 * 注意：实际实现中常用 L2 平方距离省去 sqrt 开方操作
 */
static float euclidean_distance(const Vector *a, const Vector *b) {
    float sum = 0.0f;
    for (int i = 0; i < a->dim; i++) {
        float diff = a->data[i] - b->data[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * 计算余弦距离
 * cosine_dist = 1 - cos(theta) = 1 - (a·b) / (|a|*|b|)
 *
 * 注意：向量数据库中通常返回 1 - cosine_similarity
 */
static float cosine_distance(const Vector *a, const Vector *b) {
    float dot = 0.0f;    /* 点积 a·b */
    float norm_a = 0.0f; /* |a| */
    float norm_b = 0.0f; /* |b| */

    for (int i = 0; i < a->dim; i++) {
        dot += a->data[i] * b->data[i];
        norm_a += a->data[i] * a->data[i];
        norm_b += b->data[i] * b->data[i];
    }

    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    /* 防止除零 */
    if (norm_a < 1e-6f || norm_b < 1e-6f) {
        return 1.0f; /* 正交或零向量 */
    }

    return 1.0f - dot / (norm_a * norm_b);
}

/**
 * 计算点积（内积）
 * dot(a,b) = sum(ai * bi)
 */
static float dot_product(const Vector *a, const Vector *b) {
    float sum = 0.0f;
    for (int i = 0; i < a->dim; i++) {
        sum += a->data[i] * b->data[i];
    }
    return sum;
}

/**
 * 计算向量范数（模长）
 * |v| = sqrt(sum(vi^2))
 */
static float vector_norm(const Vector *vec) {
    float sum = 0.0f;
    for (int i = 0; i < vec->dim; i++) {
        sum += vec->data[i] * vec->data[i];
    }
    return sqrtf(sum);
}

/* ============================================================
 * 向量归一化
 * ============================================================ */

/**
 * L2 归一化：将向量缩放到单位长度
 * v_normalized = v / |v|
 */
static void vector_normalize(Vector *vec) {
    float norm = vector_norm(vec);
    if (norm < 1e-6f) return; /* 避免除零 */

    for (int i = 0; i < vec->dim; i++) {
        vec->data[i] /= norm;
    }
}

/* ============================================================
 * 主函数：演示向量操作
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("      向量基础原理演示\n");
    printf("========================================\n\n");

    /* 创建演示向量 */
    Vector *v1 = vector_create(4);
    Vector *v2 = vector_create(4);

    /* v1 = [3.0, 4.0, 0.0, 0.0] */
    v1->data[0] = 3.0f;
    v1->data[1] = 4.0f;

    /* v2 = [4.0, 3.0, 0.0, 0.0] */
    v2->data[0] = 4.0f;
    v2->data[1] = 3.0f;

    printf("--- 1. 向量表示 ---\n");
    vector_print("v1", v1);
    vector_print("v2", v2);
    printf("说明：向量在内存中以连续 float 数组存储\n\n");

    printf("--- 2. 距离计算 ---\n");
    float e_dist = euclidean_distance(v1, v2);
    float c_dist = cosine_distance(v1, v2);
    float dot = dot_product(v1, v2);
    printf("欧氏距离 L2(v1, v2) = %.4f\n", e_dist);
    printf("余弦距离   (v1, v2) = %.4f\n", c_dist);
    printf("点积       (v1, v2) = %.4f\n", dot);
    printf("\n");

    printf("--- 3. 向量归一化 ---\n");
    printf("归一化前: |v1| = %.4f, |v2| = %.4f\n",
           vector_norm(v1), vector_norm(v2));

    vector_normalize(v1);
    vector_normalize(v2);

    printf("归一化后: |v1| = %.4f, |v2| = %.4f\n",
           vector_norm(v1), vector_norm(v2));
    vector_print("v1_norm", v1);
    vector_print("v2_norm", v2);

    /* 归一化后的余弦距离等于 1 - 点积 */
    printf("归一化后余弦距离 = %.4f\n", cosine_distance(v1, v2));

    printf("\n--- 4. 应用场景 ---\n");
    printf("- 欧氏距离：图像检索、推荐系统中常用\n");
    printf("- 余弦距离：文本向量（词嵌入）相似度\n");
    printf("- 点积：ANN 搜索中的内积度量\n\n");

    /* 释放资源 */
    vector_destroy(v1);
    vector_destroy(v2);

    printf("========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");
    return 0;
}
