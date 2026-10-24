#include <algo-prod/distance/distance.h>

#include <math.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#include <arm_neon.h>
#define DISTANCE_USE_NEON 1
#elif defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#define DISTANCE_USE_AVX 1
#elif defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
#include <xmmintrin.h>
#define DISTANCE_USE_SSE 1
#endif

#if defined(DISTANCE_USE_NEON) || defined(DISTANCE_USE_AVX) || defined(DISTANCE_USE_SSE)
#define DISTANCE_HAS_SIMD 1
#endif

#define DISTANCE_EPSILON 1e-6f

static void distance_store_zero4(float *dis1, float *dis2, float *dis3, float *dis4)
{
    *dis1 = 0.0f;
    *dis2 = 0.0f;
    *dis3 = 0.0f;
    *dis4 = 0.0f;
}

#if !defined(DISTANCE_HAS_SIMD)
static float distance_l2sqr_scalar(const float *lhs, const float *rhs, int32_t dims)
{
    float result = 0.0f;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i < dims; ++i) {
        float diff = lhs[i] - rhs[i];
        result += diff * diff;
    }

    return result;
}

static float distance_inner_product_scalar(const float *lhs, const float *rhs, int32_t dims)
{
    float dot = 0.0f;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }

    return -dot;
}

static float distance_cosine_scalar(const float *lhs, const float *rhs, int32_t dims)
{
    float dot = 0.0f;
    float lhs_norm = 0.0f;
    float rhs_norm = 0.0f;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) {
        return 0.0f;
    }
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - dot / (sqrtf(lhs_norm) * sqrtf(rhs_norm));
}
#endif

#if defined(DISTANCE_USE_NEON)
static float distance_horizontal_sum_f32x4(float32x4_t value)
{
    float sums[4];

    vst1q_f32(sums, value);
    return sums[0] + sums[1] + sums[2] + sums[3];
}

static float32x4_t distance_load_batch4_neon(const float *vec1,
                                                  const float *vec2,
                                                  const float *vec3,
                                                  const float *vec4,
                                                  int32_t index)
{
    float lanes[4];

    lanes[0] = vec1[index];
    lanes[1] = vec2[index];
    lanes[2] = vec3[index];
    lanes[3] = vec4[index];
    return vld1q_f32(lanes);
}

static float distance_l2sqr_simd(const float *lhs, const float *rhs, int32_t dims)
{
    float32x4_t acc = vdupq_n_f32(0.0f);
    float result;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        float32x4_t lhs_vec = vld1q_f32(lhs + i);
        float32x4_t rhs_vec = vld1q_f32(rhs + i);
        float32x4_t diff = vsubq_f32(lhs_vec, rhs_vec);
        acc = vmlaq_f32(acc, diff, diff);
    }

    result = distance_horizontal_sum_f32x4(acc);
    for (; i < dims; ++i) {
        float diff = lhs[i] - rhs[i];
        result += diff * diff;
    }

    return result;
}

static float distance_inner_product_simd(const float *lhs, const float *rhs, int32_t dims)
{
    float32x4_t acc = vdupq_n_f32(0.0f);
    float dot;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        float32x4_t lhs_vec = vld1q_f32(lhs + i);
        float32x4_t rhs_vec = vld1q_f32(rhs + i);
        acc = vmlaq_f32(acc, lhs_vec, rhs_vec);
    }

    dot = distance_horizontal_sum_f32x4(acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }

    return -dot;
}

static float distance_cosine_simd(const float *lhs, const float *rhs, int32_t dims)
{
    float32x4_t dot_acc = vdupq_n_f32(0.0f);
    float32x4_t lhs_norm_acc = vdupq_n_f32(0.0f);
    float32x4_t rhs_norm_acc = vdupq_n_f32(0.0f);
    float dot;
    float lhs_norm;
    float rhs_norm;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        float32x4_t lhs_vec = vld1q_f32(lhs + i);
        float32x4_t rhs_vec = vld1q_f32(rhs + i);
        dot_acc = vmlaq_f32(dot_acc, lhs_vec, rhs_vec);
        lhs_norm_acc = vmlaq_f32(lhs_norm_acc, lhs_vec, lhs_vec);
        rhs_norm_acc = vmlaq_f32(rhs_norm_acc, rhs_vec, rhs_vec);
    }

    dot = distance_horizontal_sum_f32x4(dot_acc);
    lhs_norm = distance_horizontal_sum_f32x4(lhs_norm_acc);
    rhs_norm = distance_horizontal_sum_f32x4(rhs_norm_acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) {
        return 0.0f;
    }
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - dot / (sqrtf(lhs_norm) * sqrtf(rhs_norm));
}

static void distance_l2sqr_batch4_simd(const float *query,
                                            const float *vec1,
                                            const float *vec2,
                                            const float *vec3,
                                            const float *vec4,
                                            int32_t dims,
                                            float *dis1,
                                            float *dis2,
                                            float *dis3,
                                            float *dis4)
{
    float32x4_t acc = vdupq_n_f32(0.0f);
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        float32x4_t query_value = vdupq_n_f32(query[i]);
        float32x4_t values = distance_load_batch4_neon(vec1, vec2, vec3, vec4, i);
        float32x4_t diff = vsubq_f32(query_value, values);
        acc = vmlaq_f32(acc, diff, diff);
    }

    vst1q_f32(results, acc);
    *dis1 = results[0];
    *dis2 = results[1];
    *dis3 = results[2];
    *dis4 = results[3];
}

static void distance_inner_product_batch4_simd(const float *query,
                                                    const float *vec1,
                                                    const float *vec2,
                                                    const float *vec3,
                                                    const float *vec4,
                                                    int32_t dims,
                                                    float *dis1,
                                                    float *dis2,
                                                    float *dis3,
                                                    float *dis4)
{
    float32x4_t acc = vdupq_n_f32(0.0f);
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        float32x4_t query_value = vdupq_n_f32(query[i]);
        float32x4_t values = distance_load_batch4_neon(vec1, vec2, vec3, vec4, i);
        acc = vmlaq_f32(acc, query_value, values);
    }

    vst1q_f32(results, acc);
    *dis1 = -results[0];
    *dis2 = -results[1];
    *dis3 = -results[2];
    *dis4 = -results[3];
}

static void distance_cosine_batch4_simd(const float *query,
                                             const float *vec1,
                                             const float *vec2,
                                             const float *vec3,
                                             const float *vec4,
                                             int32_t dims,
                                             float *dis1,
                                             float *dis2,
                                             float *dis3,
                                             float *dis4)
{
    float32x4_t dot_acc = vdupq_n_f32(0.0f);
    float32x4_t lhs_norm_acc = vdupq_n_f32(0.0f);
    float32x4_t rhs_norm_acc = vdupq_n_f32(0.0f);
    float dots[4];
    float lhs_norms[4];
    float rhs_norms[4];
    float *outputs[4];
    int32_t i;

    outputs[0] = dis1;
    outputs[1] = dis2;
    outputs[2] = dis3;
    outputs[3] = dis4;

    for (i = 0; i < dims; ++i) {
        float32x4_t query_value = vdupq_n_f32(query[i]);
        float32x4_t values = distance_load_batch4_neon(vec1, vec2, vec3, vec4, i);
        dot_acc = vmlaq_f32(dot_acc, query_value, values);
        lhs_norm_acc = vmlaq_f32(lhs_norm_acc, query_value, query_value);
        rhs_norm_acc = vmlaq_f32(rhs_norm_acc, values, values);
    }

    vst1q_f32(dots, dot_acc);
    vst1q_f32(lhs_norms, lhs_norm_acc);
    vst1q_f32(rhs_norms, rhs_norm_acc);
    for (i = 0; i < 4; ++i) {
        if (lhs_norms[i] <= 0.0f && rhs_norms[i] <= 0.0f) {
            *outputs[i] = 0.0f;
        } else if (lhs_norms[i] <= 0.0f || rhs_norms[i] <= 0.0f) {
            *outputs[i] = 1.0f;
        } else {
            *outputs[i] = 1.0f - dots[i] / (sqrtf(lhs_norms[i]) * sqrtf(rhs_norms[i]));
        }
    }
}
#elif defined(DISTANCE_USE_AVX)
static float distance_horizontal_sum_m256(__m256 value)
{
    float sums[8];

    _mm256_storeu_ps(sums, value);
    return sums[0] + sums[1] + sums[2] + sums[3] + sums[4] + sums[5] + sums[6] + sums[7];
}

static __m128 distance_load_batch4_x86(const float *vec1,
                                            const float *vec2,
                                            const float *vec3,
                                            const float *vec4,
                                            int32_t index)
{
    return _mm_setr_ps(vec1[index], vec2[index], vec3[index], vec4[index]);
}

static float distance_l2sqr_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m256 acc = _mm256_setzero_ps();
    float result;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 8; i += 8) {
        __m256 lhs_vec = _mm256_loadu_ps(lhs + i);
        __m256 rhs_vec = _mm256_loadu_ps(rhs + i);
        __m256 diff = _mm256_sub_ps(lhs_vec, rhs_vec);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(diff, diff));
    }

    result = distance_horizontal_sum_m256(acc);
    for (; i < dims; ++i) {
        float diff = lhs[i] - rhs[i];
        result += diff * diff;
    }

    return result;
}

static float distance_inner_product_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m256 acc = _mm256_setzero_ps();
    float dot;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 8; i += 8) {
        __m256 lhs_vec = _mm256_loadu_ps(lhs + i);
        __m256 rhs_vec = _mm256_loadu_ps(rhs + i);
        acc = _mm256_add_ps(acc, _mm256_mul_ps(lhs_vec, rhs_vec));
    }

    dot = distance_horizontal_sum_m256(acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }

    return -dot;
}

static float distance_cosine_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m256 dot_acc = _mm256_setzero_ps();
    __m256 lhs_norm_acc = _mm256_setzero_ps();
    __m256 rhs_norm_acc = _mm256_setzero_ps();
    float dot;
    float lhs_norm;
    float rhs_norm;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 8; i += 8) {
        __m256 lhs_vec = _mm256_loadu_ps(lhs + i);
        __m256 rhs_vec = _mm256_loadu_ps(rhs + i);
        dot_acc = _mm256_add_ps(dot_acc, _mm256_mul_ps(lhs_vec, rhs_vec));
        lhs_norm_acc = _mm256_add_ps(lhs_norm_acc, _mm256_mul_ps(lhs_vec, lhs_vec));
        rhs_norm_acc = _mm256_add_ps(rhs_norm_acc, _mm256_mul_ps(rhs_vec, rhs_vec));
    }

    dot = distance_horizontal_sum_m256(dot_acc);
    lhs_norm = distance_horizontal_sum_m256(lhs_norm_acc);
    rhs_norm = distance_horizontal_sum_m256(rhs_norm_acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) {
        return 0.0f;
    }
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - dot / (sqrtf(lhs_norm) * sqrtf(rhs_norm));
}

static void distance_l2sqr_batch4_simd(const float *query,
                                            const float *vec1,
                                            const float *vec2,
                                            const float *vec3,
                                            const float *vec4,
                                            int32_t dims,
                                            float *dis1,
                                            float *dis2,
                                            float *dis3,
                                            float *dis4)
{
    __m128 acc = _mm_setzero_ps();
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        __m128 diff = _mm_sub_ps(query_value, values);
        acc = _mm_add_ps(acc, _mm_mul_ps(diff, diff));
    }

    _mm_storeu_ps(results, acc);
    *dis1 = results[0];
    *dis2 = results[1];
    *dis3 = results[2];
    *dis4 = results[3];
}

static void distance_inner_product_batch4_simd(const float *query,
                                                    const float *vec1,
                                                    const float *vec2,
                                                    const float *vec3,
                                                    const float *vec4,
                                                    int32_t dims,
                                                    float *dis1,
                                                    float *dis2,
                                                    float *dis3,
                                                    float *dis4)
{
    __m128 acc = _mm_setzero_ps();
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        acc = _mm_add_ps(acc, _mm_mul_ps(query_value, values));
    }

    _mm_storeu_ps(results, acc);
    *dis1 = -results[0];
    *dis2 = -results[1];
    *dis3 = -results[2];
    *dis4 = -results[3];
}

static void distance_cosine_batch4_simd(const float *query,
                                             const float *vec1,
                                             const float *vec2,
                                             const float *vec3,
                                             const float *vec4,
                                             int32_t dims,
                                             float *dis1,
                                             float *dis2,
                                             float *dis3,
                                             float *dis4)
{
    __m128 dot_acc = _mm_setzero_ps();
    __m128 lhs_norm_acc = _mm_setzero_ps();
    __m128 rhs_norm_acc = _mm_setzero_ps();
    float dots[4];
    float lhs_norms[4];
    float rhs_norms[4];
    float *outputs[4];
    int32_t i;

    outputs[0] = dis1;
    outputs[1] = dis2;
    outputs[2] = dis3;
    outputs[3] = dis4;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        dot_acc = _mm_add_ps(dot_acc, _mm_mul_ps(query_value, values));
        lhs_norm_acc = _mm_add_ps(lhs_norm_acc, _mm_mul_ps(query_value, query_value));
        rhs_norm_acc = _mm_add_ps(rhs_norm_acc, _mm_mul_ps(values, values));
    }

    _mm_storeu_ps(dots, dot_acc);
    _mm_storeu_ps(lhs_norms, lhs_norm_acc);
    _mm_storeu_ps(rhs_norms, rhs_norm_acc);
    for (i = 0; i < 4; ++i) {
        if (lhs_norms[i] <= 0.0f && rhs_norms[i] <= 0.0f) {
            *outputs[i] = 0.0f;
        } else if (lhs_norms[i] <= 0.0f || rhs_norms[i] <= 0.0f) {
            *outputs[i] = 1.0f;
        } else {
            *outputs[i] = 1.0f - dots[i] / (sqrtf(lhs_norms[i]) * sqrtf(rhs_norms[i]));
        }
    }
}
#elif defined(DISTANCE_USE_SSE)
static float distance_horizontal_sum_m128(__m128 value)
{
    float sums[4];

    _mm_storeu_ps(sums, value);
    return sums[0] + sums[1] + sums[2] + sums[3];
}

static __m128 distance_load_batch4_x86(const float *vec1,
                                            const float *vec2,
                                            const float *vec3,
                                            const float *vec4,
                                            int32_t index)
{
    return _mm_setr_ps(vec1[index], vec2[index], vec3[index], vec4[index]);
}

static float distance_l2sqr_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m128 acc = _mm_setzero_ps();
    float result;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        __m128 lhs_vec = _mm_loadu_ps(lhs + i);
        __m128 rhs_vec = _mm_loadu_ps(rhs + i);
        __m128 diff = _mm_sub_ps(lhs_vec, rhs_vec);
        acc = _mm_add_ps(acc, _mm_mul_ps(diff, diff));
    }

    result = distance_horizontal_sum_m128(acc);
    for (; i < dims; ++i) {
        float diff = lhs[i] - rhs[i];
        result += diff * diff;
    }

    return result;
}

static float distance_inner_product_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m128 acc = _mm_setzero_ps();
    float dot;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        __m128 lhs_vec = _mm_loadu_ps(lhs + i);
        __m128 rhs_vec = _mm_loadu_ps(rhs + i);
        acc = _mm_add_ps(acc, _mm_mul_ps(lhs_vec, rhs_vec));
    }

    dot = distance_horizontal_sum_m128(acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
    }

    return -dot;
}

static float distance_cosine_simd(const float *lhs, const float *rhs, int32_t dims)
{
    __m128 dot_acc = _mm_setzero_ps();
    __m128 lhs_norm_acc = _mm_setzero_ps();
    __m128 rhs_norm_acc = _mm_setzero_ps();
    float dot;
    float lhs_norm;
    float rhs_norm;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i <= dims - 4; i += 4) {
        __m128 lhs_vec = _mm_loadu_ps(lhs + i);
        __m128 rhs_vec = _mm_loadu_ps(rhs + i);
        dot_acc = _mm_add_ps(dot_acc, _mm_mul_ps(lhs_vec, rhs_vec));
        lhs_norm_acc = _mm_add_ps(lhs_norm_acc, _mm_mul_ps(lhs_vec, lhs_vec));
        rhs_norm_acc = _mm_add_ps(rhs_norm_acc, _mm_mul_ps(rhs_vec, rhs_vec));
    }

    dot = distance_horizontal_sum_m128(dot_acc);
    lhs_norm = distance_horizontal_sum_m128(lhs_norm_acc);
    rhs_norm = distance_horizontal_sum_m128(rhs_norm_acc);
    for (; i < dims; ++i) {
        dot += lhs[i] * rhs[i];
        lhs_norm += lhs[i] * lhs[i];
        rhs_norm += rhs[i] * rhs[i];
    }

    if (lhs_norm <= 0.0f && rhs_norm <= 0.0f) {
        return 0.0f;
    }
    if (lhs_norm <= 0.0f || rhs_norm <= 0.0f) {
        return 1.0f;
    }

    return 1.0f - dot / (sqrtf(lhs_norm) * sqrtf(rhs_norm));
}

static void distance_l2sqr_batch4_simd(const float *query,
                                            const float *vec1,
                                            const float *vec2,
                                            const float *vec3,
                                            const float *vec4,
                                            int32_t dims,
                                            float *dis1,
                                            float *dis2,
                                            float *dis3,
                                            float *dis4)
{
    __m128 acc = _mm_setzero_ps();
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        __m128 diff = _mm_sub_ps(query_value, values);
        acc = _mm_add_ps(acc, _mm_mul_ps(diff, diff));
    }

    _mm_storeu_ps(results, acc);
    *dis1 = results[0];
    *dis2 = results[1];
    *dis3 = results[2];
    *dis4 = results[3];
}

static void distance_inner_product_batch4_simd(const float *query,
                                                    const float *vec1,
                                                    const float *vec2,
                                                    const float *vec3,
                                                    const float *vec4,
                                                    int32_t dims,
                                                    float *dis1,
                                                    float *dis2,
                                                    float *dis3,
                                                    float *dis4)
{
    __m128 acc = _mm_setzero_ps();
    float results[4];
    int32_t i;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        acc = _mm_add_ps(acc, _mm_mul_ps(query_value, values));
    }

    _mm_storeu_ps(results, acc);
    *dis1 = -results[0];
    *dis2 = -results[1];
    *dis3 = -results[2];
    *dis4 = -results[3];
}

static void distance_cosine_batch4_simd(const float *query,
                                             const float *vec1,
                                             const float *vec2,
                                             const float *vec3,
                                             const float *vec4,
                                             int32_t dims,
                                             float *dis1,
                                             float *dis2,
                                             float *dis3,
                                             float *dis4)
{
    __m128 dot_acc = _mm_setzero_ps();
    __m128 lhs_norm_acc = _mm_setzero_ps();
    __m128 rhs_norm_acc = _mm_setzero_ps();
    float dots[4];
    float lhs_norms[4];
    float rhs_norms[4];
    float *outputs[4];
    int32_t i;

    outputs[0] = dis1;
    outputs[1] = dis2;
    outputs[2] = dis3;
    outputs[3] = dis4;

    for (i = 0; i < dims; ++i) {
        __m128 query_value = _mm_set1_ps(query[i]);
        __m128 values = distance_load_batch4_x86(vec1, vec2, vec3, vec4, i);
        dot_acc = _mm_add_ps(dot_acc, _mm_mul_ps(query_value, values));
        lhs_norm_acc = _mm_add_ps(lhs_norm_acc, _mm_mul_ps(query_value, query_value));
        rhs_norm_acc = _mm_add_ps(rhs_norm_acc, _mm_mul_ps(values, values));
    }

    _mm_storeu_ps(dots, dot_acc);
    _mm_storeu_ps(lhs_norms, lhs_norm_acc);
    _mm_storeu_ps(rhs_norms, rhs_norm_acc);
    for (i = 0; i < 4; ++i) {
        if (lhs_norms[i] <= 0.0f && rhs_norms[i] <= 0.0f) {
            *outputs[i] = 0.0f;
        } else if (lhs_norms[i] <= 0.0f || rhs_norms[i] <= 0.0f) {
            *outputs[i] = 1.0f;
        } else {
            *outputs[i] = 1.0f - dots[i] / (sqrtf(lhs_norms[i]) * sqrtf(rhs_norms[i]));
        }
    }
}
#endif

static float distance_cosine(const float *lhs, const float *rhs, int32_t dims)
{
#if defined(DISTANCE_HAS_SIMD)
    return distance_cosine_simd(lhs, rhs, dims);
#else
    return distance_cosine_scalar(lhs, rhs, dims);
#endif
}

static float distance_inner_product(const float *lhs, const float *rhs, int32_t dims)
{
#if defined(DISTANCE_HAS_SIMD)
    return distance_inner_product_simd(lhs, rhs, dims);
#else
    return distance_inner_product_scalar(lhs, rhs, dims);
#endif
}

static float distance_hamming(const float *lhs, const float *rhs, int32_t dims)
{
    float result = 0.0f;
    int32_t i;

    if (!lhs || !rhs || dims <= 0) {
        return 0.0f;
    }

    for (i = 0; i < dims; ++i) {
        if (fabsf(lhs[i] - rhs[i]) > DISTANCE_EPSILON) {
            result += 1.0f;
        }
    }

    return result;
}

static int distance_batch4_inputs_are_invalid(const float *query,
                                                   const float *vectors,
                                                   int32_t dims,
                                                   int32_t id1,
                                                   int32_t id2,
                                                   int32_t id3,
                                                   int32_t id4)
{
    return !query || !vectors || dims <= 0 || id1 < 0 || id2 < 0 || id3 < 0 || id4 < 0;
}

int distance_metric_is_valid(distance_metric_t metric)
{
    return metric >= DISTANCE_METRIC_L2_SQUARED && metric <= DISTANCE_METRIC_HAMMING;
}

float distance_compute(distance_metric_t metric, const float *lhs, const float *rhs, int32_t dims)
{
    if (!distance_metric_is_valid(metric)) {
        return 0.0f;
    }

    switch (metric) {
        case DISTANCE_METRIC_L2_SQUARED:
            return distance_l2sqr(lhs, rhs, dims);
        case DISTANCE_METRIC_COSINE:
            return distance_cosine(lhs, rhs, dims);
        case DISTANCE_METRIC_INNER_PRODUCT:
            return distance_inner_product(lhs, rhs, dims);
        case DISTANCE_METRIC_HAMMING:
            return distance_hamming(lhs, rhs, dims);
        default:
            return 0.0f;
    }
}

float distance_compute_indexed(distance_metric_t metric,
                                    const float *vectors,
                                    int32_t dims,
                                    int32_t id1,
                                    int32_t id2)
{
    if (!vectors || dims <= 0 || id1 < 0 || id2 < 0) {
        return 0.0f;
    }

    return distance_compute(metric, &vectors[id1 * dims], &vectors[id2 * dims], dims);
}

float distance_compute_from_query(distance_metric_t metric,
                                       const float *query,
                                       const float *vectors,
                                       int32_t dims,
                                       int32_t id)
{
    if (!query || !vectors || dims <= 0 || id < 0) {
        return 0.0f;
    }

    return distance_compute(metric, query, &vectors[id * dims], dims);
}

void distance_compute_batch4_from_query(distance_metric_t metric,
                                             const float *query,
                                             const float *vectors,
                                             int32_t dims,
                                             int32_t id1,
                                             int32_t id2,
                                             int32_t id3,
                                             int32_t id4,
                                             float *dis1,
                                             float *dis2,
                                             float *dis3,
                                             float *dis4)
{
    const float *vec1;
    const float *vec2;
    const float *vec3;
    const float *vec4;

    if (!dis1 || !dis2 || !dis3 || !dis4) {
        return;
    }
    if (!distance_metric_is_valid(metric) ||
        distance_batch4_inputs_are_invalid(query, vectors, dims, id1, id2, id3, id4)) {
        distance_store_zero4(dis1, dis2, dis3, dis4);
        return;
    }

    vec1 = &vectors[id1 * dims];
    vec2 = &vectors[id2 * dims];
    vec3 = &vectors[id3 * dims];
    vec4 = &vectors[id4 * dims];

#if defined(DISTANCE_HAS_SIMD)
    switch (metric) {
        case DISTANCE_METRIC_L2_SQUARED:
            distance_l2sqr_batch4_simd(query, vec1, vec2, vec3, vec4, dims, dis1, dis2, dis3, dis4);
            return;
        case DISTANCE_METRIC_COSINE:
            distance_cosine_batch4_simd(query, vec1, vec2, vec3, vec4, dims, dis1, dis2, dis3, dis4);
            return;
        case DISTANCE_METRIC_INNER_PRODUCT:
            distance_inner_product_batch4_simd(query, vec1, vec2, vec3, vec4, dims, dis1, dis2, dis3, dis4);
            return;
        default:
            break;
    }
#endif

    *dis1 = distance_compute(metric, query, vec1, dims);
    *dis2 = distance_compute(metric, query, vec2, dims);
    *dis3 = distance_compute(metric, query, vec3, dims);
    *dis4 = distance_compute(metric, query, vec4, dims);
}

float distance_l2sqr(const float *lhs, const float *rhs, int32_t dims)
{
#if defined(DISTANCE_HAS_SIMD)
    return distance_l2sqr_simd(lhs, rhs, dims);
#else
    return distance_l2sqr_scalar(lhs, rhs, dims);
#endif
}

float distance_l2sqr_indexed(const float *vectors, int32_t dims, int32_t id1, int32_t id2)
{
    return distance_compute_indexed(DISTANCE_METRIC_L2_SQUARED, vectors, dims, id1, id2);
}

float distance_l2sqr_from_query(const float *query, const float *vectors, int32_t dims, int32_t id)
{
    return distance_compute_from_query(DISTANCE_METRIC_L2_SQUARED, query, vectors, dims, id);
}

void distance_l2sqr_batch4_from_query(const float *query,
                                           const float *vectors,
                                           int32_t dims,
                                           int32_t id1,
                                           int32_t id2,
                                           int32_t id3,
                                           int32_t id4,
                                           float *dis1,
                                           float *dis2,
                                           float *dis3,
                                           float *dis4)
{
    distance_compute_batch4_from_query(DISTANCE_METRIC_L2_SQUARED,
                                            query,
                                            vectors,
                                            dims,
                                            id1,
                                            id2,
                                            id3,
                                            id4,
                                            dis1,
                                            dis2,
                                            dis3,
                                            dis4);
}