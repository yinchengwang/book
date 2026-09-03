/**
 * @file monitoring.cpp
 * @brief Monitoring and alerting implementation
 */

#include "rag/monitoring.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace rag {

// ========== MetricsCollector Implementation ==========

MetricsCollector::MetricsCollector() {
    start_time_ = std::chrono::steady_clock::now();
}

void MetricsCollector::record_latency(double ms) {
    latencies_.push_back(ms);
}

void MetricsCollector::record_query(bool success, double latency_ms) {
    total_queries_++;
    if (success) {
        successful_queries_++;
    }
    record_latency(latency_ms);
}

void MetricsCollector::record_cache_hit() {
    cache_hits_++;
}

void MetricsCollector::record_cache_miss() {
    cache_misses_++;
}

void MetricsCollector::record_gpu_stats(double utilization, double memory_mb) {
    gpu_utilization_ = utilization;
    gpu_memory_mb_ = memory_mb;
}

double calculate_percentile(std::vector<double>& sorted_data, double percentile) {
    if (sorted_data.empty()) {
        return 0.0;
    }
    size_t index = static_cast<size_t>(std::ceil(percentile * sorted_data.size())) - 1;
    index = std::min(index, sorted_data.size() - 1);
    return sorted_data[index];
}

CoreMetrics MetricsCollector::collect() {
    CoreMetrics metrics;

    // Calculate latency percentiles
    if (!latencies_.empty()) {
        auto sorted = latencies_;
        std::sort(sorted.begin(), sorted.end());

        metrics.p50_latency_ms = calculate_percentile(sorted, 0.50);
        metrics.p95_latency_ms = calculate_percentile(sorted, 0.95);
        metrics.p99_latency_ms = calculate_percentile(sorted, 0.99);
    }

    // Calculate QPS
    auto now = std::chrono::steady_clock::now();
    auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time_).count();
    if (elapsed_seconds > 0) {
        metrics.qps = static_cast<double>(total_queries_) / elapsed_seconds;
    }

    // Cache metrics
    uint64_t total_cache_ops = cache_hits_ + cache_misses_;
    if (total_cache_ops > 0) {
        metrics.cache_hit_rate = static_cast<double>(cache_hits_) / total_cache_ops;
    }

    // Query metrics
    metrics.total_queries = total_queries_;
    metrics.successful_queries = successful_queries_;

    // GPU metrics
    metrics.gpu_utilization = gpu_utilization_;
    metrics.gpu_memory_used_mb = gpu_memory_mb_;

    return metrics;
}

void MetricsCollector::reset() {
    latencies_.clear();
    latencies_.shrink_to_fit();
    total_queries_ = 0;
    successful_queries_ = 0;
    cache_hits_ = 0;
    cache_misses_ = 0;
    gpu_utilization_ = 0;
    gpu_memory_mb_ = 0;
    start_time_ = std::chrono::steady_clock::now();
}

std::string MetricsCollector::to_prometheus_format() {
    auto metrics = collect();

    std::ostringstream oss;

    // Latency metrics
    oss << "# HELP rag_p50_latency_ms P50 latency in milliseconds\n";
    oss << "# TYPE rag_p50_latency_ms gauge\n";
    oss << "rag_p50_latency_ms " << std::fixed << std::setprecision(2)
        << metrics.p50_latency_ms << "\n\n";

    oss << "# HELP rag_p95_latency_ms P95 latency in milliseconds\n";
    oss << "# TYPE rag_p95_latency_ms gauge\n";
    oss << "rag_p95_latency_ms " << std::fixed << std::setprecision(2)
        << metrics.p95_latency_ms << "\n\n";

    oss << "# HELP rag_p99_latency_ms P99 latency in milliseconds\n";
    oss << "# TYPE rag_p99_latency_ms gauge\n";
    oss << "rag_p99_latency_ms " << std::fixed << std::setprecision(2)
        << metrics.p99_latency_ms << "\n\n";

    // Throughput metrics
    oss << "# HELP rag_qps Queries per second\n";
    oss << "# TYPE rag_qps gauge\n";
    oss << "rag_qps " << std::fixed << std::setprecision(2) << metrics.qps << "\n\n";

    // Cache metrics
    oss << "# HELP rag_cache_hit_rate Cache hit rate\n";
    oss << "# TYPE rag_cache_hit_rate gauge\n";
    oss << "rag_cache_hit_rate " << std::fixed << std::setprecision(4)
        << metrics.cache_hit_rate << "\n\n";

    // Query stats
    oss << "# HELP rag_total_queries Total number of queries\n";
    oss << "# TYPE rag_total_queries counter\n";
    oss << "rag_total_queries " << metrics.total_queries << "\n\n";

    oss << "# HELP rag_successful_queries Total successful queries\n";
    oss << "# TYPE rag_successful_queries counter\n";
    oss << "rag_successful_queries " << metrics.successful_queries << "\n\n";

    // GPU metrics
    oss << "# HELP rag_gpu_utilization GPU utilization percentage\n";
    oss << "# TYPE rag_gpu_utilization gauge\n";
    oss << "rag_gpu_utilization " << std::fixed << std::setprecision(2)
        << metrics.gpu_utilization << "\n\n";

    oss << "# HELP rag_gpu_memory_mb GPU memory used in MB\n";
    oss << "# TYPE rag_gpu_memory_mb gauge\n";
    oss << "rag_gpu_memory_mb " << std::fixed << std::setprecision(2)
        << metrics.gpu_memory_used_mb << "\n\n";

    return oss.str();
}

// ========== AlertManager Implementation ==========

AlertManager::AlertManager() = default;

void AlertManager::add_rule(const AlertRule& rule) {
    rules_.push_back(rule);
}

double get_metric_value(const CoreMetrics& metrics, const std::string& metric) {
    if (metric == "p50_latency_ms") return metrics.p50_latency_ms;
    if (metric == "p95_latency_ms") return metrics.p95_latency_ms;
    if (metric == "p99_latency_ms") return metrics.p99_latency_ms;
    if (metric == "qps") return metrics.qps;
    if (metric == "index_docs_per_sec") return metrics.index_docs_per_sec;
    if (metric == "cache_hit_rate") return metrics.cache_hit_rate;
    if (metric == "cache_size") return static_cast<double>(metrics.cache_size);
    if (metric == "avg_retrieval_score") return metrics.avg_retrieval_score;
    if (metric == "retrieval_timeout_rate") return metrics.retrieval_timeout_rate;
    if (metric == "gpu_utilization") return metrics.gpu_utilization;
    if (metric == "gpu_memory_used_mb") return metrics.gpu_memory_used_mb;
    if (metric == "total_queries") return static_cast<double>(metrics.total_queries);
    if (metric == "successful_queries") return static_cast<double>(metrics.successful_queries);
    return 0.0;
}

bool evaluate_condition(double value, const std::string& condition, double threshold) {
    if (condition == ">") return value > threshold;
    if (condition == "<") return value < threshold;
    if (condition == "==") return std::abs(value - threshold) < 1e-9;
    if (condition == ">=") return value >= threshold;
    if (condition == "<=") return value <= threshold;
    return false;
}

std::vector<Alert> AlertManager::check_and_alert(const CoreMetrics& metrics) {
    std::vector<Alert> triggered_alerts;

    for (const auto& rule : rules_) {
        double value = get_metric_value(metrics, rule.metric);

        if (evaluate_condition(value, rule.condition, rule.threshold)) {
            Alert alert;
            alert.rule_name = rule.name;
            alert.severity = rule.severity;
            alert.value = value;
            alert.timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();

            std::ostringstream msg;
            msg << "Alert: " << rule.name << " triggered. ";
            msg << rule.metric << " " << rule.condition << " " << rule.threshold;
            msg << " (current: " << std::fixed << std::setprecision(2) << value << ")";
            alert.message = msg.str();

            triggered_alerts.push_back(alert);

            // Update counter
            alert_counters_[rule.name]++;
        }
    }

    // Update active alerts
    active_alerts_ = triggered_alerts;

    return triggered_alerts;
}

std::vector<Alert> AlertManager::get_active_alerts() const {
    return active_alerts_;
}

void AlertManager::clear_alerts() {
    active_alerts_.clear();
    alert_counters_.clear();
}

void AlertManager::add_default_rules() {
    // HighLatency: p99_latency_ms > 2000, 60s
    {
        AlertRule rule;
        rule.name = "HighLatency";
        rule.metric = "p99_latency_ms";
        rule.condition = ">";
        rule.threshold = 2000.0;
        rule.duration_seconds = 60;
        rule.severity = AlertSeverity::WARNING;
        add_rule(rule);
    }

    // VeryHighLatency: p99_latency_ms > 5000, 30s
    {
        AlertRule rule;
        rule.name = "VeryHighLatency";
        rule.metric = "p99_latency_ms";
        rule.condition = ">";
        rule.threshold = 5000.0;
        rule.duration_seconds = 30;
        rule.severity = AlertSeverity::CRITICAL;
        add_rule(rule);
    }

    // LowCacheHitRate: cache_hit_rate < 0.3, 300s
    {
        AlertRule rule;
        rule.name = "LowCacheHitRate";
        rule.metric = "cache_hit_rate";
        rule.condition = "<";
        rule.threshold = 0.3;
        rule.duration_seconds = 300;
        rule.severity = AlertSeverity::WARNING;
        add_rule(rule);
    }

    // HighGPUUtilization: gpu_utilization > 95, 60s
    {
        AlertRule rule;
        rule.name = "HighGPUUtilization";
        rule.metric = "gpu_utilization";
        rule.condition = ">";
        rule.threshold = 95.0;
        rule.duration_seconds = 60;
        rule.severity = AlertSeverity::WARNING;
        add_rule(rule);
    }
}

// ========== Factory Functions ==========

std::unique_ptr<MetricsCollector> create_metrics_collector() {
    return std::make_unique<MetricsCollector>();
}

std::unique_ptr<AlertManager> create_alert_manager() {
    return std::make_unique<AlertManager>();
}

}  // namespace rag
