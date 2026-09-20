/**
 * @file kbase_embedding_service.cpp
 * @brief Kbase Embedding Service — wraps MiniLMEmbedder
 *
 * Uses MiniLMEmbedder (pure C++ MiniLM-L6 inference) instead of the kbase
 * library, since the kbase C implementation is not available.
 *
 * Expected model layout (produced by scripts/gguf_to_fp32.py):
 *   <model_path>/
 *     weights.bin  - all FP32 weights, layout matches meta.json
 *     vocab.txt    - WordPiece vocabulary, one token per line
 *     meta.json    - model hyperparameters and tensor offsets
 *
 * Single-text encode returns 384-dim L2-normalized vector.
 */

#include "mmrag/embedding.h"
#include "mmrag/logger.h"
#include "mmrag/minilm_embedder.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <atomic>

namespace mmrag {

// ========== Pimpl 实现 ==========

struct KbaseEmbeddingService::Impl {
    // Wraps MiniLMEmbedder for the actual inference
    std::unique_ptr<MiniLMEmbedder> embedder;

    // Embedding stats
    mutable std::mutex stats_mutex;
    mutable uint64_t encode_count = 0;
    mutable uint64_t cache_hits = 0;
    mutable uint64_t cache_misses = 0;
    mutable double total_encode_time_ms = 0.0;
};

// ========== 构造 / 析构 ==========

KbaseEmbeddingService::KbaseEmbeddingService(const KbaseEmbeddingConfig& config)
    : KbaseEmbeddingService(config.model_path, config.dimension) {}

KbaseEmbeddingService::KbaseEmbeddingService(const std::string& model_path, int dimension)
    : impl_(std::make_unique<Impl>()),
      model_path_(model_path),
      dimension_(dimension > 0 ? dimension : 384) {

    if (model_path_.empty()) {
        RAG_WARN("KbaseEmbeddingService: model_path is empty, service will be unavailable");
        ready_ = false;
        return;
    }

    // Try to load via MiniLMEmbedder
    impl_->embedder = std::make_unique<MiniLMEmbedder>(model_path_, dimension_);

    if (!impl_->embedder || !impl_->embedder->is_ready()) {
        RAG_WARN("KbaseEmbeddingService: failed to load MiniLM model from " + model_path_ +
                 ", service will be unavailable");
        ready_ = false;
        return;
    }

    // Override dimension from loaded model
    int actual_dim = impl_->embedder->dimension();
    if (actual_dim > 0 && actual_dim != dimension_) {
        RAG_INFO("KbaseEmbeddingService: overriding dimension " +
                 std::to_string(dimension_) + " with model dim " +
                 std::to_string(actual_dim));
        dimension_ = actual_dim;
    }

    ready_ = true;
    RAG_INFO("KbaseEmbeddingService loaded: model=" + model_path_ +
             ", dim=" + std::to_string(dimension_) +
             ", type=MiniLMEmbedder (FP32)");
}

KbaseEmbeddingService::~KbaseEmbeddingService() = default;

// ========== 编码接口 ==========

std::vector<float> KbaseEmbeddingService::encode(const std::string& text) {
    if (!ready_ || !impl_ || !impl_->embedder) {
        return {};
    }
    if (text.empty()) {
        return std::vector<float>(dimension_, 0.0f);
    }

    auto t0 = std::chrono::steady_clock::now();
    auto vec = impl_->embedder->encode(text);
    auto t1 = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    impl_->encode_count++;
    impl_->total_encode_time_ms +=
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (vec.empty()) return {};

    // MiniLMEmbedder already returns L2-normalized vectors, but pad/trim to expected dim
    if ((int)vec.size() != dimension_) {
        vec.resize(dimension_, 0.0f);
    }
    return vec;
}

std::vector<std::vector<float>> KbaseEmbeddingService::encode_batch(
    const std::vector<std::string>& texts) {
    std::vector<std::vector<float>> result;
    if (!ready_ || !impl_ || !impl_->embedder) {
        result.reserve(texts.size());
        for (size_t i = 0; i < texts.size(); ++i) {
            result.emplace_back();
        }
        return result;
    }
    if (texts.empty()) {
        return result;
    }

    auto t0 = std::chrono::steady_clock::now();
    auto raw_results = impl_->embedder->encode_batch(texts);
    auto t1 = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    impl_->encode_count += texts.size();
    impl_->total_encode_time_ms +=
        std::chrono::duration<double, std::milli>(t1 - t0).count();

    result.reserve(raw_results.size());
    for (auto& v : raw_results) {
        if (v.empty()) {
            result.emplace_back();
            continue;
        }
        if ((int)v.size() != dimension_) {
            v.resize(dimension_, 0.0f);
        }
        result.push_back(std::move(v));
    }
    return result;
}

EmbeddingStats KbaseEmbeddingService::get_stats() const {
    EmbeddingStats stats;
    if (!impl_) return stats;

    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    stats.encode_count = impl_->encode_count;
    stats.cache_hits = impl_->cache_hits;
    stats.cache_misses = impl_->cache_misses;
    if (impl_->encode_count > 0) {
        stats.avg_encode_time_ms = impl_->total_encode_time_ms / impl_->encode_count;
    }
    return stats;
}

}  // namespace mmrag