/**
 * @file main.cpp
 * @brief C++ 内存模型演示：内存序 / happens-before / 同步原语 / 数据竞争
 *
 * 演示内容：
 * 1. 原子操作与六种内存序
 * 2. happens-before 关系
 * 3. mutex / atomic / condition_variable 同步原语
 * 4. 数据竞争（data race）的识别与消除
 */

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ============================================================================
// 一、数据竞争演示
// ============================================================================

// 无同步保护的全局变量，演示 data race
volatile int shared_counter = 0;

// 有问题的并发写入：未定义行为
void unsafe_increment(int iterations) {
    for (int i = 0; i < iterations; ++i) {
        // 读取-修改-写入序列不是原子操作，存在 data race
        int tmp = shared_counter;  // 读
        std::this_thread::sleep_for(1ns);  // 让竞争更容易暴露
        shared_counter = tmp + 1;  // 写
    }
}

// ============================================================================
// 二、原子操作与内存序
// ============================================================================

std::atomic<int> x{0};
std::atomic<int> y{0};

// seq_cst (Sequential Consistency) - 最严格，保证全局顺序
void seq_cst_writer() {
    x.store(1, std::memory_order_seq_cst);
    y.store(1, std::memory_order_seq_cst);
}

void seq_cst_reader() {
    // 读取 y，再读取 x；seq_cst 保证看到一致的全局顺序
    while (y.load(std::memory_order_seq_cst) == 0) {
        std::this_thread::yield();
    }
    int rx = x.load(std::memory_order_seq_cst);
    std::cout << "[seq_cst] x=" << rx << " (必定为1)\n";
}

// acquire/release 语义 - 松弛但仍保证 happens-before
std::atomic<bool> ready{false};
std::atomic<int> data{0};

void release_writer() {
    data.store(100, std::memory_order_relaxed);
    ready.store(true, std::memory_order_release);  // release 屏障
}

void acquire_reader() {
    // acquire 屏障：保证看到 release 之前的所有写入
    while (!ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    int val = data.load(std::memory_order_relaxed);
    std::cout << "[acq_rel] data=" << val << " (必定为100)\n";
}

// conssume 语义 - 仅保证依赖顺序（编译器优化更激进）
std::atomic<const char*> ptr{nullptr};

void consume_producer() {
    const char local_data[] = "test data";
    // 通过 ptr 传播 local_data 的地址
    ptr.store(local_data, std::memory_order_consume);
}

void consume_consumer() {
    // 仅当 ptr 加载成功后，才安全读取 *ptr
    // C++20 前实现不一致，实际中推荐使用 acquire
    const char* p = ptr.load(std::memory_order_consume);
    if (p != nullptr) {
        // 注意：这里的行为依赖编译器对 consume 的实现
        std::cout << "[consume] p is not null\n";
    }
}

// relaxed 语义 - 仅保证原子性，不保证内存序
std::atomic<int> counter{0};

void relaxed_counter() {
    for (int i = 0; i < 10000; ++i) {
        // relaxed 只保证 counter 的增减是原子的
        // 不保证其他线程何时看到更新
        counter.fetch_add(1, std::memory_order_relaxed);
    }
}

// ============================================================================
// 三、happens-before 关系验证
// ============================================================================

std::atomic<int> a{0};
std::atomic<int> b{0};

void hb_verification() {
    // 验证 happens-before 传递律：A hb B，B hb C => A hb C
    a.store(1, std::memory_order_relaxed);
    // 假设 b 在 a 之后写入（程序顺序隐含 hb）
    b.store(1, std::memory_order_relaxed);

    // 读取顺序反映 happens-before 关系
    int rb = b.load(std::memory_order_relaxed);
    int ra = a.load(std::memory_order_relaxed);
    std::cout << "[hb] a=" << ra << " b=" << rb;
    std::cout << " (若 a=0 且 b=1，则违反程序顺序)\n";
}

// ============================================================================
// 四、同步原语演示
// ============================================================================

std::mutex mtx;
int protected_value = 0;
std::condition_variable cv;
bool ready_flag = false;

// 生产者：持有锁写入数据后通知消费者
void producer(int value) {
    {
        std::lock_guard<std::mutex> lock(mtx);
        protected_value = value;
        ready_flag = true;
    }  // 锁在此处释放
    cv.notify_one();  // 唤醒等待的消费者
}

// 消费者：等待条件变量，收到通知后读取数据
void consumer() {
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [] { return ready_flag; });  // 原子地释放锁并等待
    std::cout << "[sync] protected_value=" << protected_value << "\n";
}

// ============================================================================
// 主函数
// ============================================================================

int main() {
    std::cout << "=== C++ Memory Model Demo ===\n\n";

    // 1. 数据竞争演示
    std::cout << "[1] Data Race Demo (未定义行为):\n";
    {
        shared_counter = 0;
        std::thread t1(unsafe_increment, 100);
        std::thread t2(unsafe_increment, 100);
        t1.join();
        t2.join();
        std::cout << "  shared_counter=" << shared_counter;
        std::cout << " (期望200，实际可能小于200)\n\n";
    }

    // 2. seq_cst 演示
    std::cout << "[2] Sequential Consistency:\n";
    {
        x = 0; y = 0;
        std::thread w(seq_cst_writer);
        std::thread r(seq_cst_reader);
        w.join();
        r.join();
    }

    // 3. acquire/release 演示
    std::cout << "[3] Acquire/Release:\n";
    {
        ready = false;
        data = 0;
        std::thread w(release_writer);
        std::thread r(acquire_reader);
        w.join();
        r.join();
    }

    // 4. relaxed 演示
    std::cout << "[4] Relaxed Ordering (仅保证原子性):\n";
    {
        counter = 0;
        std::vector<std::thread> threads;
        for (int i = 0; i < 4; ++i) {
            threads.emplace_back(relaxed_counter);
        }
        for (auto& t : threads) {
            t.join();
        }
        std::cout << "  counter=" << counter.load() << " (应为40000)\n\n";
    }

    // 5. happens-before 验证
    std::cout << "[5] Happens-Before Verification:\n";
    hb_verification();
    std::cout << "\n";

    // 6. 同步原语演示
    std::cout << "[6] Mutex + Condition Variable:\n";
    {
        std::thread prod(producer, 2024);
        std::thread cons(consumer);
        prod.join();
        cons.join();
    }

    std::cout << "\n=== Demo Complete ===\n";
    return 0;
}
