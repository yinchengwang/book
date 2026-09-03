/**
 * @file main.cpp
 * @brief std::atomic 与内存序演示
 *
 * 演示内容：
 * 1. std::atomic 基本操作（load/store/exchange）
 * 2. memory_order_acquire/release 语义
 * 3. memory_order_seq_cst 顺序一致性
 * 4. CAS (Compare-And-Swap) 循环实现无锁算法
 *
 * 编译：g++ -std=c++17 -pthread -o atomic main.cpp
 */

#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <chrono>

// ============================================================================
// 1. 基本原子操作演示
// ============================================================================

/**
 * atomic_counter - 展示基本的原子递增操作
 *
 * std::atomic<int> 保证 counter++ 的原子性，
 * 避免数据竞争 (data race)。
 */
void demo_basic_atomic()
{
    std::cout << "\n========== 1. 基本原子操作 ==========\n";

    std::atomic<int> counter{0};

    // 创建多个线程同时递增
    const int num_threads = 4;
    const int increments_per_thread = 10000;

    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&counter, increments_per_thread]() {
            for (int j = 0; j < increments_per_thread; ++j) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    std::cout << "counter = " << counter.load() << " (期望: "
              << num_threads * increments_per_thread << ")\n";
    std::cout << "原子递增保证了最终结果的正确性。\n";
}

// ============================================================================
// 2. memory_order_acquire/release 语义
// ============================================================================

/**
 * 生产者-消费者模式 - 展示 release-acquire 同步
 *
 * - 生产者使用 release 写入数据
 * - 消费者使用 acquire 读取数据
 * - release 确保之前所有内存写入都对该 release 可见
 * - acquire 确保后续所有内存读取都在 release 之后
 */
struct DataBuffer {
    std::atomic<bool> ready{false};
    int value = 0;
};

void demo_acquire_release()
{
    std::cout << "\n========== 2. Acquire/Release 语义 ==========\n";

    DataBuffer buffer;
    const int num_iterations = 5;

    // 生产者线程
    auto producer = [&buffer, num_iterations]() {
        for (int i = 1; i <= num_iterations; ++i) {
            // 写入数据
            buffer.value = i * 100;
            // release 语义：确保 value 写入对消费者可见
            buffer.ready.store(true, std::memory_order_release);
        }
    };

    // 消费者线程
    auto consumer = [&buffer, num_iterations]() {
        int sum = 0;
        for (int i = 0; i < num_iterations; ++i) {
            // spin-wait，使用 acquire 检查
            while (!buffer.ready.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            sum += buffer.value;
            buffer.ready.store(false, std::memory_order_relaxed);
        }
        std::cout << "消费者收到: " << sum << "\n";
    };

    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();

    std::cout << "release-acquire 保证了生产者写入对消费者的可见性。\n";
}

// ============================================================================
// 3. memory_order_seq_cst 顺序一致性
// ============================================================================

/**
 * seq_cst_demo - 展示顺序一致性语义
 *
 * memory_order_seq_cst 是最严格的内存序，
 * 所有线程看到完全相同的操作顺序。
 */
std::atomic<int> x{0}, y{0};
std::atomic<int> z{0};

void demo_seq_cst()
{
    std::cout << "\n========== 3. Seq_cst 顺序一致性 ==========\n";

    std::atomic<int> seq_x{0}, seq_y{0};

    // 线程 A
    auto thread_a = [&seq_x, &seq_y]() {
        seq_x.store(1, std::memory_order_seq_cst);
        seq_y.store(1, std::memory_order_seq_cst);
    };

    // 线程 B
    auto thread_b = [&seq_x, &seq_y]() {
        // seq_cst 保证：如果看到 seq_y==1，则一定看到 seq_x==1
        int r1 = seq_y.load(std::memory_order_seq_cst);
        int r2 = seq_x.load(std::memory_order_seq_cst);
        std::cout << "线程B: r1=" << r1 << ", r2=" << r2 << "\n";
    };

    std::thread t1(thread_a);
    std::thread t2(thread_b);

    t1.join();
    t2.join();

    std::cout << "seq_cst 提供了全局有序的操作视图。\n";
}

// ============================================================================
// 4. CAS (Compare-And-Swap) 循环
// ============================================================================

/**
 * LockFreeQueue - 无锁栈的简化实现，使用 CAS 循环
 *
 * CAS 操作：
 *   expected = 当前值
 *   desired = 新值
 *   atomic.compare_exchange_weak(expected, desired)
 *
 * 如果 atomic == expected，则 atomic = desired，返回 true
 * 否则，expected = atomic，返回 false（需要重试）
 */
struct Node {
    int value;
    Node* next;
    Node(int v) : value(v), next(nullptr) {}
};

class LockFreeStack {
private:
    std::atomic<Node*> head_{nullptr};

public:
    void push(int value) {
        Node* new_node = new Node(value);
        Node* old_head;

        // CAS 循环：直到成功为止
        do {
            old_head = head_.load(std::memory_order_acquire);
            new_node->next = old_head;
        } while (!head_.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_acquire));
    }

    bool pop(int& result) {
        Node* old_head;

        do {
            old_head = head_.load(std::memory_order_acquire);
            if (old_head == nullptr) {
                return false;  // 栈空
            }
        } while (!head_.compare_exchange_weak(
            old_head, old_head->next,
            std::memory_order_release,
            std::memory_order_acquire));

        result = old_head->value;
        delete old_head;
        return true;
    }

    ~LockFreeStack() {
        int dummy;
        while (pop(dummy)) { /* 清理 */ }
    }
};

void demo_cas_loop()
{
    std::cout << "\n========== 4. CAS 循环无锁栈 ==========\n";

    LockFreeStack stack;

    // 生产者线程
    std::thread producer([&stack]() {
        for (int i = 1; i <= 1000; ++i) {
            stack.push(i);
        }
    });

    // 消费者线程
    std::vector<int> consumed;
    std::thread consumer([&stack, &consumed]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int val;
        while (stack.pop(val)) {
            consumed.push_back(val);
        }
    });

    producer.join();
    consumer.join();

    std::cout << "生产者推送: 1000 个元素\n";
    std::cout << "消费者弹出: " << consumed.size() << " 个元素\n";
    std::cout << "CAS 循环实现了真正的无锁并发栈。\n";
}

// ============================================================================
// 主函数
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "   std::atomic 与内存序演示\n";
    std::cout << "========================================\n";

    demo_basic_atomic();
    demo_acquire_release();
    demo_seq_cst();
    demo_cas_loop();

    std::cout << "\n========================================\n";
    std::cout << "   所有演示完成！\n";
    std::cout << "========================================\n";

    return 0;
}
