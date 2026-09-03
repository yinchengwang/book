#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

#include "rag/pipeline.h"

namespace rag {

// ========== Forward Declarations ==========

class RetrievalPipeline;

// ========== 降级级别 ==========

enum class FallbackLevel {
    NONE,              // 正常，无降级
    SKIP_RERANK,       // 跳过 Reranker
    SKIP_EXPANSION,     // 跳过 Query Expansion
    SINGLE_RETRIEVER,   // 只用单一检索器
    BM25_ONLY,         // 只用 BM25
    STATIC_RESPONSE    // 返回预设回答
};

// ========== 降级配置 ==========

struct FallbackConfig {
    bool enable = true;
    bool auto_fallback = true;

    // 降级触发条件
    double latency_threshold_ms = 2000;
    double error_rate_threshold = 0.1;
    int consecutive_errors_threshold = 5;

    // 降级顺序
    std::vector<FallbackLevel> fallback_order = {
        FallbackLevel::SKIP_RERANK,
        FallbackLevel::SKIP_EXPANSION,
        FallbackLevel::SINGLE_RETRIEVER,
        FallbackLevel::BM25_ONLY,
        FallbackLevel::STATIC_RESPONSE
    };

    // 预设回答
    std::unordered_map<std::string, std::string> static_responses;
};

// ========== 降级决策 ==========

struct FallbackDecision {
    FallbackLevel level = FallbackLevel::NONE;
    std::string reason;
    double confidence = 1.0;
};

// ========== 降级管理器 ==========

class FallbackManager {
public:
    explicit FallbackManager(const FallbackConfig& config);

    // 决定降级级别
    FallbackDecision decide_fallback(
        double current_latency_ms,
        double error_rate,
        int consecutive_errors,
        bool llm_available,
        bool gpu_available,
        bool reranker_available);

    // 执行降级
    PipelineResult execute_with_fallback(
        const std::string& query,
        int top_k,
        FallbackLevel level);

    // 获取当前降级级别
    FallbackLevel current_level() const { return current_level_; }

    // 重置
    void reset();

    // 添加预设回答
    void add_static_response(const std::string& pattern, const std::string& response);

    // 获取降级统计
    struct Stats {
        int fallback_count = 0;
        std::unordered_map<std::string, int> fallback_by_reason;
    };
    Stats get_stats() const;

private:
    FallbackConfig config_;
    FallbackLevel current_level_ = FallbackLevel::NONE;
    Stats stats_;
};

// ========== 降级 Pipeline ==========

class FallbackPipeline {
public:
    FallbackPipeline(
        std::shared_ptr<RetrievalPipeline> normal_pipeline,
        std::shared_ptr<RetrievalPipeline> skip_rerank_pipeline,
        std::shared_ptr<RetrievalPipeline> skip_expansion_pipeline,
        std::shared_ptr<RetrievalPipeline> single_retriever_pipeline,
        std::shared_ptr<RetrievalPipeline> bm25_only_pipeline,
        const FallbackConfig& config);

    // 执行（带自动降级）
    PipelineResult execute(const std::string& query, int top_k);

    // 手动指定降级级别
    PipelineResult execute_with_level(const std::string& query, int top_k, FallbackLevel level);

private:
    std::shared_ptr<RetrievalPipeline> normal_pipeline_;
    std::shared_ptr<RetrievalPipeline> skip_rerank_pipeline_;
    std::shared_ptr<RetrievalPipeline> skip_expansion_pipeline_;
    std::shared_ptr<RetrievalPipeline> single_retriever_pipeline_;
    std::shared_ptr<RetrievalPipeline> bm25_only_pipeline_;
    std::shared_ptr<FallbackManager> fallback_manager_;
};

// ========== 工厂函数 ==========

std::unique_ptr<FallbackManager> create_fallback_manager(const FallbackConfig& config);
std::unique_ptr<FallbackPipeline> create_fallback_pipeline(
    std::shared_ptr<RetrievalPipeline> normal_pipeline,
    const FallbackConfig& config);

}  // namespace rag