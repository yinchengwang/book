/**
 * @file enhanced_retriever.h
 * @brief 增强检索器 - 集成重排序和 MMR 多样性
 */
#pragma once

#include "rag/retriever.h"
#include "rag/reranker.h"
#include "rag/embedding.h"
#include <memory>
#include <atomic>

namespace rag {

// ========== 增强检索配置 ==========

/**
 * @brief 增强检索配置
 */
struct EnhancedRetrievalConfig : public RetrievalConfig {
    // 重排序配置
    bool enable_rerank = true;
    std::string reranker_type = "lightweight";  // "lightweight" | "bge"
    std::string reranker_model;

    // 召回数量 (重排前)
    int recall_top_k = 50;

    // 返回数量 (重排后)
    int return_top_k = 10;

    // MMR 多样性配置
    bool enable_mmr = false;
    float mmr_lambda = 0.7f;  // 0=只看相关性, 1=只看多样性

    // 查询扩展
    bool enable_query_expansion = false;
    std::string expansion_method = "hyde";  // "hyde" | "synonym"
};

// ========== 增强检索器 ==========

/**
 * @brief 增强检索器
 *
 * 在 HybridRetriever 基础上增加:
 * - 重排序 (Cross-Encoder 或 Lightweight)
 * - MMR 多样性
 * - 查询扩展 (可选)
 */
class EnhancedRetriever : public Retriever {
public:
    /**
     * @brief 构造函数
     * @param base_retriever 基础检索器
     * @param reranker 重排序器
     * @param embed_service Embedding 服务 (用于 MMR)
     * @param config 配置
     */
    EnhancedRetriever(
        std::shared_ptr<Retriever> base_retriever,
        std::shared_ptr<Reranker> reranker,
        std::shared_ptr<EmbeddingService> embed_service,
        const EnhancedRetrievalConfig& config);

    ~EnhancedRetriever() = default;

    // ========== Retriever 接口 ==========

    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;

    std::string name() const override { return "enhanced"; }
    const RetrievalConfig& config() const override { return config_; }

    // ========== 配置方法 ==========

    /// 设置配置
    void set_config(const EnhancedRetrievalConfig& config) { config_ = config; }

    /// 获取基础检索器
    std::shared_ptr<Retriever> base_retriever() { return base_retriever_; }

    /// 获取重排序器
    std::shared_ptr<Reranker> reranker() { return reranker_; }

    // ========== 统计信息 ==========

    /**
     * @brief 检索统计
     */
    struct Stats {
        uint64_t total_queries = 0;
        uint64_t rerank_count = 0;
        uint64_t mmr_count = 0;
        double avg_recall_time_ms = 0.0;
        double avg_rerank_time_ms = 0.0;
    };

    Stats get_stats() const;

private:
    // MMR 多样性选择
    std::vector<RetrievalResult> select_with_mmr(
        const std::vector<RetrievalResult>& candidates,
        const std::string& query,
        int top_k);

    // 提取 Chunks
    std::vector<Chunk> extract_chunks(
        const std::vector<RetrievalResult>& results);

    // 计算余弦相似度
    float cosine_similarity(
        const std::vector<float>& a,
        const std::vector<float>& b);

    std::shared_ptr<Retriever> base_retriever_;
    std::shared_ptr<Reranker> reranker_;
    std::shared_ptr<EmbeddingService> embed_service_;
    EnhancedRetrievalConfig config_;

    // 统计
    mutable std::atomic<uint64_t> total_queries_{0};
    mutable std::atomic<uint64_t> rerank_count_{0};
    mutable std::atomic<uint64_t> mmr_count_{0};
};

// ========== 工厂函数 ==========

/**
 * @brief 创建增强检索器
 */
std::shared_ptr<EnhancedRetriever> create_enhanced_retriever(
    std::shared_ptr<Retriever> base_retriever,
    const EnhancedRetrievalConfig& config,
    std::shared_ptr<EmbeddingService> embed_service = nullptr);

}  // namespace rag
