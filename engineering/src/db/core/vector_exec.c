/**
 * @file vector_exec.c
 * @brief 向量化执行引擎实现
 */

#include "db/core/vector_exec.h"
#include <db/sql/sql_executor.h>  /* 需要完整类型定义 */
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* 前向声明 */
struct TupleTableSlot_s;  /* 已在 sql_executor.h 中定义，这里仅用于类型安全 */

/* ========================================================================
 * SIMD 内核的编译期能力探测
 *
 * 分三层，任一层不可用都会安全退化到下一层，最终一定有可移植标量实现：
 *   VEXEC_HAVE_SSE2   —— x86 基线（x86_64 硬件保证有 SSE2），无需 target 属性
 *   VEXEC_HAVE_SSE42  —— 提供 _mm_cmpgt_epi64（int64 比较），靠 GCC/Clang 的
 *                        函数级 target 属性编译，不要求整个 TU 开 -msse4.2
 *   VEXEC_HAVE_AVX2   —— 256 位内核，同样靠函数级 target 属性
 *
 * MSVC 没有 __attribute__((target(...)))，也没有 __builtin_cpu_supports，
 * 因此在 MSVC 上只编译 SSE2 与标量两层；非 x86 平台只编译标量层。
 * ======================================================================== */

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#  define VEXEC_X86 1
#endif

#if defined(VEXEC_X86) && (defined(__SSE2__) || defined(_M_X64))
#  define VEXEC_HAVE_SSE2 1
#  include <emmintrin.h>
#endif

/* 函数级多版本编译：仅 GCC/Clang 支持，且必须同时具备运行时检测手段
   （__builtin_cpu_supports），否则无从判断能否安全执行高版本内核。 */
#if defined(VEXEC_HAVE_SSE2) && (defined(__GNUC__) || defined(__clang__))
#  define VEXEC_HAVE_CPU_PROBE 1
#  define VEXEC_HAVE_SSE42 1
#  define VEXEC_HAVE_AVX2  1
#  include <immintrin.h>
#  define VEXEC_TARGET(feat) __attribute__((target(feat)))
#else
#  define VEXEC_TARGET(feat)
#endif

/* ========================================================================
 * 位图写入公共辅助
 * ======================================================================== */

/**
 * 把一次 SIMD 迭代得到的通道掩码 w（bit j 表示元素 i+j 是否命中）写入位图。
 *
 * 所有内核的循环都从 i=0 开始、每次前进 step 个元素，step ∈ {2,4,8} 均整除 64，
 * 因此 (i%64) 恒为 step 的倍数，(i%64)+step <= 64 恒成立，实际永不跨字。
 * 仍保留跨字分支作为防御：该分支只可能在 bit>0 时进入，故不存在 w>>64 的 UB。
 */
static inline void vexec_bits_write(uint64_t *result, int i, int step, uint64_t w) {
    int widx = i >> 6;
    int bit  = i & 63;
    result[widx] |= (w << bit);
    if (bit + step > 64) {
        result[widx + 1] |= (w >> (64 - bit));
    }
}

/**
 * 标量比较尾部：从当前 i 处理到 n。
 * 依赖上下文中已存在的 a / b / n / op / result / i 变量，四种数值类型通用。
 * 这段逻辑就是全局唯一的「标量参考语义」，SIMD 内核必须与它逐位一致。
 */
#define VEXEC_SCALAR_TAIL()                                        \
    for (; i < n; i++) {                                           \
        bool m_ = false;                                           \
        switch (op) {                                              \
            case CMP_EQ: m_ = (a[i] == b); break;                  \
            case CMP_NE: m_ = (a[i] != b); break;                  \
            case CMP_LT: m_ = (a[i] <  b); break;                  \
            case CMP_LE: m_ = (a[i] <= b); break;                  \
            case CMP_GT: m_ = (a[i] >  b); break;                  \
            case CMP_GE: m_ = (a[i] >= b); break;                  \
            default:     m_ = false;       break;                  \
        }                                                          \
        if (m_) result[i >> 6] |= (1ULL << (i & 63));              \
    }

/* ========================================================================
 * 列块操作
 * ======================================================================== */

VectorBlock *vector_block_create(int capacity, int num_columns) {
    VectorBlock *block = (VectorBlock *)calloc(1, sizeof(VectorBlock));
    if (!block) return NULL;

    block->capacity = capacity > 0 ? capacity : 1024;
    block->num_columns = num_columns;
    block->num_rows = 0;

    block->columns = (void **)calloc((size_t)num_columns, sizeof(void *));
    block->column_sizes = (int *)calloc((size_t)num_columns, sizeof(int));
    block->null_bitmap = (uint64_t *)calloc((size_t)((capacity + 63) / 64), sizeof(uint64_t));
    block->sel_bitmap = (uint64_t *)calloc((size_t)((capacity + 63) / 64), sizeof(uint64_t));
    block->selection_vector = (int *)calloc((size_t)capacity, sizeof(int));

    /* 每列类型标签：calloc 会清零成 0=COLUMN_INT8，必须显式填 -1（未知），
       保证 ANN 等从不设置列类型的路径行为与现状完全一致。 */
    block->column_types = (int *)calloc((size_t)num_columns, sizeof(int));
    for (int i = 0; i < num_columns; i++) block->column_types[i] = -1;

    return block;
}

void vector_block_destroy(VectorBlock *block) {
    if (!block) return;
    for (int i = 0; i < block->num_columns; i++) {
        free(block->columns[i]);
    }
    free(block->columns);
    free(block->column_sizes);
    free(block->column_types);
    free(block->null_bitmap);
    free(block->sel_bitmap);
    free(block->selection_vector);
    free(block);
}

int vector_block_set_column(VectorBlock *block, int col_idx, void *data, int element_size) {
    if (!block || col_idx < 0 || col_idx >= block->num_columns) return -1;
    block->columns[col_idx] = data;
    block->column_sizes[col_idx] = element_size;
    return 0;
}

void *vector_block_get_column(VectorBlock *block, int col_idx) {
    if (!block || col_idx < 0 || col_idx >= block->num_columns) return NULL;
    return block->columns[col_idx];
}

void vector_block_set_num_rows(VectorBlock *block, int num_rows) {
    if (block) block->num_rows = num_rows < block->capacity ? num_rows : block->capacity;
}

void vector_block_set_null(VectorBlock *block, int row_idx, bool isnull) {
    if (!block || row_idx < 0 || row_idx >= block->capacity) return;
    if (isnull) {
        block->null_bitmap[row_idx / 64] |= (1ULL << (row_idx % 64));
    } else {
        block->null_bitmap[row_idx / 64] &= ~(1ULL << (row_idx % 64));
    }
}

bool vector_block_is_null(VectorBlock *block, int row_idx) {
    if (!block || row_idx < 0 || row_idx >= block->capacity) return false;
    return (block->null_bitmap[row_idx / 64] & (1ULL << (row_idx % 64))) != 0;
}

/* 设置某列的数据类型标签（ColumnType 枚举值） */
void vector_block_set_column_type(VectorBlock *block, int col_idx, int col_type) {
    if (!block || !block->column_types || col_idx < 0 || col_idx >= block->num_columns) return;
    block->column_types[col_idx] = col_type;
}

/* 读取某列的数据类型标签；越界/未知返回 -1 */
int vector_block_get_column_type(const VectorBlock *block, int col_idx) {
    if (!block || !block->column_types || col_idx < 0 || col_idx >= block->num_columns) return -1;
    return block->column_types[col_idx];
}

/* ========================================================================
 * 批次操作
 * ======================================================================== */

VectorBatch *vector_batch_create(int capacity) {
    VectorBatch *batch = (VectorBatch *)calloc(1, sizeof(VectorBatch));
    if (!batch) return NULL;

    batch->capacity = capacity > 0 ? capacity : 4096;
    batch->blocks = (VectorBlock **)calloc(16, sizeof(VectorBlock *));
    batch->num_blocks = 0;
    batch->num_rows = 0;
    batch->current_block = 0;

    return batch;
}

void vector_batch_destroy(VectorBatch *batch) {
    if (!batch) return;
    for (int i = 0; i < batch->num_blocks; i++) {
        vector_block_destroy(batch->blocks[i]);
    }
    free(batch->blocks);
    free(batch);
}

int vector_batch_add_block(VectorBatch *batch, VectorBlock *block) {
    if (!batch || !block) return -1;
    if (batch->num_blocks >= 16) return -1;

    batch->blocks[batch->num_blocks++] = block;
    batch->num_rows += block->num_rows;
    return 0;
}

VectorBlock *vector_batch_next(VectorBatch *batch) {
    if (!batch || batch->current_block >= batch->num_blocks) return NULL;
    return batch->blocks[batch->current_block++];
}

/* ========================================================================
 * SIMD 距离计算（纯 C 实现）
 * ======================================================================== */

float vector_distance_l2_simd(const float *a, const float *b, int dim) {
    if (!a || !b || dim <= 0) return 0;
    float sum = 0;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

float vector_distance_cosine_simd(const float *a, const float *b, int dim) {
    if (!a || !b || dim <= 0) return 0;
    float dot = 0, norm_a = 0, norm_b = 0;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    if (norm_a == 0 || norm_b == 0) return 1.0f;
    return 1.0f - dot / (sqrtf(norm_a) * sqrtf(norm_b));
}

float vector_dot_product_simd(const float *a, const float *b, int dim) {
    if (!a || !b || dim <= 0) return 0;
    float sum = 0;
    for (int i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

void vector_batch_l2_distance_simd(const float *query, const float **vectors,
                                 int dim, int num_vectors, float *distances) {
    if (!query || !vectors || !distances || dim <= 0) return;
    for (int i = 0; i < num_vectors; i++) {
        distances[i] = vector_distance_l2_simd(query, vectors[i], dim);
    }
}

void vector_batch_cosine_distance_simd(const float *query, const float **vectors,
                                      int dim, int num_vectors, float *distances) {
    if (!query || !vectors || !distances || dim <= 0) return;
    for (int i = 0; i < num_vectors; i++) {
        distances[i] = vector_distance_cosine_simd(query, vectors[i], dim);
    }
}

bool vector_has_simd_support(void) {
    /* 如实报告：只要检测到任一可用扩展就返回 true */
    return simd_get_best_extension() != SIMD_NONE;
}

const char *vector_get_simd_type(void) {
    /* 返回本次运行**实际分派**到的内核名，而非编译期猜测 */
    switch (simd_get_best_extension()) {
        case SIMD_AVX512: return "avx512";
        case SIMD_AVX2:   return "avx2";
        case SIMD_AVX:    return "avx";
        case SIMD_SSE4:   return "sse4.2";
        case SIMD_SSE2:   return "sse2";
        case SIMD_SSE:    return "sse";
        case SIMD_NEON:   return "neon";
        case SIMD_NONE:
        default:          return "scalar";
    }
}

/* ========================================================================
 * SIMD 过滤
 *
 * 每种数值类型有 2~3 份内核：
 *   filter_xxx_scalar —— 可移植标量参考，**始终参与编译**，是语义的唯一真源
 *   filter_xxx_sse2   —— 128 位内核（x86 基线）
 *   filter_xxx_sse42  —— 仅 int64 需要（SSE2 无 64 位整数比较指令）
 *   filter_xxx_avx2   —— 256 位内核
 *
 * 公开函数负责：入参校验 → 清零 ceil(n/64) 个字 → 按运行时 CPU 能力挑最快内核。
 * 每个 SIMD 内核自己用 VEXEC_SCALAR_TAIL() 收尾，保证任意长度都正确。
 *
 * 浮点 NaN 语义：SSE/AVX 的 cmpeq/cmplt/cmple/cmpgt/cmpge 都是 ordered（遇 NaN 为假），
 * cmpneq 是 unordered（遇 NaN 为真），与 C 的 == < <= > >= != 完全吻合，
 * 因此 SIMD 结果与标量参考逐位一致，无需特殊处理。
 * ======================================================================== */

/* ------------------------------ int32 ------------------------------ */

static void filter_int32_scalar(const int32_t *a, int32_t b, int n,
                                CompareOp op, uint64_t *result) {
    int i = 0;
    VEXEC_SCALAR_TAIL();
}

#ifdef VEXEC_HAVE_SSE2
/* SSE2：每轮 4 个 int32。_mm_movemask_ps 对 4 个 32 位通道各取 1 位，正好对齐位图。 */
static void filter_int32_sse2(const int32_t *a, int32_t b, int n,
                              CompareOp op, uint64_t *result) {
    const __m128i vb   = _mm_set1_epi32(b);
    const __m128i ones = _mm_set1_epi32(-1);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i va = _mm_loadu_si128((const __m128i *)(const void *)(a + i));
        __m128i mask = _mm_setzero_si128();
        switch (op) {
            case CMP_EQ: mask = _mm_cmpeq_epi32(va, vb); break;
            case CMP_NE: mask = _mm_xor_si128(_mm_cmpeq_epi32(va, vb), ones); break;
            case CMP_GT: mask = _mm_cmpgt_epi32(va, vb); break;
            case CMP_LT: mask = _mm_cmpgt_epi32(vb, va); break;                      /* b > a */
            case CMP_GE: mask = _mm_xor_si128(_mm_cmpgt_epi32(vb, va), ones); break; /* !(a < b) */
            case CMP_LE: mask = _mm_xor_si128(_mm_cmpgt_epi32(va, vb), ones); break; /* !(a > b) */
            default: break;
        }
        vexec_bits_write(result, i, 4,
                         (uint64_t)(unsigned)_mm_movemask_ps(_mm_castsi128_ps(mask)));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_SSE2 */

#ifdef VEXEC_HAVE_AVX2
/* AVX2：每轮 8 个 int32 */
VEXEC_TARGET("avx2")
static void filter_int32_avx2(const int32_t *a, int32_t b, int n,
                              CompareOp op, uint64_t *result) {
    const __m256i vb   = _mm256_set1_epi32(b);
    const __m256i ones = _mm256_set1_epi32(-1);
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256i va = _mm256_loadu_si256((const __m256i *)(const void *)(a + i));
        __m256i mask = _mm256_setzero_si256();
        switch (op) {
            case CMP_EQ: mask = _mm256_cmpeq_epi32(va, vb); break;
            case CMP_NE: mask = _mm256_xor_si256(_mm256_cmpeq_epi32(va, vb), ones); break;
            case CMP_GT: mask = _mm256_cmpgt_epi32(va, vb); break;
            case CMP_LT: mask = _mm256_cmpgt_epi32(vb, va); break;
            case CMP_GE: mask = _mm256_xor_si256(_mm256_cmpgt_epi32(vb, va), ones); break;
            case CMP_LE: mask = _mm256_xor_si256(_mm256_cmpgt_epi32(va, vb), ones); break;
            default: break;
        }
        vexec_bits_write(result, i, 8,
                         (uint64_t)(unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(mask)));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_AVX2 */

void vector_filter_int_simd(const int32_t *a, int32_t b,
                          int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    memset(result, 0, (size_t)((num_elements + 63) / 64) * sizeof(uint64_t));

    SimdExtension best = simd_get_best_extension();
    (void)best;
#ifdef VEXEC_HAVE_AVX2
    if (best >= SIMD_AVX2 && best != SIMD_NEON) {
        filter_int32_avx2(a, b, num_elements, op, result);
        return;
    }
#endif
#ifdef VEXEC_HAVE_SSE2
    if (best >= SIMD_SSE2 && best != SIMD_NEON) {
        filter_int32_sse2(a, b, num_elements, op, result);
        return;
    }
#endif
    filter_int32_scalar(a, b, num_elements, op, result);
}

/* ------------------------------ int64 ------------------------------ */

static void filter_int64_scalar(const int64_t *a, int64_t b, int n,
                                CompareOp op, uint64_t *result) {
    int i = 0;
    VEXEC_SCALAR_TAIL();
}

#ifdef VEXEC_HAVE_SSE42
/* SSE4.2：每轮 2 个 int64。SSE2 没有 _mm_cmpgt_epi64（SSE4.2 才有）、
   也没有 _mm_cmpeq_epi64（SSE4.1 才有），所以 SSE2 级别的 int64 直接落标量。 */
VEXEC_TARGET("sse4.2")
static void filter_int64_sse42(const int64_t *a, int64_t b, int n,
                               CompareOp op, uint64_t *result) {
    const __m128i vb   = _mm_set1_epi64x(b);
    const __m128i ones = _mm_set1_epi32(-1);
    int i = 0;
    for (; i + 2 <= n; i += 2) {
        __m128i va = _mm_loadu_si128((const __m128i *)(const void *)(a + i));
        __m128i mask = _mm_setzero_si128();
        switch (op) {
            case CMP_EQ: mask = _mm_cmpeq_epi64(va, vb); break;
            case CMP_NE: mask = _mm_xor_si128(_mm_cmpeq_epi64(va, vb), ones); break;
            case CMP_GT: mask = _mm_cmpgt_epi64(va, vb); break;
            case CMP_LT: mask = _mm_cmpgt_epi64(vb, va); break;
            case CMP_GE: mask = _mm_xor_si128(_mm_cmpgt_epi64(vb, va), ones); break;
            case CMP_LE: mask = _mm_xor_si128(_mm_cmpgt_epi64(va, vb), ones); break;
            default: break;
        }
        vexec_bits_write(result, i, 2,
                         (uint64_t)(unsigned)_mm_movemask_pd(_mm_castsi128_pd(mask)));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_SSE42 */

#ifdef VEXEC_HAVE_AVX2
/* AVX2：每轮 4 个 int64 */
VEXEC_TARGET("avx2")
static void filter_int64_avx2(const int64_t *a, int64_t b, int n,
                              CompareOp op, uint64_t *result) {
    const __m256i vb   = _mm256_set1_epi64x(b);
    const __m256i ones = _mm256_set1_epi32(-1);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256i va = _mm256_loadu_si256((const __m256i *)(const void *)(a + i));
        __m256i mask = _mm256_setzero_si256();
        switch (op) {
            case CMP_EQ: mask = _mm256_cmpeq_epi64(va, vb); break;
            case CMP_NE: mask = _mm256_xor_si256(_mm256_cmpeq_epi64(va, vb), ones); break;
            case CMP_GT: mask = _mm256_cmpgt_epi64(va, vb); break;
            case CMP_LT: mask = _mm256_cmpgt_epi64(vb, va); break;
            case CMP_GE: mask = _mm256_xor_si256(_mm256_cmpgt_epi64(vb, va), ones); break;
            case CMP_LE: mask = _mm256_xor_si256(_mm256_cmpgt_epi64(va, vb), ones); break;
            default: break;
        }
        vexec_bits_write(result, i, 4,
                         (uint64_t)(unsigned)_mm256_movemask_pd(_mm256_castsi256_pd(mask)));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_AVX2 */

void vector_filter_int64_simd(const int64_t *a, int64_t b,
                            int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    memset(result, 0, (size_t)((num_elements + 63) / 64) * sizeof(uint64_t));

    SimdExtension best = simd_get_best_extension();
    (void)best;
#ifdef VEXEC_HAVE_AVX2
    if (best >= SIMD_AVX2 && best != SIMD_NEON) {
        filter_int64_avx2(a, b, num_elements, op, result);
        return;
    }
#endif
#ifdef VEXEC_HAVE_SSE42
    if (best >= SIMD_SSE4 && best != SIMD_NEON) {
        filter_int64_sse42(a, b, num_elements, op, result);
        return;
    }
#endif
    /* SSE2 及以下没有 64 位整数比较指令，走标量 */
    filter_int64_scalar(a, b, num_elements, op, result);
}

/* ------------------------------ float ------------------------------ */

static void filter_float_scalar(const float *a, float b, int n,
                                CompareOp op, uint64_t *result) {
    int i = 0;
    VEXEC_SCALAR_TAIL();
}

#ifdef VEXEC_HAVE_SSE2
/* SSE2：每轮 4 个 float（cmpps 的 ordered/unordered 语义与 C 一致） */
static void filter_float_sse2(const float *a, float b, int n,
                              CompareOp op, uint64_t *result) {
    const __m128 vb = _mm_set1_ps(b);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(a + i);
        __m128 mask = _mm_setzero_ps();
        switch (op) {
            case CMP_EQ: mask = _mm_cmpeq_ps(va, vb);  break;
            case CMP_NE: mask = _mm_cmpneq_ps(va, vb); break;
            case CMP_LT: mask = _mm_cmplt_ps(va, vb);  break;
            case CMP_LE: mask = _mm_cmple_ps(va, vb);  break;
            case CMP_GT: mask = _mm_cmpgt_ps(va, vb);  break;
            case CMP_GE: mask = _mm_cmpge_ps(va, vb);  break;
            default: break;
        }
        vexec_bits_write(result, i, 4, (uint64_t)(unsigned)_mm_movemask_ps(mask));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_SSE2 */

#ifdef VEXEC_HAVE_AVX2
/* AVX2：每轮 8 个 float。谓词取与 SSE cmpps 等价的变体：
   EQ_OQ / NEQ_UQ / LT_OS / LE_OS / GT_OS / GE_OS。 */
VEXEC_TARGET("avx2")
static void filter_float_avx2(const float *a, float b, int n,
                              CompareOp op, uint64_t *result) {
    const __m256 vb = _mm256_set1_ps(b);
    int i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 mask = _mm256_setzero_ps();
        switch (op) {
            case CMP_EQ: mask = _mm256_cmp_ps(va, vb, _CMP_EQ_OQ);  break;
            case CMP_NE: mask = _mm256_cmp_ps(va, vb, _CMP_NEQ_UQ); break;
            case CMP_LT: mask = _mm256_cmp_ps(va, vb, _CMP_LT_OS);  break;
            case CMP_LE: mask = _mm256_cmp_ps(va, vb, _CMP_LE_OS);  break;
            case CMP_GT: mask = _mm256_cmp_ps(va, vb, _CMP_GT_OS);  break;
            case CMP_GE: mask = _mm256_cmp_ps(va, vb, _CMP_GE_OS);  break;
            default: break;
        }
        vexec_bits_write(result, i, 8, (uint64_t)(unsigned)_mm256_movemask_ps(mask));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_AVX2 */

void vector_filter_float_simd(const float *a, float b,
                            int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    memset(result, 0, (size_t)((num_elements + 63) / 64) * sizeof(uint64_t));

    SimdExtension best = simd_get_best_extension();
    (void)best;
#ifdef VEXEC_HAVE_AVX2
    if (best >= SIMD_AVX2 && best != SIMD_NEON) {
        filter_float_avx2(a, b, num_elements, op, result);
        return;
    }
#endif
#ifdef VEXEC_HAVE_SSE2
    if (best >= SIMD_SSE2 && best != SIMD_NEON) {
        filter_float_sse2(a, b, num_elements, op, result);
        return;
    }
#endif
    filter_float_scalar(a, b, num_elements, op, result);
}

/* ------------------------------ double ------------------------------ */

static void filter_double_scalar(const double *a, double b, int n,
                                 CompareOp op, uint64_t *result) {
    int i = 0;
    VEXEC_SCALAR_TAIL();
}

#ifdef VEXEC_HAVE_SSE2
/* SSE2：每轮 2 个 double */
static void filter_double_sse2(const double *a, double b, int n,
                               CompareOp op, uint64_t *result) {
    const __m128d vb = _mm_set1_pd(b);
    int i = 0;
    for (; i + 2 <= n; i += 2) {
        __m128d va = _mm_loadu_pd(a + i);
        __m128d mask = _mm_setzero_pd();
        switch (op) {
            case CMP_EQ: mask = _mm_cmpeq_pd(va, vb);  break;
            case CMP_NE: mask = _mm_cmpneq_pd(va, vb); break;
            case CMP_LT: mask = _mm_cmplt_pd(va, vb);  break;
            case CMP_LE: mask = _mm_cmple_pd(va, vb);  break;
            case CMP_GT: mask = _mm_cmpgt_pd(va, vb);  break;
            case CMP_GE: mask = _mm_cmpge_pd(va, vb);  break;
            default: break;
        }
        vexec_bits_write(result, i, 2, (uint64_t)(unsigned)_mm_movemask_pd(mask));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_SSE2 */

#ifdef VEXEC_HAVE_AVX2
/* AVX2：每轮 4 个 double */
VEXEC_TARGET("avx2")
static void filter_double_avx2(const double *a, double b, int n,
                               CompareOp op, uint64_t *result) {
    const __m256d vb = _mm256_set1_pd(b);
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        __m256d va = _mm256_loadu_pd(a + i);
        __m256d mask = _mm256_setzero_pd();
        switch (op) {
            case CMP_EQ: mask = _mm256_cmp_pd(va, vb, _CMP_EQ_OQ);  break;
            case CMP_NE: mask = _mm256_cmp_pd(va, vb, _CMP_NEQ_UQ); break;
            case CMP_LT: mask = _mm256_cmp_pd(va, vb, _CMP_LT_OS);  break;
            case CMP_LE: mask = _mm256_cmp_pd(va, vb, _CMP_LE_OS);  break;
            case CMP_GT: mask = _mm256_cmp_pd(va, vb, _CMP_GT_OS);  break;
            case CMP_GE: mask = _mm256_cmp_pd(va, vb, _CMP_GE_OS);  break;
            default: break;
        }
        vexec_bits_write(result, i, 4, (uint64_t)(unsigned)_mm256_movemask_pd(mask));
    }
    VEXEC_SCALAR_TAIL();
}
#endif /* VEXEC_HAVE_AVX2 */

void vector_filter_double_simd(const double *a, double b,
                             int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    memset(result, 0, (size_t)((num_elements + 63) / 64) * sizeof(uint64_t));

    SimdExtension best = simd_get_best_extension();
    (void)best;
#ifdef VEXEC_HAVE_AVX2
    if (best >= SIMD_AVX2 && best != SIMD_NEON) {
        filter_double_avx2(a, b, num_elements, op, result);
        return;
    }
#endif
#ifdef VEXEC_HAVE_SSE2
    if (best >= SIMD_SSE2 && best != SIMD_NEON) {
        filter_double_sse2(a, b, num_elements, op, result);
        return;
    }
#endif
    filter_double_scalar(a, b, num_elements, op, result);
}

/* ------------------------------ string ------------------------------ */

void vector_filter_string_simd(const char **a, const char *b,
                            int num_elements, CompareOp op, uint64_t *result) {
    /* 字符串比较保持标量实现：变长数据的 SIMD 比较（pcmpistri / 前缀向量化 +
       字典序回退）不在本 gap 范围，留待字符串/文本模态专项任务。
       位图布局与写入语义与上面各数值变体完全一致。 */
    if (!a || !result || num_elements <= 0) return;
    for (int i = 0; i < num_elements; i++) {
        int cmp = strcmp(a[i], b);
        bool match = false;
        switch (op) {
            case CMP_EQ: match = (cmp == 0); break;
            case CMP_NE: match = (cmp != 0); break;
            default: match = false;
        }
        if (match) {
            result[i / 64] |= (1ULL << (i % 64));
        }
    }
}

/* ------------------------------------------------------------------------
 * 列类型常量
 *
 * 取值必须与 db/core/columnar_store.h 的 ColumnType 枚举一致。这里重复定义而不
 * include，是因为 vector_filter_execute 所在的 db_core 不能反向依赖上层的
 * db/vectorized/vectorized.h（会形成循环依赖），而 columnar_store.h 又会把
 * 列存的一整套类型拉进本 TU。教学级代码优先保持依赖面干净。
 * ------------------------------------------------------------------------ */
#define VEXEC_COL_TYPE_INT32  2
#define VEXEC_COL_TYPE_INT64  3
#define VEXEC_COL_TYPE_FLOAT  8
#define VEXEC_COL_TYPE_DOUBLE 9

VectorFilterResult *vector_filter_execute(VectorBlock *block,
                                       int column_idx, void *value, CompareOp op) {
    if (!block || !value) return NULL;

    void *column = vector_block_get_column(block, column_idx);
    if (!column) return NULL;

    VectorFilterResult *result = (VectorFilterResult *)calloc(1, sizeof(VectorFilterResult));
    if (!result) return NULL;
    result->num_matches = 0;

    /* --------------------------------------------------------------------
     * 类型化路径（加法式）：列带数值类型标签时走 SIMD 位图内核。
     *
     * 这里不复用 db_vectorized 的 vecx_filter_block，原因有二：
     *   1) 既有调用方（vector_query.c 的 exec_vector_filter）要的是**原始行号**，
     *      而 vecx_filter_block 产出的是 gather 后的压缩块，行号信息已丢失；
     *   2) db_core 不能依赖上层的 db_vectorized（循环依赖）。
     * 因此内联实现「位图 → 排除 null → 原始行号数组」，也顺带省掉一次 gather。
     *
     * 列类型为 -1（未知）、字符串或其它类型时，一律落回下方的旧路径，
     * 保证 ANN 执行链（从不设置列类型）行为与改动前逐位一致。
     * -------------------------------------------------------------------- */
    const int col_type = vector_block_get_column_type(block, column_idx);
    const int n = block->num_rows;
    if (n > 0 && (col_type == VEXEC_COL_TYPE_INT32 || col_type == VEXEC_COL_TYPE_INT64 ||
                  col_type == VEXEC_COL_TYPE_FLOAT || col_type == VEXEC_COL_TYPE_DOUBLE)) {
        int nwords = (n + 63) / 64;
        uint64_t *bitmap = (uint64_t *)calloc((size_t)nwords, sizeof(uint64_t));
        if (!bitmap) {
            free(result);
            return NULL;
        }

        switch (col_type) {
            case VEXEC_COL_TYPE_INT32:
                vector_filter_int_simd((const int32_t *)column, *(const int32_t *)value,
                                       n, op, bitmap);
                break;
            case VEXEC_COL_TYPE_INT64:
                vector_filter_int64_simd((const int64_t *)column, *(const int64_t *)value,
                                         n, op, bitmap);
                break;
            case VEXEC_COL_TYPE_FLOAT:
                vector_filter_float_simd((const float *)column, *(const float *)value,
                                         n, op, bitmap);
                break;
            default: /* VEXEC_COL_TYPE_DOUBLE */
                vector_filter_double_simd((const double *)column, *(const double *)value,
                                          n, op, bitmap);
                break;
        }

        /* null 行永不匹配 */
        if (block->null_bitmap) {
            for (int w = 0; w < nwords; w++) bitmap[w] &= ~block->null_bitmap[w];
        }

        result->matches = (int64_t *)malloc((size_t)n * sizeof(int64_t));
        if (!result->matches) {
            free(bitmap);
            free(result);
            return NULL;
        }

        /* 位图 → 原始行号（升序） */
        for (int i = 0; i < n; i++) {
            if (bitmap[i / 64] & (1ULL << (i % 64))) {
                result->matches[result->num_matches++] = (int64_t)i;
            }
        }

        free(bitmap);
        return result;
    }

    /* --------------------------------------------------------------------
     * 旧路径：列无类型标签（-1）时，按定长 64 字节字符串逐行 strcmp。
     * 行为与本次改动前完全一致。
     * -------------------------------------------------------------------- */
    result->matches = (int64_t *)malloc((size_t)block->num_rows * sizeof(int64_t));

    for (int i = 0; i < block->num_rows; i++) {
        char *str_val = (char *)column + i * 64;
        int cmp = strcmp(str_val, (const char *)value);
        bool match = false;

        switch (op) {
            case CMP_EQ: match = (cmp == 0); break;
            case CMP_NE: match = (cmp != 0); break;
            case CMP_LT: match = (cmp < 0); break;
            case CMP_LE: match = (cmp <= 0); break;
            case CMP_GT: match = (cmp > 0); break;
            case CMP_GE: match = (cmp >= 0); break;
            default: break;
        }

        if (match) {
            result->matches[result->num_matches++] = i;
        }
    }

    return result;
}

void vector_filter_result_free(VectorFilterResult *result) {
    if (!result) return;
    free(result->matches);
    free(result);
}

void vector_filter_combine(VectorFilterResult **results, size_t num_results, bool is_and) {
    (void)results; (void)num_results; (void)is_and;
}

/* ========================================================================
 * 向量化算子
 * ======================================================================== */

VectorScanExecState *vector_scan_state_create(int batch_size) {
    VectorScanExecState *state = (VectorScanExecState *)calloc(1, sizeof(VectorScanExecState));
    if (state) {
        state->mode = VECTOR_EXEC_BATCH;
        state->batch_size = batch_size > 0 ? batch_size : 1024;
        state->batch = NULL;
        state->current_row = 0;
        state->filter_expr = NULL;
    }
    return state;
}

struct TupleTableSlot_s *vector_scan_exec(VectorScanExecState *state) {
    (void)state;
    return NULL;
}

VectorHashJoinExecState *vector_hashjoin_state_create(int batch_size) {
    VectorHashJoinExecState *state = (VectorHashJoinExecState *)calloc(1, sizeof(VectorHashJoinExecState));
    if (state) {
        state->mode = VECTOR_EXEC_BATCH;
        state->batch_size = batch_size > 0 ? batch_size : 1024;
        state->hash_table = NULL;
        state->build_batch = NULL;
        state->probe_batch = NULL;
    }
    return state;
}

struct TupleTableSlot_s *vector_hashjoin_exec(VectorHashJoinExecState *state) {
    (void)state;
    return NULL;
}

VectorAggExecState *vector_agg_state_create(int batch_size, int num_groups) {
    VectorAggExecState *state = (VectorAggExecState *)calloc(1, sizeof(VectorAggExecState));
    if (state) {
        state->mode = VECTOR_EXEC_BATCH;
        state->batch_size = batch_size > 0 ? batch_size : 1024;
        state->num_groups = num_groups;
        state->hash_table = NULL;
        state->num_aggs = 0;
    }
    return state;
}

struct TupleTableSlot_s *vector_agg_exec(VectorAggExecState *state) {
    (void)state;
    return NULL;
}

int vector_filter(VectorScanExecState *state, struct Expr_s *filter_expr) {
    (void)state; (void)filter_expr;
    return 0;
}

/* ========================================================================
 * SIMD 检测（如实报告，不再硬编码 SIMD_NONE）
 *
 * 优先用运行时 CPU 特征检测（GCC/Clang 的 __builtin_cpu_supports，内部读取
 * CPUID 结果，首次调用会自动 __builtin_cpu_init）；拿不到运行时检测时退回
 * 编译期宏。x86_64 的 ABI 基线本身就保证 SSE2，所以最低报 SSE2。
 * ======================================================================== */

/* 纯硬件能力探测：不受任何开关影响 */
static SimdExtension vexec_detect_hw(void) {
#ifdef VEXEC_HAVE_CPU_PROBE
    /* GCC/Clang x86：真正的运行时 CPUID 检测。
       只上报本文件确有对应内核的级别（AVX2 / SSE4.2 / SSE2）。 */
    if (__builtin_cpu_supports("avx2"))   return SIMD_AVX2;
    if (__builtin_cpu_supports("sse4.2")) return SIMD_SSE4;
    if (__builtin_cpu_supports("sse2"))   return SIMD_SSE2;
#endif
#if defined(VEXEC_HAVE_SSE2)
    /* 无运行时检测手段（如 MSVC）：x86_64 硬件基线保证 SSE2 */
    return SIMD_SSE2;
#else
    /* 非 x86 或未开 SSE2：只有可移植标量内核 */
    return SIMD_NONE;
#endif
}

/**
 * 解析 MMDB_SIMD 降级开关。
 *
 * 该开关**只能把分派降到更低的级别，永远不能抬高**——否则会在不支持的 CPU 上
 * 执行非法指令（#UD）。取值：scalar / sse2 / sse4.2（或 sse42）/ avx2；
 * 未设置、取值无效、或请求级别高于硬件实际能力时，一律沿用硬件检测结果。
 *
 * 用途：在同一台机器上回归验证标量 / SSE2 / AVX2 各层内核结果逐位一致。
 */
static SimdExtension vexec_apply_env_downgrade(SimdExtension hw) {
    const char *env = getenv("MMDB_SIMD");
    SimdExtension want;

    if (!env || !*env) return hw;

    if      (strcmp(env, "scalar") == 0) want = SIMD_NONE;
    else if (strcmp(env, "sse2")   == 0) want = SIMD_SSE2;
    else if (strcmp(env, "sse4.2") == 0 || strcmp(env, "sse42") == 0) want = SIMD_SSE4;
    else if (strcmp(env, "avx2")   == 0) want = SIMD_AVX2;
    else return hw;  /* 无法识别：忽略，保持如实报告 */

    return (want < hw) ? want : hw;  /* 只降不升 */
}

SimdExtension simd_detect_extension(void) {
    /* 缓存首次解析结果：分派函数每次调用都会问一遍，CPUID + getenv 不宜重复做。
       多线程首次并发进入时会各算一遍再写同一个值，结果相同，属良性竞争。
       -1 表示尚未初始化（SimdExtension 的合法值都 >= 0）。 */
    static int cached = -1;
    int c = cached;
    if (c < 0) {
        c = (int)vexec_apply_env_downgrade(vexec_detect_hw());
        cached = c;
    }
    return (SimdExtension)c;
}

SimdExtension simd_get_best_extension(void) {
    /* 与 simd_detect_extension() 同义：本实现里「检测到的」就是「实际会用的」 */
    return simd_detect_extension();
}

bool simd_has_extension(SimdExtension ext) {
    /* 「无 SIMD」任何机器都满足 */
    if (ext == SIMD_NONE) return true;

    SimdExtension best = simd_get_best_extension();

    /* NEON 与 x86 系列不在同一条能力链上，枚举值大小没有可比性，单独判断 */
    if (ext == SIMD_NEON || best == SIMD_NEON) return ext == best;

    return best >= ext;
}
