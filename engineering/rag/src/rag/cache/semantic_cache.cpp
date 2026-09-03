// engineering/rag/src/rag/cache/semantic_cache.cpp

#include "rag/retrieval_cache.h"
#include "rag/logger.h"

namespace rag {

SemanticCache::SemanticCache(std::shared_ptr<EmbeddingService> embed_service,
                             const CacheConfig& config)
    : embed_service_(embed_service), config_(config),
      lru_cache_(config) {}

SemanticCache::~SemanticCache() = default;

std::optional<StageOutput> SemanticCache::get(const std::string& query) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 1. 先尝试精确匹配
    auto exact_result = lru_cache_.get(query);
    if (exact_result.has_value()) {
        return exact_result;
    }

    // 2. 如果启用语义缓存，尝试语义匹配
    if (!config_.semantic_cache || !embed_service_) {
        return std::nullopt;
    }

    // 编码查询
    auto query_vec = embed_service_->encode(query);

    // 遍历缓存找相似
    for (const auto& [key, cached_vec] : embeddings_cache_) {
        float sim = cosine_similarity(query_vec, cached_vec);
        if (sim >= config_.similarity_threshold) {
            // 找到相似查询
            auto result = lru_cache_.get(key);
            if (result.has_value()) {
                semantic_hits_++;
                RAG_DEBUG("Semantic cache hit: " + query + " -> " + key +
                         " (similarity: " + std::to_string(sim) + ")");
                return result;
            }
        }
    }

    return std::nullopt;
}

void SemanticCache::put(const std::string& query, const StageOutput& output) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 添加到 LRU 缓存
    lru_cache_.put(query, output);

    // 编码并缓存向量
    if (embed_service_) {
        auto vec = embed_service_->encode(query);
        embeddings_cache_[query] = vec;
    }
}

void SemanticCache::put_exact(const std::string& key, const StageOutput& output) {
    lru_cache_.put(key, output);
}

void SemanticCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    lru_cache_.invalidate(key);
    embeddings_cache_.erase(key);
}

std::vector<float> SemanticCache::get_cached_embedding(const std::string& query) {
    auto it = embeddings_cache_.find(query);
    if (it != embeddings_cache_.end()) {
        return it->second;
    }

    if (embed_service_) {
        auto vec = embed_service_->encode(query);
        embeddings_cache_[query] = vec;
        return vec;
    }

    return {};
}

// ========== MultiLevelCache ==========

MultiLevelCache::MultiLevelCache(std::shared_ptr<EmbeddingService> embed_service,
                                 const CacheConfig& config)
    : semantic_cache_(std::make_unique<SemanticCache>(embed_service, config)) {}

MultiLevelCache::~MultiLevelCache() = default;

std::optional<StageOutput> MultiLevelCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    // L1: 语义缓存
    auto result = semantic_cache_->get(key);
    if (result.has_value()) {
        stats_.l1_hits++;
        return result;
    }

    stats_.misses++;
    return std::nullopt;
}

void MultiLevelCache::put(const std::string& key, const StageOutput& output) {
    std::lock_guard<std::mutex> lock(mutex_);

    semantic_cache_->put(key, output);
}

void MultiLevelCache::invalidate(const std::string& key) {
    semantic_cache_->invalidate(key);
}

void MultiLevelCache::clear() {
    // 清理所有缓存
}

MultiLevelCache::Stats MultiLevelCache::get_stats() const {
    Stats stats;
    stats.l1_hits = semantic_cache_->semantic_hits() + semantic_cache_->exact_hits();
    stats.misses = semantic_cache_->misses();
    return stats;
}

}  // namespace rag