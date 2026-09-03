/**
 * @file retry.cpp
 * @brief 重试策略实现
 */

#include "rag/retry.h"
#include <algorithm>

namespace rag {

// ========== RetryConfig 实现 ==========

bool RetryConfig::should_retry(const std::string& error_code) const {
    if (retryable_codes.empty()) {
        // 默认只重试临时性错误
        return error_code == "RAG-100-001" ||  // 内部错误
               error_code == "RAG-300-004" ||  // 推理失败
               error_code == "RAG-500-001" ||  // 检索失败
               error_code == "RAG-600-002";    // 生成失败
    }
    return std::find(retryable_codes.begin(), retryable_codes.end(), error_code) != retryable_codes.end();
}

bool RetryConfig::should_retry(const RAGException& e) const {
    return should_retry(e.code());
}

// ========== RetryHandler 实现 ==========

RetryHandler::RetryHandler(const RetryConfig& config) : config_(config) {}

std::chrono::milliseconds RetryHandler::calculate_next_delay(int attempt) {
    int delay_ms;

    switch (config_.backoff) {
        case BackoffStrategy::LINEAR:
            delay_ms = config_.initial_delay_ms * attempt;
            break;

        case BackoffStrategy::EXPONENTIAL:
            delay_ms = config_.initial_delay_ms * (1 << (attempt - 1));  // 2^(n-1)
            break;

        case BackoffStrategy::FIBONACCI: {
            // 斐波那契数列
            int fib_n_2 = 1;  // fib(1)
            int fib_n_1 = 1;  // fib(2)
            for (int i = 3; i <= attempt; ++i) {
                int fib_n = fib_n_1 + fib_n_2;
                fib_n_2 = fib_n_1;
                fib_n_1 = fib_n;
            }
            delay_ms = config_.initial_delay_ms * fib_n_1;
            break;
        }
    }

    return std::chrono::milliseconds(std::min(delay_ms, config_.max_delay_ms));
}

}  // namespace rag
