// engineering/rag/include/rag/async_pipeline.h
#pragma once

#include "rag/pipeline.h"
#include "rag/retrieval_cache.h"
#include <memory>
#include <vector>
#include <future>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace rag {

struct AsyncConfig {
    int num_workers = 4;           // 工作线程数
    int queue_size = 100;          // 任务队列大小
    int max_concurrent = 10;       // 最大并发数
    bool priority_enabled = true;  // 优先级队列
    bool enable_batch = true;      // 批量处理
    int batch_timeout_ms = 100;    // 批量超时
};

struct AsyncTask {
    std::string query;
    int top_k;
    int priority;
    std::promise<PipelineResult> promise;
    std::chrono::steady_clock::time_point submit_time;
};

class ThreadPool {
public:
    explicit ThreadPool(const AsyncConfig& config);
    ~ThreadPool();

    // 提交任务
    template<typename F>
    std::future<typename std::result_of<F()>::type> submit(F&& func, int priority = 0);

    // 批量提交
    std::vector<std::future<PipelineResult>> submit_batch(
        const std::vector<std::string>& queries,
        int top_k);

    // 状态
    size_t queue_size() const;
    size_t active_workers() const;
    bool is_shutdown() const { return shutdown_; }

    // 优雅关闭
    void shutdown();
    void wait_for_idle();

private:
    void worker_loop();

    AsyncConfig config_;
    std::vector<std::thread> workers_;
    std::queue<AsyncTask> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    std::atomic<bool> shutdown_{false};
    std::atomic<size_t> active_count_{0};

    // 优先级队列支持
    struct PriorityComparator {
        bool operator()(const AsyncTask* a, const AsyncTask* b) const {
            // 数字越大优先级越高
            return a->priority < b->priority;
        }
    };
};

class AsyncPipeline {
public:
    AsyncPipeline(std::shared_ptr<RetrievalPipeline> pipeline,
                  std::shared_ptr<RetrievalCache> cache,
                  const AsyncConfig& config);
    ~AsyncPipeline();

    // 异步执行
    std::future<PipelineResult> execute_async(
        const std::string& query,
        int top_k = 5,
        int priority = 0);

    // 批量异步执行
    std::vector<std::future<PipelineResult>> execute_batch_async(
        const std::vector<std::string>& queries,
        int top_k = 5);

    // 统计
    struct Stats {
        uint64_t total_queries = 0;
        uint64_t cached_queries = 0;
        uint64_t active_queries = 0;
        double avg_latency_ms = 0.0;
        double p50_latency_ms = 0.0;
        double p95_latency_ms = 0.0;
    };
    Stats get_stats() const;
    void reset_stats();

    // 配置
    void update_config(const AsyncConfig& config);
    const AsyncConfig& config() const { return config_; }

private:
    PipelineResult execute_internal(const std::string& query, int top_k);

    std::shared_ptr<RetrievalPipeline> pipeline_;
    std::shared_ptr<RetrievalCache> cache_;
    std::shared_ptr<ThreadPool> thread_pool_;
    AsyncConfig config_;

    // 统计
    mutable std::mutex stats_mutex_;
    Stats stats_;
    std::vector<double> latencies_;
};

// ========== 工厂函数 ==========

std::shared_ptr<AsyncPipeline> create_async_pipeline(
    std::shared_ptr<RetrievalPipeline> pipeline,
    const AsyncConfig& config = {});

}  // namespace rag