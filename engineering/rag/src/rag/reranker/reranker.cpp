/**
 * @file reranker.cpp
 * @brief 重排序器实现
 */

#include "rag/reranker.h"
#include "rag/logger.h"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <cmath>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <filesystem>

namespace rag {

// ========== 辅助函数 ==========

static int count_matches(const std::vector<std::string>& keywords,
                         const std::string& content) {
    auto content_lower = content;
    std::transform(content_lower.begin(), content_lower.end(),
                   content_lower.begin(), ::tolower);

    int matched = 0;
    for (const auto& kw : keywords) {
        if (content_lower.find(kw) != std::string::npos) {
            matched++;
        }
    }
    return matched;
}

// ========== LightweightReranker 实现 ==========

LightweightReranker::LightweightReranker() = default;

LightweightReranker::LightweightReranker(const LightweightRerankerConfig& config)
    : config_(config) {}

std::vector<RerankResult> LightweightReranker::rerank(
    const std::string& query,
    const std::vector<Chunk>& candidates,
    int top_n) {

    if (query.empty() || candidates.empty() || top_n <= 0) {
        return {};
    }

    std::vector<std::pair<Chunk, float>> scored;
    scored.reserve(candidates.size());

    for (const auto& chunk : candidates) {
        float score = compute_score(query, chunk);
        scored.emplace_back(chunk, score);
    }

    // 按分数降序排序
    std::sort(scored.begin(), scored.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    // 取 Top-N
    std::vector<RerankResult> results;
    int count = std::min(top_n, static_cast<int>(scored.size()));

    for (int i = 0; i < count; i++) {
        const auto& [chunk, score] = scored[i];

        RerankResult result;
        result.chunk_id = chunk.id;
        result.content = chunk.content;
        result.score = score;

        // 生成原因
        auto keywords = tokenize(query);
        auto matched = count_matches(keywords, chunk.content);
        result.reason = "matched " + std::to_string(matched) +
                        "/" + std::to_string(keywords.size()) + " keywords";

        results.push_back(result);
    }

    return results;
}

std::vector<std::vector<RerankResult>> LightweightReranker::rerank_batch(
    const std::vector<std::string>& queries,
    const std::vector<std::vector<Chunk>>& candidates_list,
    int top_n) {

    std::vector<std::vector<RerankResult>> results;
    results.reserve(queries.size());

    for (size_t i = 0; i < queries.size(); i++) {
        if (i < candidates_list.size()) {
            results.push_back(rerank(queries[i], candidates_list[i], top_n));
        } else {
            results.push_back({});
        }
    }

    return results;
}

float LightweightReranker::compute_score(const std::string& query,
                                          const Chunk& chunk) {
    float score = 0.0f;

    // 1. 关键词匹配分数
    float keyword_score = keyword_match_score(query, chunk.content);
    score += keyword_score * config_.keyword_weight;

    // 2. BM25 分数
    auto bm25_it = bm25_scores_.find(chunk.id);
    if (bm25_it != bm25_scores_.end()) {
        score += bm25_it->second * config_.bm25_weight;
    }

    // 3. 向量相似度分数
    auto vec_it = vector_scores_.find(chunk.id);
    if (vec_it != vector_scores_.end()) {
        score += vec_it->second * config_.vector_weight;
    }

    // 4. 位置奖励 (靠前的 chunk 可能更重要)
    if (chunk.chunk_index == 0) {
        score += config_.position_weight;
    }

    // 5. 长度惩罚 (太长或太短都略微减分)
    size_t len = chunk.content.length();
    if (len < 100 || len > 2000) {
        score -= 0.02f;
    }

    // 确保分数在 [0, 1] 范围内
    return std::max(0.0f, std::min(1.0f, score));
}

float LightweightReranker::keyword_match_score(
    const std::string& query,
    const std::string& content) {

    auto query_words = tokenize(query);
    auto content_lower = to_lower(content);

    if (query_words.empty()) {
        return 0.0f;
    }

    int matched = 0;
    for (const auto& word : query_words) {
        if (content_lower.find(word) != std::string::npos) {
            matched++;
        }
    }

    // 匹配度
    float coverage = static_cast<float>(matched) / query_words.size();

    // 精确匹配奖励 (文档以查询开头)
    float exact_match = 0.0f;
    auto content_start_lower = content_lower;
    if (content_start_lower.size() >= query.size()) {
        content_start_lower = content_start_lower.substr(0, query.size());
        if (to_lower(query).find(content_start_lower) == 0 ||
            content_start_lower.find(to_lower(query)) == 0) {
            exact_match = 0.2f;
        }
    }

    return coverage + exact_match;
}

std::vector<std::string> LightweightReranker::tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::stringstream ss(text);
    std::string word;

    while (ss >> word) {
        // 清理标点
        word.erase(std::remove_if(word.begin(), word.end(),
            [](unsigned char c) { return std::ispunct(c); }), word.end());

        if (!word.empty() && word.length() > 1) {
            words.push_back(to_lower(word));
        }
    }

    return words;
}

std::string LightweightReranker::to_lower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

void LightweightReranker::set_bm25_scores(
    const std::unordered_map<std::string, float>& scores) {
    bm25_scores_ = scores;
}

void LightweightReranker::set_vector_scores(
    const std::unordered_map<std::string, float>& scores) {
    vector_scores_ = scores;
}

void LightweightReranker::set_weights(float keyword, float bm25,
                                        float vector, float position) {
    config_.keyword_weight = keyword;
    config_.bm25_weight = bm25;
    config_.vector_weight = vector;
    config_.position_weight = position;
}

void LightweightReranker::clear_scores() {
    bm25_scores_.clear();
    vector_scores_.clear();
}

// ========== BGEReranker 实现 ==========

struct BGEReranker::Impl {
    BGERerankerConfig config;
    bool ready = false;

    Impl(const BGERerankerConfig& cfg) : config(cfg) {}
};

BGEReranker::BGEReranker(const std::string& model_path, const std::string& device)
    : impl_(std::make_unique<Impl>(BGERerankerConfig{model_path, device})) {}

BGEReranker::~BGEReranker() = default;

bool BGEReranker::init(const std::string& model_path) {
    if (!model_path.empty()) {
        impl_->config.model_path = model_path;
    }

    // 检查模型是否存在
    if (impl_->config.model_path.empty()) {
        RAG_WARN("BGE-Reranker model path is empty");
        return false;
    }

    if (!std::filesystem::exists(impl_->config.model_path)) {
        RAG_WARN("BGE-Reranker model not found at: " + impl_->config.model_path);
        return false;
    }

    // 实际模型加载需要 transformers 库
    RAG_INFO("BGE-Reranker model found, note: full loading requires transformers");
    ready_ = true;
    return true;
}

std::vector<RerankResult> BGEReranker::rerank(
    const std::string& query,
    const std::vector<Chunk>& candidates,
    int top_n) {

    if (!ready_) {
        RAG_WARN("BGE-Reranker not ready, returning original order");
        std::vector<RerankResult> results;
        int count = std::min(top_n, static_cast<int>(candidates.size()));
        for (int i = 0; i < count; i++) {
            results.push_back({
                candidates[i].id,
                candidates[i].content,
                1.0f / (i + 1),  // 原始排名作为分数
                "original order"
            });
        }
        return results;
    }

    // 简化实现：使用随机分数
    std::vector<RerankResult> results;
    for (const auto& chunk : candidates) {
        results.push_back({
            chunk.id,
            chunk.content,
            static_cast<float>(rand()) / RAND_MAX,
            "bge-reranker (placeholder)"
        });
    }

    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.score > b.score; });

    if (top_n > 0 && top_n < static_cast<int>(results.size())) {
        results.resize(top_n);
    }

    return results;
}

std::vector<std::vector<RerankResult>> BGEReranker::rerank_batch(
    const std::vector<std::string>& queries,
    const std::vector<std::vector<Chunk>>& candidates_list,
    int top_n) {

    std::vector<std::vector<RerankResult>> results;
    results.reserve(queries.size());

    for (size_t i = 0; i < queries.size(); i++) {
        if (i < candidates_list.size()) {
            results.push_back(rerank(queries[i], candidates_list[i], top_n));
        } else {
            results.push_back({});
        }
    }

    return results;
}

// ========== 工厂函数 ==========

std::shared_ptr<Reranker> create_reranker(
    const std::string& type,
    const std::string& model_path) {

    if (type == "lightweight") {
        return std::make_shared<LightweightReranker>();
    }

    if (type == "bge") {
        auto reranker = std::make_shared<BGEReranker>(model_path);
        reranker->init(model_path);
        return reranker;
    }

    // 默认使用轻量级
    RAG_WARN("Unknown reranker type: " + type + ", using lightweight");
    return std::make_shared<LightweightReranker>();
}

}  // namespace rag
