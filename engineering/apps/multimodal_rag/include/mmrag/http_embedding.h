/**
 * @file http_embedding.h
 * @brief HTTP Embedding 服务 - OpenAI 兼容 API
 *
 * 通过 HTTP 调用任意 OpenAI-compatible /v1/embeddings 接口
 * 支持智谱AI、OpenAI、通义千问、本地 vLLM 等
 */
#pragma once

#include "embedding.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace mmrag {

/**
 * @brief HTTP Embedding 服务配置
 */
struct HttpEmbeddingConfig {
    std::string api_url = "https://open.bigmodel.cn";  // API base (scheme + host[:port])
    std::string api_path = "/api/paas/v4/embeddings";  // API endpoint path
    std::string api_key = "";                   // API Key (Bearer token)
    std::string model = "embedding-3";          // 模型名称
    int dimension = 1024;                       // 向量维度
    size_t max_cache_size = 10000;              // 缓存大小
    int timeout_seconds = 30;                   // 请求超时
    int max_retries = 3;                        // 最大重试次数
};

/**
 * @brief HTTP Embedding 服务
 *
 * 通过 HTTP POST 调用 OpenAI-compatible /v1/embeddings 接口。
 * 兼容：智谱AI、OpenAI、通义千问、本地 vLLM、Ollama OpenAI 端点等。
 *
 * 特性：
 * - LRU 缓存减少 API 调用
 * - 服务不可用时自动降级到 SimpleEmbeddingService
 * - 支持批量编码
 */
class HttpEmbeddingService : public EmbeddingService {
public:
    explicit HttpEmbeddingService(const HttpEmbeddingConfig& config);
    ~HttpEmbeddingService() override;

    HttpEmbeddingService(const HttpEmbeddingService&) = delete;
    HttpEmbeddingService& operator=(const HttpEmbeddingService&) = delete;

    // ========== EmbeddingService 接口 ==========
    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;
    int dimension() const override { return dimension_; }
    bool is_ready() const override;
    std::string model_name() const override { return config_.model; }

    // ========== 扩展接口 ==========
    const HttpEmbeddingConfig& config() const { return config_; }

    struct CacheStats {
        size_t size;
        size_t hits;
        size_t misses;
        double hit_rate;
    };
    CacheStats get_cache_stats() const;
    void clear_cache();

private:
    std::vector<float> call_api(const std::string& text);
    std::vector<std::vector<float>> call_api_batch(
        const std::vector<std::string>& texts);
    void enable_fallback();
    bool check_service_available();

    HttpEmbeddingConfig config_;
    int dimension_;
    bool using_fallback_ = false;
    bool service_checked_ = false;
    bool service_available_ = false;

    // LRU 缓存
    struct CacheEntry {
        std::vector<float> embedding;
        size_t access_time;
    };
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable size_t cache_access_counter_ = 0;
    mutable std::mutex cache_mutex_;
    mutable size_t cache_hits_ = 0;
    mutable size_t cache_misses_ = 0;

    std::unique_ptr<SimpleEmbeddingService> fallback_;
};

}  // namespace mmrag
