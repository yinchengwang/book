/**
 * @file test_monitoring.cpp
 * @brief Monitoring and alerting tests
 */

#include "rag/monitoring.h"
#include <gtest/gtest.h>
#include <cmath>

namespace rag {
namespace test {

// ========== Test RecordLatency ==========

TEST(RecordLatency, BasicRecording) {
    MetricsCollector collector;

    collector.record_latency(10.0);
    collector.record_latency(20.0);
    collector.record_latency(30.0);

    auto metrics = collector.collect();

    EXPECT_EQ(metrics.p50_latency_ms, 20.0);
    EXPECT_EQ(metrics.p95_latency_ms, 30.0);
    EXPECT_EQ(metrics.p99_latency_ms, 30.0);
}

TEST(RecordLatency, EmptyCollector) {
    MetricsCollector collector;

    auto metrics = collector.collect();

    EXPECT_EQ(metrics.p50_latency_ms, 0.0);
    EXPECT_EQ(metrics.p95_latency_ms, 0.0);
    EXPECT_EQ(metrics.p99_latency_ms, 0.0);
}

// ========== Test CalculatePercentiles ==========

TEST(CalculatePercentiles, SortedData) {
    MetricsCollector collector;

    // Record 100 latencies from 1 to 100
    for (int i = 1; i <= 100; i++) {
        collector.record_latency(static_cast<double>(i));
    }

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.p50_latency_ms, 50.0);
    EXPECT_DOUBLE_EQ(metrics.p95_latency_ms, 95.0);
    EXPECT_DOUBLE_EQ(metrics.p99_latency_ms, 99.0);
}

TEST(CalculatePercentiles, UnsortedData) {
    MetricsCollector collector;

    // Record out-of-order latencies
    collector.record_latency(100.0);
    collector.record_latency(10.0);
    collector.record_latency(50.0);
    collector.record_latency(30.0);
    collector.record_latency(90.0);

    auto metrics = collector.collect();

    EXPECT_EQ(metrics.p50_latency_ms, 50.0);
    EXPECT_EQ(metrics.p95_latency_ms, 90.0);
    EXPECT_EQ(metrics.p99_latency_ms, 100.0);
}

// ========== Test RecordCache ==========

TEST(RecordCache, BasicRecording) {
    MetricsCollector collector;

    collector.record_cache_hit();
    collector.record_cache_hit();
    collector.record_cache_miss();
    collector.record_cache_hit();

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.cache_hit_rate, 0.75);  // 3 hits / 4 total
}

TEST(RecordCache, AllHits) {
    MetricsCollector collector;

    collector.record_cache_hit();
    collector.record_cache_hit();
    collector.record_cache_hit();

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.cache_hit_rate, 1.0);
}

TEST(RecordCache, AllMisses) {
    MetricsCollector collector;

    collector.record_cache_miss();
    collector.record_cache_miss();

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.cache_hit_rate, 0.0);
}

TEST(RecordCache, EmptyCache) {
    MetricsCollector collector;

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.cache_hit_rate, 0.0);
}

// ========== Test CollectMetrics ==========

TEST(CollectMetrics, QueryStats) {
    MetricsCollector collector;

    collector.record_query(true, 10.0);
    collector.record_query(true, 20.0);
    collector.record_query(false, 30.0);

    auto metrics = collector.collect();

    EXPECT_EQ(metrics.total_queries, 3);
    EXPECT_EQ(metrics.successful_queries, 2);
}

TEST(CollectMetrics, GpuStats) {
    MetricsCollector collector;

    collector.record_gpu_stats(85.5, 1024.0);

    auto metrics = collector.collect();

    EXPECT_DOUBLE_EQ(metrics.gpu_utilization, 85.5);
    EXPECT_DOUBLE_EQ(metrics.gpu_memory_used_mb, 1024.0);
}

TEST(CollectMetrics, CombinedMetrics) {
    MetricsCollector collector;

    collector.record_latency(10.0);
    collector.record_latency(20.0);
    collector.record_latency(30.0);
    collector.record_query(true, 15.0);
    collector.record_query(true, 25.0);
    collector.record_cache_hit();
    collector.record_cache_hit();
    collector.record_cache_miss();
    collector.record_gpu_stats(50.0, 512.0);

    auto metrics = collector.collect();

    // Latency
    EXPECT_EQ(metrics.p50_latency_ms, 20.0);

    // Cache
    EXPECT_DOUBLE_EQ(metrics.cache_hit_rate, 2.0 / 3.0);

    // Query
    EXPECT_EQ(metrics.total_queries, 2);
    EXPECT_EQ(metrics.successful_queries, 2);

    // GPU
    EXPECT_DOUBLE_EQ(metrics.gpu_utilization, 50.0);
    EXPECT_DOUBLE_EQ(metrics.gpu_memory_used_mb, 512.0);
}

// ========== Test PrometheusFormat ==========

TEST(PrometheusFormat, BasicOutput) {
    MetricsCollector collector;

    collector.record_latency(10.0);
    collector.record_latency(20.0);
    collector.record_cache_hit();

    auto output = collector.to_prometheus_format();

    // Check that output contains expected metrics
    EXPECT_TRUE(output.find("rag_p50_latency_ms") != std::string::npos);
    EXPECT_TRUE(output.find("rag_p95_latency_ms") != std::string::npos);
    EXPECT_TRUE(output.find("rag_p99_latency_ms") != std::string::npos);
    EXPECT_TRUE(output.find("rag_cache_hit_rate") != std::string::npos);
    EXPECT_TRUE(output.find("# TYPE rag_p50_latency_ms gauge") != std::string::npos);
}

TEST(PrometheusFormat, ContainsValues) {
    MetricsCollector collector;

    collector.record_latency(25.0);
    collector.record_gpu_stats(75.5, 256.0);

    auto output = collector.to_prometheus_format();

    EXPECT_TRUE(output.find("25") != std::string::npos);
    EXPECT_TRUE(output.find("75.5") != std::string::npos);
    EXPECT_TRUE(output.find("256") != std::string::npos);
}

// ========== Test AlertRule ==========

TEST(AlertRule, BasicRule) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "TestLatency";
    rule.metric = "p99_latency_ms";
    rule.condition = ">";
    rule.threshold = 1000.0;
    rule.duration_seconds = 60;
    rule.severity = AlertSeverity::WARNING;

    manager.add_rule(rule);

    // Should not trigger yet
    CoreMetrics metrics;
    metrics.p99_latency_ms = 500.0;

    auto alerts = manager.check_and_alert(metrics);
    EXPECT_TRUE(alerts.empty());
}

TEST(AlertRule, AddMultipleRules) {
    AlertManager manager;

    AlertRule rule1;
    rule1.name = "LatencyRule";
    rule1.metric = "p99_latency_ms";
    rule1.condition = ">";
    rule1.threshold = 1000.0;
    rule1.severity = AlertSeverity::WARNING;

    AlertRule rule2;
    rule2.name = "CacheRule";
    rule2.metric = "cache_hit_rate";
    rule2.condition = "<";
    rule2.threshold = 0.5;
    rule2.severity = AlertSeverity::INFO;

    manager.add_rule(rule1);
    manager.add_rule(rule2);

    CoreMetrics metrics;
    metrics.p99_latency_ms = 2000.0;
    metrics.cache_hit_rate = 0.8;

    auto alerts = manager.check_and_alert(metrics);

    EXPECT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].rule_name, "LatencyRule");
}

// ========== Test CheckAlert ==========

TEST(CheckAlert, LatencyThreshold) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "HighLatency";
    rule.metric = "p99_latency_ms";
    rule.condition = ">";
    rule.threshold = 1000.0;
    rule.severity = AlertSeverity::WARNING;

    manager.add_rule(rule);

    CoreMetrics metrics;
    metrics.p99_latency_ms = 1500.0;

    auto alerts = manager.check_and_alert(metrics);

    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].rule_name, "HighLatency");
    EXPECT_EQ(alerts[0].severity, AlertSeverity::WARNING);
    EXPECT_DOUBLE_EQ(alerts[0].value, 1500.0);
}

TEST(CheckAlert, CacheHitRate) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "LowCacheHit";
    rule.metric = "cache_hit_rate";
    rule.condition = "<";
    rule.threshold = 0.3;
    rule.severity = AlertSeverity::CRITICAL;

    manager.add_rule(rule);

    CoreMetrics metrics;
    metrics.cache_hit_rate = 0.2;

    auto alerts = manager.check_and_alert(metrics);

    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].rule_name, "LowCacheHit");
    EXPECT_EQ(alerts[0].severity, AlertSeverity::CRITICAL);
}

TEST(CheckAlert, GPUUtilization) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "HighGPU";
    rule.metric = "gpu_utilization";
    rule.condition = ">";
    rule.threshold = 95.0;
    rule.severity = AlertSeverity::CRITICAL;

    manager.add_rule(rule);

    CoreMetrics metrics;
    metrics.gpu_utilization = 98.5;

    auto alerts = manager.check_and_alert(metrics);

    ASSERT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].rule_name, "HighGPU");
    EXPECT_DOUBLE_EQ(alerts[0].value, 98.5);
}

TEST(CheckAlert, MultipleConditions) {
    AlertManager manager;

    AlertRule rule1;
    rule1.name = "LatencyRule";
    rule1.metric = "p99_latency_ms";
    rule1.condition = ">";
    rule1.threshold = 1000.0;
    rule1.severity = AlertSeverity::WARNING;

    AlertRule rule2;
    rule2.name = "CacheRule";
    rule2.metric = "cache_hit_rate";
    rule2.condition = "<";
    rule2.threshold = 0.5;
    rule2.severity = AlertSeverity::WARNING;

    manager.add_rule(rule1);
    manager.add_rule(rule2);

    CoreMetrics metrics;
    metrics.p99_latency_ms = 2000.0;
    metrics.cache_hit_rate = 0.2;

    auto alerts = manager.check_and_alert(metrics);

    EXPECT_EQ(alerts.size(), 2);
}

TEST(CheckAlert, NoAlert) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "LatencyRule";
    rule.metric = "p99_latency_ms";
    rule.condition = ">";
    rule.threshold = 1000.0;
    rule.severity = AlertSeverity::WARNING;

    manager.add_rule(rule);

    CoreMetrics metrics;
    metrics.p99_latency_ms = 500.0;

    auto alerts = manager.check_and_alert(metrics);

    EXPECT_TRUE(alerts.empty());
}

TEST(CheckAlert, ActiveAlerts) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "HighLatency";
    rule.metric = "p99_latency_ms";
    rule.condition = ">";
    rule.threshold = 1000.0;
    rule.severity = AlertSeverity::WARNING;

    manager.add_rule(rule);

    CoreMetrics metrics;
    metrics.p99_latency_ms = 2000.0;

    manager.check_and_alert(metrics);

    auto active = manager.get_active_alerts();
    EXPECT_EQ(active.size(), 1);
    EXPECT_EQ(active[0].rule_name, "HighLatency");
}

TEST(CheckAlert, ClearAlerts) {
    AlertManager manager;

    AlertRule rule;
    rule.name = "HighLatency";
    rule.metric = "p99_latency_ms";
    rule.condition = ">";
    rule.threshold = 1000.0;
    rule.severity = AlertSeverity::WARNING;

    manager.add_rule(rule);

    CoreMetrics metrics1;
    metrics1.p99_latency_ms = 2000.0;
    manager.check_and_alert(metrics1);

    manager.clear_alerts();

    auto active = manager.get_active_alerts();
    EXPECT_TRUE(active.empty());
}

// ========== Test DefaultRules ==========

TEST(DefaultRules, AllDefaultRulesAdded) {
    AlertManager manager;
    manager.add_default_rules();

    CoreMetrics metrics;

    // These should trigger alerts based on default rules
    metrics.p99_latency_ms = 6000.0;  // > 5000 for VeryHighLatency
    metrics.cache_hit_rate = 0.1;     // < 0.3 for LowCacheHitRate
    metrics.gpu_utilization = 98.0;   // > 95 for HighGPUUtilization

    auto alerts = manager.check_and_alert(metrics);

    // Should trigger VeryHighLatency, LowCacheHitRate, and HighGPUUtilization
    EXPECT_EQ(alerts.size(), 3);

    // Check severity levels
    bool has_critical = false;
    int warning_count = 0;

    for (const auto& alert : alerts) {
        if (alert.severity == AlertSeverity::CRITICAL) {
            has_critical = true;
        } else if (alert.severity == AlertSeverity::WARNING) {
            warning_count++;
        }
    }

    EXPECT_TRUE(has_critical);
    EXPECT_EQ(warning_count, 2);
}

TEST(DefaultRules, NoAlertsWhenHealthy) {
    AlertManager manager;
    manager.add_default_rules();

    CoreMetrics metrics;
    metrics.p99_latency_ms = 500.0;
    metrics.cache_hit_rate = 0.7;
    metrics.gpu_utilization = 50.0;

    auto alerts = manager.check_and_alert(metrics);

    EXPECT_TRUE(alerts.empty());
}

TEST(DefaultRules, HighLatencyAlert) {
    AlertManager manager;
    manager.add_default_rules();

    CoreMetrics metrics;
    metrics.p99_latency_ms = 3000.0;  // > 2000 for HighLatency

    auto alerts = manager.check_and_alert(metrics);

    EXPECT_EQ(alerts.size(), 1);
    EXPECT_EQ(alerts[0].rule_name, "HighLatency");
}

// ========== Test Factory Functions ==========

TEST(FactoryFunctions, CreateMetricsCollector) {
    auto collector = create_metrics_collector();
    ASSERT_NE(collector, nullptr);

    collector->record_latency(10.0);
    auto metrics = collector->collect();
    EXPECT_EQ(metrics.p50_latency_ms, 10.0);
}

TEST(FactoryFunctions, CreateAlertManager) {
    auto manager = create_alert_manager();
    ASSERT_NE(manager, nullptr);

    manager->add_default_rules();

    CoreMetrics metrics;
    metrics.p99_latency_ms = 6000.0;

    auto alerts = manager->check_and_alert(metrics);
    EXPECT_FALSE(alerts.empty());
}

}  // namespace test
}  // namespace rag
