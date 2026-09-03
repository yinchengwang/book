#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>

namespace rag {

// ========== Alert Levels ==========

enum class AlertSeverity {
    INFO,
    WARNING,
    CRITICAL
};

// ========== Alert Rules ==========

struct AlertRule {
    std::string name;
    std::string metric;
    std::string condition;  // ">", "<", "==", ">=", "<="
    double threshold;
    int duration_seconds = 60;
    AlertSeverity severity = AlertSeverity::WARNING;
};

struct Alert {
    std::string rule_name;
    AlertSeverity severity;
    std::string message;
    double value;
    int64_t timestamp;
};

// ========== Core Metrics ==========

struct CoreMetrics {
    // Latency
    double p50_latency_ms = 0;
    double p95_latency_ms = 0;
    double p99_latency_ms = 0;

    // Throughput
    double qps = 0;
    double index_docs_per_sec = 0;

    // Cache
    double cache_hit_rate = 0;
    size_t cache_size = 0;

    // Retrieval quality
    double avg_retrieval_score = 0;
    double retrieval_timeout_rate = 0;

    // GPU
    double gpu_utilization = 0;
    double gpu_memory_used_mb = 0;

    // Query statistics
    uint64_t total_queries = 0;
    uint64_t successful_queries = 0;
};

// ========== Metrics Collector ==========

class MetricsCollector {
public:
    MetricsCollector();

    // Record latency
    void record_latency(double ms);

    // Record query
    void record_query(bool success, double latency_ms);

    // Record cache
    void record_cache_hit();
    void record_cache_miss();

    // Record GPU
    void record_gpu_stats(double utilization, double memory_mb);

    // Get metrics
    CoreMetrics collect();

    // Reset
    void reset();

    // Prometheus format output
    std::string to_prometheus_format();

private:
    std::vector<double> latencies_;
    uint64_t total_queries_ = 0;
    uint64_t successful_queries_ = 0;
    uint64_t cache_hits_ = 0;
    uint64_t cache_misses_ = 0;
    double gpu_utilization_ = 0;
    double gpu_memory_mb_ = 0;

    std::chrono::steady_clock::time_point start_time_;
};

// ========== Alert Manager ==========

class AlertManager {
public:
    AlertManager();

    // Add rule
    void add_rule(const AlertRule& rule);

    // Check and alert
    std::vector<Alert> check_and_alert(const CoreMetrics& metrics);

    // Get active alerts
    std::vector<Alert> get_active_alerts() const;

    // Clear alerts
    void clear_alerts();

    // Default rules
    void add_default_rules();

private:
    std::vector<AlertRule> rules_;
    std::unordered_map<std::string, int> alert_counters_;
    std::vector<Alert> active_alerts_;
};

// ========== Factory Functions ==========

std::unique_ptr<MetricsCollector> create_metrics_collector();
std::unique_ptr<AlertManager> create_alert_manager();

}  // namespace rag
