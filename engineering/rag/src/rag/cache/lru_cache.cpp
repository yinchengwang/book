// engineering/rag/src/rag/cache/lru_cache.cpp

#include "rag/retrieval_cache.h"
#include "rag/types.h"
#include <algorithm>

namespace rag {

LruCache::LruCache(const CacheConfig& config) : config_(config) {}

LruCache::~LruCache() = default;

std::optional<StageOutput> LruCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        misses_++;
        return std::nullopt;
    }

    if (is_expired(it->second)) {
        // 过期，删除
        cache_.erase(it);
        lru_order_.erase(lru_map_[key]);
        lru_map_.erase(key);
        misses_++;
        return std::nullopt;
    }

    // 命中，更新 LRU 顺序
    hits_++;
    lru_order_.erase(lru_map_[key]);
    lru_order_.push_front(key);
    lru_map_[key] = lru_order_.begin();

    return it->second.output;
}

void LruCache::put(const std::string& key, const StageOutput& output) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果已存在，更新
    if (cache_.find(key) != cache_.end()) {
        cache_[key] = {output, current_timestamp()};
        lru_order_.erase(lru_map_[key]);
        lru_order_.push_front(key);
        lru_map_[key] = lru_order_.begin();
        return;
    }

    // 如果满了，驱逐 LRU
    if (cache_.size() >= config_.max_size) {
        evict_lru();
    }

    // 添加新条目
    auto timestamp = current_timestamp();
    cache_[key] = {output, timestamp};
    lru_order_.push_front(key);
    lru_map_[key] = lru_order_.begin();
}

void LruCache::invalidate(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        lru_order_.erase(lru_map_[key]);
        lru_map_.erase(key);
        cache_.erase(it);
    }
}

void LruCache::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    cache_.clear();
    lru_order_.clear();
    lru_map_.clear();
}

bool LruCache::is_expired(const Entry& entry) const {
    if (config_.ttl_seconds <= 0) return false;

    auto now = current_timestamp();
    return (now - entry.timestamp) > config_.ttl_seconds;
}

void LruCache::evict_lru() {
    if (lru_order_.empty()) return;

    // 驱逐最久未使用的
    auto key = lru_order_.back();
    lru_order_.pop_back();
    lru_map_.erase(key);
    cache_.erase(key);
}

}  // namespace rag