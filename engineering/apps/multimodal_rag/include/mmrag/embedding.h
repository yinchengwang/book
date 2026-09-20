/**
 * @file embedding.h
 * @brief Embedding 服务接口
 *
 * 定义 Embedding 服务的接口，支持多种实现（Simple/BGE-M3）
 */
#pragma once

#include "mmrag/config.h"
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace mmrag {

// ========== Embedding 服务接口 ==========

/**
 * @brief Embedding 服务接口
 */
class EmbeddingService {
public:
    virtual ~EmbeddingService() = default;

    /**
     * @brief 编码单个文本为向量
     * @param text 输入文本
     * @return 归一化向量，维度由 dimension() 返回
     * @note 失败时返回全零向量
     */
    virtual std::vector<float> encode(const std::string& text) = 0;

    /**
     * @brief 批量编码
     * @param texts 输入文本列表
     * @return 向量列表，顺序与输入一致
     */
    virtual std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) = 0;

    /**
     * @brief 获取向量维度
     */
    virtual int dimension() const = 0;

    /**
     * @brief 是否就绪
     */
    virtual bool is_ready() const = 0;

    /**
     * @brief 获取模型名称
     */
    virtual std::string model_name() const = 0;
};

// ========== Embedding 统计信息 ==========

/**
 * @brief Embedding 服务统计信息
 */
struct EmbeddingStats {
    uint64_t encode_count = 0;        // 编码次数
    uint64_t cache_hits = 0;          // 缓存命中
    uint64_t cache_misses = 0;        // 缓存未命中
    double cache_hit_rate = 0.0;      // 命中率
    double avg_encode_time_ms = 0.0;  // 平均编码时间
};

// ========== 简单 Embedding 服务 (随机向量) ==========

/**
 * @brief 简单 Embedding 服务（随机向量）
 *
 * 用于测试和开发，生产环境应使用 BGE-M3
 */
class SimpleEmbeddingService : public EmbeddingService {
public:
    /**
     * @brief 构造函数
     * @param dimension 向量维度，默认 768
     */
    explicit SimpleEmbeddingService(int dimension = 768);

    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;

    int dimension() const override { return dimension_; }
    bool is_ready() const override { return ready_; }
    std::string model_name() const override { return "simple"; }

    // 模拟加载
    void load() { ready_ = true; }

    // 统计信息
    EmbeddingStats get_stats() const;

private:
    int dimension_;
    bool ready_ = false;
    mutable uint64_t encode_count_ = 0;
};

// ========== BGE-M3 Embedding 服务 ==========

/**
 * @brief BGE-M3 Embedding 服务配置
 */
struct BGEM3Config {
    std::string model_path;       // 模型路径或 HuggingFace 模型名
    std::string device = "cpu";   // cpu 或 cuda
    int batch_size = 32;          // 批处理大小
    bool use_cache = true;        // 是否启用缓存
    size_t max_cache_size = 10000; // 缓存大小
};

/**
 * @brief BGE-M3 Embedding 服务
 *
 * 支持多语言、稠密向量 + 稀疏向量混合
 * 使用 Pimpl 模式隐藏 transformers 依赖
 */
class BGEM3EmbeddingService : public EmbeddingService {
public:
    /**
     * @brief 构造函数
     * @param config BGE-M3 配置
     */
    explicit BGEM3EmbeddingService(const BGEM3Config& config);

    /**
     * @brief 默认构造函数（使用随机向量回退）
     */
    BGEM3EmbeddingService();

    ~BGEM3EmbeddingService();

    // 禁用拷贝
    BGEM3EmbeddingService(const BGEM3EmbeddingService&) = delete;
    BGEM3EmbeddingService& operator=(const BGEM3EmbeddingService&) = delete;

    // 移动语义
    BGEM3EmbeddingService(BGEM3EmbeddingService&&) noexcept;
    BGEM3EmbeddingService& operator=(BGEM3EmbeddingService&&) noexcept;

    // 基础接口
    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;

    int dimension() const override;
    bool is_ready() const override { return ready_; }
    std::string model_name() const override { return "bge-m3"; }

    // ========== 扩展接口 ==========

    /**
     * @brief 稀疏向量
     */
    struct SparseVector {
        std::vector<uint32_t> indices;  // token ID
        std::vector<float> values;       // 权重
    };

    /**
     * @brief 稀疏向量编码
     * @note 用于混合检索
     */
    SparseVector encode_sparse(const std::string& text);

    /**
     * @brief 带详细信息的编码
     */
    struct EmbeddingResult {
        std::vector<float> dense;    // 稠密向量
        SparseVector sparse;         // 稀疏向量
    };

    EmbeddingResult encode_with_info(const std::string& text);

    // 统计和缓存管理
    EmbeddingStats get_stats() const;
    void clear_cache();
    size_t cache_size() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;

    // 用于回退的简单服务
    std::unique_ptr<SimpleEmbeddingService> fallback_;
};

// ========== Kbase (GGUF MiniLM-L6) Embedding 服务 ==========

/**
 * @brief Kbase Embedding 服务配置
 */
struct KbaseEmbeddingConfig {
    std::string model_path;       // GGUF 模型路径（MiniLM-L6 q4_k_m）
    int dimension = 384;          // MiniLM-L6 输出维度（默认 384）
};

/**
 * @brief Kbase GGUF MiniLM-L6 Embedding 服务
 *
 * 使用 kbase 真实推理（WordPiece tokenizer + MiniLM-L6 encoder + L2 norm）。
 * 失败时 is_ready() 返回 false，encode 返回空向量。
 */
class KbaseEmbeddingService : public EmbeddingService {
public:
    /**
     * @brief 构造函数（接受模型路径）
     * @param model_path GGUF 模型路径
     * @param dimension 期望维度（默认 384）
     */
    KbaseEmbeddingService(const std::string& model_path, int dimension = 384);

    /**
     * @brief 构造函数（接受配置）
     */
    explicit KbaseEmbeddingService(const KbaseEmbeddingConfig& config);

    ~KbaseEmbeddingService() override;

    // 禁用拷贝
    KbaseEmbeddingService(const KbaseEmbeddingService&) = delete;
    KbaseEmbeddingService& operator=(const KbaseEmbeddingService&) = delete;

    std::vector<float> encode(const std::string& text) override;
    std::vector<std::vector<float>> encode_batch(
        const std::vector<std::string>& texts) override;

    int dimension() const override { return dimension_; }
    bool is_ready() const override { return ready_; }
    std::string model_name() const override { return "kbase-minilm-l6"; }

    // 获取模型路径
    const std::string& model_path() const { return model_path_; }

    // 统计信息
    EmbeddingStats get_stats() const;

private:
    // 内部实现（Pimpl 隐藏 C 结构体）
    struct Impl;
    std::unique_ptr<Impl> impl_;

    std::string model_path_;
    int dimension_ = 384;
    bool ready_ = false;
};

// ========== Embedding 配置 (已在 config.h 中定义) ==========

// 使用 mmrag::EmbeddingConfig from rag/config.h

// ========== 创建函数 (声明) ==========

std::shared_ptr<EmbeddingService> create_embedding_service(
    const std::string& type,
    const std::string& model_path = "",
    int dimension = 768);

/**
 * @brief 创建 Embedding 服务（完整配置版本）
 *
 * 支持从 EmbeddingConfig 读取 HTTP API 配置（api_url, api_key 等），
 * 适用于 http 类型的 Embedding 服务。
 */
std::shared_ptr<EmbeddingService> create_embedding_service(
    const EmbeddingConfig& config);

// ========== 向量工具函数 ==========

/**
 * @brief 计算余弦相似度
 */
float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

/**
 * @brief L2 归一化向量
 */
void normalize_l2(std::vector<float>& vec);

/**
 * @brief 计算向量范数
 */
float vector_norm(const std::vector<float>& vec);

}  // namespace mmrag
