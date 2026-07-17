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

    return block;
}

void vector_block_destroy(VectorBlock *block) {
    if (!block) return;
    for (int i = 0; i < block->num_columns; i++) {
        free(block->columns[i]);
    }
    free(block->columns);
    free(block->column_sizes);
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
    return false;
}

const char *vector_get_simd_type(void) {
    return "scalar";
}

/* ========================================================================
 * SIMD 过滤
 * ======================================================================== */

static uint64_t apply_op_int(int32_t val, int32_t cmp, CompareOp op) {
    switch (op) {
        case CMP_EQ: return (val == cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_NE: return (val != cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_LT: return (val < cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_LE: return (val <= cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_GT: return (val > cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_GE: return (val >= cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        default: return 0;
    }
}

void vector_filter_int_simd(const int32_t *a, int32_t b,
                          int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    for (int i = 0; i < num_elements; i++) {
        if (apply_op_int(a[i], b, op)) {
            result[i / 64] |= (1ULL << (i % 64));
        }
    }
}

static uint64_t apply_op_float(float val, float cmp, CompareOp op) {
    switch (op) {
        case CMP_EQ: return (val == cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_NE: return (val != cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_LT: return (val < cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_LE: return (val <= cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_GT: return (val > cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        case CMP_GE: return (val >= cmp) ? 0xFFFFFFFFFFFFFFFFULL : 0;
        default: return 0;
    }
}

void vector_filter_float_simd(const float *a, float b,
                            int num_elements, CompareOp op, uint64_t *result) {
    if (!a || !result || num_elements <= 0) return;
    for (int i = 0; i < num_elements; i++) {
        if (apply_op_float(a[i], b, op)) {
            result[i / 64] |= (1ULL << (i % 64));
        }
    }
}

void vector_filter_string_simd(const char **a, const char *b,
                            int num_elements, CompareOp op, uint64_t *result) {
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

VectorFilterResult *vector_filter_execute(VectorBlock *block,
                                       int column_idx, void *value, CompareOp op) {
    if (!block || !value) return NULL;

    VectorFilterResult *result = (VectorFilterResult *)calloc(1, sizeof(VectorFilterResult));
    if (!result) return NULL;

    result->matches = (int64_t *)malloc((size_t)block->num_rows * sizeof(int64_t));
    result->num_matches = 0;

    void *column = vector_block_get_column(block, column_idx);
    if (!column) {
        free(result->matches);
        free(result);
        return NULL;
    }

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
 * SIMD 检测
 * ======================================================================== */

SimdExtension simd_detect_extension(void) {
    return SIMD_NONE;
}

bool simd_has_extension(SimdExtension ext) {
    return ext == SIMD_NONE;
}

SimdExtension simd_get_best_extension(void) {
    return SIMD_NONE;
}
