/**
 * @file vecx_agg.c
 * @brief Gap#2 向量化执行引擎 Task4：向量化标量聚合算子（count/sum/min/max/avg）
 *
 * 实现思路（整块与选择子集共用一条流水线）：
 *   1) 入参与列类型校验：只接受 INT32 / INT64 / FLOAT / DOUBLE 四种数值列；
 *   2) 一趟遍历同时喂 count / sum / min / max 四个累加器，遍历范围由
 *      sel 是否为 NULL 决定，两种范围共用同一个循环体（避免两份逻辑漂移）；
 *   3) null 行直接跳过，不参与任何聚合（包括 COUNT）；
 *   4) 收尾时按 kind 取用对应累加器，AVG 的除法只在最后做一次。
 *
 * 精度约定：整型列在 int64_t 上累加，最后一次性转 double，
 * 避免超过 2^53 的整数和被 double 静默截断；浮点列直接用 double 累加
 * （float 列先提升到 double）。
 *
 * 只读约定：本算子不写入输入块的任何字段，输出只经 out / has_result 两个出参。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"

#include <stdint.h>

/**
 * 判断某行是否为 null。
 * null_bitmap 由 vector_block_create 用 calloc 分配，正常情况恒非 NULL；
 * 这里仍防御性判空（NULL 视为"整块没有 null 行"）。
 * 直接按位读位图，避免对 const VectorBlock * 做去 const 转换。
 */
static inline int agg_is_null(const VectorBlock *b, int row) {
    if (!b->null_bitmap) return 0;
    return (b->null_bitmap[row / 64] & (1ULL << (row % 64))) != 0;
}

int vecx_agg_scalar(const VectorBlock *b, int col, vecx_agg_kind_t kind,
                    const int *sel, int nsel, double *out, int *has_result) {
    /* 先把出参写成"无结果"，这样调用方即使忽略返回码也不会读到未初始化内存 */
    if (out) *out = 0.0;
    if (has_result) *has_result = 0;

    if (!b || !out || !has_result) return -1;
    if (col < 0 || col >= b->num_columns) return -1;
    if (!b->columns || !b->columns[col]) return -1;

    switch (kind) {
        case VECX_AGG_COUNT:
        case VECX_AGG_SUM:
        case VECX_AGG_MIN:
        case VECX_AGG_MAX:
        case VECX_AGG_AVG:
            break;
        default:
            return -1;  /* kind 不在枚举范围内 */
    }

    /* 列类型分派：整型走 int64_t 累加器，浮点走 double 累加器；
       未知(-1)、字符串及其它类型一律视为入参错误 */
    const int col_type = vector_block_get_column_type(b, col);
    int is_int;
    switch (col_type) {
        case COLUMN_INT32:
        case COLUMN_INT64:
            is_int = 1;
            break;
        case COLUMN_FLOAT:
        case COLUMN_DOUBLE:
            is_int = 0;
            break;
        default:
            return -1;
    }

    const void *data = b->columns[col];

    int64_t count = 0;
    int64_t sum_i = 0, min_i = 0, max_i = 0;   /* 整型列累加器 */
    double  sum_f = 0.0, min_f = 0.0, max_f = 0.0;  /* 浮点列累加器 */

    /* sel==NULL 时遍历 [0, num_rows)（忽略 nsel）；否则遍历 sel[0..nsel-1]。
       sel 非 NULL 且 nsel<=0 是合法的空集，循环自然不执行。 */
    const int n = sel ? nsel : b->num_rows;
    for (int i = 0; i < n; i++) {
        const int row = sel ? sel[i] : i;
        if (row < 0 || row >= b->num_rows) continue;  /* 越界行号静默跳过，不算错误 */
        if (agg_is_null(b, row)) continue;            /* null 行不参与任何聚合 */

        if (is_int) {
            const int64_t v = (col_type == COLUMN_INT32)
                                  ? (int64_t)((const int32_t *)data)[row]
                                  : ((const int64_t *)data)[row];
            if (count == 0) {
                /* MIN/MAX 用首个有效值做初值，不用哨兵，极值数据也能正确返回 */
                min_i = v;
                max_i = v;
            } else {
                if (v < min_i) min_i = v;
                if (v > max_i) max_i = v;
            }
            /* 教学级实现不做溢出检测：和超出 int64_t 范围时结果回绕。
               写成无符号加法是为了避免有符号溢出的未定义行为（无符号回绕是良定义的）。 */
            sum_i = (int64_t)((uint64_t)sum_i + (uint64_t)v);
        } else {
            const double v = (col_type == COLUMN_FLOAT)
                                 ? (double)((const float *)data)[row]
                                 : ((const double *)data)[row];
            if (count == 0) {
                min_f = v;
                max_f = v;
            } else {
                if (v < min_f) min_f = v;
                if (v > max_f) max_f = v;
            }
            sum_f += v;
        }
        count++;
    }

    /* COUNT 对空集返回 0 而不是 NULL，故始终算"有结果" */
    if (kind == VECX_AGG_COUNT) {
        *out = (double)count;
        *has_result = 1;
        return 0;
    }

    /* SUM/MIN/MAX/AVG 对空集返回 SQL NULL：has_result 保持 0，*out 保持 0.0 */
    if (count == 0) return 0;

    switch (kind) {
        case VECX_AGG_SUM:
            *out = is_int ? (double)sum_i : sum_f;
            break;
        case VECX_AGG_MIN:
            *out = is_int ? (double)min_i : min_f;
            break;
        case VECX_AGG_MAX:
            *out = is_int ? (double)max_i : max_f;
            break;
        case VECX_AGG_AVG:
            /* 除法只做一次，不边遍历边求平均 */
            *out = (is_int ? (double)sum_i : sum_f) / (double)count;
            break;
        default:
            return -1;  /* 前面已校验过，不可达 */
    }
    *has_result = 1;
    return 0;
}
