/**
 * @file vectorized.h
 * @brief Gap#2 向量化执行引擎——列存批处理算子层（db_vectorized）
 *
 * 本库以 db_core 的 VectorBlock 为列存载体，算子以「块进块出」的方式组织：
 * 输入一个或多个 VectorBlock，经过滤/投影/聚合/连接等处理后输出新的 VectorBlock。
 *
 * 设计约定：
 * - 单线程、教学级实现，优先清晰可移植；
 * - 位图按 64 位字组织，第 i 行对应 word (i/64) 的 bit (i%64)；
 * - 不含 JIT / Arrow / 多线程并行（并行执行属 Gap#4）；
 * - SIMD 内核在后续任务（T2+）引入，本任务只落地骨架、类型标签、位图/gather 基础。
 */
#ifndef DB_VECTORIZED_VECTORIZED_H
#define DB_VECTORIZED_VECTORIZED_H

#include <stdint.h>
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 位图 / 选择向量基础操作
 * 位图按 64 位字组织，第 i 行对应 bit (i%64) of word (i/64)
 * ======================================================================== */

/**
 * @brief 统计位图 [0, nrows) 内置位行数
 * @param bm    位图（至少 ceil(nrows/64) 个字）
 * @param nrows 有效行数；超出 nrows 的置位不计
 */
int vecx_bitmap_count(const uint64_t *bm, int nrows);

/**
 * @brief 位图转选择向量：把置位行号按升序写入 sel_out
 * @param bm      位图
 * @param nrows   有效行数
 * @param sel_out 输出数组，容量须 >= nrows
 * @return 匹配（置位）行数
 */
int vecx_bitmap_to_selection(const uint64_t *bm, int nrows, int *sel_out);

/**
 * @brief 两个位图按字 AND / OR，结果写入 out（out 长度须 >= nwords）
 */
void vecx_bitmap_and(const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out);
void vecx_bitmap_or (const uint64_t *a, const uint64_t *b, int nwords, uint64_t *out);

/* ========================================================================
 * 块压缩 / 收集
 * ======================================================================== */

/**
 * @brief 按选择向量深拷贝压缩出一个新块
 *
 * 新块行数为 nsel；逐列按 column_sizes 记录的元素大小 memcpy 选定行，
 * 复制每列类型标签，并把源块选中行的 null 标记重映射到新块对应位置。
 *
 * 所有权：返回的块拥有各自 malloc 的列缓冲，随 vector_block_destroy 释放；
 * 本函数不获取/释放 src 的任何内存。
 *
 * @param src  源块；为 NULL 返回 NULL
 * @param sel  选择向量（源块行号，升序不强制要求）；为 NULL 返回 NULL
 * @param nsel 选中行数；<= 0 返回 NULL
 * @return 新块（用 vector_block_destroy 释放），失败返回 NULL
 */
VectorBlock *vecx_block_gather(const VectorBlock *src, const int *sel, int nsel);

/* ========================================================================
 * SIMD 能力上报
 * ======================================================================== */

/**
 * @brief 返回本次运行实际分派到的 SIMD 内核名
 *
 * 取值："avx2" / "sse4.2" / "sse2" / "scalar"。
 * 结果与 db_core 的 vector_get_simd_type() 一致，二者同源于
 * simd_get_best_extension() 的运行时 CPU 检测（不是编译期硬编码）。
 *
 * @return 静态字符串常量，永不为 NULL，调用方不得释放
 */
const char *vecx_active_simd(void);

/* ========================================================================
 * 过滤算子（块进块出）
 *
 * 与 db_core 的 vector_filter_execute 的区别：
 * - vector_filter_execute 返回 VectorFilterResult（**原始行号**数组），供既有
 *   ANN 执行链（vector_query.c 的 exec_vector_filter）按行号自行 gather；
 * - 本层的 vecx_filter_* 返回**压缩后的新块**，是算子流水线的标准形态。
 * 两者语义不同，互不替代。
 * ======================================================================== */

/**
 * @brief 谓词：作用在单列上的一个比较条件
 *
 * 比较值按目标列的类型标签从下列字段中取用（调用方负责填对字段）：
 * - COLUMN_INT32  → i64（内部截断为 int32_t）
 * - COLUMN_INT64  → i64
 * - COLUMN_FLOAT  → f64（内部截断为 float）
 * - COLUMN_DOUBLE → f64
 * - COLUMN_STRING → str（以 NUL 结尾的 C 串，仅支持 CMP_EQ / CMP_NE）
 */
typedef struct vecx_pred_s {
    int col;             /**< 列索引 */
    CompareOp op;        /**< 比较操作符 */
    int64_t i64;         /**< 整型比较值 */
    double f64;          /**< 浮点比较值 */
    const char *str;     /**< 字符串比较值 */
} vecx_pred_t;

/**
 * @brief 单条件向量化过滤：位图内核 → 排除 null → 选择向量 → gather 出新块
 *
 * 内核按列类型标签分派到 db_core 的 vector_filter_*_simd（运行时 CPU 检测）。
 * null 行一律不匹配（先算比较位图，再与 ~null_bitmap 逐字 AND）。
 *
 * @param in    输入块（不被修改）
 * @param col   列索引
 * @param op    比较操作符
 * @param value 比较值指针，按列类型解释：
 *              COLUMN_INT32→const int32_t*，COLUMN_INT64→const int64_t*，
 *              COLUMN_FLOAT→const float*，COLUMN_DOUBLE→const double*，
 *              COLUMN_STRING→const char* const*（指向 C 串指针的指针）
 * @param out   输出块指针；无匹配时写入 NULL，有匹配时写入新块
 *              （由调用方用 vector_block_destroy 释放）
 * @return >0 匹配行数；0 无匹配（*out=NULL）；-1 入参非法 / 列类型未知或不支持
 */
int vecx_filter_block(const VectorBlock *in, int col, CompareOp op,
                      const void *value, VectorBlock **out);

/**
 * @brief 多条件向量化过滤：逐条件产位图后按 AND / OR 合并，再压缩出新块
 *
 * 条件可跨列、跨类型；合并在位图层完成，只做一次 gather。
 * nconds==1 时 is_and 取值不影响结果。null 行一律不匹配。
 *
 * @param in     输入块（不被修改）
 * @param conds  条件数组
 * @param nconds 条件数（须 > 0）
 * @param is_and 1=AND（全部条件同时满足）；0=OR（任一条件满足）
 * @param out    输出块指针；无匹配时写入 NULL
 * @return >0 匹配行数；0 无匹配（*out=NULL）；-1 入参非法 / 某条件的列类型不支持
 */
int vecx_filter_multi(const VectorBlock *in, const vecx_pred_t *conds,
                      int nconds, int is_and, VectorBlock **out);

/* ========================================================================
 * 标量聚合算子（整块/选择子集 → 单个标量）
 * ======================================================================== */

/** 聚合种类 */
typedef enum {
    VECX_AGG_COUNT = 0,  /**< 非 null 行数 */
    VECX_AGG_SUM,        /**< 求和 */
    VECX_AGG_MIN,        /**< 最小值 */
    VECX_AGG_MAX,        /**< 最大值 */
    VECX_AGG_AVG         /**< 平均值 = sum / count */
} vecx_agg_kind_t;

/**
 * @brief 在一个块的某数值列上做标量聚合，结果统一以 double 返回
 *
 * 语义（与 SQL 聚合一致）：
 * - 遍历范围：sel==NULL 时为 [0, b->num_rows)（此时忽略 nsel）；
 *   sel!=NULL 时为 sel[0..nsel-1] 指定的行号（可乱序、可不连续、可重复；
 *   聚合**不做去重**，同一行在 sel 里出现两次就被计入两次）。
 *   越界行号静默跳过，不算入参错误。
 * - null 行一律跳过，不参与任何聚合（包括 COUNT）。
 * - COUNT：结果 = 遍历范围内的非 null 行数。始终 *has_result=1，
 *   空集时 *out=0.0（SQL 的 COUNT 对空集返回 0 而不是 NULL）。
 * - SUM / MIN / MAX / AVG：无任何非 null 行时 has_result=0、*out=0.0
 *   （SQL 对空集返回 NULL）。有值时 has_result=1。
 * - MIN / MAX 用**首个有效值**做初值，不用 DBL_MAX/-DBL_MAX 哨兵，
 *   这样极值数据（含 ±inf）也能被正确返回。
 * - 整型列的 SUM / MIN / MAX 在 int64_t 上累加后再转 double，
 *   避免超过 2^53 的整数和被 double 静默截断。
 *
 * 支持的列类型标签（vector_block_set_column_type 设置过的）：
 * COLUMN_INT32 / COLUMN_INT64 / COLUMN_FLOAT / COLUMN_DOUBLE。
 * 未知(-1)、字符串及其它类型返回 -1（这是**入参错误**，不是"空结果"，
 * 不要与 has_result=0 混为一谈）。
 *
 * @param b          输入块（不被修改）
 * @param col        列索引
 * @param kind       聚合种类
 * @param sel        选择向量；NULL 表示整块
 * @param nsel       sel 的长度；sel==NULL 时忽略；sel 非 NULL 且 nsel<=0 是合法空集
 * @param out        结果输出（非 NULL）；has_result=0 时写 0.0
 * @param has_result 是否有结果（非 NULL）：1=有值，0=空集（SQL NULL）
 * @return 0 成功；-1 入参非法 / 列类型不支持
 */
int vecx_agg_scalar(const VectorBlock *b, int col, vecx_agg_kind_t kind,
                    const int *sel, int nsel, double *out, int *has_result);

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTORIZED_VECTORIZED_H */
