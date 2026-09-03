/**
 * @file enhanced_retriever.cpp
 * @brief 增强检索器实现
 */

#include "rag/enhanced_retriever.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <atomic>
#include <unordered_map>

namespace rag {

// ========== EnhancedRetriever 实现 ==========

EnhancedRetriever::EnhancedRetriever(
    std::shared_ptr<Retriever> base_retriever,
    std::shared_ptr<Reranker> reranker,
    std::shared_ptr<EmbeddingService> embed_service,
    const EnhancedRetrievalConfig& config)
    : base_retriever_(base_retriever)
    , reranker_(reranker)
    , embed_service_(embed_service)
    , config_(config) {

    RAG_INFO("EnhancedRetriever initialized");
    RAG_INFO("  enable_rerank: " + std::to_string(config.enable_rerank));
    RAG_INFO("  reranker_type: " + config.reranker_type);
    RAG_INFO("  enable_mmr: " + std::to_string(config.enable_mmr));
}

std::vector<RetrievalResult> EnhancedRetriever::retrieve(
    const std::string& query,
    int top_k) {

    total_queries_++;
    auto start = std::chrono::steady_clock::now();

    // 1. 基础检索 (召回)
    int recall_k = config_.recall_top_k > 0 ? config_.recall_top_k : top_k * 5;
    auto candidates = base_retriever_->retrieve(query, recall_k);

    if (candidates.empty()) {
        RAG_DEBUG("No candidates retrieved for query");
        return {};
    }

    // 2. 重排序 (可选)
    if (config_.enable_rerank && reranker_ && reranker_->is_ready()) {
        rerank_count_++;
        auto rerank_start = std::chrono::steady_clock::now();

        auto chunks = extract_chunks(candidates);
        auto reranked = reranker_->rerank(query, chunks, top_k);

        // 合并分数回原始结果
        std::unordered_map<std::string, float> rerank_scores;
        for (const auto& r : reranked) {
            rerank_scores[r.chunk_id] = r.score;
        }

        for (auto& result : candidates) {
            auto it = rerank_scores.find(result.chunk.id);
            if (it != rerank_scores.end()) {
                result.score = it->second;
            }
        }

        // 按新分数排序
        std::sort(candidates.begin(), candidates.end(),
                  [](const auto& a, const auto& b) {
                      return a.score > b.score;
                  });

        auto rerank_end = std::chrono::steady_clock::now();
        RAG_DEBUG("Rerank time: " +
                  std::to_string(std::chrono::duration<double>(rerank_end - rerank_start).count() * 1000) + "ms");
    }

    // 3. MMR 多样性 (可选)
    if (config_.enable_mmr && candidates.size() > static_cast<size_t>(top_k)) {
        candidates = select_with_mmr(candidates, query, top_k);
    }

    // 4. 截取 Top-K
    if (static_cast<int>(candidates.size()) > top_k) {
        candidates.resize(top_k);
    }

    // 更新排名
    for (size_t i = 0; i < candidates.size(); i++) {
        candidates[i].rank = static_cast<int>(i) + 1;
    }

    return candidates;
}

std::vector<RetrievalResult> EnhancedRetriever::select_with_mmr(
    const std::vector<RetrievalResult>& candidates,
    const std::string& query,
    int top_k) {

    mmr_count_++;
    std::vector<RetrievalResult> selected;
    std::vector<RetrievalResult> remaining = candidates;

    if (!embed_service_ || !embed_service_->is_ready()) {
        // 没有 Embedding 服务，回退到简单截取
        RAG_DEBUG("Embedding service not available, skipping MMR");
        if (top_k > 0 && top_k < static_cast<int>(candidates.size())) {
            std::vector<RetrievalResult> result(candidates.begin(), candidates.begin() + top_k);
            return result;
        }
        return candidates;
    }

    // 获取查询向量
    auto query_vec = embed_service_->encode(query);

    while (selected.size() < static_cast<size_t>(top_k) && !remaining.empty()) {
        float best_mmr = -1.0f;
        size_t best_idx = 0;

        for (size_t i = 0; i < remaining.size(); i++) {
            const auto& candidate = remaining[i];

            // 相关性分数 (已归一化)
            float relevance = candidate.score;

            // 多样性分数 (与已选结果的最大相似度)
            float max_similarity = 0.0f;
            for (const auto& sel : selected) {
                auto sel_vec = embed_service_->encode(sel.chunk.content);
                float sim = cosine_similarity(query_vec, sel_vec);
                max_similarity = std::max(max_similarity, sim);
            }

            // MMR 分数 = λ * relevance + (1-λ) * (1 - max_similarity)
            float mmr = config_.mmr_lambda * relevance +
                        (1 - config_.mmr_lambda) * (1 - max_similarity);

            if (mmr > best_mmr) {
                best_mmr = mmr;
                best_idx = i;
            }
        }

        selected.push_back(remaining[best_idx]);
        remaining.erase(remaining.begin() + best_idx);
    }

    RAG_DEBUG("MMR selected " + std::to_string(selected.size()) + " from " +
              std::to_string(candidates.size()) + " candidates");

    return selected;
}

std::vector<Chunk> EnhancedRetriever::extract_chunks(
    const std::vector<RetrievalResult>& results) {

    std::vector<Chunk> chunks;
    chunks.reserve(results.size());

    for (const auto& r : results) {
        chunks.push_back(r.chunk);
    }

    return chunks;
}

float EnhancedRetriever::cosine_similarity(
    const std::vector<float>& a,
    const std::vector<float>& b) {

    if (a.size() != b.size() || a.empty()) {
        return 0.0f;
    }

    float dot = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    norm_a = std::sqrt(norm_a);
    norm_b = std::sqrt(norm_b);

    if (norm_a < 1e-10f || norm_b < 1e-10f) {
        return 0.0f;
    }

    return dot / (norm_a * norm_b);
}

EnhancedRetriever::Stats EnhancedRetriever::get_stats() const {
    Stats stats;
    stats.total_queries = total_queries_.load();
    stats.rerank_count = rerank_count_.load();
    stats.mmr_count = mmr_count_.load();
    return stats;
}

// ========== 工厂函数 ==========

std::shared_ptr<EnhancedRetriever> create_enhanced_retriever(
    std::shared_ptr<Retriever> base_retriever,
    const EnhancedRetrievalConfig& config,
    std::shared_ptr<EmbeddingService> embed_service) {

    // 创建重排序器
    std::shared_ptr<Reranker> reranker;
    if (config.enable_rerank) {
        if (config.reranker_type == "lightweight") {
            reranker = std::make_shared<LightweightReranker>();
        } else if (config.reranker_type == "bge") {
            reranker = create_reranker("bge", config.reranker_model);
        } else {
            reranker = std::make_shared<LightweightReranker>();
        }
    }

    // 如果没有提供 Embedding 服务，创建一个简单的
    if (!embed_service) {
        embed_service = std::make_shared<SimpleEmbeddingService>(768);
        static_cast<SimpleEmbeddingService*>(embed_service.get())->load();
    }

    return std::make_shared<EnhancedRetriever>(
        base_retriever, reranker, embed_service, config);
}

}  // namespace rag
