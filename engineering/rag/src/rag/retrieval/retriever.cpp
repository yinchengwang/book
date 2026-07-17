/**
 * @file retriever.cpp
 * @brief 检索器实现
 */

#include "rag/retriever.h"
#include "rag/logger.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <cmath>

namespace rag {

// ========== HNSWRetriever 实现 ==========

HNSWRetriever::HNSWRetriever(std::shared_ptr<VectorIndex> index,
                             std::shared_ptr<EmbeddingService> embed_service,
                             const RetrievalConfig& config)
    : index_(std::move(index)),
      embed_service_(std::move(embed_service)),
      config_(config) {}

std::vector<RetrievalResult> HNSWRetriever::retrieve(const std::string& query, int top_k) {
    if (!embed_service_->is_ready()) {
        RAG_WARN("Embedding service not ready");
        return {};
    }

    // 1. 编码查询
    auto query_vector = embed_service_->encode(query);

    // 2. 搜索
    auto results = index_->search(query_vector, top_k);

    // 3. 转换为检索结果
    std::vector<RetrievalResult> retriever_results;
    for (const auto& [id, score] : results) {
        RetrievalResult result;
        result.chunk.id = id;
        result.vector_score = 1.0f / (1.0f + score);  // 转换为相似度
        result.score = result.vector_score;
        result.source = "hnsw";
        retriever_results.push_back(result);
    }

    return retriever_results;
}

// ========== BM25Retriever 实现 ==========

BM25Retriever::BM25Retriever(std::shared_ptr<BM25Index> index,
                             const RetrievalConfig& config)
    : index_(std::move(index)),
      config_(config) {}

std::vector<RetrievalResult> BM25Retriever::retrieve(const std::string& query, int top_k) {
    // 搜索
    auto results = index_->search(query, top_k);

    // 转换为检索结果
    std::vector<RetrievalResult> retriever_results;
    float max_score = 0;
    for (const auto& [id, score] : results) {
        if (score > max_score) max_score = score;
    }

    for (const auto& [id, score] : results) {
        RetrievalResult result;
        result.chunk.id = id;
        result.bm25_score = max_score > 0 ? score / max_score : 0;  // 归一化
        result.score = result.bm25_score;
        result.source = "bm25";
        retriever_results.push_back(result);
    }

    return retriever_results;
}

void BM25Retriever::add_document(const std::string& id, const std::string& content) {
    index_->add(id, content);
}

// ========== HybridRetriever 实现 ==========

HybridRetriever::HybridRetriever(std::shared_ptr<HNSWRetriever> hnsw,
                                 std::shared_ptr<BM25Retriever> bm25,
                                 const RetrievalConfig& config)
    : hnsw_(std::move(hnsw)),
      bm25_(std::move(bm25)),
      config_(config) {}

std::vector<RetrievalResult> HybridRetriever::retrieve(const std::string& query, int top_k) {
    // 1. 并行执行两种检索
    auto hnsw_results = hnsw_->retrieve(query, top_k * 2);
    auto bm25_results = bm25_->retrieve(query, top_k * 2);

    if (hnsw_results.empty() && bm25_results.empty()) {
        return {};
    }

    if (hnsw_results.empty()) {
        return bm25_results;
    }

    if (bm25_results.empty()) {
        return hnsw_results;
    }

    // 2. RRF 融合
    return rrf_fusion(hnsw_results, bm25_results, top_k);
}

std::vector<RetrievalResult> HybridRetriever::rrf_fusion(
    const std::vector<RetrievalResult>& hnsw_results,
    const std::vector<RetrievalResult>& bm25_results,
    int top_k) {

    // 构建分数映射
    std::unordered_map<std::string, float> rrf_scores;
    std::unordered_map<std::string, RetrievalResult> result_map;

    float k = config_.rrf_k;  // 通常 60

    // HNSW RRF 分数
    for (int i = 0; i < static_cast<int>(hnsw_results.size()); ++i) {
        const auto& result = hnsw_results[i];
        float score = 1.0f / (k + i + 1);
        rrf_scores[result.chunk.id] += score * config_.hybrid_weight;
        result_map[result.chunk.id] = result;
    }

    // BM25 RRF 分数
    for (int i = 0; i < static_cast<int>(bm25_results.size()); ++i) {
        const auto& result = bm25_results[i];
        float score = 1.0f / (k + i + 1);
        rrf_scores[result.chunk.id] += score * (1.0f - config_.hybrid_weight);
        if (result_map.find(result.chunk.id) != result_map.end()) {
            result_map[result.chunk.id].bm25_score = result.bm25_score;
        } else {
            result_map[result.chunk.id] = result;
        }
    }

    // 排序
    std::vector<std::pair<std::string, float>> sorted;
    for (const auto& [id, score] : rrf_scores) {
        sorted.emplace_back(id, score);
    }

    std::partial_sort(sorted.begin(), sorted.begin() + top_k, sorted.end(),
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });

    // 构建结果
    std::vector<RetrievalResult> results;
    for (int i = 0; i < std::min(top_k, static_cast<int>(sorted.size())); ++i) {
        auto& result = result_map[sorted[i].first];
        result.score = sorted[i].second;
        result.rank = i + 1;
        results.push_back(result);
    }

    return results;
}

std::vector<RetrievalResult> HybridRetriever::weighted_fusion(
    const std::vector<RetrievalResult>& hnsw_results,
    const std::vector<RetrievalResult>& bm25_results,
    int top_k) {

    // 构建分数映射
    std::unordered_map<std::string, std::pair<float, float>> scores;
    std::unordered_map<std::string, RetrievalResult> result_map;

    for (const auto& result : hnsw_results) {
        scores[result.chunk.id].first = result.vector_score;
        result_map[result.chunk.id] = result;
    }

    for (const auto& result : bm25_results) {
        scores[result.chunk.id].second = result.bm25_score;
        if (result_map.find(result.chunk.id) == result_map.end()) {
            result_map[result.chunk.id] = result;
        }
    }

    // 加权融合
    std::vector<std::pair<std::string, float>> fused;
    for (const auto& [id, sc] : scores) {
        float combined = config_.hybrid_weight * sc.first +
                        (1.0f - config_.hybrid_weight) * sc.second;
        fused.emplace_back(id, combined);
    }

    std::partial_sort(fused.begin(), fused.begin() + top_k, fused.end(),
                     [](const auto& a, const auto& b) {
                         return a.second > b.second;
                     });

    std::vector<RetrievalResult> results;
    for (int i = 0; i < std::min(top_k, static_cast<int>(fused.size())); ++i) {
        auto& result = result_map[fused[i].first];
        result.score = fused[i].second;
        result.rank = i + 1;
        results.push_back(result);
    }

    return results;
}

// ========== 工厂函数 ==========

std::shared_ptr<Retriever> create_retriever(
    const RetrievalConfig& config,
    std::shared_ptr<VectorIndex> vector_index,
    std::shared_ptr<BM25Index> bm25_index,
    std::shared_ptr<EmbeddingService> embed_service) {

    auto hnsw = std::make_shared<HNSWRetriever>(vector_index, embed_service, config);
    auto bm25 = std::make_shared<BM25Retriever>(bm25_index, config);
    return std::make_shared<HybridRetriever>(hnsw, bm25, config);
}

}  // namespace rag
