/**
 * @file main.cpp
 * @brief 条件变量（Condition Variable）演示
 *
 * 演示内容：
 * 1. 生产者-消费者模型
 * 2. 线程间通知机制
 * 3. 虚假唤醒（Spurious Wakeup）避免
 *
 * 编译：g++ -std=c++17 -pthread main.cpp -o cond_var
 */

#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;

/* ========================================================================
 * 示例1：基本线程通知（信号量模式）
 * ======================================================================== */

class SimpleNotifier {
private:
    std::mutex mtx_;
    std::condition_variable cv_;
    std::atomic<bool> ready_{false};

public:
    // 通知方：设置标志并通知
    void notify() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            ready_ = true;
        }
        cv_.notify_one();  // 唤醒一个等待线程
    }

    // 等待方：等待通知（使用 while 循环防止虚假唤醒）
    void wait_for_signal() {
        std::unique_lock<std::mutex> lock(mtx_);
        // 关键：必须使用 while，不能用 if
        // 虚假唤醒是 POSIX 标准允许的行为
        while (!ready_) {
            cv_.wait(lock);  // 释放锁并等待
        }
        ready_ = false;  // 重置标志
    }
};

/* ========================================================================
 * 示例2：生产者-消费者队列
 * ======================================================================== */

template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool done_{false};

public:
    // 生产者：放入数据
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();  // 通知消费者有新数据
    }

    // 消费者：取出数据
    // 返回 false 表示队列已关闭且为空
    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        
        // 关键：使用 while 循环处理虚假唤醒
        while (queue_.empty() && !done_) {
            cv_.wait(lock);
        }
        
        if (queue_.empty() && done_) {
            return false;  // 队列已关闭
        }
        
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 关闭队列，唤醒所有等待者
    void close() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            done_ = true;
        }
        cv_.notify_all();  // 唤醒所有等待线程
    }
};

/* ========================================================================
 * 示例3：虚假唤醒演示
 * ======================================================================== */

void demonstrate_spurious_wakeup() {
    std::mutex mtx;
    std::condition_variable cv;
    int counter = 0;

    // 线程A：等待计数器达到5
    std::thread waiter([&]() {
        std::unique_lock<std::mutex> lock(mtx);
        // 如果这里用 if，虚假唤醒会导致计数器还未达到5就继续执行
        // 用 while 才能确保条件真正满足
        while (counter < 5) {
            cv.wait(lock);
        }
        std::cout << "[Waiter] 计数器达到 " << counter << "，继续执行\n";
    });

    // 线程B：递增计数器
    std::thread incrementer([&]() {
        for (int i = 0; i < 10; ++i) {
            std::this_thread::sleep_for(100ms);
            {
                std::lock_guard<std::mutex> lock(mtx);
                ++counter;
                std::cout << "[Incrementer] counter = " << counter << "\n";
            }
            cv.notify_one();
        }
    });

    // 主线程等待完成
    waiter.join();
    incrementer.join();
    std::this_thread::sleep_for(2s);
}

/* ========================================================================
 * 主函数
 * ======================================================================== */

int main() {
    std::cout << "=== 条件变量演示 ===\n\n";

    // 示例1：基本通知
    std::cout << "--- 示例1：基本线程通知 ---\n";
    SimpleNotifier notifier;
    
    std::thread waiter([&]() {
        std::cout << "[Waiter] 等待通知...\n";
        notifier.wait_for_signal();
        std::cout << "[Waiter] 收到通知！\n";
    });

    std::this_thread::sleep_for(500ms);
    std::cout << "[Notifier] 发送通知\n";
    notifier.notify();
    waiter.join();
    std::this_thread::sleep_for(200ms);

    // 示例2：生产者-消费者
    std::cout << "\n--- 示例2：生产者-消费者队列 ---\n";
    ThreadSafeQueue<int> queue;

    // 消费者
    std::thread consumer([&]() {
        int value;
        while (queue.pop(value)) {
            std::cout << "[Consumer] 消费: " << value << "\n";
            std::this_thread::sleep_for(150ms);
        }
        std::cout << "[Consumer] 队列已关闭，退出\n";
    });

    // 生产者
    std::thread producer([&]() {
        for (int i = 1; i <= 5; ++i) {
            std::this_thread::sleep_for(100ms);
            std::cout << "[Producer] 生产: " << i << "\n";
            queue.push(i);
        }
        queue.close();  // 关闭队列
    });

    std::this_thread::sleep_for(2s);
    consumer.join();
    producer.join();

    // 示例3：虚假唤醒
    std::cout << "\n--- 示例3：虚假唤醒演示 ---\n";
    demonstrate_spurious_wakeup();

    std::cout << "\n=== 演示完成 ===\n";
    return 0;
}
