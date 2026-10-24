/**
 * @file sparse_vector.c
 * @brief 稀疏向量实现
 *
 * 基于 COO (Coordinate) 格式的稀疏向量存储，仅保留非零元素。
 * 内部自动管理 indices/values 数组的动态扩容。
 */
#include "db/sparse_vector.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 初始容量 */
#define SPARSE_INIT_CAPACITY 16

/* ========================================================================
 * 内部辅助函数
 * ======================================================================== */

/**
 * @brief 在有序数组中查找 index 的位置
 * @return 找到返回该位置下标，未找到返回应插入的位置（负值取反）
 */
static int32_t sparse_find_pos(const sparse_vector_t *vec, uint32_t index) {
    int32_t lo = 0, hi = (int32_t)vec->nnz - 1;
    while (lo <= hi) {
        int32_t mid = lo + (hi - lo) / 2;
        if (vec->indices[mid] == index) {
            return mid;
        } else if (vec->indices[mid] < index) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    /* lo 是应插入的位置 */
    return -(lo + 1);
}

/**
 * @brief 确保容量足够
 * @return 0 成功，-1 内存不足
 */
static int sparse_ensure_capacity(sparse_vector_t *vec, uint32_t required) {
    if (required <= vec->capacity) {
        return 0;
    }
    uint32_t new_cap = vec->capacity ? vec->capacity * 2 : SPARSE_INIT_CAPACITY;
    while (new_cap < required) {
        new_cap *= 2;
    }

    uint32_t *new_indices = realloc(vec->indices, new_cap * sizeof(uint32_t));
    float *new_values = realloc(vec->values, new_cap * sizeof(float));
    if (!new_indices || !new_values) {
        LOG_ERROR("sparse_vector: 内存分配失败, new_cap=%u", new_cap);
        free(new_indices);
        free(new_values);
        return -1;
    }
    vec->indices = new_indices;
    vec->values = new_values;
    vec->capacity = new_cap;
    return 0;
}

/* ========================================================================
 * 公共 API 实现
 * ======================================================================== */

sparse_vector_t* sparse_vector_create(uint32_t dim) {
    if (dim == 0) {
        LOG_ERROR("sparse_vector: 维度不能为 0");
        return NULL;
    }
    sparse_vector_t *vec = calloc(1, sizeof(sparse_vector_t));
    if (!vec) {
        LOG_ERROR("sparse_vector: 内存分配失败");
        return NULL;
    }
    vec->dim = dim;
    vec->nnz = 0;
    vec->capacity = SPARSE_INIT_CAPACITY;
    vec->indices = malloc(vec->capacity * sizeof(uint32_t));
    vec->values = malloc(vec->capacity * sizeof(float));
    if (!vec->indices || !vec->values) {
        LOG_ERROR("sparse_vector: 初始数组分配失败");
        free(vec->indices);
        free(vec->values);
        free(vec);
        return NULL;
    }
    return vec;
}

void sparse_vector_free(sparse_vector_t *vec) {
    if (!vec) return;
    free(vec->indices);
    free(vec->values);
    free(vec);
}

int sparse_vector_set(sparse_vector_t *vec, uint32_t index, float value) {
    if (!vec || index >= vec->dim) {
        LOG_ERROR("sparse_vector_set: 参数错误, index=%u, dim=%u", index, vec ? vec->dim : 0);
        return -1;
    }

    int32_t pos = sparse_find_pos(vec, index);

    if (pos >= 0) {
        /* 元素已存在 */
        if (fabsf(value) < 1e-10f) {
            /* 删除元素：移位覆盖 */
            memmove(&vec->indices[pos], &vec->indices[pos + 1],
                    (vec->nnz - pos - 1) * sizeof(uint32_t));
            memmove(&vec->values[pos], &vec->values[pos + 1],
                    (vec->nnz - pos - 1) * sizeof(float));
            vec->nnz--;
        } else {
            vec->values[pos] = value;
        }
        return 0;
    }

    /* 元素不存在，需要插入 */
    if (fabsf(value) < 1e-10f) {
        return 0;  /* 设置为 0，无需插入 */
    }

    uint32_t insert_pos = (uint32_t)(-(pos + 1));
    if (sparse_ensure_capacity(vec, vec->nnz + 1) < 0) {
        return -1;
    }

    /* 后移腾出位置 */
    if (insert_pos < vec->nnz) {
        memmove(&vec->indices[insert_pos + 1], &vec->indices[insert_pos],
                (vec->nnz - insert_pos) * sizeof(uint32_t));
        memmove(&vec->values[insert_pos + 1], &vec->values[insert_pos],
                (vec->nnz - insert_pos) * sizeof(float));
    }
    vec->indices[insert_pos] = index;
    vec->values[insert_pos] = value;
    vec->nnz++;
    return 0;
}

float sparse_vector_get(const sparse_vector_t *vec, uint32_t index) {
    if (!vec || index >= vec->dim) {
        return 0.0f;
    }
    int32_t pos = sparse_find_pos(vec, index);
    if (pos >= 0) {
        return vec->values[pos];
    }
    return 0.0f;
}

float sparse_vector_dot_product(const sparse_vector_t *a, const sparse_vector_t *b) {
    if (!a || !b || a->dim != b->dim) {
        return 0.0f;
    }

    /* 双指针扫描有序数组 */
    float result = 0.0f;
    uint32_t i = 0, j = 0;
    while (i < a->nnz && j < b->nnz) {
        if (a->indices[i] < b->indices[j]) {
            i++;
        } else if (a->indices[i] > b->indices[j]) {
            j++;
        } else {
            result += a->values[i] * b->values[j];
            i++;
            j++;
        }
    }
    return result;
}

float sparse_vector_cosine_similarity(const sparse_vector_t *a, const sparse_vector_t *b) {
    if (!a || !b || a->dim != b->dim) {
        return 0.0f;
    }

    float dot = sparse_vector_dot_product(a, b);
    if (fabsf(dot) < 1e-10f) {
        return 0.0f;
    }

    /* 计算 L2 范数 */
    float norm_a = 0.0f, norm_b = 0.0f;
    for (uint32_t i = 0; i < a->nnz; i++) {
        norm_a += a->values[i] * a->values[i];
    }
    for (uint32_t i = 0; i < b->nnz; i++) {
        norm_b += b->values[i] * b->values[i];
    }
    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);

    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 0.0f;
    }
    return dot / (norm_a * norm_b);
}

sparse_vector_t* sparse_vector_from_dense(const float *dense, uint32_t dim, float threshold) {
    if (!dense || dim == 0) {
        LOG_ERROR("sparse_vector_from_dense: 参数错误");
        return NULL;
    }

    sparse_vector_t *vec = sparse_vector_create(dim);
    if (!vec) {
        return NULL;
    }

    for (uint32_t i = 0; i < dim; i++) {
        if (fabsf(dense[i]) >= threshold) {
            if (sparse_vector_set(vec, i, dense[i]) < 0) {
                sparse_vector_free(vec);
                return NULL;
            }
        }
    }
    return vec;
}
