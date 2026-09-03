/**
 * @file fallback.cpp
 * @brief 降级策略和故障恢复实现
 */

#include "rag/fallback.h"
#include "rag/pipeline.h"
#include <algorithm>
#include <chrono>

namespace rag {

// ========== 默认预设回答 ==========

static std::unordered_map<std::string, std::string> get_default_static_responses() {
    return {
        {"你好", "你好！有什么可以帮助你的吗？"},
        {"谢谢", "不客气！"},
        {"再见", "再见，有问题随时问我！"}
    };
}

// ========== FallbackManager ==========

FallbackManager::FallbackManager(const FallbackConfig& config)
    : config_(config), current_level_(FallbackLevel::NONE) {
    // 合并默认预设回答
    for (const auto& [pattern, response] : get_default_static_responses()) {
        if (config_.static_responses.find(pattern) == config_.static_responses.end()) {
            config_.static_responses[pattern] = response;
        }
    }
}

FallbackDecision FallbackManager::decide_fallback(
    double current_latency_ms,
    double error_rate,
    int consecutive_errors,
    bool llm_available,
    bool gpu_available,
    bool reranker_available) {

    FallbackDecision decision;
    decision.level = FallbackLevel::NONE;

    // 1. 连续错误 > threshold → 直接 STATIC_RESPONSE
    if (consecutive_errors >= config_.consecutive_errors_threshold) {
        decision.level = FallbackLevel::STATIC_RESPONSE;
        decision.reason = "consecutive_errors";
        decision.confidence = 1.0;
        stats_.fallback_count++;
        stats_.fallback_by_reason[decision.reason]++;
        return decision;
    }

    // 2. LLM 不可用 → STATIC_RESPONSE
    if (!llm_available) {
        decision.level = FallbackLevel::STATIC_RESPONSE;
        decision.reason = "llm_unavailable";
        decision.confidence = 1.0;
        stats_.fallback_count++;
        stats_.fallback_by_reason[decision.reason]++;
        return decision;
    }

    // 3. GPU 不可用 → 跳过 rerank
    if (!gpu_available && reranker_available) {
        decision.level = FallbackLevel::SKIP_RERANK;
        decision.reason = "gpu_unavailable";
        decision.confidence = 0.9;
        stats_.fallback_count++;
        stats_.fallback_by_reason[decision.reason]++;
        return decision;
    }

    // 4. 错误率 > threshold → 降一级
    if (error_rate > config_.error_rate_threshold) {
        // 根据当前级别降级
        if (current_level_ == FallbackLevel::NONE) {
            decision.level = FallbackLevel::SKIP_RERANK;
        } else if (current_level_ == FallbackLevel::SKIP_RERANK) {
            decision.level = FallbackLevel::SKIP_EXPANSION;
        } else if (current_level_ == FallbackLevel::SKIP_EXPANSION) {
            decision.level = FallbackLevel::SINGLE_RETRIEVER;
        } else if (current_level_ == FallbackLevel::SINGLE_RETRIEVER) {
            decision.level = FallbackLevel::BM25_ONLY;
        } else {
            decision.level = FallbackLevel::STATIC_RESPONSE;
        }
        decision.reason = "high_error_rate";
        decision.confidence = 0.8;
        stats_.fallback_count++;
        stats_.fallback_by_reason[decision.reason]++;
        return decision;
    }

    // 5. 延迟 > threshold → 逐级降级
    if (current_latency_ms > config_.latency_threshold_ms) {
        // 根据当前级别降级
        if (current_level_ == FallbackLevel::NONE) {
            decision.level = FallbackLevel::SKIP_RERANK;
        } else if (current_level_ == FallbackLevel::SKIP_RERANK) {
            decision.level = FallbackLevel::SKIP_EXPANSION;
        } else if (current_level_ == FallbackLevel::SKIP_EXPANSION) {
            decision.level = FallbackLevel::SINGLE_RETRIEVER;
        } else if (current_level_ == FallbackLevel::SINGLE_RETRIEVER) {
            decision.level = FallbackLevel::BM25_ONLY;
        } else {
            decision.level = FallbackLevel::STATIC_RESPONSE;
        }
        decision.reason = "high_latency";
        decision.confidence = 0.7;
        stats_.fallback_count++;
        stats_.fallback_by_reason[decision.reason]++;
        return decision;
    }

    return decision;
}

PipelineResult FallbackManager::execute_with_fallback(
    const std::string& query,
    int top_k,
    FallbackLevel level) {

    PipelineResult result;

    // STATIC_RESPONSE - 返回预设回答
    if (level == FallbackLevel::STATIC_RESPONSE) {
        result.success = true;
        result.answer = "";
        result.confidence = 1.0;

        // 查找匹配的预设回答
        for (const auto& [pattern, response] : config_.static_responses) {
            if (query.find(pattern) != std::string::npos) {
                result.answer = response;
                return result;
            }
        }

        // 默认回答
        result.answer = "抱歉，我现在无法回答这个问题。";
        return result;
    }

    // 其他级别需要 pipeline，但这里我们无法访问具体的 pipeline
    // 返回失败状态，由 FallbackPipeline 使用对应的 pipeline
    result.success = false;
    result.error_message = "Fallback level requires pipeline execution";
    return result;
}

void FallbackManager::reset() {
    current_level_ = FallbackLevel::NONE;
}

void FallbackManager::add_static_response(const std::string& pattern, const std::string& response) {
    config_.static_responses[pattern] = response;
}

FallbackManager::Stats FallbackManager::get_stats() const {
    return stats_;
}

// ========== FallbackPipeline ==========

FallbackPipeline::FallbackPipeline(
    std::shared_ptr<RetrievalPipeline> normal_pipeline,
    std::shared_ptr<RetrievalPipeline> skip_rerank_pipeline,
    std::shared_ptr<RetrievalPipeline> skip_expansion_pipeline,
    std::shared_ptr<RetrievalPipeline> single_retriever_pipeline,
    std::shared_ptr<RetrievalPipeline> bm25_only_pipeline,
    const FallbackConfig& config)
    : normal_pipeline_(normal_pipeline)
    , skip_rerank_pipeline_(skip_rerank_pipeline)
    , skip_expansion_pipeline_(skip_expansion_pipeline)
    , single_retriever_pipeline_(single_retriever_pipeline)
    , bm25_only_pipeline_(bm25_only_pipeline)
    , fallback_manager_(std::make_shared<FallbackManager>(config)) {
}

PipelineResult FallbackPipeline::execute(const std::string& query, int top_k) {
    // 尝试正常执行
    if (normal_pipeline_) {
        auto result = normal_pipeline_->execute(query, top_k);
        if (result.success && fallback_manager_->current_level() == FallbackLevel::NONE) {
            return result;
        }
    }

    // 降级执行
    auto decision = fallback_manager_->decide_fallback(
        0.0, 0.0, 0, true, true, true);

    return execute_with_level(query, top_k, decision.level);
}

PipelineResult FallbackPipeline::execute_with_level(
    const std::string& query,
    int top_k,
    FallbackLevel level) {

    PipelineResult result;
    auto start = std::chrono::steady_clock::now();

    switch (level) {
        case FallbackLevel::NONE:
            if (normal_pipeline_) {
                result = normal_pipeline_->execute(query, top_k);
            }
            break;

        case FallbackLevel::SKIP_RERANK:
            if (skip_rerank_pipeline_) {
                result = skip_rerank_pipeline_->execute(query, top_k);
            }
            break;

        case FallbackLevel::SKIP_EXPANSION:
            if (skip_expansion_pipeline_) {
                result = skip_expansion_pipeline_->execute(query, top_k);
            }
            break;

        case FallbackLevel::SINGLE_RETRIEVER:
            if (single_retriever_pipeline_) {
                result = single_retriever_pipeline_->execute(query, top_k);
            }
            break;

        case FallbackLevel::BM25_ONLY:
            if (bm25_only_pipeline_) {
                result = bm25_only_pipeline_->execute(query, top_k);
            }
            break;

        case FallbackLevel::STATIC_RESPONSE:
            result = fallback_manager_->execute_with_fallback(query, top_k, level);
            break;
    }

    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

// ========== 工厂函数 ==========

std::unique_ptr<FallbackManager> create_fallback_manager(const FallbackConfig& config) {
    return std::make_unique<FallbackManager>(config);
}

std::unique_ptr<FallbackPipeline> create_fallback_pipeline(
    std::shared_ptr<RetrievalPipeline> normal_pipeline,
    const FallbackConfig& config) {
    return std::make_unique<FallbackPipeline>(
        normal_pipeline,
        nullptr,  // skip_rerank_pipeline
        nullptr,  // skip_expansion_pipeline
        nullptr,  // single_retriever_pipeline
        nullptr,  // bm25_only_pipeline
        config);
}

}  // namespace rag