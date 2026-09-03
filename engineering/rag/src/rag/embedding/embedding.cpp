/**
 * @file embedding.cpp
 * @brief Embedding 服务实现
 */

#include "rag/embedding.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <random>
#include <chrono>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <filesystem>

namespace rag {

// ========== SimpleEmbeddingService 实现 ==========

SimpleEmbeddingService::SimpleEmbeddingService(int dimension)
    : dimension_(dimension) {}

std::vector<float> SimpleEmbeddingService::encode(const std::string& text) {
    std::vector<float> embedding(dimension_);

    // 使用文本内容生成确定性随机种子
    std::hash<std::string> hasher;
    size_t seed = hasher(text);
    std::mt19937 gen(static_cast<unsigned>(seed));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // 生成随机向量
    float norm = 0.0f;
    for (int i = 0; i < dimension_; i++) {
        embedding[i] = dist(gen);
        norm += embedding[i] * embedding[i];
    }

    // L2 归一化
    norm = std::sqrt(norm) + 1e-10f;
    for (int i = 0; i < dimension_; i++) {
        embedding[i] /= norm;
    }

    encode_count_++;
    return embedding;
}

std::vector<std::vector<float>> SimpleEmbeddingService::encode_batch(
    const std::vector<std::string>& texts) {

    std::vector<std::vector<float>> results;
    results.reserve(texts.size());

    for (const auto& text : texts) {
        results.push_back(encode(text));
    }

    return results;
}

EmbeddingStats SimpleEmbeddingService::get_stats() const {
    EmbeddingStats stats;
    stats.encode_count = encode_count_;
    return stats;
}

// ========== 向量工具函数 ==========

float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
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

void normalize_l2(std::vector<float>& vec) {
    float norm = vector_norm(vec);
    if (norm > 1e-10f) {
        for (float& v : vec) {
            v /= norm;
        }
    }
}

float vector_norm(const std::vector<float>& vec) {
    float norm = 0.0f;
    for (float v : vec) {
        norm += v * v;
    }
    return std::sqrt(norm);
}

// ========== BGEM3EmbeddingService Pimpl 实现 ==========

struct BGEM3EmbeddingService::Impl {
    // 配置
    BGEM3Config config;

    // 模型组件 (使用 void* 避免直接依赖)
    void* tokenizer = nullptr;
    void* model = nullptr;

    // 缓存
    struct CacheEntry {
        std::vector<float> dense;
        int64_t access_time;
    };
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable std::mutex cache_mutex_;

    // 统计
    std::atomic<uint64_t> encode_count_{0};
    std::atomic<uint64_t> cache_hits_{0};
    std::atomic<uint64_t> cache_misses_{0};
    std::chrono::steady_clock::time_point start_time_;

    Impl(const BGEM3Config& cfg) : config(cfg), start_time_(std::chrono::steady_clock::now()) {}
};

BGEM3EmbeddingService::BGEM3EmbeddingService()
    : impl_(nullptr), ready_(false), fallback_(std::make_unique<SimpleEmbeddingService>(1024)) {
    RAG_WARN("BGE-M3 not available, using fallback random embeddings");
}

BGEM3EmbeddingService::BGEM3EmbeddingService(const BGEM3Config& config)
    : impl_(std::make_unique<Impl>(config)), fallback_(std::make_unique<SimpleEmbeddingService>(1024)) {

    // 检查模型路径是否存在
    if (config.model_path.empty()) {
        RAG_WARN("BGE-M3 model path is empty, using fallback");
        ready_ = false;
        return;
    }

    // 检查模型文件是否存在
    if (!std::filesystem::exists(config.model_path)) {
        RAG_WARN("BGE-M3 model not found at: " + config.model_path + ", using fallback");
        ready_ = false;
        return;
    }

    try {
        // 尝试加载模型 (这里简化处理，实际应调用 transformers)
        // 由于我们没有真实的 transformers 依赖，暂时使用回退
        RAG_INFO("BGE-M3 model found at: " + config.model_path);
        RAG_INFO("Note: Full model loading requires transformers library");
        RAG_INFO("Using fallback embeddings for now");

        ready_ = true;
    } catch (const std::exception& e) {
        RAG_ERROR("Failed to load BGE-M3: " + std::string(e.what()));
        ready_ = false;
    }
}

BGEM3EmbeddingService::~BGEM3EmbeddingService() = default;

BGEM3EmbeddingService::BGEM3EmbeddingService(BGEM3EmbeddingService&&) noexcept = default;
BGEM3EmbeddingService& BGEM3EmbeddingService::operator=(BGEM3EmbeddingService&&) noexcept = default;

int BGEM3EmbeddingService::dimension() const {
    if (impl_ && ready_) {
        return 1024;  // BGE-M3 维度
    }
    return fallback_ ? fallback_->dimension() : 1024;
}

std::vector<float> BGEM3EmbeddingService::encode(const std::string& text) {
    if (!ready_ || !impl_) {
        return fallback_->encode(text);
    }

    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        auto it = impl_->cache_.find(text);
        if (it != impl_->cache_.end()) {
            impl_->cache_hits_++;
            return it->second.dense;
        }
        impl_->cache_misses_++;
    }

    // 由于没有真实模型，使用回退 + 缓存
    auto embedding = fallback_->encode(text);

    // 写入缓存
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        if (impl_->cache_.size() < impl_->config.max_cache_size) {
            impl_->cache_[text] = {embedding,
                static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - impl_->start_time_).count())};
        }
    }

    impl_->encode_count_++;
    return embedding;
}

std::vector<std::vector<float>> BGEM3EmbeddingService::encode_batch(
    const std::vector<std::string>& texts) {

    if (!ready_ || !impl_) {
        return fallback_->encode_batch(texts);
    }

    std::vector<std::vector<float>> results;
    results.reserve(texts.size());

    std::vector<std::string> uncached;
    std::vector<size_t> uncached_indices;

    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        for (size_t i = 0; i < texts.size(); i++) {
            auto it = impl_->cache_.find(texts[i]);
            if (it != impl_->cache_.end()) {
                results.push_back(it->second.dense);
                impl_->cache_hits_++;
            } else {
                results.push_back({});
                uncached.push_back(texts[i]);
                uncached_indices.push_back(i);
                impl_->cache_misses_++;
            }
        }
    }

    // 批量编码未缓存的文本
    if (!uncached.empty()) {
        auto embeddings = fallback_->encode_batch(uncached);

        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        for (size_t i = 0; i < uncached.size(); i++) {
            results[uncached_indices[i]] = embeddings[i];

            if (impl_->cache_.size() < impl_->config.max_cache_size) {
                impl_->cache_[uncached[i]] = {embeddings[i],
                    static_cast<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - impl_->start_time_).count())};
            }
        }
    }

    impl_->encode_count_ += texts.size();
    return results;
}

BGEM3EmbeddingService::SparseVector BGEM3EmbeddingService::encode_sparse(
    const std::string& text) {

    SparseVector sparse;

    if (!ready_ || !impl_) {
        return sparse;
    }

    // 简化实现：基于字符 n-gram
    const int ngram_size = 2;
    std::unordered_map<uint32_t, float> tf;

    for (size_t i = 0; i + ngram_size <= text.size(); i++) {
        std::string ngram = text.substr(i, ngram_size);
        std::hash<std::string> hasher;
        uint32_t hash = static_cast<uint32_t>(hasher(ngram));

        tf[hash] += 1.0f;
    }

    // 归一化
    size_t total = text.size() > 0 ? text.size() - ngram_size + 1 : 1;
    for (auto& [token_id, freq] : tf) {
        freq = (freq * (1.5f + 1.0f)) / (freq + 1.5f);  // BM25 风格
        sparse.indices.push_back(token_id);
        sparse.values.push_back(freq);
    }

    return sparse;
}

BGEM3EmbeddingService::EmbeddingResult BGEM3EmbeddingService::encode_with_info(
    const std::string& text) {

    EmbeddingResult result;
    result.dense = encode(text);
    result.sparse = encode_sparse(text);
    return result;
}

EmbeddingStats BGEM3EmbeddingService::get_stats() const {
    EmbeddingStats stats;

    if (impl_) {
        stats.encode_count = impl_->encode_count_.load();
        stats.cache_hits = impl_->cache_hits_.load();
        stats.cache_misses = impl_->cache_misses_.load();

        uint64_t total = stats.cache_hits + stats.cache_misses;
        if (total > 0) {
            stats.cache_hit_rate = static_cast<double>(stats.cache_hits) / total;
        }
    }

    if (fallback_) {
        auto fallback_stats = fallback_->get_stats();
        stats.encode_count += fallback_stats.encode_count;
    }

    return stats;
}

void BGEM3EmbeddingService::clear_cache() {
    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        impl_->cache_.clear();
        RAG_DEBUG("Embedding cache cleared");
    }
}

size_t BGEM3EmbeddingService::cache_size() const {
    if (impl_) {
        std::lock_guard<std::mutex> lock(impl_->cache_mutex_);
        return impl_->cache_.size();
    }
    return 0;
}

// ========== 工厂函数 ==========

std::shared_ptr<EmbeddingService> create_embedding_service(
    const std::string& type, int dimension) {

    if (type == "bge-m3") {
        return std::make_shared<BGEM3EmbeddingService>(BGEM3Config{});
    }

    // 默认使用 Simple
    return std::make_shared<SimpleEmbeddingService>(dimension);
}

}  // namespace rag
