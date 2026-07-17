/**
 * @file main.cpp
 * @brief C++ std::thread 基础演示
 *
 * 本文件演示：
 * 1. std::thread 创建与基本操作
 * 2. join() 与 detach() 的区别
 * 3. 线程局部存储（thread_local）
 * 4. 线程 ID（std::this_thread::get_id）
 * 5. 互斥量保护共享数据
 * 6. lambda 作为线程函数
 *
 * 学习目标：
 * - 理解线程的创建和管理
 * - 掌握 join/detach 的生命周期差异
 * - 理解线程局部存储的作用域
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>
#include <atomic>
#include <string>
#include <functional>
#include <future>

// ================================================================
// 辅助函数
// ================================================================

// 打印带标题的分隔符
void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

// 打印线程 ID 信息
void print_thread_id(const std::string& label) {
    std::cout << label << " -> thread ID: "
              << std::this_thread::get_id()
              << " (std::hash: " << std::hash<std::thread::id>{}(std::this_thread::get_id())
              << ")\n";
}

// ================================================================
// 第一部分：std::thread 基础
// ================================================================

/**
 * @brief 简单的线程函数
 */
void thread_func(int id) {
    std::cout << "  [Worker " << id << "] 启动，休眠 100ms\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "  [Worker " << id << "] 完成\n";
}

/**
 * @brief 带参数和返回值的线程函数
 */
int thread_with_args(int a, int b) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return a + b;
}

void demo_basic_thread() {
    print_separator("1. std::thread 基础");

    // 获取主线程 ID
    std::cout << "主线程 ID: " << std::this_thread::get_id() << "\n\n";

    // 创建多个线程
    std::vector<std::thread> workers;
    const int num_threads = 3;

    std::cout << "创建 " << num_threads << " 个工作线程...\n";
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back(thread_func, i);
    }

    // 等待所有线程完成（join）
    std::cout << "主线程等待所有工作线程完成...\n";
    for (auto& t : workers) {
        if (t.joinable()) {
            t.join();
        }
    }
    std::cout << "所有工作线程已完成\n";

    // 使用 async 获取返回值
    std::cout << "\n使用 std::async 获取返回值:\n";
    std::future<int> fut = std::async(std::launch::async, thread_with_args, 10, 20);
    int result = fut.get();
    std::cout << "  异步计算结果: 10 + 20 = " << result << "\n";
}

// ================================================================
// 第二部分：join 与 detach
// ================================================================

std::atomic<bool> g_detach_flag{false};

void detach_worker(int id) {
    std::cout << "  [Detach Worker " << id << "] 开始执行\n";
    for (int i = 0; i < 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "  [Detach Worker " << id << "] 步骤 " << i + 1 << "/3\n";
    }
    std::cout << "  [Detach Worker " << id << "] 完成\n";
}

void demo_join_detach() {
    print_separator("2. join() 与 detach()");

    // ----- join() 示例 -----
    std::cout << "--- join() 示例 ---\n";
    {
        std::thread t1(detach_worker, 1);
        std::cout << "  主线程: t1.joinable() = " << std::boolalpha << t1.joinable() << "\n";
        std::cout << "  主线程: 等待 t1 完成...\n";
        t1.join();  // 主线程阻塞直到 t1 完成
        std::cout << "  t1 已完成并回收资源\n";
        // t1 现在不可 joinable
        std::cout << "  主线程: t1.joinable() = " << t1.joinable() << "\n";
    }
    std::cout << "  join() 块结束\n\n";

    // ----- detach() 示例 -----
    std::cout << "--- detach() 示例 ---\n";
    {
        std::thread t2(detach_worker, 2);
        std::cout << "  主线程: t2.joinable() = " << t2.joinable() << "\n";
        std::cout << "  主线程: 调用 detach()，不等待完成\n";
        t2.detach();  // 线程独立运行，主线程不阻塞
        std::cout << "  主线程: t2.detach() 后，t2.joinable() = " << std::boolalpha << t2.joinable() << "\n";
    }
    // t2 在后台独立运行，不再与主线程关联
    std::cout << "  detach() 块结束，t2 在后台继续运行\n";
    std::cout << "  注意: detach() 后，主线程不应再访问线程局部存储\n";

    // 给后台线程一些时间完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// ================================================================
// 第三部分：线程局部存储（thread_local）
// ================================================================

/**
 * @brief 线程局部变量演示
 *
 * thread_local 变量的特点：
 * - 每个线程有独立的实例
 * - 线程结束时自动销毁
 * - 用于避免锁竞争（如线程局部的计数器）
 */
thread_local int g_thread_local_counter = 0;
int g_global_counter = 0;
std::mutex g_counter_mutex;

void increment_counters(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        // 线程局部变量：每个线程独立计数
        ++g_thread_local_counter;

        // 全局变量：需要互斥锁保护
        std::lock_guard<std::mutex> lock(g_counter_mutex);
        ++g_global_counter;
    }
}

void demo_thread_local() {
    print_separator("3. 线程局部存储 (thread_local)");

    const int iterations = 1000;

    std::cout << "启动 4 个线程，每个线程递增 " << iterations << " 次\n\n";

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(increment_counters, iterations);
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "结果:\n";
    std::cout << "  - g_global_counter = " << g_global_counter
              << " (预期: " << 4 * iterations << ")\n";
    std::cout << "  - g_thread_local_counter = " << g_thread_local_counter
              << " (每个线程独立，在主线程中为 0)\n";
    std::cout << "\n说明:\n";
    std::cout << "  - thread_local 变量在每个线程中有独立存储\n";
    std::cout << "  - 在主线程中访问 g_thread_local_counter = 0\n";
    std::cout << "  - 线程局部存储常用于 TLS（Thread-Local Storage）\n";
}

// ================================================================
// 第四部分：线程 ID
// ================================================================

void demo_thread_id() {
    print_separator("4. 线程 ID");

    std::cout << "主线程 ID: " << std::this_thread::get_id() << "\n\n";

    // 在线程函数中获取自己的 ID
    auto id_worker = [](int worker_id) {
        std::cout << "  Worker " << worker_id << " 的线程 ID: "
                  << std::this_thread::get_id() << "\n";
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back(id_worker, i);
    }

    // 获取已启动线程的 ID
    std::cout << "\n主线程获取子线程 ID:\n";
    for (int i = 0; i < 3; ++i) {
        std::thread& t = threads[i];
        if (t.joinable()) {
            std::cout << "  threads[" << i << "].get_id() = " << t.get_id() << "\n";
        }
    }

    for (auto& t : threads) {
        t.join();
    }

    std::cout << "\n线程 ID 的用途:\n";
    std::cout << "  - 调试：标识哪个线程在执行\n";
    std::cout << "  - 线程局部存储的键管理\n";
    std::cout << "  - 比较是否同一线程: std::this_thread::get_id() == other.get_id()\n";
}

// ================================================================
// 第五部分：lambda 作为线程函数
// ================================================================

void demo_lambda_thread() {
    print_separator("5. Lambda 作为线程函数");

    int shared_data = 100;
    std::mutex data_mutex;

    // 使用 lambda 捕获外部变量
    auto lambda_thread = [&shared_data, &data_mutex](int multiplier) {
        std::cout << "  Lambda 线程: 初始 shared_data = " << shared_data << "\n";
        {
            std::lock_guard<std::mutex> lock(data_mutex);
            shared_data *= multiplier;
        }
        std::cout << "  Lambda 线程: 修改后 shared_data = " << shared_data << "\n";
    };

    std::thread t1(lambda_thread, 2);
    std::thread t2(lambda_thread, 3);

    t1.join();
    t2.join();

    std::cout << "\n主线程: 最终 shared_data = " << shared_data << "\n";
    std::cout << "  顺序执行: 100 * 2 * 3 = " << 100 * 2 * 3 << "\n";
    std::cout << "  并发执行结果取决于线程完成顺序\n";
}

// ================================================================
// 主函数
// ================================================================

int main() {
    std::cout << "C++ std::thread 基础演示\n";
    std::cout << "========================\n";

    demo_basic_thread();
    demo_join_detach();
    demo_thread_local();
    demo_thread_id();
    demo_lambda_thread();

    print_separator("演示完成");
    std::cout << "本演示覆盖了以下主题：\n";
    std::cout << "1. std::thread 创建与 join\n";
    std::cout << "2. join() vs detach() 的生命周期差异\n";
    std::cout << "3. thread_local 线程局部存储\n";
    std::cout << "4. std::this_thread::get_id 获取线程 ID\n";
    std::cout << "5. lambda 表达式作为线程函数\n";
    std::cout << "\n";

    // 等待后台线程完成（避免输出混乱）
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    return 0;
}
