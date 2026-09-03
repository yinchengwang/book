/**
 * @file test_fallback.cpp
 * @brief 降级策略模块测试
 */

#include <gtest/gtest.h>
#include "rag/fallback.h"
#include "rag/pipeline.h"

using namespace rag;

TEST(FallbackDecisionNone, NoFallbackNeeded) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;
    config.error_rate_threshold = 0.1;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        100.0,   // low latency
        0.01,    // low error rate
        0,       // no consecutive errors
        true,    // LLM available
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::NONE);
    EXPECT_EQ(decision.reason, "");
}

TEST(FallbackDecisionLatency, LatencyTriggerFallback) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        3000.0,  // high latency > threshold
        0.01,    // low error rate
        0,       // no consecutive errors
        true,    // LLM available
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::SKIP_RERANK);
    EXPECT_EQ(decision.reason, "high_latency");
}

TEST(FallbackDecisionError, ErrorTriggerFallback) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;
    config.error_rate_threshold = 0.1;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        100.0,   // low latency
        0.2,     // high error rate > threshold
        0,       // no consecutive errors
        true,    // LLM available
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::SKIP_RERANK);
    EXPECT_EQ(decision.reason, "high_error_rate");
}

TEST(SkipRerank, GpuUnavailable) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;
    config.error_rate_threshold = 0.1;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        100.0,   // low latency
        0.01,    // low error rate
        0,       // no consecutive errors
        true,    // LLM available
        false,   // GPU unavailable
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::SKIP_RERANK);
    EXPECT_EQ(decision.reason, "gpu_unavailable");
}

TEST(SkipExpansion, HighLatencyAfterSkipRerank) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;
    config.error_rate_threshold = 0.1;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    // First fallback to SKIP_RERANK
    manager.decide_fallback(3000.0, 0.01, 0, true, true, true);
    EXPECT_EQ(manager.current_level(), FallbackLevel::SKIP_RERANK);

    // Second fallback - should go to SKIP_EXPANSION
    auto decision = manager.decide_fallback(
        3000.0,  // high latency
        0.01,    // low error rate
        0,       // no consecutive errors
        true,    // LLM available
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::SKIP_EXPANSION);
}

TEST(StaticResponse, ConsecutiveErrors) {
    FallbackConfig config;
    config.enable = true;
    config.consecutive_errors_threshold = 5;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        100.0,   // low latency
        0.01,    // low error rate
        5,       // consecutive errors >= threshold
        true,    // LLM available
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::STATIC_RESPONSE);
    EXPECT_EQ(decision.reason, "consecutive_errors");
}

TEST(StaticResponse, LlmUnavailable) {
    FallbackConfig config;
    config.enable = true;

    FallbackManager manager(config);

    auto decision = manager.decide_fallback(
        100.0,   // low latency
        0.01,    // low error rate
        0,       // no consecutive errors
        false,   // LLM unavailable
        true,    // GPU available
        true     // reranker available
    );

    EXPECT_EQ(decision.level, FallbackLevel::STATIC_RESPONSE);
    EXPECT_EQ(decision.reason, "llm_unavailable");
}

TEST(StaticResponse, ExecuteStaticResponse) {
    FallbackConfig config;
    config.enable = true;

    FallbackManager manager(config);

    // Test default static response for "你好"
    auto result = manager.execute_with_fallback("你好", 5, FallbackLevel::STATIC_RESPONSE);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.answer, "你好！有什么可以帮助你的吗？");
}

TEST(AddStaticResponse, CustomStaticResponse) {
    FallbackConfig config;
    config.enable = true;

    FallbackManager manager(config);

    // Add custom static response
    manager.add_static_response("天气", "今天天气很好！");

    auto result = manager.execute_with_fallback("天气怎么样？", 5, FallbackLevel::STATIC_RESPONSE);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.answer, "今天天气很好！");
}

TEST(AddStaticResponse, FallbackStats) {
    FallbackConfig config;
    config.enable = true;
    config.latency_threshold_ms = 2000;

    FallbackManager manager(config);

    // Trigger a few fallbacks
    manager.decide_fallback(3000.0, 0.01, 0, true, true, true);  // latency
    manager.decide_fallback(3000.0, 0.01, 0, true, true, true);  // latency
    manager.decide_fallback(100.0, 0.2, 0, true, true, true);    // error rate

    auto stats = manager.get_stats();
    EXPECT_EQ(stats.fallback_count, 3);
    EXPECT_EQ(stats.fallback_by_reason["high_latency"], 2);
    EXPECT_EQ(stats.fallback_by_reason["high_error_rate"], 1);
}

TEST(Reset, ResetFallbackLevel) {
    FallbackConfig config;
    config.enable = true;

    FallbackManager manager(config);

    // Trigger a fallback
    manager.decide_fallback(3000.0, 0.01, 0, true, true, true);
    EXPECT_NE(manager.current_level(), FallbackLevel::NONE);

    // Reset
    manager.reset();
    EXPECT_EQ(manager.current_level(), FallbackLevel::NONE);
}