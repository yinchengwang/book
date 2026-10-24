/**
 * @file vecx_prune.c
 * @brief Gap#2 向量化执行引擎 Task8：MinMax Index + Zone-map Skip 实现
 *
 * MinMaxIndex：扫描块指定列的非 null 行，返回 [min, max]。
 * ZoneMapSkip：根据 [lo, hi] 区间与比较谓词判断 granule 是否必然无匹配。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ========================================================================
 * MinMax Index（整型版）
 * ======================================================================== */

int vecx_block_minmax_i64(const VectorBlock *b, int col, int64_t *lo, int64_t *hi) {
    if (!b || !lo || !hi) return -1;
    if (col < 0 || col >= b->num_columns) return -1;
    if (b->num_rows <= 0) return -1;

    int col_type = vector_block_get_column_type(b, col);
    if (col_type != COLUMN_INT32 && col_type != COLUMN_INT64 &&
        col_type != COLUMN_FLOAT && col_type != COLUMN_DOUBLE) {
        return -1;
    }

    void *col_data = vector_block_get_column((VectorBlock *)b, col);
    if (!col_data) return -1;

    const int n = b->num_rows;
    int count = 0;
    int64_t lo_val = 0, hi_val = 0;

    /* 按列类型分派，用首个有效值做初值（不用哨兵，支持极值） */
    for (int row = 0; row < n; row++) {
        /* null 跳过：检查 null_bitmap 第 row 位 */
        if (b->null_bitmap) {
            if (b->null_bitmap[row / 64] & (1ULL << (row % 64))) {
                continue;  /* 该行是 null，跳过 */
            }
        }

        int64_t v;
        switch (col_type) {
            case COLUMN_INT32: {
                const int32_t *p = (const int32_t *)col_data;
                v = (int64_t)p[row];
                break;
            }
            case COLUMN_INT64: {
                const int64_t *p = (const int64_t *)col_data;
                v = p[row];
                break;
            }
            case COLUMN_FLOAT: {
                const float *p = (const float *)col_data;
                v = (int64_t)p[row];
                break;
            }
            case COLUMN_DOUBLE: {
                const double *p = (const double *)col_data;
                v = (int64_t)p[row];
                break;
            }
            default:
                return -1;  /* 不应到达 */
        }

        if (count == 0) {
            lo_val = v;
            hi_val = v;
        } else {
            if (v < lo_val) lo_val = v;
            if (v > hi_val) hi_val = v;
        }
        count++;
    }

    if (count == 0) return -1;  /* 全 null */
    *lo = lo_val;
    *hi = hi_val;
    return 0;
}

/* ========================================================================
 * MinMax Index（浮点版）
 * ======================================================================== */

int vecx_block_minmax_f64(const VectorBlock *b, int col, double *lo, double *hi) {
    if (!b || !lo || !hi) return -1;
    if (col < 0 || col >= b->num_columns) return -1;
    if (b->num_rows <= 0) return -1;

    int col_type = vector_block_get_column_type(b, col);
    if (col_type != COLUMN_INT32 && col_type != COLUMN_INT64 &&
        col_type != COLUMN_FLOAT && col_type != COLUMN_DOUBLE) {
        return -1;
    }

    void *col_data = vector_block_get_column((VectorBlock *)b, col);
    if (!col_data) return -1;

    const int n = b->num_rows;
    int count = 0;
    double lo_val = 0.0, hi_val = 0.0;

    for (int row = 0; row < n; row++) {
        if (b->null_bitmap) {
            if (b->null_bitmap[row / 64] & (1ULL << (row % 64))) {
                continue;
            }
        }

        double v;
        switch (col_type) {
            case COLUMN_INT32: {
                const int32_t *p = (const int32_t *)col_data;
                v = (double)p[row];
                break;
            }
            case COLUMN_INT64: {
                const int64_t *p = (const int64_t *)col_data;
                v = (double)p[row];
                break;
            }
            case COLUMN_FLOAT: {
                const float *p = (const float *)col_data;
                v = (double)p[row];
                break;
            }
            case COLUMN_DOUBLE: {
                const double *p = (const double *)col_data;
                v = p[row];
                break;
            }
            default:
                return -1;
        }

        if (count == 0) {
            lo_val = v;
            hi_val = v;
        } else {
            if (v < lo_val) lo_val = v;
            if (v > hi_val) hi_val = v;
        }
        count++;
    }

    if (count == 0) return -1;
    *lo = lo_val;
    *hi = hi_val;
    return 0;
}

/* ========================================================================
 * Zone-map Skip（整型版）
 * ======================================================================== */

int vecx_zonemap_skip_i64(int64_t lo, int64_t hi, CompareOp op, int64_t v) {
    /* 无效区间：不跳过（安全策略，强制扫描） */
    if (lo > hi) return 0;

    switch (op) {
        case CMP_EQ:  /* v < lo || v > hi → 跳过 */
            return (v < lo || v > hi) ? 1 : 0;
        case CMP_NE:  /* 永不跳过（区间内可能有非 v 的值） */
            return 0;
        case CMP_LT:  /* lo >= v → 跳过 */
            return (lo >= v) ? 1 : 0;
        case CMP_LE:  /* lo > v → 跳过 */
            return (lo > v) ? 1 : 0;
        case CMP_GT:  /* hi <= v → 跳过 */
            return (hi <= v) ? 1 : 0;
        case CMP_GE:  /* hi < v → 跳过 */
            return (hi < v) ? 1 : 0;
        default:
            return 0;
    }
}

/* ========================================================================
 * Zone-map Skip（浮点版）
 *
 * NaN 语义：C 标准规定 NaN 比较永远返回 false。
 * 若 lo 或 hi 任一为 NaN，则所有比较结果均为 false → 不跳过（返回 0）。
 * 若 v 为 NaN，则所有比较结果也为 false → 不跳过（返回 0）。
 * ±inf 处理：C 比较语义中 +inf > 任何有限数，-inf < 任何有限数。
 * ======================================================================== */

int vecx_zonemap_skip_f64(double lo, double hi, CompareOp op, double v) {
    /* NaN 检查：lo/hi/v 任一为 NaN 时所有比较均 false → 不跳过 */
    if (isnan(lo) || isnan(hi) || isnan(v)) return 0;

    /* 无效区间（安全策略） */
    if (lo > hi) return 0;

    switch (op) {
        case CMP_EQ:
            return (v < lo || v > hi) ? 1 : 0;
        case CMP_NE:
            return 0;
        case CMP_LT:
            return (lo >= v) ? 1 : 0;
        case CMP_LE:
            return (lo > v) ? 1 : 0;
        case CMP_GT:
            return (hi <= v) ? 1 : 0;
        case CMP_GE:
            return (hi < v) ? 1 : 0;
        default:
            return 0;
    }
}
