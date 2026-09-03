#include <rag/benchmark.h>
#include <rag/pipeline.h>

#include <algorithm>
#include <cmath>
#include <future>
#include <thread>
#include <numeric>

namespace rag {

// ========== LatencyRecorder 实现 ==========

void LatencyRecorder::record(double ms) {
    latencies_.push_back(ms);
}

void LatencyRecorder::reset() {
    latencies_.clear();
}

double LatencyRecorder::avg() const {
    if (latencies_.empty()) return 0.0;
    double sum = std::accumulate(latencies_.begin(), latencies_.end(), 0.0);
    return sum / latencies_.size();
}

double LatencyRecorder::min() const {
    if (latencies_.empty()) return 0.0;
    return *std::min_element(latencies_.begin(), latencies_.end());
}

double LatencyRecorder::max() const {
    if (latencies_.empty()) return 0.0;
    return *std::max_element(latencies_.begin(), latencies_.end());
}

double LatencyRecorder::percentile(double p) const {
    if (latencies_.empty()) return 0.0;
    if (p <= 0.0) return min();
    if (p >= 100.0) return max();

    std::vector<double> sorted = latencies_;
    std::sort(sorted.begin(), sorted.end());

    double idx = (p / 100.0) * (sorted.size() - 1);
    size_t lower = static_cast<size_t>(std::floor(idx));
    size_t upper = static_cast<size_t>(std::ceil(idx));

    if (lower == upper) return sorted[lower];

    double fraction = idx - lower;
    return sorted[lower] * (1.0 - fraction) + sorted[upper] * fraction;
}

// ========== RAGBenchmark 实现 ==========

RAGBenchmark::RAGBenchmark(std::shared_ptr<RetrievalPipeline> pipeline)
    : pipeline_(std::move(pipeline)) {}

void RAGBenchmark::warmup() {
    if (!pipeline_) return;
    for (int i = 0; i < 10; ++i) {
        pipeline_->execute("warmup query", 5);
    }
}

double RAGBenchmark::measure_latency(const std::string& query, int top_k) {
    if (!pipeline_) return 0.0;

    auto start = std::chrono::high_resolution_clock::now();
    pipeline_->execute(query, top_k);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return duration.count() / 1000.0;
}

double RAGBenchmark::measure_qps(int num_requests, int num_threads) {
    if (!pipeline_ || num_requests <= 0 || num_threads <= 0) return 0.0;

    std::atomic<int> completed{0};

    int batch = num_requests / num_threads;

    auto worker = [this, &completed, batch]() {
        for (int i = 0; i < batch; ++i) {
            pipeline_->execute("benchmark query", 5);
            completed++;
        }
    };

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    double seconds = duration.count() / 1000.0;
    if (seconds <= 0.0) return 0.0;

    return completed.load() / seconds;
}

BenchmarkResult RAGBenchmark::run(const BenchmarkConfig& config) {
    BenchmarkResult result;
    result.iterations = config.num_iterations;

    if (!pipeline_) return result;

    // Warmup
    for (int i = 0; i < config.num_warmup; ++i) {
        pipeline_->execute("warmup", 5);
    }

    recorder_.reset();

    // Run benchmark
    for (int i = 0; i < config.num_iterations; ++i) {
        std::string query = config.queries.empty() ? "benchmark query" : config.queries[i % config.queries.size()];

        auto start = std::chrono::high_resolution_clock::now();
        try {
            pipeline_->execute(query, 10);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double latency_ms = duration.count() / 1000.0;
            recorder_.record(latency_ms);
            result.successful++;
        } catch (...) {
            result.failed++;
        }
    }

    // Calculate statistics
    if (!recorder_.values().empty()) {
        result.avg_latency_ms = recorder_.avg();
        result.min_latency_ms = recorder_.min();
        result.max_latency_ms = recorder_.max();
        result.p50_latency_ms = recorder_.percentile(50);
        result.p95_latency_ms = recorder_.percentile(95);
        result.p99_latency_ms = recorder_.percentile(99);
    }

    // Calculate QPS
    double total_time_s = recorder_.avg() * config.num_iterations / 1000.0;
    result.total_time_s = total_time_s;
    if (total_time_s > 0) {
        result.qps = config.num_iterations / total_time_s;
    }

    return result;
}

BenchmarkResult RAGBenchmark::evaluate_recall(
    const std::vector<std::string>& queries,
    const std::vector<std::vector<std::string>>& expected_results) {
    BenchmarkResult result;
    result.iterations = static_cast<int>(queries.size());

    if (!pipeline_ || queries.size() != expected_results.size()) {
        return result;
    }

    double total_recall = 0.0;
    double total_precision = 0.0;
    double total_mrr = 0.0;
    double total_ndcg = 0.0;

    for (size_t i = 0; i < queries.size(); ++i) {
        try {
            auto pipeline_result = pipeline_->execute(queries[i], 10);
            const auto& retrieved = pipeline_result.results;
            const auto& expected = expected_results[i];

            // Calculate recall@10
            int relevant_retrieved = 0;
            for (const auto& doc : retrieved) {
                if (std::find(expected.begin(), expected.end(), doc.chunk.id) != expected.end()) {
                    relevant_retrieved++;
                }
            }
            double recall = expected.empty() ? 0.0 : static_cast<double>(relevant_retrieved) / expected.size();
            total_recall += recall;

            // Calculate precision@5
            int prec_count = 0;
            size_t k = std::min(static_cast<size_t>(5), retrieved.size());
            for (size_t j = 0; j < k; ++j) {
                if (std::find(expected.begin(), expected.end(), retrieved[j].chunk.id) != expected.end()) {
                    prec_count++;
                }
            }
            double precision = retrieved.empty() ? 0.0 : static_cast<double>(prec_count) / 5.0;
            total_precision += precision;

            // Calculate MRR
            for (size_t j = 0; j < retrieved.size(); ++j) {
                if (std::find(expected.begin(), expected.end(), retrieved[j].chunk.id) != expected.end()) {
                    total_mrr += 1.0 / (j + 1);
                    break;
                }
            }

            // Calculate NDCG@10
            double dcg = 0.0;
            size_t dcg_k = std::min(static_cast<size_t>(10), retrieved.size());
            for (size_t j = 0; j < dcg_k; ++j) {
                double rel = (std::find(expected.begin(), expected.end(), retrieved[j].chunk.id) != expected.end()) ? 1.0 : 0.0;
                dcg += rel / std::log2(j + 2);
            }
            double idcg = 0.0;
            size_t idcg_k = std::min(static_cast<size_t>(10), expected.size());
            for (size_t j = 0; j < idcg_k; ++j) {
                idcg += 1.0 / std::log2(j + 2);
            }
            double ndcg = (idcg > 0.0) ? dcg / idcg : 0.0;
            total_ndcg += ndcg;

            result.successful++;
        } catch (...) {
            result.failed++;
        }
    }

    int n = static_cast<int>(queries.size());
    if (n > 0) {
        result.recall_at_10 = total_recall / n;
        result.precision_at_5 = total_precision / n;
        result.mrr = total_mrr / n;
        result.ndcg_at_10 = total_ndcg / n;
    }

    return result;
}

// ========== 工厂函数 ==========

std::unique_ptr<RAGBenchmark> create_benchmark(std::shared_ptr<RetrievalPipeline> pipeline) {
    return std::make_unique<RAGBenchmark>(std::move(pipeline));
}

}  // namespace rag