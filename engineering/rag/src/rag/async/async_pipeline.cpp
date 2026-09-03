// engineering/rag/src/rag/async/async_pipeline.cpp

#include "rag/async_pipeline.h"
#include "rag/logger.h"
#include <algorithm>

namespace rag {

// ========== AsyncPipeline ==========

AsyncPipeline::AsyncPipeline(
    std::shared_ptr<RetrievalPipeline> pipeline,
    std::shared_ptr<RetrievalCache> cache,
    const AsyncConfig& config)
    : pipeline_(pipeline), cache_(cache), config_(config) {

    thread_pool_ = std::make_shared<ThreadPool>(config);
}

AsyncPipeline::~AsyncPipeline() = default;

std::future<PipelineResult> AsyncPipeline::execute_async(
    const std::string& query,
    int top_k,
    int priority) {

    // 检查缓存
    if (cache_) {
        auto cached = cache_->get(query);
        if (cached.has_value()) {
            std::promise<PipelineResult> p;
            PipelineResult result;
            result.success = true;
            result.results = cached->results;
            result.from_cache = true;
            p.set_value(result);

            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.cached_queries++;

            return p.get_future();
        }
    }

    // 提交到线程池
    AsyncTask task;
    task.query = query;
    task.top_k = top_k;
    task.priority = priority;
    task.submit_time = std::chrono::steady_clock::now();

    auto future = task.promise.get_future();

    {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.total_queries++;
        stats_.active_queries++;
    }

    std::thread([this, task = std::move(task)]() mutable {
        auto start = std::chrono::high_resolution_clock::now();

        auto result = execute_internal(task.query, task.top_k);

        auto end = std::chrono::high_resolution_clock::now();
        double latency_ms = std::chrono::duration<double, std::milli>(
            end - start).count();

        // 更新统计
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.active_queries--;
            latencies_.push_back(latency_ms);

            // 保持最近 1000 个延迟数据
            if (latencies_.size() > 1000) {
                latencies_.erase(latencies_.begin());
            }

            // 计算百分位
            if (!latencies_.empty()) {
                std::sort(latencies_.begin(), latencies_.end());
                size_t idx50 = static_cast<size_t>(latencies_.size() * 0.5);
                size_t idx95 = static_cast<size_t>(latencies_.size() * 0.95);
                stats_.p50_latency_ms = latencies_[idx50];
                stats_.p95_latency_ms = latencies_[idx95];

                double sum = 0;
                for (auto t : latencies_) sum += t;
                stats_.avg_latency_ms = sum / latencies_.size();
            }
        }

        // 缓存结果
        if (result.success && cache_) {
            StageOutput output;
            output.status = StageOutput::Status::SUCCESS;
            output.results = result.results;
            cache_->put(task.query, output);
        }

        task.promise.set_value(result);
    }).detach();

    return future;
}

std::vector<std::future<PipelineResult>> AsyncPipeline::execute_batch_async(
    const std::vector<std::string>& queries,
    int top_k) {

    std::vector<std::future<PipelineResult>> futures;
    futures.reserve(queries.size());

    for (const auto& query : queries) {
        futures.push_back(execute_async(query, top_k));
    }

    return futures;
}

PipelineResult AsyncPipeline::execute_internal(const std::string& query, int top_k) {
    if (pipeline_) {
        return pipeline_->execute(query, top_k);
    }

    PipelineResult result;
    result.success = false;
    result.error_message = "Pipeline not configured";
    return result;
}

AsyncPipeline::Stats AsyncPipeline::get_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void AsyncPipeline::reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = {};
    latencies_.clear();
}

void AsyncPipeline::update_config(const AsyncConfig& config) {
    config_ = config;
}

std::shared_ptr<AsyncPipeline> create_async_pipeline(
    std::shared_ptr<RetrievalPipeline> pipeline,
    const AsyncConfig& config) {

    return std::make_shared<AsyncPipeline>(pipeline, nullptr, config);
}

}  // namespace rag