/**
 * @file hybrid_search.h
 * @brief 混合检索融合算法（内部接口）
 *
 * 当前提供 Reciprocal Rank Fusion (RRF) 基础实现。
 * 算法来源：Cormack, Clarke, Buettcher, "Reciprocal Rank Fusion outperforms
 * Condorcet and individual Rank Learning Methods", SIGIR 2009。
 *
 * 融合公式：score(d) = sum_i (1 / (k + rank_i(d)))
 * 默认 k=60 是论文推荐值。
 */
#ifndef MMDB_HYBRID_SEARCH_H
#define MMDB_HYBRID_SEARCH_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RRF 融合配置 */
typedef struct {
    int32_t k;  /* RRF 常数，默认 60 */
} mmdb_rrf_config_t;

/* 初始化配置（设置默认 k=60） */
void mmdb_rrf_config_init(mmdb_rrf_config_t* cfg);

/* 文档在多通道中的排名与融合结果 */
typedef struct {
    const uint8_t* id;       /* 文档 ID（外部管理生命周期） */
    size_t id_len;
    double rrf_score;        /* 输出：融合后的得分 */
    size_t source_ranks[8];  /* 各通道排名（0 表示未在该通道命中） */
    size_t source_count;     /* 已填充的 source_ranks 数量（<= 8） */
} mmdb_rrf_doc_t;

/* 就地 RRF 融合：对每个 doc 累加 1/(k+rank_i)
 *
 * 入参:
 *   docs       - 文档数组
 *   doc_count  - 文档数量；为 0 时直接返回 0
 *   cfg        - RRF 配置；为 NULL 时返回 -1
 *
 * 返回:
 *   0  - 成功
 *  -1  - 参数非法
 *
 * 边界行为：
 *   - source_count 超过 8 时只取前 8 个通道
 *   - source_ranks[i] == 0 视为该通道未命中，不贡献得分
 */
int mmdb_rrf_fuse(
    mmdb_rrf_doc_t* docs,
    size_t doc_count,
    const mmdb_rrf_config_t* cfg);

#ifdef __cplusplus
}
#endif

#endif  /* MMDB_HYBRID_SEARCH_H */
