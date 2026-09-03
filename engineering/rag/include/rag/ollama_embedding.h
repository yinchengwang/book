/**
 * @file ollama_embedding.h
 * @brief Ollama Embedding 服务
 *
 * 通过 Ollama API 实现真实的语义向量编码，支持缓存和降级
 */
#pragma once

#include "embedding.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <memory>

namespace rag {

/**
 * @brief Ollama Embedding 服务配置
 */
struct OllamaEmbeddingConfig {
    std::string base_url = "http://localhost:11434";  // Ollama 服务地址
    std::string model = "nomic-embed-text";           // Embedding 模型
    size_t max_cache_size = 10000;                    // 缓存大小
    int timeout_seconds = 30;                         // 请求超时
    int max_retries = 3;                              // 最大重试次数
};

/**
 * @brief Ollama Embedding 服务
 *
 * 通过 Ollama API 调用本地 Embedding 模型（如 nomic-embed-text）
 * 实现真实的语义向量编码，而非随机向量。
 *
 * 特性：
 * - LRU 缓存减少 API 调用
 * - 服务不可用时自动降级到 SimpleEmbeddingService
 * - 批量编码优化
 */
class OllamaEmbeddingService : public EmbeddingService {
public:
    /**
     * @brief 构造函数
     * @param config Ollama 配置
     */
    explicit OllamaEmbeddingService(const OllamaEmbeddingConfig& config);

    /**
     * @brief 简略构造函数
     * @param base_url Ollama 服务地址
     * @param model 模型名称
     * @param max_cache_size 缓存大小
     */
    OllamaEmbeddingService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "nomic-embed-text",
        size_t max_cache_size = 10000);

    ~OllamaEmbeddingService() override;

    // 禁用拷贝
    OllamaEmbeddingService(const OllamaEmbeddingService&) = delete;
    OllamaEmbeddingService& operator=(const OllamaEmbeddingService&) = delete;

    // ========== EmbeddingService 接口 ==========

    /**
     * @brief 编码单个文本
     * @param text 输入文本
     * @return 归一化向量
     *
     * 流程：
     * 1. 检查缓存
     * 2. 缓存未命中则调用 Ollama API
     * 3. 结果存入缓存并返回
     */
    std::vector<float> encode(const std::string& text) override;

    /**
     * @brief 批量编码
     * @param texts 输入文本列表
     * @return 向量列表
     *
     * 优化：批量调用减少网络往返
     */
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;

    /**
     * @brief 获取向量维度
     */
    int dimension() const override { return dimension_; }

    /**
     * @brief 检查服务是否就绪
     * @return Ollama 服务可用返回 true
     */
    bool is_ready() const override;

    /**
     * @brief 获取模型名称
     */
    std::string model_name() const override { return config_.model; }

    // ========== 扩展接口 ==========

    /**
     * @brief 获取配置
     */
    const OllamaEmbeddingConfig& config() const { return config_; }

    /**
     * @brief 获取缓存统计
     */
    struct CacheStats {
        size_t size;
        size_t hits;
        size_t misses;
        double hit_rate;
    };
    CacheStats get_cache_stats() const;

    /**
     * @brief 清空缓存
     */
    void clear_cache();

    /**
     * @brief 缓存大小
     */
    size_t cache_size() const { return cache_.size(); }

    /**
     * @brief 获取底层 Simple 服务（用于降级）
     */
    SimpleEmbeddingService* fallback() const { return fallback_.get(); }

    /**
     * @brief 是否使用降级服务
     */
    bool using_fallback() const { return using_fallback_; }

private:
    /**
     * @brief 调用 Ollama API 编码单个文本
     */
    std::vector<float> call_api(const std::string& text);

    /**
     * @brief 调用 Ollama API 批量编码
     */
    std::vector<std::vector<float>> call_api_batch(
        const std::vector<std::string>& texts);

    /**
     * @brief 启用降级模式
     */
    void enable_fallback();

    /**
     * @brief 检测服务可用性
     */
    bool check_service_available();

    /**
     * @brief 获取模型维度
     */
    int detect_dimension();

    // 配置
    OllamaEmbeddingConfig config_;

    // 状态
    int dimension_ = 768;                    // 向量维度
    bool using_fallback_ = false;            // 是否使用降级服务
    bool service_checked_ = false;           // 是否已检查服务
    bool service_available_ = false;         // 服务是否可用

    // 缓存（LRU）
    struct CacheEntry {
        std::vector<float> embedding;
        size_t access_time;  // 简化的时间戳
    };
    std::unordered_map<std::string, CacheEntry> cache_;
    mutable size_t cache_access_counter_ = 0;
    mutable std::mutex cache_mutex_;

    // 缓存统计
    mutable size_t cache_hits_ = 0;
    mutable size_t cache_misses_ = 0;

    // 降级服务
    std::unique_ptr<SimpleEmbeddingService> fallback_;
};

// ========== 创建函数 ==========

/**
 * @brief 创建 Ollama Embedding 服务
 * @param config 配置
 * @return 服务智能指针
 */
std::shared_ptr<EmbeddingService> create_ollama_embedding_service(
    const OllamaEmbeddingConfig& config);

/**
 * @brief 创建 Ollama Embedding 服务（简略版本）
 */
std::shared_ptr<EmbeddingService> create_ollama_embedding_service(
    const std::string& base_url = "http://localhost:11434",
    const std::string& model = "nomic-embed-text",
    size_t max_cache_size = 10000);

}  // namespace rag
