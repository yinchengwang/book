/**
 * @file rrf.c
 * @brief Reciprocal Rank Fusion 多通道融合算法
 *
 * 算法来源：Cormack, Clarke, Buettcher, "Reciprocal Rank Fusion outperforms
 * Condorcet and individual Rank Learning Methods", SIGIR 2009。
 * 融合公式：score(d) = sum_i (1 / (k + rank_i(d)))
 * 默认 k=60 是论文推荐值。
 */
#include "sdk/impl/hybrid_search.h"

/* 初始化 RRF 配置：默认 k=60 */
void mmdb_rrf_config_init(mmdb_rrf_config_t* cfg) {
    if (!cfg) return;
    cfg->k = 60;
}

/* 就地融合多通道排名到单一 RRF 得分 */
int mmdb_rrf_fuse(mmdb_rrf_doc_t* docs, size_t doc_count,
                  const mmdb_rrf_config_t* cfg) {
    if (!cfg) return -1;
    if (doc_count == 0) return 0;
    if (!docs) return -1;

    double k = (double)cfg->k;
    /* source_ranks 固定容量为 8，循环上限取 min(source_count, 8) */
    const size_t cap = sizeof(docs[0].source_ranks) / sizeof(docs[0].source_ranks[0]);
    for (size_t i = 0; i < doc_count; i++) {
        double s = 0.0;
        size_t n = docs[i].source_count < cap ? docs[i].source_count : cap;
        for (size_t j = 0; j < n; j++) {
            size_t rank = docs[i].source_ranks[j];
            if (rank > 0) {
                s += 1.0 / (k + (double)rank);
            }
        }
        docs[i].rrf_score = s;
    }
    return 0;
}
