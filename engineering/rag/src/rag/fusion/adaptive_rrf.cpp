/**
 * @file adaptive_rrf.cpp
 * @brief 动态 RRF 权重融合实现
 */

#include "rag/adaptive_rrf.h"
#include "rag/logger.h"
#include <algorithm>
#include <unordered_map>
#include <cmath>

namespace rag {

// ========== AdaptiveRRF 实现 ==========

AdaptiveRRF::AdaptiveRRF(int rrf_k)
    : rrf_k_(rrf_k) {}

RRFWeights AdaptiveRRF::get_weights(QueryType query_type) {
    RRFWeights weights;

    switch (query_type) {
        case QueryType::FACTUAL:
            // 事实型查询：BM25 更重要
            weights.hnsw_weight = 0.4f;
            weights.bm25_weight = 0.6f;
            weights.graph_weight = 0.0f;
            break;

        case QueryType::ANALYTICAL:
            // 分析型查询：HNSW 向量检索更重要
            weights.hnsw_weight = 0.7f;
            weights.bm25_weight = 0.3f;
            weights.graph_weight = 0.0f;
            break;

        case QueryType::MULTI_HOP:
            // 多跳查询：Graph 最重要
            weights.hnsw_weight = 0.3f;
            weights.bm25_weight = 0.2f;
            weights.graph_weight = 0.5f;
            break;

        case QueryType::COMPARATIVE:
            // 比较型查询：三者平衡
            weights.hnsw_weight = 0.5f;
            weights.bm25_weight = 0.3f;
            weights.graph_weight = 0.2f;
            break;

        case QueryType::SUMMARY:
            // 摘要型查询：HNSW 略重要
            weights.hnsw_weight = 0.6f;
            weights.bm25_weight = 0.4f;
            weights.graph_weight = 0.0f;
            break;

        case QueryType::CHAT:
            // 闲聊型：HNSW 略重要
            weights.hnsw_weight = 0.6f;
            weights.bm25_weight = 0.4f;
            weights.graph_weight = 0.0f;
            break;

        default:
            // 默认：平衡权重
            weights.hnsw_weight = 0.5f;
            weights.bm25_weight = 0.5f;
            weights.graph_weight = 0.0f;
            break;
    }

    return weights;
}

float AdaptiveRRF::rrf_score(int rank, float weight) {
    // RRF 公式: score += weight * (1 / (k + rank))
    return weight * (1.0f / (rrf_k_ + rank));
}

std::vector<RetrievalResult> AdaptiveRRF::rrf_fuse(
    const std::vector<std::pair<std::vector<RetrievalResult>, float>>& sources,
    int top_k) {

    if (sources.empty()) {
        return {};
    }

    // 计算所有唯一 chunk 的融合得分
    std::unordered_map<std::string, std::pair<float, Chunk>> chunk_scores;

    for (const auto& [results, weight] : sources) {
        for (int rank = 0; rank < static_cast<int>(results.size()); ++rank) {
            const auto& result = results[rank];
            float score = rrf_score(rank, weight);

            auto it = chunk_scores.find(result.chunk.id);
            if (it == chunk_scores.end()) {
                chunk_scores[result.chunk.id] = {score, result.chunk};
            } else {
                it->second.first += score;
            }
        }
    }

    // 转换为结果向量并排序
    std::vector<RetrievalResult> fused;
    fused.reserve(chunk_scores.size());

    for (auto& [id, pair] : chunk_scores) {
        RetrievalResult result;
        result.chunk = pair.second;
        result.score = pair.first;
        result.source = "fused";
        fused.push_back(result);
    }

    // 按得分降序排序
    std::sort(fused.begin(), fused.end(),
              [](const RetrievalResult& a, const RetrievalResult& b) {
                  return a.score > b.score;
              });

    // 取 Top-K
    if (top_k > 0 && top_k < static_cast<int>(fused.size())) {
        fused.resize(top_k);
    }

    // 更新排名
    for (int i = 0; i < static_cast<int>(fused.size()); ++i) {
        fused[i].rank = i + 1;
    }

    return fused;
}

std::vector<RetrievalResult> AdaptiveRRF::fuse(
    const std::vector<RetrievalResult>& hnsw,
    const std::vector<RetrievalResult>& bm25,
    QueryType query_type,
    int top_k) {

    RRFWeights weights = get_weights(query_type);

    std::vector<std::pair<std::vector<RetrievalResult>, float>> sources;
    sources.reserve(2);
    sources.emplace_back(hnsw, weights.hnsw_weight);
    sources.emplace_back(bm25, weights.bm25_weight);

    return rrf_fuse(sources, top_k);
}

std::vector<RetrievalResult> AdaptiveRRF::fuse_with_graph(
    const std::vector<RetrievalResult>& hnsw,
    const std::vector<RetrievalResult>& bm25,
    const std::vector<RetrievalResult>& graph,
    QueryType query_type,
    int top_k) {

    RRFWeights weights = get_weights(query_type);

    std::vector<std::pair<std::vector<RetrievalResult>, float>> sources;
    sources.reserve(3);
    sources.emplace_back(hnsw, weights.hnsw_weight);
    sources.emplace_back(bm25, weights.bm25_weight);
    sources.emplace_back(graph, weights.graph_weight);

    return rrf_fuse(sources, top_k);
}

// ========== ConfidenceWeightedFusion 实现 ==========

ConfidenceWeightedFusion::ConfidenceWeightedFusion(int rrf_k)
    : rrf_k_(rrf_k) {}

std::vector<RetrievalResult> ConfidenceWeightedFusion::fuse(
    const std::vector<RetrievalResult>& hnsw,
    const std::vector<RetrievalResult>& bm25,
    float hnsw_confidence,
    float bm25_confidence,
    int top_k) {

    // 使用置信度作为 RRF 权重
    std::vector<std::pair<std::vector<RetrievalResult>, float>> sources;
    sources.reserve(2);

    if (!hnsw.empty()) {
        sources.emplace_back(hnsw, hnsw_confidence);
    }
    if (!bm25.empty()) {
        sources.emplace_back(bm25, bm25_confidence);
    }

    if (sources.empty()) {
        return {};
    }

    // 计算所有唯一 chunk 的融合得分
    std::unordered_map<std::string, std::pair<float, Chunk>> chunk_scores;

    for (const auto& [results, weight] : sources) {
        for (int rank = 0; rank < static_cast<int>(results.size()); ++rank) {
            const auto& result = results[rank];
            float rrf_contrib = weight * (1.0f / (rrf_k_ + rank));

            auto it = chunk_scores.find(result.chunk.id);
            if (it == chunk_scores.end()) {
                chunk_scores[result.chunk.id] = {rrf_contrib, result.chunk};
            } else {
                it->second.first += rrf_contrib;
            }
        }
    }

    // 转换为结果向量并排序
    std::vector<RetrievalResult> fused;
    fused.reserve(chunk_scores.size());

    for (auto& [id, pair] : chunk_scores) {
        RetrievalResult result;
        result.chunk = pair.second;
        result.score = pair.first;
        result.source = "confidence_fused";
        fused.push_back(result);
    }

    // 按得分降序排序
    std::sort(fused.begin(), fused.end(),
              [](const RetrievalResult& a, const RetrievalResult& b) {
                  return a.score > b.score;
              });

    // 取 Top-K
    if (top_k > 0 && top_k < static_cast<int>(fused.size())) {
        fused.resize(top_k);
    }

    // 更新排名
    for (int i = 0; i < static_cast<int>(fused.size()); ++i) {
        fused[i].rank = i + 1;
    }

    return fused;
}

// ========== 工厂函数 ==========

std::unique_ptr<AdaptiveRRF> create_adaptive_rrf(int rrf_k) {
    return std::make_unique<AdaptiveRRF>(rrf_k);
}

std::unique_ptr<ConfidenceWeightedFusion> create_confidence_fusion(int rrf_k) {
    return std::make_unique<ConfidenceWeightedFusion>(rrf_k);
}

}  // namespace rag