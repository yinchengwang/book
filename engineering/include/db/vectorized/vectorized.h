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

#ifdef __cplusplus
}
#endif

#endif /* DB_VECTORIZED_VECTORIZED_H */
