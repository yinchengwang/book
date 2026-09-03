/**
 * @file metrics.cpp
 * @brief 指标收集实现
 */

#include "rag/metrics.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <numeric>

namespace rag {

// ========== InMemoryMetricsCollector 实现 ==========

InMemoryMetricsCollector::InMemoryMetricsCollector() = default;

std::string InMemoryMetricsCollector::make_key(
    const std::string& name,
    const std::map<std::string, std::string>& labels) {

    std::ostringstream oss;
    oss << name;

    if (!labels.empty()) {
        oss << "{";
        bool first = true;
        for (const auto& [key, value] : labels) {
            if (!first) oss << ",";
            oss << key << "=\"" << value << "\"";
            first = false;
        }
        oss << "}";
    }

    return oss.str();
}

void InMemoryMetricsCollector::inc_counter(const std::string& name,
                                           const std::map<std::string, std::string>& labels) {
    inc_counter_by(name, 1.0, labels);
}

void InMemoryMetricsCollector::inc_counter_by(const std::string& name, double value,
                                              const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = make_key(name, labels);
    counters_[key] += value;
}

void InMemoryMetricsCollector::set_gauge(const std::string& name, double value,
                                         const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = make_key(name, labels);
    gauges_[key] = value;
}

void InMemoryMetricsCollector::observe_histogram(const std::string& name, double value,
                                                 const std::map<std::string, std::string>& labels) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string key = make_key(name, labels);
    histograms_[key].push_back(value);
}

std::vector<MetricValue> InMemoryMetricsCollector::collect() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<MetricValue> values;

    for (const auto& [key, value] : counters_) {
        MetricValue mv;
        mv.name = key;
        mv.value = value;
        values.push_back(mv);
    }

    for (const auto& [key, value] : gauges_) {
        MetricValue mv;
        mv.name = key;
        mv.value = value;
        values.push_back(mv);
    }

    return values;
}

std::string InMemoryMetricsCollector::to_prometheus_format() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::ostringstream oss;

    // 计数器
    for (const auto& [key, value] : counters_) {
        oss << "# HELP " << key << "\n";
        oss << "# TYPE " << key << " counter\n";
        oss << key << " " << value << "\n\n";
    }

    // 仪表
    for (const auto& [key, value] : gauges_) {
        oss << "# HELP " << key << "\n";
        oss << "# TYPE " << key << " gauge\n";
        oss << key << " " << value << "\n\n";
    }

    // 直方图
    for (const auto& [key, values] : histograms_) {
        if (values.empty()) continue;

        oss << "# HELP " << key << "\n";
        oss << "# TYPE " << key << " histogram\n";

        // 计算分位数
        auto sorted = values;
        std::sort(sorted.begin(), sorted.end());

        std::vector<double> buckets = {0.01, 0.05, 0.1, 0.5, 0.9, 0.95, 0.99, 1.0};

        for (double bucket : buckets) {
            int count = static_cast<int>(bucket * sorted.size());
            oss << key << "_bucket{le=\"" << bucket << "\"} " << count << "\n";
        }
        oss << key << "_bucket{le=\"+Inf\"} " << sorted.size() << "\n";
        oss << key << "_sum " << std::accumulate(sorted.begin(), sorted.end(), 0.0) << "\n";
        oss << key << "_count " << sorted.size() << "\n\n";
    }

    return oss.str();
}

// ========== RAGMetrics 实现 ==========

RAGMetrics::RAGMetrics(std::shared_ptr<MetricsCollector> collector)
    : collector_(std::move(collector)) {}

// 索引指标
void RAGMetrics::inc_indexed_documents(int count) {
    collector_->inc_counter_by("rag_documents_indexed_total", count);
}

void RAGMetrics::inc_index_chunks(int count) {
    collector_->inc_counter_by("rag_chunks_created_total", count);
}

void RAGMetrics::observe_index_duration(double ms) {
    collector_->observe_histogram("rag_index_duration_seconds", ms / 1000.0);
}

void RAGMetrics::inc_index_errors() {
    collector_->inc_counter("rag_index_errors_total");
}

// 查询指标
void RAGMetrics::inc_query_count() {
    collector_->inc_counter("rag_queries_total");
}

void RAGMetrics::observe_query_latency(double ms) {
    collector_->observe_histogram("rag_query_latency_seconds", ms / 1000.0);
}

void RAGMetrics::observe_embedding_latency(double ms) {
    collector_->observe_histogram("rag_embedding_latency_seconds", ms / 1000.0);
}

void RAGMetrics::observe_search_latency(double ms) {
    collector_->observe_histogram("rag_search_latency_seconds", ms / 1000.0);
}

void RAGMetrics::observe_generation_latency(double ms) {
    collector_->observe_histogram("rag_generation_latency_seconds", ms / 1000.0);
}

void RAGMetrics::observe_chunks_returned(int count) {
    collector_->inc_counter_by("rag_chunks_returned_total", count);
}

void RAGMetrics::inc_empty_results() {
    collector_->inc_counter("rag_empty_results_total");
}

// 模型指标
void RAGMetrics::set_embedding_loaded(bool loaded) {
    collector_->set_gauge("rag_embedding_model_loaded", loaded ? 1 : 0);
}

void RAGMetrics::set_llm_loaded(bool loaded) {
    collector_->set_gauge("rag_llm_model_loaded", loaded ? 1 : 0);
}

void RAGMetrics::inc_tokens_processed(int count) {
    collector_->inc_counter_by("rag_tokens_processed_total", count);
}

void RAGMetrics::inc_tokens_generated(int count) {
    collector_->inc_counter_by("rag_tokens_generated_total", count);
}

// 系统指标
void RAGMetrics::set_memory_usage(size_t bytes) {
    collector_->set_gauge("rag_memory_usage_bytes", static_cast<double>(bytes));
}

void RAGMetrics::set_embedding_queue_size(int size) {
    collector_->set_gauge("rag_embedding_queue_size", size);
}

void RAGMetrics::set_query_queue_size(int size) {
    collector_->set_gauge("rag_query_queue_size", size);
}

// ========== DefaultHealthChecker 实现 ==========

DefaultHealthChecker::DefaultHealthChecker() = default;

void DefaultHealthChecker::register_check(const std::string& name,
                                          std::function<HealthCheckResult()> check) {
    checks_[name] = std::move(check);
}

std::vector<HealthCheckResult> DefaultHealthChecker::check_all() {
    std::vector<HealthCheckResult> results;

    for (const auto& [name, check] : checks_) {
        auto start = std::chrono::steady_clock::now();
        auto result = check();
        auto end = std::chrono::steady_clock::now();

        result.name = name;
        result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();
        result.timestamp = end;

        results.push_back(result);
    }

    return results;
}

HealthStatus DefaultHealthChecker::overall_status() {
    auto results = check_all();

    if (results.empty()) {
        return HealthStatus::UNKNOWN;
    }

    bool has_unhealthy = false;
    bool has_degraded = false;

    for (const auto& result : results) {
        if (result.status == HealthStatus::UNHEALTHY) {
            has_unhealthy = true;
        } else if (result.status == HealthStatus::DEGRADED) {
            has_degraded = true;
        }
    }

    if (has_unhealthy) return HealthStatus::UNHEALTHY;
    if (has_degraded) return HealthStatus::DEGRADED;

    return HealthStatus::HEALTHY;
}

std::string DefaultHealthChecker::to_json() {
    auto results = check_all();
    std::ostringstream oss;

    oss << "{\n";
    oss << "  \"status\": \"";

    switch (overall_status()) {
        case HealthStatus::HEALTHY: oss << "healthy"; break;
        case HealthStatus::DEGRADED: oss << "degraded"; break;
        case HealthStatus::UNHEALTHY: oss << "unhealthy"; break;
        default: oss << "unknown"; break;
    }

    oss << "\",\n";
    oss << "  \"timestamp\": " << std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() << ",\n";
    oss << "  \"checks\": [\n";

    bool first = true;
    for (const auto& result : results) {
        if (!first) oss << ",\n";
        first = false;

        oss << "    {\n";
        oss << "      \"name\": \"" << result.name << "\",\n";
        oss << "      \"status\": \"";

        switch (result.status) {
            case HealthStatus::HEALTHY: oss << "healthy"; break;
            case HealthStatus::DEGRADED: oss << "degraded"; break;
            case HealthStatus::UNHEALTHY: oss << "unhealthy"; break;
            default: oss << "unknown"; break;
        }

        oss << "\",\n";
        oss << "      \"message\": \"" << result.message << "\",\n";
        oss << "      \"latency_ms\": " << std::fixed << std::setprecision(2)
            << result.latency_ms << "\n";
        oss << "    }";
    }

    oss << "\n  ]\n";
    oss << "}";

    return oss.str();
}

void DefaultHealthChecker::set_vector_index_size_func(std::function<size_t()> func) {
    vector_index_size_func_ = std::move(func);
}

void DefaultHealthChecker::set_document_count_func(std::function<int64_t()> func) {
    document_count_func_ = std::move(func);
}

void DefaultHealthChecker::set_embedding_loaded_func(std::function<bool()> func) {
    embedding_loaded_func_ = std::move(func);
}

void DefaultHealthChecker::set_llm_loaded_func(std::function<bool()> func) {
    llm_loaded_func_ = std::move(func);
}

// ========== 工厂函数 ==========

std::unique_ptr<MetricsCollector> create_metrics_collector() {
    return std::make_unique<InMemoryMetricsCollector>();
}

std::unique_ptr<HealthChecker> create_health_checker() {
    return std::make_unique<DefaultHealthChecker>();
}

}  // namespace rag
