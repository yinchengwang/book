/**
 * @file vecx_project.c
 * @brief Gap#2 Task7 向量表达式投影：表达式树 + 逐行求值 + 投影
 *
 * 表达式是二叉树：COL / CONST / ADD / SUB / MUL。
 * 求值方式：紧循环逐行 scalar 求值（教学级）。
 */

#include "db/vectorized/vectorized.h"
#include "db/core/columnar_store.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 表达式节点结构
 * ======================================================================== */

struct vecx_expr_s {
    vecx_expr_op_t op;     /**< 操作符 */
    int            col;    /**< 列索引（仅 VEXPR_COL） */
    double         const_val; /**< 常量值（仅 VEXPR_CONST） */
    vecx_expr_t   *left;   /**< 左子节点 */
    vecx_expr_t   *right;  /**< 右子节点 */
};

/* ========================================================================
 * 表达式构造
 * ======================================================================== */

vecx_expr_t *vecx_expr_col(int col) {
    if (col < 0) return NULL;
    vecx_expr_t *e = (vecx_expr_t *)malloc(sizeof(vecx_expr_t));
    if (!e) return NULL;
    e->op = VEXPR_COL;
    e->col = col;
    e->const_val = 0.0;
    e->left = NULL;
    e->right = NULL;
    return e;
}

vecx_expr_t *vecx_expr_const(double v) {
    vecx_expr_t *e = (vecx_expr_t *)malloc(sizeof(vecx_expr_t));
    if (!e) return NULL;
    e->op = VEXPR_CONST;
    e->col = -1;
    e->const_val = v;
    e->left = NULL;
    e->right = NULL;
    return e;
}

vecx_expr_t *vecx_expr_bin(vecx_expr_op_t op, vecx_expr_t *left, vecx_expr_t *right) {
    if (!left && !right) return NULL;
    vecx_expr_t *e = (vecx_expr_t *)malloc(sizeof(vecx_expr_t));
    if (!e) return NULL;
    e->op = op;
    e->col = -1;
    e->const_val = 0.0;
    e->left = left;
    e->right = right;
    return e;
}

/* ========================================================================
 * 递归释放
 * ======================================================================== */

void vecx_expr_free(vecx_expr_t *e) {
    if (!e) return;
    if (e->left) vecx_expr_free(e->left);
    if (e->right) vecx_expr_free(e->right);
    free(e);
}

/* ========================================================================
 * 逐行求值（教学级递归）
 * ======================================================================== */

/** 判断某行是否为 null */
static inline int row_is_null(const VectorBlock *b, int row) {
    if (!b->null_bitmap) return 0;
    return (b->null_bitmap[row / 64] & (1ULL << (row % 64))) != 0;
}

/** 按列类型从列数据中取 double 值 */
static double get_double(const VectorBlock *in, int col, int row) {
    void *data = in->columns[col];
    int col_type = vector_block_get_column_type(in, col);
    switch (col_type) {
        case COLUMN_INT32: return (double)((const int32_t *)data)[row];
        case COLUMN_INT64: return (double)((const int64_t *)data)[row];
        case COLUMN_FLOAT: return (double)((const float *)data)[row];
        case COLUMN_DOUBLE: return ((const double *)data)[row];
        default: return 0.0; /* 不支持的类型返回 0 */
    }
}

/** 递归求值一行 */
static double eval_row(const vecx_expr_t *e, const VectorBlock *in, int row) {
    switch (e->op) {
        case VEXPR_COL:
            return get_double(in, e->col, row);
        case VEXPR_CONST:
            return e->const_val;
        case VEXPR_ADD:
            return eval_row(e->left, in, row) + eval_row(e->right, in, row);
        case VEXPR_SUB:
            return eval_row(e->left, in, row) - eval_row(e->right, in, row);
        case VEXPR_MUL:
            return eval_row(e->left, in, row) * eval_row(e->right, in, row);
        default:
            return 0.0;
    }
}

/* ========================================================================
 * 投影：深拷贝输入块 + 追加表达式结果列
 * ======================================================================== */

int vecx_project(const VectorBlock *in, const vecx_expr_t *e, VectorBlock **out) {
    if (!in || !e || !out) return -1;
    if (in->num_rows <= 0) {
        *out = NULL;
        return 0;
    }

    /* 校验列索引（仅对 COL 表达式有效） */
    if (e->op == VEXPR_COL) {
        if (e->col < 0 || e->col >= in->num_columns) return -1;
        if (in->num_columns > 0) {
            int col_type = vector_block_get_column_type(in, e->col);
            if (col_type < 0) return -1; /* 列类型未知 */
        }
    }

    /* 创建输出块：输入列数 + 1（结果列） */
    int ncols_out = in->num_columns + 1;
    VectorBlock *dst = vector_block_create(in->num_rows, ncols_out);
    if (!dst) return -1;

    /* 逐列深拷贝（如果有列的话） */
    for (int c = 0; c < in->num_columns; c++) {
        int elem_size = in->column_sizes[c];
        if (elem_size <= 0) {
            vector_block_set_column_type(dst, c, vector_block_get_column_type(in, c));
            continue;
        }

        char *buf = (char *)malloc((size_t)in->num_rows * (size_t)elem_size);
        if (!buf) {
            vector_block_destroy(dst);
            return -1;
        }

        if (in->columns && in->columns[c]) {
            memcpy(buf, in->columns[c], (size_t)in->num_rows * (size_t)elem_size);
        }
        vector_block_set_column(dst, c, buf, elem_size);
        vector_block_set_column_type(dst, c, vector_block_get_column_type(in, c));
    }

    /* 拷贝 null 位图（如果有的话） */
    if (in->null_bitmap) {
        int nwords = (in->num_rows + 63) / 64;
        dst->null_bitmap = (uint64_t *)malloc((size_t)nwords * sizeof(uint64_t));
        if (!dst->null_bitmap) {
            vector_block_destroy(dst);
            return -1;
        }
        memcpy(dst->null_bitmap, in->null_bitmap, (size_t)nwords * sizeof(uint64_t));
    }

    /* 计算表达式结果列 */
    double *res_col = (double *)malloc((size_t)in->num_rows * sizeof(double));
    if (!res_col) {
        vector_block_destroy(dst);
        return -1;
    }

    /* 逐行求值并传播 null */
    for (int r = 0; r < in->num_rows; r++) {
        /* 检查该行任一输入列是否为 null */
        int any_null = row_is_null(in, r);
        if (!any_null) {
            res_col[r] = eval_row(e, in, r);
        }
        /* else: 结果保持 0.0，null 标记下面统一设 */
    }

    /* 设置结果列类型为 COLUMN_DOUBLE */
    vector_block_set_column(dst, in->num_columns, res_col, (int)sizeof(double));
    vector_block_set_column_type(dst, in->num_columns, COLUMN_DOUBLE);

    /* 设置结果列的 null 标记：若输入行任一列为 null → 结果 null */
    for (int r = 0; r < in->num_rows; r++) {
        int any_null = row_is_null(in, r);
        if (any_null) {
            vector_block_set_null(dst, r, true);
        }
    }

    vector_block_set_num_rows(dst, in->num_rows);
    *out = dst;
    return 0;
}
