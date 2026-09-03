// recall_helper.h — Recall@10 辅助函数
//
// 提供 brute_force_top10：在给定向量子集上做暴力 L2 搜索，返回 top-10 的子集内索引。
// 供 cross_lang_consistency_test.cpp 中 VectorKNN1M 等测试复用。

#ifndef RECALL_HELPER_H
#define RECALL_HELPER_H

#include <cstddef>
#include <vector>
#include <algorithm>
#include <utility>

// 在子集上做暴力 L2 搜索，返回 top-10 的子集内索引
inline std::vector<size_t> brute_force_top10(
    const std::vector<std::vector<float>>& subset,
    const std::vector<float>& query,
    size_t dim)
{
    std::vector<std::pair<float, size_t>> dists;
    dists.reserve(subset.size());
    for (size_t i = 0; i < subset.size(); i++) {
        float dist = 0.0f;
        for (size_t d = 0; d < dim; d++) {
            float diff = query[d] - subset[i][d];
            dist += diff * diff;
        }
        dists.push_back({dist, i});
    }
    size_t top_n = std::min((size_t)10, dists.size());
    std::partial_sort(dists.begin(), dists.begin() + top_n, dists.end());
    std::vector<size_t> top10;
    for (size_t i = 0; i < top_n; i++) top10.push_back(dists[i].second);
    return top10;
}

#endif  // RECALL_HELPER_H
