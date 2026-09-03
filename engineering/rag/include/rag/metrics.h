/**
 * @file metrics.h
 * @brief 指标收集 - Prometheus 指标和健康检查
 */
#pragma once

#include "rag/types.h"
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>
#include <mutex>

namespace rag {

// ========== 指标类型 ==========

/**
 * @brief 指标类型
 */
enum class MetricType {
    COUNTER,    // 计数器
    GAUGE,      // 仪表
    HISTOGRAM,  // 直方图
    SUMMARY     // 摘要
};

// ========== 指标定义 ==========

/**
 * @brief 指标定义
 */
struct MetricDef {
    std::string name;
    std::string description;
    std::string unit;
    MetricType type;
    std::map<std::string, std::string> labels;
};

// ========== 指标值 ==========

/**
 * @brief 指标值
 */
struct MetricValue {
    std::string name;
    double value = 0;
    std::map<std::string, std::string> labels;
};

// ========== 指标收集器 ==========

/**
 * @brief 指标收集器接口
 */
class MetricsCollector {
public:
    virtual ~MetricsCollector() = default;

    // ========== 计数器 ==========

    // 增加计数器
    virtual void inc_counter(const std::string& name,
                             const std::map<std::string, std::string>& labels = {}) = 0;

    // 增加计数器（带值）
    virtual void inc_counter_by(const std::string& name, double value,
                                const std::map<std::string, std::string>& labels = {}) = 0;

    // ========== 仪表 ==========

    // 设置仪表值
    virtual void set_gauge(const std::string& name, double value,
                           const std::map<std::string, std::string>& labels = {}) = 0;

    // ========== 直方图 ==========

    // 观察值
    virtual void observe_histogram(const std::string& name, double value,
                                   const std::map<std::string, std::string>& labels = {}) = 0;

    // ========== 查询 ==========

    // 获取所有指标
    virtual std::vector<MetricValue> collect() = 0;

    // 获取 Prometheus 格式输出
    virtual std::string to_prometheus_format() = 0;
};

// ========== 内存指标收集器 ==========

/**
 * @brief 基于内存的指标收集器
 */
class InMemoryMetricsCollector : public MetricsCollector {
public:
    InMemoryMetricsCollector();
    ~InMemoryMetricsCollector() override = default;

    void inc_counter(const std::string& name,
                     const std::map<std::string, std::string>& labels = {}) override;
    void inc_counter_by(const std::string& name, double value,
                        const std::map<std::string, std::string>& labels = {}) override;
    void set_gauge(const std::string& name, double value,
                   const std::map<std::string, std::string>& labels = {}) override;
    void observe_histogram(const std::string& name, double value,
                           const std::map<std::string, std::string>& labels = {}) override;

    std::vector<MetricValue> collect() override;
    std::string to_prometheus_format() override;

private:
    // 指标存储
    std::map<std::string, double> counters_;
    std::map<std::string, double> gauges_;
    std::map<std::string, std::vector<double>> histograms_;
    std::mutex mutex_;

    std::string make_key(const std::string& name,
                         const std::map<std::string, std::string>& labels);
};

// ========== RAG 指标 ==========

/**
 * @brief RAG 系统指标收集器
 */
class RAGMetrics {
public:
    explicit RAGMetrics(std::shared_ptr<MetricsCollector> collector);

    // ========== 索引指标 ==========

    void inc_indexed_documents(int count = 1);
    void inc_index_chunks(int count = 1);
    void observe_index_duration(double ms);
    void inc_index_errors();

    // ========== 查询指标 ==========

    void inc_query_count();
    void observe_query_latency(double ms);
    void observe_embedding_latency(double ms);
    void observe_search_latency(double ms);
    void observe_generation_latency(double ms);
    void observe_chunks_returned(int count);
    void inc_empty_results();

    // ========== 模型指标 ==========

    void set_embedding_loaded(bool loaded);
    void set_llm_loaded(bool loaded);
    void inc_tokens_processed(int count);
    void inc_tokens_generated(int count);

    // ========== 系统指标 ==========

    void set_memory_usage(size_t bytes);
    void set_embedding_queue_size(int size);
    void set_query_queue_size(int size);

private:
    std::shared_ptr<MetricsCollector> collector_;
};

// ========== 健康检查 ==========

/**
 * @brief 健康检查状态
 */
enum class HealthStatus {
    HEALTHY,
    DEGRADED,
    UNHEALTHY,
    UNKNOWN
};

/**
 * @brief 健康检查结果
 */
struct HealthCheckResult {
    std::string name;
    HealthStatus status = HealthStatus::UNKNOWN;
    std::string message;
    double latency_ms = 0;
    std::chrono::steady_clock::time_point timestamp;
};

/**
 * @brief 健康检查器接口
 */
class HealthChecker {
public:
    virtual ~HealthChecker() = default;

    // 注册检查
    virtual void register_check(const std::string& name,
                                std::function<HealthCheckResult()> check) = 0;

    // 执行检查
    virtual std::vector<HealthCheckResult> check_all() = 0;

    // 获取总体状态
    virtual HealthStatus overall_status() = 0;

    // JSON 输出
    virtual std::string to_json() = 0;
};

// ========== 默认健康检查器 ==========

/**
 * @brief 默认健康检查器实现
 */
class DefaultHealthChecker : public HealthChecker {
public:
    DefaultHealthChecker();
    ~DefaultHealthChecker() override = default;

    void register_check(const std::string& name,
                        std::function<HealthCheckResult()> check) override;

    std::vector<HealthCheckResult> check_all() override;
    HealthStatus overall_status() override;
    std::string to_json() override;

    // 设置检查器
    void set_vector_index_size_func(std::function<size_t()> func);
    void set_document_count_func(std::function<int64_t()> func);
    void set_embedding_loaded_func(std::function<bool()> func);
    void set_llm_loaded_func(std::function<bool()> func);

private:
    std::map<std::string, std::function<HealthCheckResult()>> checks_;
    std::function<size_t()> vector_index_size_func_;
    std::function<int64_t()> document_count_func_;
    std::function<bool()> embedding_loaded_func_;
    std::function<bool()> llm_loaded_func_;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建指标收集器
 */
std::unique_ptr<MetricsCollector> create_metrics_collector();

/**
 * @brief 创建健康检查器
 */
std::unique_ptr<HealthChecker> create_health_checker();

}  // namespace rag
