/**
 * @file tracing.h
 * @brief 可观测性 - Tracing 和 Metrics
 *
 * 功能:
 * - OpenTelemetry 风格的分布式 tracing
 * - Prometheus 风格的 metrics
 * - Pipeline 各阶段的性能监控
 */
#pragma once

#include "rag/pipeline.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <variant>

namespace rag {

// ========== Span & Trace ==========

/**
 * @brief Span 属性
 */
struct SpanAttribute {
    std::string key;
    std::variant<int, int64_t, float, double, std::string, bool> value;

    template<typename T>
    T get_value() const {
        if (std::holds_alternative<T>(value)) {
            return std::get<T>(value);
        }
        return T{};
    }
};

/**
 * @brief Span 事件
 */
struct SpanEvent {
    std::string name;
    int64_t timestamp_ns = 0;
    std::unordered_map<std::string, std::string> attributes;
};

/**
 * @brief Span 状态
 */
enum class SpanStatus {
    OK,
    ERROR,
    UNSET
};

/**
 * @brief Span
 */
class Span {
public:
    std::string name;
    std::string trace_id;
    std::string span_id;
    std::string parent_span_id;

    int64_t start_time_ns = 0;
    int64_t end_time_ns = 0;

    std::vector<SpanAttribute> attributes;
    std::vector<SpanEvent> events;

    SpanStatus status = SpanStatus::UNSET;
    std::string status_message;

    // 操作
    void add_attribute(const std::string& key, const std::string& value);
    void add_attribute(const std::string& key, int value);
    void add_attribute(const std::string& key, float value);

    void add_event(const std::string& name,
                   const std::unordered_map<std::string, std::string>& attrs = {});

    void set_status(SpanStatus status, const std::string& message = "");

    int64_t duration_ns() const {
        return end_time_ns > start_time_ns ? end_time_ns - start_time_ns : 0;
    }

    double duration_ms() const {
        return duration_ns() / 1e6;
    }
};

// ========== Tracer ==========

/**
 * @brief Tracer
 */
class RAGTracer {
public:
    RAGTracer() = default;
    ~RAGTracer() = default;

    // ========== Span 管理 ==========

    /**
     * @brief 开始 span
     * @param name span 名称
     * @param trace_id trace ID
     * @param parent_span_id 父 span ID
     * @return 新的 span
     */
    std::shared_ptr<Span> start_span(
        const std::string& name,
        const std::string& trace_id = "",
        const std::string& parent_span_id = "");

    /**
     * @brief 结束 span
     */
    void end_span(std::shared_ptr<Span> span);

    /**
     * @brief 添加事件
     */
    void add_event(std::shared_ptr<Span> span,
                   const std::string& event_name,
                   const std::unordered_map<std::string, std::string>& attrs = {});

    // ========== Pipeline Tracing ==========

    /**
     * @brief 跟踪查询分类
     */
    std::shared_ptr<Span> trace_query_classification(
        const std::string& query,
        QueryType type);

    /**
     * @brief 跟踪检索
     */
    std::shared_ptr<Span> trace_retrieval(
        const std::string& query,
        int recall_count,
        int retrieval_ms);

    /**
     * @brief 跟踪重排
     */
    std::shared_ptr<Span> trace_rerank(
        const std::string& query,
        int candidate_count,
        int final_count,
        int rerank_ms);

    /**
     * @brief 跟踪生成
     */
    std::shared_ptr<Span> trace_generation(
        const std::string& prompt,
        const std::string& response,
        int generation_ms);

    // ========== 配置 ==========

    struct Config {
        bool enabled = true;
        float sampling_rate = 1.0f;
        int max_spans_per_trace = 1000;
        bool export_to_console = false;
        std::string otlp_endpoint;  // OpenTelemetry Collector endpoint
    };

    void configure(const Config& config);
    const Config& config() const { return config_; }

private:
    std::string generate_id();
    bool should_sample();

    Config config_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Span>>> trace_spans_;
    uint64_t span_counter_ = 0;
};

// ========== Metrics ==========

/**
 * @brief 计数器
 */
class Counter {
public:
    void inc(double value = 1.0) { value_ += value; }
    double value() const { return value_; }
    void reset() { value_ = 0.0; }

private:
    double value_ = 0.0;
};

/**
 * @brief 计量器
 */
class Gauge {
public:
    void set(double value) { value_ = value; }
    double value() const { return value_; }

private:
    double value_ = 0.0;
};

/**
 * @brief 直方图
 */
class Histogram {
public:
    void observe(double value) {
        values_.push_back(value);
        sum_ += value;
        count_++;
    }

    double sum() const { return sum_; }
    uint64_t count() const { return count_; }

    // 分位数
    double quantile(double q) const {
        if (values_.empty()) return 0.0;
        auto sorted = values_;
        std::sort(sorted.begin(), sorted.end());
        size_t idx = static_cast<size_t>(q * sorted.size());
        idx = std::min(idx, sorted.size() - 1);
        return sorted[idx];
    }

    double p50() const { return quantile(0.5); }
    double p95() const { return quantile(0.95); }
    double p99() const { return quantile(0.99); }

    void reset() {
        values_.clear();
        sum_ = 0.0;
        count_ = 0;
    }

private:
    std::vector<double> values_;
    double sum_ = 0.0;
    uint64_t count_ = 0;
};

// ========== Retrieval Metrics ==========

/**
 * @brief 检索指标
 */
struct RetrievalMetrics {
    // 延迟
    Histogram query_latency_ms;
    Histogram retrieval_latency_ms;
    Histogram rerank_latency_ms;
    Histogram generation_latency_ms;

    // 吞吐量
    Counter queries_total;
    Gauge active_queries;

    // 检索质量
    Counter retrieval_empty;
    Histogram retrieval_scores;
    Histogram rerank_score_improvement;

    // 命中率
    Counter cache_hits;
    Counter cache_misses;

    // 多模态
    Counter modality_text;
    Counter modality_table;
    Counter modality_image;
    Counter modality_video;
    Counter modality_audio;

    // 错误
    Counter errors_total;
    std::unordered_map<std::string, Counter> errors_by_type;

    // 重置
    void reset();
};

// ========== Metrics Collector ==========

/**
 * @brief Metrics 收集器
 */
class MetricsCollector {
public:
    MetricsCollector() = default;
    ~MetricsCollector() = default;

    // ========== 记录指标 ==========

    void record_query_latency(double ms);
    void record_retrieval_latency(double ms);
    void record_rerank_latency(double ms);
    void record_generation_latency(double ms);

    void increment_queries();
    void set_active_queries(int count);

    void increment_cache_hit();
    void increment_cache_miss();

    void record_retrieval_score(double score);
    void record_rerank_improvement(double improvement);

    void increment_modality(const std::string& modality);

    void record_error(const std::string& error_type);

    // ========== 获取指标 ==========

    const RetrievalMetrics& metrics() const { return metrics_; }

    // ========== 导出 ==========

    /**
     * @brief 导出为 Prometheus 格式
     */
    std::string export_prometheus() const;

    /**
     * @brief 导出为 JSON
     */
    std::string export_json() const;

    // ========== 配置 ==========

    struct Config {
        bool enabled = true;
        int export_interval_ms = 60000;  // 导出间隔
        std::string export_endpoint;     // 推送端点
    };

    void configure(const Config& config);

private:
    RetrievalMetrics metrics_;
    Config config_;
};

// ========== Pipeline Observer ==========

/**
 * @brief Pipeline 观察器
 *
 * 包装 Pipeline，自动收集 tracing 和 metrics
 */
class PipelineObserver {
public:
    /**
     * @brief 构造函数
     * @param pipeline 要观察的 Pipeline
     * @param tracer tracer
     * @param metrics metrics 收集器
     */
    PipelineObserver(
        std::shared_ptr<RetrievalPipeline> pipeline,
        std::shared_ptr<RAGTracer> tracer = nullptr,
        std::shared_ptr<MetricsCollector> metrics = nullptr);

    ~PipelineObserver() = default;

    // ========== 执行 (带观测) ==========

    /**
     * @brief 执行查询 (带 tracing 和 metrics)
     */
    PipelineResult execute_with_observation(
        const std::string& query,
        int top_k = 5);

    /**
     * @brief 异步执行
     */
    std::future<PipelineResult> execute_async_with_observation(
        const std::string& query,
        int top_k = 5);

    // ========== 配置 ==========

    void set_tracer(std::shared_ptr<RAGTracer> tracer);
    void set_metrics(std::shared_ptr<MetricsCollector> metrics);

    bool is_tracing_enabled() const { return tracer_ != nullptr; }
    bool is_metrics_enabled() const { return metrics_ != nullptr; }

private:
    std::shared_ptr<RetrievalPipeline> pipeline_;
    std::shared_ptr<RAGTracer> tracer_;
    std::shared_ptr<MetricsCollector> metrics_;
};

// ========== Factory ==========

/**
 * @brief 创建 Tracer
 */
std::shared_ptr<RAGTracer> create_tracer(const RAGTracer::Config& config = {});

/**
 * @brief 创建 Metrics 收集器
 */
std::shared_ptr<MetricsCollector> create_metrics_collector(
    const MetricsCollector::Config& config = {});

/**
 * @brief 创建 Pipeline 观察器
 */
std::shared_ptr<PipelineObserver> create_pipeline_observer(
    std::shared_ptr<RetrievalPipeline> pipeline,
    std::shared_ptr<RAGTracer> tracer = nullptr,
    std::shared_ptr<MetricsCollector> metrics = nullptr);

}  // namespace rag
