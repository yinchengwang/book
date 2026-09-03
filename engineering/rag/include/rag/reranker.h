/**
 * @file reranker.h
 * @brief 重排序器 - Cross-Encoder 和轻量级重排序
 */
#pragma once

#include "rag/types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>

namespace rag {

// ========== 重排序结果 ==========

/**
 * @brief 重排序结果
 */
struct RerankResult {
    std::string chunk_id;      // Chunk ID
    std::string content;       // Chunk 内容
    float score = 0.0f;        // 重排分数
    std::string reason;        // 重排原因 (可选)
};

// ========== 重排序器接口 ==========

/**
 * @brief 重排序器接口
 *
 * 在初步检索后对候选结果进行精细排序
 */
class Reranker {
public:
    virtual ~Reranker() = default;

    /**
     * @brief 初始化
     * @param model_path 模型路径或配置
     * @return 是否初始化成功
     */
    virtual bool init(const std::string& model_path) = 0;

    /**
     * @brief 重排序
     * @param query 查询文本
     * @param candidates 候选结果列表
     * @param top_n 返回结果数
     * @return 排序后的结果
     */
    virtual std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) = 0;

    /**
     * @brief 批量重排序
     */
    virtual std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates_list,
        int top_n) = 0;

    /**
     * @brief 获取模型名称
     */
    virtual std::string model_name() const = 0;

    /**
     * @brief 是否就绪
     */
    virtual bool is_ready() const = 0;
};

// ========== 轻量级重排序器 ==========

/**
 * @brief 轻量级重排序器配置
 */
struct LightweightRerankerConfig {
    // 权重配置
    float keyword_weight = 0.4f;
    float bm25_weight = 0.3f;
    float vector_weight = 0.3f;
    float position_weight = 0.05f;
};

/**
 * @brief 轻量级重排序器
 *
 * 基于关键词匹配和 BM25/向量分数融合，无外部依赖
 */
class LightweightReranker : public Reranker {
public:
    /**
     * @brief 默认构造函数
     */
    LightweightReranker();

    /**
     * @brief 带配置的构造函数
     */
    explicit LightweightReranker(const LightweightRerankerConfig& config);

    bool init(const std::string& model_path) override {
        (void)model_path;
        return true;  // 无需初始化
    }

    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) override;

    std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates_list,
        int top_n) override;

    std::string model_name() const override { return "lightweight"; }
    bool is_ready() const override { return true; }

    // ========== 配置方法 ==========

    /// 设置 BM25 分数
    void set_bm25_scores(const std::unordered_map<std::string, float>& scores);

    /// 设置向量相似度分数
    void set_vector_scores(const std::unordered_map<std::string, float>& scores);

    /// 设置权重
    void set_weights(float keyword, float bm25, float vector, float position);

    /// 清空分数缓存
    void clear_scores();

    /// 获取配置
    const LightweightRerankerConfig& config() const { return config_; }

private:
    float compute_score(const std::string& query, const Chunk& chunk);
    float keyword_match_score(const std::string& query, const std::string& content);
    std::vector<std::string> tokenize(const std::string& text);
    std::string to_lower(const std::string& str);

    LightweightRerankerConfig config_;
    std::unordered_map<std::string, float> bm25_scores_;
    std::unordered_map<std::string, float> vector_scores_;
};

// ========== BGE-Reranker (可选) ==========

/**
 * @brief BGE-Reranker 配置
 */
struct BGERerankerConfig {
    std::string model_path;
    std::string device = "cpu";
    int batch_size = 32;
    bool normalize = true;
};

/**
 * @brief BGE-Reranker 重排序器
 *
 * 需要 transformers 库支持
 */
class BGEReranker : public Reranker {
public:
    BGEReranker(const std::string& model_path = "",
                const std::string& device = "cpu");

    ~BGEReranker();

    bool init(const std::string& model_path) override;
    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) override;
    std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates_list,
        int top_n) override;

    std::string model_name() const override { return "bge-reranker"; }
    bool is_ready() const override { return ready_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    bool ready_ = false;
};

// ========== 重排序统计 ==========

/**
 * @brief 重排序器统计信息
 */
struct RerankerStats {
    uint64_t rerank_count = 0;
    uint64_t total_candidates = 0;
    double avg_latency_ms = 0.0;
    size_t memory_usage_bytes = 0;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建重排序器
 * @param type 类型: "lightweight" | "bge"
 * @param model_path 模型路径
 * @return 重排序器智能指针
 */
std::shared_ptr<Reranker> create_reranker(
    const std::string& type,
    const std::string& model_path = "");

}  // namespace rag
