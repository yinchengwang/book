/**
 * @file ollama_embedding.cpp
 * @brief Ollama Embedding 服务实现
 *
 * 通过 Ollama API 实现语义向量编码，支持缓存和降级
 */

#include "rag/ollama_embedding.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstring>
#include <sstream>

// HTTP 客户端 - 优先使用 httplib，如果不可用则使用 libcurl
#ifdef RAG_USE_HTTPLIB
#include <httplib.h>
#else
#include <curl/curl.h>
#endif

// JSON 解析
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace rag {

// ========== 工具函数 ==========

/**
 * @brief 简单 LRU 缓存清理
 * 当缓存超过 max_size 时，删除最老的条目
 */
template<typename Map>
void trim_cache(Map& cache, size_t max_size, size_t& access_counter) {
    while (cache.size() > max_size) {
        // 找到访问时间最早的条目
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

// ========== OllamaEmbeddingService 实现 ==========

OllamaEmbeddingService::OllamaEmbeddingService(const OllamaEmbeddingConfig& config)
    : config_(config), dimension_(detect_dimension()) {

    // 初始化降级服务
    fallback_ = std::make_unique<SimpleEmbeddingService>(dimension_);

    // 检查服务可用性
    if (check_service_available()) {
        RAG_INFO("Ollama Embedding service is ready (model: " + config_.model + ")");
    } else {
        RAG_WARN("Ollama service not available, using fallback");
        enable_fallback();
    }
}

OllamaEmbeddingService::OllamaEmbeddingService(
    const std::string& base_url,
    const std::string& model,
    size_t max_cache_size)
    : OllamaEmbeddingService(OllamaEmbeddingConfig{
        base_url, model, max_cache_size, 30, 3}) {}

OllamaEmbeddingService::~OllamaEmbeddingService() = default;

std::vector<float> OllamaEmbeddingService::encode(const std::string& text) {
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
            // 更新访问时间
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
        RAG_WARN("Ollama API call failed, using fallback: " + std::string(e.what()));
        enable_fallback();
        return fallback_->encode(text);
    }

    // 存入缓存
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        // LRU 淘汰
        trim_cache(cache_, config_.max_cache_size, cache_access_counter_);

        cache_[text] = {embedding, ++cache_access_counter_};
    }

    return embedding;
}

std::vector<std::vector<float>> OllamaEmbeddingService::encode_batch(
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
                // 更新访问时间
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
            RAG_WARN("Ollama batch API call failed, using fallback: " + std::string(e.what()));
            enable_fallback();
            auto fallback_results = fallback_->encode_batch(texts);
            // 只返回原始索引的结果
            for (size_t i = 0; i < uncached_indices.size(); i++) {
                results[uncached_indices[i]] = embeddings[i];
            }
            return results;
        }

        // 更新结果和缓存
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            for (size_t i = 0; i < uncached.size(); i++) {
                results[uncached_indices[i]] = embeddings[i];

                // LRU 淘汰
                trim_cache(cache_, config_.max_cache_size, cache_access_counter_);

                cache_[uncached[i]] = {embeddings[i], ++cache_access_counter_};
            }
        }
    }

    return results;
}

bool OllamaEmbeddingService::is_ready() const {
    if (using_fallback_) {
        return fallback_->is_ready();
    }

    if (!service_checked_) {
        // 懒检查
        const_cast<OllamaEmbeddingService*>(this)->service_checked_ = true;
        const_cast<OllamaEmbeddingService*>(this)->service_available_ =
            const_cast<OllamaEmbeddingService*>(this)->check_service_available();
    }

    return service_available_;
}

OllamaEmbeddingService::CacheStats OllamaEmbeddingService::get_cache_stats() const {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    CacheStats stats;
    stats.size = cache_.size();
    stats.hits = cache_hits_;
    stats.misses = cache_misses_;

    size_t total = cache_hits_ + cache_misses_;
    stats.hit_rate = total > 0 ? static_cast<double>(cache_hits_) / total : 0.0;

    return stats;
}

void OllamaEmbeddingService::clear_cache() {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    cache_.clear();
    cache_access_counter_ = 0;
    RAG_DEBUG("Ollama embedding cache cleared");
}

// ========== 私有方法实现 ==========

std::vector<float> OllamaEmbeddingService::call_api(const std::string& text) {
    std::vector<std::vector<float>> results = call_api_batch({text});
    if (results.empty()) {
        throw RAGException(errors::INTERNAL_ERROR, "Empty response from Ollama API");
    }
    return results[0];
}

std::vector<std::vector<float>> OllamaEmbeddingService::call_api_batch(
    const std::vector<std::string>& texts) {

    std::vector<std::vector<float>> results;

#ifdef RAG_USE_HTTPLIB
    // 使用 httplib
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(config_.timeout_seconds));

    // 构造请求 - Ollama embeddings API 是单条的，需要循环调用
    // 注意：Ollama 的 /api/embeddings 不支持批量，需要逐个调用
    for (const auto& text : texts) {
        nlohmann::json req_json;
        req_json["model"] = config_.model;
        req_json["prompt"] = text;

        auto res = cli.Post("/api/embeddings", req_json.dump(), "application/json");

        if (!res || res->status != 200) {
            std::string err_msg = res
                ? "HTTP " + std::to_string(res->status)
                : "Connection failed";
            throw RAGException(errors::INTERNAL_ERROR, "Ollama API error: " + err_msg);
        }

        auto res_json = nlohmann::json::parse(res->body);
        if (res_json.contains("embedding")) {
            results.push_back(res_json["embedding"].get<std::vector<float>>());
        } else {
            throw RAGException(errors::INTERNAL_ERROR, "Invalid response format");
        }
    }

#else
    // 使用 libcurl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw RAGException(errors::INTERNAL_ERROR, "Failed to init CURL");
    }

    // 设置通用选项
    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/embeddings").c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);

    for (const auto& text : texts) {
        nlohmann::json req_json;
        req_json["model"] = config_.model;
        req_json["prompt"] = text;

        std::string req_body = req_json.dump();

        struct StringBuffer {
            char* data;
            size_t size;
        } buffer = {nullptr, 0};

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());

        // 设置 Content-Type
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // 响应回调
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            [](char* ptr, size_t size, size_t nmemb, void* userdata) {
                auto* buf = static_cast<StringBuffer*>(userdata);
                size_t new_size = buf->size + size * nmemb;
                buf->data = static_cast<char*>(realloc(buf->data, new_size + 1));
                memcpy(buf->data + buf->size, ptr, size * nmemb);
                buf->size = new_size;
                buf->data[new_size] = '\0';
                return size * nmemb;
            });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

        CURLcode res_code = curl_easy_perform(curl);

        if (res_code != CURLE_OK) {
            free(buffer.data);
            curl_slist_free_all(headers);
            throw RAGException(errors::INTERNAL_ERROR,
                "CURL error: " + std::string(curl_easy_strerror(res_code)));
        }

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code != 200) {
            free(buffer.data);
            curl_slist_free_all(headers);
            throw RAGException(errors::INTERNAL_ERROR,
                "HTTP error: " + std::to_string(http_code));
        }

        try {
            auto res_json = nlohmann::json::parse(buffer.data);
            if (res_json.contains("embedding")) {
                results.push_back(res_json["embedding"].get<std::vector<float>>());
            }
        } catch (const std::exception&) {
            free(buffer.data);
            curl_slist_free_all(headers);
            throw RAGException(errors::PARSE_ERROR, "Failed to parse Ollama response");
        }

        free(buffer.data);
        curl_slist_free_all(headers);

        // 重置 POST 状态以复用连接
        curl_easy_reset(curl);
        curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/embeddings").c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    }

    curl_easy_cleanup(curl);
#endif

    return results;
}

void OllamaEmbeddingService::enable_fallback() {
    using_fallback_ = true;
    if (fallback_) {
        fallback_->load();
    }
}

bool OllamaEmbeddingService::check_service_available() {
    service_checked_ = true;

#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(5));

    auto res = cli.Get("/api/tags");
    service_available_ = (res != nullptr && res->status == 200);

#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/tags").c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);  // HEAD 请求

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    service_available_ = (res == CURLE_OK && http_code == 200);
#endif

    return service_available_;
}

int OllamaEmbeddingService::detect_dimension() {
    // 根据模型名称推断维度
    if (config_.model.find("nomic") != std::string::npos) {
        return 768;
    } else if (config_.model.find("bge") != std::string::npos) {
        return 1024;
    } else if (config_.model.find("e5") != std::string::npos) {
        return 1024;
    } else if (config_.model.find("gte") != std::string::npos) {
        return 768;
    }
    // 默认 nomic-embed-text 维度
    return 768;
}

// ========== 工厂函数 ==========

std::shared_ptr<EmbeddingService> create_ollama_embedding_service(
    const OllamaEmbeddingConfig& config) {
    return std::make_shared<OllamaEmbeddingService>(config);
}

std::shared_ptr<EmbeddingService> create_ollama_embedding_service(
    const std::string& base_url,
    const std::string& model,
    size_t max_cache_size) {
    return std::make_shared<OllamaEmbeddingService>(base_url, model, max_cache_size);
}

}  // namespace rag
