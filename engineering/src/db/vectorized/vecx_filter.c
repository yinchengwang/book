/**
 * @file vecx_filter.c
 * @brief Gap#2 向量化执行引擎 Task3：块进块出的向量化过滤算子
 *
 * 实现思路（单条件与多条件共用一条流水线）：
 *   1) 按列类型标签分派到 db_core 的 vector_filter_*_simd 内核，产出比较位图；
 *   2) 与 ~null_bitmap 逐字 AND，把 null 行剔除（null 行永不匹配）；
 *   3) 多条件时在位图层按 AND / OR 合并，避免中间物化；
 *   4) 位图 → 选择向量 → vecx_block_gather 压缩出新块，全程只 gather 一次。
 *
 * 内存约定：位图与选择向量均在堆上按 num_rows 分配（块行数可能远超栈安全上限），
 * 输出块归调用方所有，用 vector_block_destroy 释放。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/vector_exec.h"
#include "db/core/columnar_store.h"

#include <stdlib.h>
#include <string.h>

/**
 * 按列类型分派 SIMD 比较内核，把比较结果写入 bitmap（内核自身负责先清零）。
 *
 * @param in     输入块
 * @param col    列索引（调用方保证已做范围校验）
 * @param op     比较操作符
 * @param value  比较值指针，语义见 vecx_filter_block 的文档
 * @param bitmap 输出位图，容量须 >= ceil(in->num_rows/64) 个字
 * @return 0 成功；-1 列数据缺失或列类型未知 / 不支持
 */
static int vecx_filter_kernel(const VectorBlock *in, int col, CompareOp op,
                              const void *value, uint64_t *bitmap) {
    const int n = in->num_rows;
    const int col_type = vector_block_get_column_type(in, col);
    /* get_column 的形参非 const，此处只读取指针，去 const 是安全的 */
    void *col_data = vector_block_get_column((VectorBlock *)in, col);
    if (!col_data) return -1;

    switch (col_type) {
        case COLUMN_INT32:
            vector_filter_int_simd((const int32_t *)col_data,
                                   *(const int32_t *)value, n, op, bitmap);
            return 0;
        case COLUMN_INT64:
            vector_filter_int64_simd((const int64_t *)col_data,
                                     *(const int64_t *)value, n, op, bitmap);
            return 0;
        case COLUMN_FLOAT:
            vector_filter_float_simd((const float *)col_data,
                                     *(const float *)value, n, op, bitmap);
            return 0;
        case COLUMN_DOUBLE:
            vector_filter_double_simd((const double *)col_data,
                                      *(const double *)value, n, op, bitmap);
            return 0;
        case COLUMN_STRING: {
            /* 字符串列的列缓冲是 C 串指针数组；value 是指向 C 串指针的指针。
               vector_filter_string_simd 不清零 result，故调用方须传已清零的位图。 */
            const char *needle = *(const char *const *)value;
            if (!needle) return -1;
            vector_filter_string_simd((const char **)col_data, needle, n, op, bitmap);
            return 0;
        }
        default:
            /* -1（未知）与其余尚未支持的类型统一按非法处理 */
            return -1;
    }
}

/** 把 null 行从位图中剔除：bitmap[w] &= ~null_bitmap[w] */
static void vecx_filter_apply_null_mask(const VectorBlock *in, uint64_t *bitmap, int nwords) {
    if (!in->null_bitmap) return;
    for (int w = 0; w < nwords; w++) {
        bitmap[w] &= ~in->null_bitmap[w];
    }
}

/**
 * 位图 → 选择向量 → gather 的公共收尾。
 * @return 匹配行数；0 时 *out 保持 NULL；-1 分配失败
 */
static int vecx_filter_finish(const VectorBlock *in, const uint64_t *bitmap,
                              VectorBlock **out) {
    const int n = in->num_rows;

    int *sel = (int *)malloc((size_t)n * sizeof(int));
    if (!sel) return -1;

    int nsel = vecx_bitmap_to_selection(bitmap, n, sel);
    if (nsel <= 0) {
        free(sel);
        return 0;  /* 无匹配：*out 已是 NULL */
    }

    VectorBlock *dst = vecx_block_gather(in, sel, nsel);
    free(sel);
    if (!dst) return -1;

    *out = dst;
    return nsel;
}

int vecx_filter_block(const VectorBlock *in, int col, CompareOp op,
                      const void *value, VectorBlock **out) {
    if (!in || !value || !out) return -1;
    if (col < 0 || col >= in->num_columns) return -1;

    *out = NULL;
    if (in->num_rows <= 0) return 0;  /* 空块：无匹配，不算错误 */

    const int nwords = (in->num_rows + 63) / 64;
    uint64_t *bitmap = (uint64_t *)calloc((size_t)nwords, sizeof(uint64_t));
    if (!bitmap) return -1;

    if (vecx_filter_kernel(in, col, op, value, bitmap) != 0) {
        free(bitmap);
        return -1;
    }
    vecx_filter_apply_null_mask(in, bitmap, nwords);

    int nsel = vecx_filter_finish(in, bitmap, out);
    free(bitmap);
    return nsel;
}

int vecx_filter_multi(const VectorBlock *in, const vecx_pred_t *conds,
                      int nconds, int is_and, VectorBlock **out) {
    if (!in || !conds || nconds <= 0 || !out) return -1;

    /* 先做一次全量的列索引校验，避免部分条件已算完才发现入参非法 */
    for (int i = 0; i < nconds; i++) {
        if (conds[i].col < 0 || conds[i].col >= in->num_columns) return -1;
    }

    *out = NULL;
    if (in->num_rows <= 0) return 0;

    const int nwords = (in->num_rows + 63) / 64;
    uint64_t *acc = (uint64_t *)calloc((size_t)nwords, sizeof(uint64_t));
    uint64_t *cur = (uint64_t *)calloc((size_t)nwords, sizeof(uint64_t));
    if (!acc || !cur) {
        free(acc);
        free(cur);
        return -1;
    }

    for (int i = 0; i < nconds; i++) {
        const vecx_pred_t *p = &conds[i];
        const int col_type = vector_block_get_column_type(in, p->col);

        /* 按目标列类型把谓词里的比较值取成对应的标量，再交给内核 */
        int32_t v_i32;
        int64_t v_i64;
        float v_f32;
        double v_f64;
        const char *v_str;
        const void *value = NULL;

        switch (col_type) {
            case COLUMN_INT32:  v_i32 = (int32_t)p->i64; value = &v_i32; break;
            case COLUMN_INT64:  v_i64 = p->i64;          value = &v_i64; break;
            case COLUMN_FLOAT:  v_f32 = (float)p->f64;   value = &v_f32; break;
            case COLUMN_DOUBLE: v_f64 = p->f64;          value = &v_f64; break;
            case COLUMN_STRING: v_str = p->str;          value = &v_str; break;
            default:
                free(acc);
                free(cur);
                return -1;
        }

        /* 字符串内核不清零输出，故每轮显式清零 cur */
        memset(cur, 0, (size_t)nwords * sizeof(uint64_t));
        if (vecx_filter_kernel(in, p->col, p->op, value, cur) != 0) {
            free(acc);
            free(cur);
            return -1;
        }

        /* 第一个条件直接落到累加器；AND 若从全 0 起步会恒为空，必须先拷贝 */
        if (i == 0) {
            memcpy(acc, cur, (size_t)nwords * sizeof(uint64_t));
        } else if (is_and) {
            vecx_bitmap_and(acc, cur, nwords, acc);
        } else {
            vecx_bitmap_or(acc, cur, nwords, acc);
        }
    }

    /* null 掩码对所有条件都相同，AND / OR 下都可以最后统一施加一次：
       (b1 & ~m) op (b2 & ~m) == (b1 op b2) & ~m，op ∈ {&, |}。 */
    vecx_filter_apply_null_mask(in, acc, nwords);

    int nsel = vecx_filter_finish(in, acc, out);
    free(acc);
    free(cur);
    return nsel;
}
