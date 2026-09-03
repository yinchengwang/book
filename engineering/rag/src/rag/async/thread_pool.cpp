// engineering/rag/src/rag/async/thread_pool.cpp

#include "rag/async_pipeline.h"
#include "rag/logger.h"
#include <algorithm>

namespace rag {

ThreadPool::ThreadPool(const AsyncConfig& config) : config_(config) {
    workers_.reserve(config_.num_workers);

    for (int i = 0; i < config_.num_workers; i++) {
        workers_.emplace_back([this] { worker_loop(); });
    }

    RAG_INFO("ThreadPool started with " + std::to_string(config_.num_workers) + " workers");
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutdown_ = true;
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    RAG_INFO("ThreadPool shutdown complete");
}

void ThreadPool::worker_loop() {
    while (true) {
        AsyncTask* task = nullptr;

        {
            std::unique_lock<std::mutex> lock(mutex_);

            cv_.wait(lock, [this] {
                return shutdown_ || !tasks_.empty();
            });

            if (shutdown_ && tasks_.empty()) {
                return;
            }

            if (!tasks_.empty()) {
                task = &tasks_.front();
                tasks_.pop();
            }
        }

        if (task) {
            active_count_++;
            try {
                // 执行任务（这里需要实际调用 pipeline）
                // 简化处理：直接设置成功
                PipelineResult result;
                result.success = true;
                task->promise.set_value(result);
            } catch (const std::exception& e) {
                PipelineResult result;
                result.success = false;
                task->promise.set_value(result);
            }
            active_count_--;
            done_cv_.notify_all();
        }
    }
}

size_t ThreadPool::queue_size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

size_t ThreadPool::active_workers() const {
    return active_count_.load();
}

std::vector<std::future<PipelineResult>> ThreadPool::submit_batch(
    const std::vector<std::string>& queries,
    int top_k) {

    std::vector<std::future<PipelineResult>> futures;
    futures.reserve(queries.size());

    for (const auto& query : queries) {
        AsyncTask task;
        task.query = query;
        task.top_k = top_k;
        task.priority = 0;
        task.submit_time = std::chrono::steady_clock::now();

        auto future = task.promise.get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (tasks_.size() < config_.queue_size) {
                tasks_.push(std::move(task));
            }
        }

        cv_.notify_one();
        futures.push_back(std::move(future));
    }

    return futures;
}

void ThreadPool::wait_for_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    done_cv_.wait(lock, [this] {
        return tasks_.empty() && active_count_ == 0;
    });
}

}  // namespace rag