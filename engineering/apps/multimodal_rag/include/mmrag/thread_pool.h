/**
 * @file thread_pool.h
 * @brief 简单线程池实现
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace mmrag {

/**
 * @brief 简单的固定大小线程池
 *
 * 使用 std::function 任务队列。后台线程循环取出任务执行。
 * 析构时优雅关闭：等待队列中所有任务执行完毕或超时。
 */
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = 4);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 提交任务。如果队列已满返回 false，任务不会被执行。
    bool submit(std::function<void()> task);

    // 当前待处理任务数（含正在执行）
    size_t pending() const;

    // 当前工作线程数
    size_t size() const { return workers_.size(); }

    // 关闭池子（等待所有任务完成）
    void shutdown();

private:
    void worker_loop();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::vector<std::thread> workers_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> shutdown_called_{false};
    size_t pending_{0};
};

}  // namespace mmrag