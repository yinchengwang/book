/**
 * @file http_embedding.cpp
 * @brief HTTP Embedding 服务实现
 *
 * 通过 HTTP 调用 OpenAI-compatible /v1/embeddings 接口
 * 支持智谱AI、OpenAI、通义千问、本地 vLLM 等
 */

#include "mmrag/http_embedding.h"
#include "mmrag/error.h"
#include "mmrag/logger.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <sstream>

// HTTP 客户端 - 使用 httplib
#ifdef RAG_USE_HTTPLIB
#include <httplib.h>
#endif

// JSON 解析
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace mmrag {

// ========== 工具函数 ==========

template<typename Map>
void http_trim_cache(Map& cache, size_t max_size, size_t& access_counter) {
    while (cache.size() > max_size) {
        size_t min_time = SIZE_MAX;
        auto min_iter = cache.end();

        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.access_time < min_time) {
                min_time = it->second.access_time;
                min_iter = it;
            }
        }

        if (min_iter != cache.end()) {
            cache.erase(min_iter);
        }
    }
}

// ========== HttpEmbeddingService 实现 ==========

HttpEmbeddingService::HttpEmbeddingService(const HttpEmbeddingConfig& config)
    : config_(config), dimension_(config.dimension) {

    // 初始化降级服务
    fallback_ = std::make_unique<SimpleEmbeddingService>(dimension_);

    // 检查服务可用性
    if (check_service_available()) {
        RAG_INFO("HTTP Embedding service is ready (model: " + config_.model +
                 ", url: " + config_.api_url + ")");
    } else {
        RAG_WARN("HTTP Embedding service not available, using fallback");
        enable_fallback();
    }
}

HttpEmbeddingService::~HttpEmbeddingService() = default;

std::vector<float> HttpEmbeddingService::encode(const std::string& text) {
    if (text.empty()) {
        return std::vector<float>(dimension_, 0.0f);
    }

    // 如果使用降级服务，直接调用
    if (using_fallback_) {
        return fallback_->encode(text);
    }

    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = cache_.find(text);
        if (it != cache_.end()) {
            cache_hits_++;
            cache_[text].access_time = ++cache_access_counter_;
            return it->second.embedding;
        }
        cache_misses_++;
    }

    // 调用 API
    std::vector<float> embedding;
    try {
        embedding = call_api(text);
    } catch (const std::exception& e) {
        RAG_WARN("HTTP Embedding API call failed, using fallback: " + std::string(e.what()));
        enable_fallback();
        return fallback_->encode(text);
    }

    // 存入缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        http_trim_cache(cache_, config_.max_cache_size, cache_access_counter_);
        cache_[text] = {embedding, ++cache_access_counter_};
    }

    return embedding;
}

std::vector<std::vector<float>> HttpEmbeddingService::encode_batch(
    const std::vector<std::string>& texts) {

    if (texts.empty()) {
        return {};
    }

    // 如果使用降级服务，直接调用
    if (using_fallback_) {
        return fallback_->encode_batch(texts);
    }

    std::vector<std::vector<float>> results;
    results.reserve(texts.size());

    std::vector<std::string> uncached;
    std::vector<size_t> uncached_indices;

    // 检查缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        for (size_t i = 0; i < texts.size(); i++) {
            if (texts[i].empty()) {
                results.push_back(std::vector<float>(dimension_, 0.0f));
                continue;
            }

            auto it = cache_.find(texts[i]);
            if (it != cache_.end()) {
                results.push_back(it->second.embedding);
                cache_hits_++;
                cache_[texts[i]].access_time = ++cache_access_counter_;
            } else {
                results.push_back({});
                uncached.push_back(texts[i]);
                uncached_indices.push_back(i);
                cache_misses_++;
            }
        }
    }

    // 批量调用 API
    if (!uncached.empty()) {
        std::vector<std::vector<float>> embeddings;
        try {
            embeddings = call_api_batch(uncached);
        } catch (const std::exception& e) {
            RAG_WARN("HTTP batch API call failed, using fallback: " + std::string(e.what()));
            enable_fallback();
            return fallback_->encode_batch(texts);
        }

        // 更新结果和缓存
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            for (size_t i = 0; i < uncached.size(); i++) {
                results[uncached_indices[i]] = embeddings[i];
                http_trim_cache(cache_, config_.max_cache_size, cache_access_counter_);
                cache_[uncached[i]] = {embeddings[i], ++cache_access_counter_};
            }
        }
    }

    return results;
}

bool HttpEmbeddingService::is_ready() const {
    if (using_fallback_) {
        return fallback_->is_ready();
    }

    if (!service_checked_) {
        const_cast<HttpEmbeddingService*>(this)->service_checked_ = true;
        const_cast<HttpEmbeddingService*>(this)->service_available_ =
            const_cast<HttpEmbeddingService*>(this)->check_service_available();
    }

    return service_available_;
}

HttpEmbeddingService::CacheStats HttpEmbeddingService::get_cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    CacheStats stats;
    stats.size = cache_.size();
    stats.hits = cache_hits_;
    stats.misses = cache_misses_;
    size_t total = cache_hits_ + cache_misses_;
    stats.hit_rate = total > 0 ? static_cast<double>(cache_hits_) / total : 0.0;
    return stats;
}

void HttpEmbeddingService::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    cache_access_counter_ = 0;
    RAG_DEBUG("HTTP embedding cache cleared");
}

// ========== 私有方法实现 ==========

std::vector<float> HttpEmbeddingService::call_api(const std::string& text) {
    std::vector<std::vector<float>> results = call_api_batch({text});
    if (results.empty()) {
        throw RAGException(errors::INTERNAL_ERROR, "Empty response from HTTP Embedding API");
    }
    return results[0];
}

std::vector<std::vector<float>> HttpEmbeddingService::call_api_batch(
    const std::vector<std::string>& texts) {

    std::vector<std::vector<float>> results;

#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.api_url);
    cli.set_max_timeout(std::chrono::seconds(config_.timeout_seconds));

    // 构造 OpenAI-compatible 请求
    nlohmann::json req_json;
    req_json["input"] = texts;
    req_json["model"] = config_.model;

    // 设置 headers
    httplib::Headers headers;
    if (!config_.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config_.api_key);
    }

    auto res = cli.Post(config_.api_path, headers, req_json.dump(), "application/json");

    if (!res || res->status != 200) {
        std::string err_msg = res
            ? "HTTP " + std::to_string(res->status)
            : "Connection failed";
        if (res) {
            err_msg += ": " + res->body;
        }
        throw RAGException(errors::INTERNAL_ERROR, "Embedding API error: " + err_msg);
    }

    // 解析响应
    auto res_json = nlohmann::json::parse(res->body);
    if (res_json.contains("data") && res_json["data"].is_array()) {
        // 按 index 排序
        auto data = res_json["data"];
        std::sort(data.begin(), data.end(),
            [](const auto& a, const auto& b) {
                return a["index"].template get<int>() < b["index"].template get<int>();
            });

        for (const auto& item : data) {
            if (item.contains("embedding")) {
                results.push_back(item["embedding"].get<std::vector<float>>());
            }
        }
    } else {
        throw RAGException(errors::INTERNAL_ERROR, "Invalid response format: missing 'data' array");
    }

    if (results.size() != texts.size()) {
        throw RAGException(errors::INTERNAL_ERROR,
            "Response count mismatch: expected " + std::to_string(texts.size()) +
            ", got " + std::to_string(results.size()));
    }

#else
    throw RAGException(errors::INTERNAL_ERROR,
        "HTTP Embedding requires httplib (RAG_USE_HTTPLIB not defined)");
#endif

    return results;
}

void HttpEmbeddingService::enable_fallback() {
    using_fallback_ = true;
    if (fallback_) {
        fallback_->load();
    }
}

bool HttpEmbeddingService::check_service_available() {
    service_checked_ = true;

#ifdef RAG_USE_HTTPLIB
    // 尝试调用 API 验证可用性
    httplib::Client cli(config_.api_url);
    cli.set_max_timeout(std::chrono::seconds(5));

    // 发送一个简单的 embedding 请求
    nlohmann::json req_json;
    req_json["input"] = "test";
    req_json["model"] = config_.model;

    httplib::Headers headers;
    if (!config_.api_key.empty()) {
        headers.emplace("Authorization", "Bearer " + config_.api_key);
    }

    auto res = cli.Post(config_.api_path, headers, req_json.dump(), "application/json");
    service_available_ = (res != nullptr && res->status == 200);

    if (!service_available_ && res) {
        RAG_WARN("HTTP Embedding health check failed: HTTP " +
                 std::to_string(res->status) + " - " + res->body);
    }

    return service_available_;
#else
    return false;
#endif
}

}  // namespace mmrag
