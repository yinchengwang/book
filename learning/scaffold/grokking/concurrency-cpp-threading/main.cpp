/**
 * concurrency-cpp-threading — C++17 多线程基础
 *
 * 演示 std::thread / std::future & std::promise /
 *       std::packaged_task / std::async
 *
 * 编译：g++ -std=c++17 -Wall -Wextra -pthread -o main main.cpp
 */

#include <chrono>
#include <future>
#include <iostream>
#include <numeric>
#include <string>
#include <thread>
#include <vector>

// ============================================================
// 1. std::thread — 基本线程创建与 join
// ============================================================
void hello_thread(int id)
{
    std::cout << "  [thread " << id << "] 运行中, thread_id="
              << std::this_thread::get_id() << "\n";
}

void demo_basic_thread()
{
    std::cout << "=== 1. std::thread 基本用法 ===\n";
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
        threads.emplace_back(hello_thread, i);

    for (auto &t : threads)
        t.join();

    std::cout << "  全部线程已 join\n\n";
}

// ============================================================
// 2. std::future & std::promise — 从线程返回值
// ============================================================
void demo_promise_future()
{
    std::cout << "=== 2. future & promise ===\n";

    std::promise<int> prom;
    std::future<int>  fut = prom.get_future();

    std::thread t([&prom]() {
        // 模拟耗时计算
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        prom.set_value(42);
    });

    int result = fut.get(); // 阻塞直到值就绪
    std::cout << "  promise 返回: " << result << "\n";
    t.join();
    std::cout << "\n";
}

// ============================================================
// 3. std::packaged_task — 包装可调用对象
// ============================================================
int sum_range(int start, int end)
{
    int s = 0;
    for (int i = start; i <= end; ++i)
        s += i;
    return s;
}

void demo_packaged_task()
{
    std::cout << "=== 3. packaged_task ===\n";

    std::packaged_task<int(int, int)> task(sum_range);
    std::future<int>                  result = task.get_future();

    std::thread t(std::move(task), 1, 100);

    std::cout << "  sum(1,100) = " << result.get() << "\n";
    t.join();
    std::cout << "\n";
}

// ============================================================
// 4. std::async — 高级异步任务
// ============================================================
int slow_square(int x)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    return x * x;
}

void demo_async()
{
    std::cout << "=== 4. std::async ===\n";

    // 默认策略（可能同步也可能异步）
    auto f1 = std::async(slow_square, 5);
    auto f2 = std::async(slow_square, 10);
    auto f3 = std::async(slow_square, 15);

    // 三个 async 可能并行执行
    std::cout << "  5^2 = " << f1.get() << "\n";
    std::cout << " 10^2 = " << f2.get() << "\n";
    std::cout << " 15^2 = " << f3.get() << "\n";

    // std::launch::async 强制异步
    auto f4 = std::async(std::launch::async, [](int n) {
        return n * n * n;
    }, 7);
    std::cout << "  7^3 = " << f4.get() << "\n";
    std::cout << "\n";
}

// ============================================================
// 主函数
// ============================================================
int main()
{
    std::cout << "C++17 多线程基础演示\n";
    std::cout << "====================\n\n";

    demo_basic_thread();
    demo_promise_future();
    demo_packaged_task();
    demo_async();

    std::cout << "全部演示完成.\n";
    return 0;
}
