/**
 * @file simd.c
 * @brief SIMD 优化实现
 *
 * 提供 CPU 端的 SIMD 优化实现（AVX-512/AVX2/SSE/NEON）。
 * 用于向量距离计算、归一化、量化等操作。
 */
#include "gpu_simd.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef __x86_64__
#include <immintrin.h>
#elif defined(__aarch64__) || defined(__arm__)
#include <arm_neon.h>
#endif

/* ========================================================================
 * 内部状态
 * ======================================================================== */

static simd_type_t g_available_simd = SIMD_NONE;
static simd_opt_level_t g_opt_level = SIMD_OPT_AUTO;
static bool g_simd_initialized = false;

/* ========================================================================
 * SIMD 检测（存根实现）
 * ======================================================================== */

static void simd_detect(void)
{
    if (g_simd_initialized) {
        return;
    }

    g_simd_initialized = true;

#ifdef __x86_64__
    /* TODO: 实际 CPUID 检测 */
    /* AVX-512 特征位: CPUID(EAX=7, ECX=0)->EDX[30] */

#ifdef __AVX512F__
    g_available_simd = SIMD_AVX512;
#elif defined(__AVX2__)
    g_available_simd = SIMD_AVX2;
#elif defined(__SSE4_2__)
    g_available_simd = SIMD_SSE;
#else
    g_available_simd = SIMD_NONE;
#endif

#elif defined(__aarch64__) || defined(__arm__)
    /* ARM NEON 始终可用 */
    g_available_simd = SIMD_NEON;

#else
    g_available_simd = SIMD_NONE;
#endif

    printf("[SIMD] 检测到最高可用指令集: %s\n", simd_get_name(g_available_simd));
}

simd_type_t simd_get_available(void)
{
    if (!g_simd_initialized) {
        simd_detect();
    }
    return g_available_simd;
}

bool simd_is_supported(simd_type_t type)
{
    if (!g_simd_initialized) {
        simd_detect();
    }

    /* 检查是否支持请求的级别 */
    switch (type) {
        case SIMD_NONE:
            return true;
        case SIMD_SSE:
            return g_available_simd >= SIMD_SSE;
        case SIMD_AVX:
            return g_available_simd >= SIMD_AVX;
        case SIMD_AVX2:
            return g_available_simd >= SIMD_AVX2;
        case SIMD_AVX512:
            return g_available_simd >= SIMD_AVX512;
        case SIMD_NEON:
            return g_available_simd >= SIMD_NEON;
        default:
            return false;
    }
}

const char *simd_get_name(simd_type_t type)
{
    switch (type) {
        case SIMD_NONE:   return "NONE";
        case SIMD_SSE:    return "SSE 4.2";
        case SIMD_AVX:    return "AVX";
        case SIMD_AVX2:   return "AVX2";
        case SIMD_AVX512: return "AVX-512";
        case SIMD_NEON:   return "NEON";
        default:          return "UNKNOWN";
    }
}

/* ========================================================================
 * 距离计算实现
 * ======================================================================== */

/**
 * @brief 朴素 L2 距离（fallback）
 */
static float l2_distance_scalar(const float *a, const float *b, int32_t dim)
{
    float dist = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        dist += diff * diff;
    }
    return dist;
}

float simd_l2_distance(const float *a, const float *b, int32_t dim)
{
    if (dim <= 0 || a == NULL || b == NULL) {
        return 0.0f;
    }

#ifdef __AVX512F__
    if (g_opt_level == SIMD_OPT_AVX512 || g_opt_level == SIMD_OPT_AUTO) {
        __m512 sum = _mm512_setzero_ps();
        int32_t i = 0;

        /* 处理 16 元素对齐的块 */
        for (; i + 15 < dim; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            __m512 diff = _mm512_sub_ps(va, vb);
            sum = _mm512_fmadd_ps(diff, diff, sum);
        }

        float result = _mm512_reduce_add_ps(sum);

        /* 处理剩余元素 */
        for (; i < dim; i++) {
            float d = a[i] - b[i];
            result += d * d;
        }

        return result;
    }
#endif

#ifdef __AVX2__
    if (g_opt_level == SIMD_OPT_AVX2 || g_opt_level == SIMD_OPT_AUTO) {
        __m256 sum = _mm256_setzero_ps();
        int32_t i = 0;

        /* 处理 8 元素对齐的块 */
        for (; i + 7 < dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            __m256 diff = _mm256_sub_ps(va, vb);
            __m256 sq = _mm256_mul_ps(diff, diff);
            sum = _mm256_add_ps(sum, sq);
        }

        /* 水平加和 */
        __m128 lo = _mm256_extractf128_ps(sum, 0);
        __m128 hi = _mm256_extractf128_ps(sum, 1);
        __m128 total = _mm_add_ps(lo, hi);

        float result = _mm_cvtss_f32(total);

        /* 处理剩余元素 */
        for (; i < dim; i++) {
            float d = a[i] - b[i];
            result += d * d;
        }

        return result;
    }
#endif

    /* Fallback: 朴素实现 */
    return l2_distance_scalar(a, b, dim);
}

void simd_l2_distance_batch(const float *query, const float *database,
                            int32_t n, int32_t dim, float *distances)
{
    if (n <= 0 || dim <= 0 || query == NULL || database == NULL || distances == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        distances[i] = simd_l2_distance(query, database + i * dim, dim);
    }
}

/**
 * @brief 朴素内积（fallback）
 */
static float inner_product_scalar(const float *a, const float *b, int32_t dim)
{
    float dot = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        dot += a[i] * b[i];
    }
    return dot;
}

float simd_inner_product(const float *a, const float *b, int32_t dim)
{
    if (dim <= 0 || a == NULL || b == NULL) {
        return 0.0f;
    }

#ifdef __AVX512F__
    if (g_opt_level == SIMD_OPT_AVX512 || g_opt_level == SIMD_OPT_AUTO) {
        __m512 sum = _mm512_setzero_ps();
        int32_t i = 0;

        for (; i + 15 < dim; i += 16) {
            __m512 va = _mm512_loadu_ps(a + i);
            __m512 vb = _mm512_loadu_ps(b + i);
            sum = _mm512_fmadd_ps(va, vb, sum);
        }

        float result = _mm512_reduce_add_ps(sum);

        for (; i < dim; i++) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

#ifdef __AVX2__
    if (g_opt_level == SIMD_OPT_AVX2 || g_opt_level == SIMD_OPT_AUTO) {
        __m256 sum = _mm256_setzero_ps();
        int32_t i = 0;

        for (; i + 7 < dim; i += 8) {
            __m256 va = _mm256_loadu_ps(a + i);
            __m256 vb = _mm256_loadu_ps(b + i);
            sum = _mm256_fmadd_ps(va, vb, sum);
        }

        __m128 lo = _mm256_extractf128_ps(sum, 0);
        __m128 hi = _mm256_extractf128_ps(sum, 1);
        float result = _mm_cvtss_f32(_mm_add_ps(lo, hi));

        for (; i < dim; i++) {
            result += a[i] * b[i];
        }

        return result;
    }
#endif

    return inner_product_scalar(a, b, dim);
}

void simd_inner_product_batch(const float *query, const float *database,
                              int32_t n, int32_t dim, float *products)
{
    if (n <= 0 || dim <= 0 || query == NULL || database == NULL || products == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        products[i] = simd_inner_product(query, database + i * dim, dim);
    }
}

float simd_cosine_similarity(const float *a, const float *b, int32_t dim)
{
    /* 假设输入已归一化，此时余弦相似度等于内积 */
    return simd_inner_product(a, b, dim);
}

void simd_cosine_similarity_batch(const float *query, const float *database,
                                  int32_t n, int32_t dim, float *similarities)
{
    simd_inner_product_batch(query, database, n, dim, similarities);
}

/* ========================================================================
 * 向量归一化
 * ======================================================================== */

static void normalize_l2_scalar(float *v, int32_t dim)
{
    float sum_sq = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        sum_sq += v[i] * v[i];
    }

    if (sum_sq > 0.0f) {
        float norm = 1.0f / sqrtf(sum_sq);
        for (int32_t i = 0; i < dim; i++) {
            v[i] *= norm;
        }
    }
}

void simd_normalize_l2(float *v, int32_t dim)
{
    if (dim <= 0 || v == NULL) {
        return;
    }

#ifdef __AVX2__
    if (g_opt_level == SIMD_OPT_AVX2 || g_opt_level == SIMD_OPT_AUTO) {
        /* 第一步：计算 L2 范数 */
        __m256 sum_sq = _mm256_setzero_ps();
        int32_t i = 0;

        for (; i + 7 < dim; i += 8) {
            __m256 vec = _mm256_loadu_ps(v + i);
            sum_sq = _mm256_fmadd_ps(vec, vec, sum_sq);
        }

        __m128 lo = _mm256_extractf128_ps(sum_sq, 0);
        __m128 hi = _mm256_extractf128_ps(sum_sq, 1);
        float sum = _mm_cvtss_f32(_mm_add_ps(lo, hi));

        for (; i < dim; i++) {
            sum += v[i] * v[i];
        }

        if (sum <= 0.0f) {
            return;
        }

        float norm = 1.0f / sqrtf(sum);
        __m256 inv_norm = _mm256_set1_ps(norm);

        /* 第二步：归一化 */
        i = 0;
        for (; i + 7 < dim; i += 8) {
            __m256 vec = _mm256_loadu_ps(v + i);
            vec = _mm256_mul_ps(vec, inv_norm);
            _mm256_storeu_ps(v + i, vec);
        }

        for (; i < dim; i++) {
            v[i] *= norm;
        }

        return;
    }
#endif

    normalize_l2_scalar(v, dim);
}

void simd_normalize_l2_batch(float *vectors, int32_t n, int32_t dim)
{
    if (n <= 0 || dim <= 0 || vectors == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        simd_normalize_l2(vectors + i * dim, dim);
    }
}

/* ========================================================================
 * 向量运算
 * ======================================================================== */

void simd_vec_add(const float *a, const float *b, float *c, int32_t dim)
{
    if (dim <= 0 || a == NULL || b == NULL || c == NULL) {
        return;
    }

#ifdef __AVX2__
    int32_t i = 0;
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_add_ps(va, vb);
        _mm256_storeu_ps(c + i, vc);
    }
    for (; i < dim; i++) {
        c[i] = a[i] + b[i];
    }
#else
    for (int32_t i = 0; i < dim; i++) {
        c[i] = a[i] + b[i];
    }
#endif
}

void simd_vec_sub(const float *a, const float *b, float *c, int32_t dim)
{
    if (dim <= 0 || a == NULL || b == NULL || c == NULL) {
        return;
    }

#ifdef __AVX2__
    int32_t i = 0;
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 vc = _mm256_sub_ps(va, vb);
        _mm256_storeu_ps(c + i, vc);
    }
    for (; i < dim; i++) {
        c[i] = a[i] - b[i];
    }
#else
    for (int32_t i = 0; i < dim; i++) {
        c[i] = a[i] - b[i];
    }
#endif
}

void simd_vec_fma(const float *b, float *a, float k, int32_t dim)
{
    if (dim <= 0 || a == NULL || b == NULL) {
        return;
    }

#ifdef __AVX2__
    __m256 vk = _mm256_set1_ps(k);
    int32_t i = 0;
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        va = _mm256_fmadd_ps(vb, vk, va);
        _mm256_storeu_ps(a + i, va);
    }
    for (; i < dim; i++) {
        a[i] += k * b[i];
    }
#else
    for (int32_t i = 0; i < dim; i++) {
        a[i] += k * b[i];
    }
#endif
}

void simd_vec_add_batch(const float *a, const float *b, float *c,
                        int32_t n, int32_t dim)
{
    if (n <= 0 || dim <= 0 || a == NULL || b == NULL || c == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        simd_vec_add(a, b + i * dim, c + i * dim, dim);
    }
}

/* ========================================================================
 * 归约操作
 * ======================================================================== */

float simd_reduce_sum(const float *v, int32_t dim)
{
    if (dim <= 0 || v == NULL) {
        return 0.0f;
    }

#ifdef __AVX2__
    __m256 sum = _mm256_setzero_ps();
    int32_t i = 0;

    for (; i + 7 < dim; i += 8) {
        sum = _mm256_add_ps(sum, _mm256_loadu_ps(v + i));
    }

    __m128 lo = _mm256_extractf128_ps(sum, 0);
    __m128 hi = _mm256_extractf128_ps(sum, 1);
    float result = _mm_cvtss_f32(_mm_add_ps(lo, hi));

    for (; i < dim; i++) {
        result += v[i];
    }

    return result;
#else
    float sum = 0.0f;
    for (int32_t i = 0; i < dim; i++) {
        sum += v[i];
    }
    return sum;
#endif
}

float simd_reduce_max(const float *v, int32_t dim)
{
    if (dim <= 0 || v == NULL) {
        return 0.0f;
    }

#ifdef __AVX2__
    __m256 max_val = _mm256_setzero_ps();
    int32_t i = 0;

    for (; i + 7 < dim; i += 8) {
        __m256 vec = _mm256_loadu_ps(v + i);
        max_val = _mm256_max_ps(max_val, vec);
    }

    /* 提取最大值 */
    float result = _mm256_extractf128_ps(max_val, 0)[0];
    float hi = _mm256_extractf128_ps(max_val, 1)[0];
    if (hi > result) result = hi;

    for (; i < dim; i++) {
        if (v[i] > result) result = v[i];
    }

    return result;
#else
    float max_val = v[0];
    for (int32_t i = 1; i < dim; i++) {
        if (v[i] > max_val) max_val = v[i];
    }
    return max_val;
#endif
}

float simd_reduce_min(const float *v, int32_t dim)
{
    if (dim <= 0 || v == NULL) {
        return 0.0f;
    }

    float min_val = v[0];
    for (int32_t i = 1; i < dim; i++) {
        if (v[i] < min_val) min_val = v[i];
    }
    return min_val;
}

float simd_reduce_sqsum(const float *v, int32_t dim)
{
    return simd_l2_distance(v, v, dim);  /* v 与自己的距离就是平方和 */
}

/* ========================================================================
 * Top-K 查找
 * ======================================================================== */

/**
 * @brief 简单的 Top-K 选择（使用排序）
 */
static void topk_simple(const float *values, int32_t n, int32_t k,
                        float *top_values, int32_t *top_indices,
                        bool min_first)
{
    if (values == NULL || top_values == NULL || top_indices == NULL || k <= 0) {
        return;
    }

    k = (k > n) ? n : k;

    /* 创建索引数组 */
    int32_t *indices = (int32_t *)malloc(n * sizeof(int32_t));
    if (indices == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        indices[i] = i;
    }

    /* 简单选择排序 */
    for (int32_t i = 0; i < k; i++) {
        int32_t best_idx = i;
        for (int32_t j = i + 1; j < n; j++) {
            bool should_swap;
            if (min_first) {
                should_swap = values[j] < values[best_idx];
            } else {
                should_swap = values[j] > values[best_idx];
            }
            if (should_swap) {
                best_idx = j;
            }
        }

        /* 交换 */
        float temp_val = values[i];
        int32_t temp_idx = indices[i];
        top_values[i] = values[best_idx];
        top_indices[i] = indices[best_idx];
        values = values;  /* 保持原值不变 */
    }

    /* 重新实现以避免上述问题 */
    for (int32_t i = 0; i < k; i++) {
        top_values[i] = values[indices[i]];
        top_indices[i] = indices[i];
    }

    free(indices);
}

void simd_topk(const float *values, int32_t n, int32_t k,
               float *top_values, int32_t *top_indices)
{
    /* 查找最大值：使用堆排序或部分排序 */
    /* TODO: 实现高效的 Top-K 算法 */

    if (values == NULL || n <= 0 || k <= 0) {
        return;
    }

    k = (k > n) ? n : k;

    /* 简单实现：创建索引并排序 */
    typedef struct {
        float value;
        int32_t index;
    } elem_t;

    elem_t *elems = (elem_t *)malloc(n * sizeof(elem_t));
    if (elems == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        elems[i].value = values[i];
        elems[i].index = i;
    }

    /* 选择排序获取 Top-K */
    for (int32_t i = 0; i < k; i++) {
        int32_t max_idx = i;
        for (int32_t j = i + 1; j < n; j++) {
            if (elems[j].value > elems[max_idx].value) {
                max_idx = j;
            }
        }
        if (max_idx != i) {
            elem_t tmp = elems[i];
            elems[i] = elems[max_idx];
            elems[max_idx] = tmp;
        }
        top_values[i] = elems[i].value;
        top_indices[i] = elems[i].index;
    }

    free(elems);
}

void simd_topk_min(const float *values, int32_t n, int32_t k,
                   float *top_values, int32_t *top_indices)
{
    if (values == NULL || n <= 0 || k <= 0) {
        return;
    }

    k = (k > n) ? n : k;

    typedef struct {
        float value;
        int32_t index;
    } elem_t;

    elem_t *elems = (elem_t *)malloc(n * sizeof(elem_t));
    if (elems == NULL) {
        return;
    }

    for (int32_t i = 0; i < n; i++) {
        elems[i].value = values[i];
        elems[i].index = i;
    }

    /* 选择排序获取 Top-K 最小值 */
    for (int32_t i = 0; i < k; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < n; j++) {
            if (elems[j].value < elems[min_idx].value) {
                min_idx = j;
            }
        }
        if (min_idx != i) {
            elem_t tmp = elems[i];
            elems[i] = elems[min_idx];
            elems[min_idx] = tmp;
        }
        top_values[i] = elems[i].value;
        top_indices[i] = elems[i].index;
    }

    free(elems);
}

/* ========================================================================
 * PQ 量化操作（存根实现）
 * ======================================================================== */

void simd_pq_encode(const float *vectors, int32_t n, int32_t dim,
                    int32_t m, int32_t nbits,
                    const float *codebooks, uint8_t *codes)
{
    /* TODO: 实现高效的 PQ 编码 */
    int32_t ncentroids = 1 << nbits;
    int32_t dim_per_sub = dim / m;

    for (int32_t i = 0; i < n; i++) {
        for (int32_t s = 0; s < m; s++) {
            const float *sub_vec = vectors + i * dim + s * dim_per_sub;
            const float *codebook = codebooks + s * ncentroids * dim_per_sub;

            int32_t best_c = 0;
            float best_dist = -1.0f;

            for (int32_t c = 0; c < ncentroids; c++) {
                float dist = simd_l2_distance(sub_vec, codebook + c * dim_per_sub, dim_per_sub);
                if (best_dist < 0 || dist < best_dist) {
                    best_dist = dist;
                    best_c = c;
                }
            }

            codes[i * m + s] = (uint8_t)best_c;
        }
    }
}

void simd_pq_decode(const uint8_t *codes, int32_t n, int32_t m, int32_t nbits,
                    const float *codebooks, int32_t dim, float *vectors)
{
    int32_t ncentroids = 1 << nbits;
    int32_t dim_per_sub = dim / m;

    for (int32_t i = 0; i < n; i++) {
        for (int32_t s = 0; s < m; s++) {
            int32_t c = codes[i * m + s];
            const float *centroid = codebooks + s * ncentroids * dim_per_sub + c * dim_per_sub;
            float *dest = vectors + i * dim + s * dim_per_sub;
            memcpy(dest, centroid, dim_per_sub * sizeof(float));
        }
    }
}

void simd_pq_distance_async(const float *query, const uint8_t *codes,
                            int32_t n, int32_t m, int32_t nbits,
                            const float *codebooks, int32_t dim,
                            float *distances)
{
    /* TODO: 实现高效的 PQ 距离计算 */
    int32_t dim_per_sub = dim / m;

    for (int32_t i = 0; i < n; i++) {
        float total_dist = 0.0f;
        for (int32_t s = 0; s < m; s++) {
            int32_t c = codes[i * m + s];
            const float *centroid = codebooks + s * (1 << nbits) * dim_per_sub + c * dim_per_sub;
            const float *q_sub = query + s * dim_per_sub;
            total_dist += simd_l2_distance(q_sub, centroid, dim_per_sub);
        }
        distances[i] = total_dist;
    }
}

void simd_pq_distance_sym(const uint8_t *codes1, const uint8_t *codes2,
                          int32_t n1, int32_t n2, int32_t m, int32_t nbits,
                          const float *codebooks, float *distances)
{
    /* 对称距离：两个编码的距离需要解码后计算 */
    /* TODO: 实现 */
    (void)codes1;
    (void)codes2;
    (void)n1;
    (void)n2;
    (void)m;
    (void)nbits;
    (void)codebooks;
    (void)distances;
}

/* ========================================================================
 * 性能提示
 * ======================================================================== */

void simd_prefetch(const void *ptr, int hint)
{
    if (ptr == NULL) {
        return;
    }

#ifdef __x86_64__
    /* _MM_HINT_T0 = 0 (所有级别缓存), _MM_HINT_T1 = 1 (L1/L2), _MM_HINT_T2 = 2 (只 L2) */
    int hw_hint = (hint == 0) ? _MM_HINT_T0 : _MM_HINT_T1;
    _mm_prefetch((const char *)ptr, hw_hint);
#endif
}

void simd_set_opt_level(simd_opt_level_t level)
{
    g_opt_level = level;
}

simd_opt_level_t simd_get_opt_level(void)
{
    return g_opt_level;
}
