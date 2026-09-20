/**
 * @file thread_pool.cpp
 * @brief 简单线程池实现
 */

#include "mmrag/thread_pool.h"
#include <chrono>

namespace mmrag {

ThreadPool::ThreadPool(size_t num_threads) {
    if (num_threads == 0) num_threads = 1;
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

bool ThreadPool::submit(std::function<void()> task) {
    if (!task || stop_.load()) return false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load()) return false;
        tasks_.push(std::move(task));
        ++pending_;
    }
    cv_.notify_one();
    return true;
}

size_t ThreadPool::pending() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return pending_;
}

void ThreadPool::shutdown() {
    bool expected = false;
    if (!shutdown_called_.compare_exchange_strong(expected, true)) {
        return;  // 已调用过
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_.store(true);
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return stop_.load() || !tasks_.empty(); });

            if (stop_.load() && tasks_.empty()) {
                return;
            }
            if (tasks_.empty()) continue;

            task = std::move(tasks_.front());
            tasks_.pop();
        }

        try {
            task();
        } catch (...) {
            // 吞掉任务中的异常避免线程终止
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (pending_ > 0) --pending_;
        }
    }
}

}  // namespace mmrag