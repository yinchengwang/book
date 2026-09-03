// engineering/rag/include/rag/retrieval_cache.h
#pragma once

#include "rag/pipeline.h"
#include "rag/embedding.h"
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <unordered_map>
#include <list>
#include <mutex>

namespace rag {

struct CacheConfig {
    size_t max_size = 10000;          // 最大缓存条目
    int ttl_seconds = 3600;           // 过期时间 (1小时)
    bool semantic_cache = true;       // 启用语义缓存
    float similarity_threshold = 0.95f;  // 语义相似度阈值
    bool persist_to_disk = false;     // 持久化到磁盘
    std::string persist_path = "./rag_data/cache";
};

// ========== LRU 缓存 ==========

class LruCache {
public:
    explicit LruCache(const CacheConfig& config);
    ~LruCache();

    // 基本操作
    std::optional<StageOutput> get(const std::string& key);
    void put(const std::string& key, const StageOutput& output);
    void invalidate(const std::string& key);
    void clear();

    // 统计
    size_t size() const { return cache_.size(); }
    size_t hits() const { return hits_; }
    size_t misses() const { return misses_; }
    double hit_rate() const {
        size_t total = hits_ + misses_;
        return total > 0 ? (double)hits_ / total : 0.0;
    }

private:
    struct Entry {
        StageOutput output;
        int64_t timestamp;
    };

    bool is_expired(const Entry& entry) const;
    void evict_lru();

    CacheConfig config_;
    std::unordered_map<std::string, Entry> cache_;
    std::list<std::string> lru_order_;  // 最近使用的在前面
    std::unordered_map<std::string, std::list<std::string>::iterator> lru_map_;

    mutable size_t hits_ = 0;
    mutable size_t misses_ = 0;
    mutable std::mutex mutex_;
};

// ========== 语义缓存 ==========

class SemanticCache {
public:
    explicit SemanticCache(std::shared_ptr<EmbeddingService> embed_service,
                          const CacheConfig& config);
    ~SemanticCache();

    std::optional<StageOutput> get(const std::string& query);
    void put(const std::string& query, const StageOutput& output);
    void invalidate(const std::string& key);

    // 手动添加精确匹配
    void put_exact(const std::string& key, const StageOutput& output);

    // 统计
    size_t size() const { return lru_cache_.size(); }
    size_t exact_hits() const { return lru_cache_.hits(); }
    size_t semantic_hits() const { return semantic_hits_; }
    size_t misses() const { return lru_cache_.misses(); }

private:
    std::vector<float> get_cached_embedding(const std::string& query);

    std::shared_ptr<EmbeddingService> embed_service_;
    CacheConfig config_;
    LruCache lru_cache_;
    std::unordered_map<std::string, std::vector<float>> embeddings_cache_;

    mutable size_t semantic_hits_ = 0;
    mutable std::mutex mutex_;
};

// ========== 多级缓存 ==========

class MultiLevelCache {
public:
    explicit MultiLevelCache(std::shared_ptr<EmbeddingService> embed_service,
                             const CacheConfig& config);
    ~MultiLevelCache();

    std::optional<StageOutput> get(const std::string& key);
    void put(const std::string& key, const StageOutput& output);
    void invalidate(const std::string& key);
    void clear();

    // 统计
    struct Stats {
        size_t l1_hits = 0;
        size_t l2_hits = 0;
        size_t l3_hits = 0;
        size_t misses = 0;
    };
    Stats get_stats() const;

private:
    std::unique_ptr<LruCache> l1_cache_;    // 进程内 LRU
    std::unique_ptr<SemanticCache> semantic_cache_;  // 语义缓存
    std::unique_ptr<LruCache> l3_cache_;    // 持久化缓存

    mutable Stats stats_;
    mutable std::mutex mutex_;
};

}  // namespace rag