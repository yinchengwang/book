/**
 * @file retry.h
 * @brief 重试策略 - 指数退避、可重试错误判断
 */
#pragma once

#include "rag/error.h"
#include <functional>
#include <chrono>
#include <vector>
#include <string>
#include <thread>

namespace rag {

// ========== 退避策略 ==========

/**
 * @brief 退避策略类型
 */
enum class BackoffStrategy {
    LINEAR,       // 线性退避: delay = initial_delay * attempt
    EXPONENTIAL,  // 指数退避: delay = initial_delay * (2 ^ attempt)
    FIBONACCI     // 斐波那契退避: delay = fib(attempt) * initial_delay
};

/**
 * @brief 重试配置
 */
struct RetryConfig {
    int max_attempts = 3;              // 最大重试次数
    int initial_delay_ms = 1000;       // 初始延迟（毫秒）
    int max_delay_ms = 10000;          // 最大延迟（毫秒）
    BackoffStrategy backoff = BackoffStrategy::EXPONENTIAL;
    std::vector<std::string> retryable_codes;  // 可重试的错误码

    // 检查错误是否可重试
    bool should_retry(const std::string& error_code) const;
    bool should_retry(const RAGException& e) const;
};

// ========== 重试处理器 ==========

/**
 * @brief 重试处理器
 */
class RetryHandler {
public:
    explicit RetryHandler(const RetryConfig& config);

    // 执行带重试的操作
    template<typename T>
    T execute(std::function<T()> operation) {
        int attempt = 0;
        std::chrono::milliseconds delay(config_.initial_delay_ms);

        while (true) {
            try {
                return operation();
            } catch (const RAGException& e) {
                attempt++;
                if (attempt >= config_.max_attempts || !config_.should_retry(e)) {
                    throw;
                }
                std::this_thread::sleep_for(delay);
                delay = calculate_next_delay(attempt);
            }
        }
    }

    // 执行带重试的操作（无返回值）
    void execute_void(std::function<void()> operation) {
        execute<void*>([&]() {
            operation();
            return nullptr;
        });
    }

    // 获取配置
    const RetryConfig& config() const { return config_; }

private:
    std::chrono::milliseconds calculate_next_delay(int attempt);

    RetryConfig config_;
};

// ========== 便捷函数 ==========

/**
 * @brief 创建默认重试配置
 */
inline RetryConfig default_retry_config() {
    RetryConfig config;
    config.max_attempts = 3;
    config.initial_delay_ms = 1000;
    config.max_delay_ms = 10000;
    config.backoff = BackoffStrategy::EXPONENTIAL;
    config.retryable_codes = {
        "RAG-100-001",  // 内部错误
        "RAG-300-004",  // 推理失败
        "RAG-500-001",  // 检索失败
        "RAG-600-002"   // 生成失败
    };
    return config;
}

}  // namespace rag
