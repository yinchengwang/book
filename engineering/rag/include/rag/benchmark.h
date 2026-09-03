#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>

namespace rag {

// ========== 基准测试配置 ==========

struct BenchmarkConfig {
    int num_iterations = 100;
    int num_warmup = 10;
    int num_threads = 1;
    bool enable_gpu = true;
    std::vector<std::string> queries;
};

// ========== 基准测试结果 ==========

struct BenchmarkResult {
    // 延迟统计
    double avg_latency_ms = 0;
    double min_latency_ms = 0;
    double max_latency_ms = 0;
    double p50_latency_ms = 0;
    double p95_latency_ms = 0;
    double p99_latency_ms = 0;

    // 吞吐量
    double qps = 0;
    double throughput_mbps = 0;

    // 准确性（如果有标注数据）
    double recall_at_10 = 0;
    double precision_at_5 = 0;
    double mrr = 0;
    double ndcg_at_10 = 0;

    // 系统信息
    int iterations = 0;
    int successful = 0;
    int failed = 0;
    double total_time_s = 0;
};

// ========== 延迟记录器 ==========

class LatencyRecorder {
public:
    void record(double ms);
    void reset();

    double avg() const;
    double min() const;
    double max() const;
    double percentile(double p) const;

    std::vector<double> values() const { return latencies_; }

private:
    std::vector<double> latencies_;
};

// ========== 基准测试器 ==========

class RAGBenchmark {
public:
    RAGBenchmark(std::shared_ptr<class RetrievalPipeline> pipeline);

    // 运行基准测试
    BenchmarkResult run(const BenchmarkConfig& config);

    // 单独测试延迟
    void warmup();
    double measure_latency(const std::string& query, int top_k);

    // 测试吞吐量
    double measure_qps(int num_requests, int num_threads);

    // 测试召回率（需要标注数据）
    BenchmarkResult evaluate_recall(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<std::string>>& expected_results);

private:
    std::shared_ptr<class RetrievalPipeline> pipeline_;
    LatencyRecorder recorder_;
};

// ========== 工厂函数 ==========

std::unique_ptr<RAGBenchmark> create_benchmark(
    std::shared_ptr<RetrievalPipeline> pipeline);

}  // namespace rag