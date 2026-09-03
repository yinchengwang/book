/*
 * main.cpp - 无锁数据结构演示
 *
 * 演示内容：
 *   1. 基于 CAS 的无锁栈 (Lock-Free Stack)
 *   2. ABA 问题的产生与解决思路
 *   3. 内存回收策略 (Hazard Pointer / Reference Count)
 *
 * 编译：g++ -std=c++17 -lpthread main.cpp -o lockfree
 */

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// 一、无锁栈 (Lock-Free Stack) 基于 CAS 循环
// ─────────────────────────────────────────────────────────────────────────────

template <typename T>
struct Node {
    T data;
    std::atomic<Node<T>*> next;

    explicit Node(const T& v) : data(v), next(nullptr) {}
};

// 无锁栈：push/pop 均使用 CAS 保证原子性
template <typename T>
class LockFreeStack {
    std::atomic<Node<T>*> head_{nullptr};

public:
    // 入栈：总是插到头部
    // CAS 循环：如果 head_ 还是旧值，就把它换成新节点；否则重试
    void push(const T& value) {
        Node<T>* new_node = new Node<T>(value);
        Node<T>* old_head;
        do {
            old_head = head_.load(std::memory_order_relaxed);
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
        // compare_exchange_weak 成功时，old_head 是交换前的旧值，
        // 失败时 old_head 被更新为当前 head_ 的最新值，循环重试
    }

    // 出栈：返回 nullptr 表示栈空
    T* pop() {
        Node<T>* old_head;
        Node<T>* next;
        do {
            old_head = head_.load(std::memory_order_acquire);
            if (old_head == nullptr) return nullptr;  // 栈空
            next = old_head->next.load(std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(
            old_head, next,
            std::memory_order_release,
            std::memory_order_relaxed));
        // 成功后 old_head 已脱离链表，可以安全回收
        T* result = new T(old_head->data);
        delete old_head;  // 本例简化为直接 delete，实际应配合 Hazard Pointer
        return result;
    }

    bool empty() const { return head_.load(std::memory_order_relaxed) == nullptr; }
};

// ─────────────────────────────────────────────────────────────────────────────
// 二、ABA 问题的演示
// ─────────────────────────────────────────────────────────────────────────────

/*
 * ABA 问题产生过程：
 *   线程 A: 读到 head = A，准备 pop
 *   线程 B: pop(A) 成功，回收 A，再 push(B)，此时 head = B
 *   线程 C: push(A)，此时 head = A（地址复用！）
 *   线程 A: CAS(head, A, A->next) 成功 —— 但此时栈状态已被 B 篡改
 *
 * 结果：线程 A 以为自己 pop 的是 B 之后的节点，实际上越过了 B，
 *       导致 B 对应的数据丢失（被泄漏或重复使用）。
 */

// 演示 ABA 问题的简化代码
void demo_aba_problem() {
    std::atomic<int*> head{nullptr};
    // 模拟节点的 next 指针（栈中简单模拟）
    int a_next_addr = 0, b_next_addr = 0, c_next_addr = 0;
    int a = 1, b = 2, c = 3;

    // 初始：head -> a -> nullptr
    head.store(&a, std::memory_order_relaxed);

    // 线程 B：pop a，再 push b
    int* old_head = head.load(std::memory_order_acquire);
    // 模拟 pop a：将 head 指向 a.next（这里用 0 表示 nullptr）
    head.store(nullptr, std::memory_order_release);
    // 此时 head -> nullptr，a 被 pop

    // 线程 B 再次 push b
    head.store(&b, std::memory_order_release);
    // 此时 head -> b -> nullptr

    // 线程 C：push c（或复用 A 的地址）
    head.store(&a, std::memory_order_release);  // A 地址被复用
    // 此时 head -> a -> nullptr（但 a 已经被 pop 过了！）

    printf("[ABA 演示] head 地址 = %p，值 = %d\n", (void*)head.load(), *head.load());
    printf("[ABA 演示] 注意：A 的地址被复用，但 A 已被 pop 过一次\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// 三、内存回收策略：带引用计数的节点
// ─────────────────────────────────────────────────────────────────────────────

/*
 * Hazard Pointer 思路（参考 engineering/src/db/concurrency/mvcc_version.c）：
 *   - 每个线程持有一个 "危险指针" 槽位，记录正在访问的节点地址
 *   - pop 时，不立即 delete 节点，而是标记为待回收
 *   - 等没有线程再持有该节点的 Hazard Pointer 时，才真正释放
 *
 * 引用计数思路（参考 MVCC xmin/xmax 可见性判断）：
 *   - 每个节点持有一个原子引用计数
 *   - reader 增加计数，用完后递减
 *   - 当计数归零且已脱离链表时，才可安全回收
 */

template <typename T>
struct RefCountedNode {
    T data;
    std::atomic<RefCountedNode<T>*> next;
    std::atomic<int> ref_count{1};  // 引用计数

    explicit RefCountedNode(const T& v) : data(v), next(nullptr) {}

    // 增加引用
    void add_ref() { ref_count.fetch_add(1, std::memory_order_relaxed); }

    // 释放引用，返回是否应该删除自身
    bool release() {
        if (ref_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
            // 最后持有者负责删除
            delete this;
            return true;
        }
        return false;
    }
};

template <typename T>
class RefCountedStack {
    std::atomic<RefCountedNode<T>*> head_{nullptr};

public:
    void push(const T& value) {
        RefCountedNode<T>* new_node = new RefCountedNode<T>(value);
        RefCountedNode<T>* old_head;
        do {
            old_head = head_.load(std::memory_order_relaxed);
            new_node->next.store(old_head, std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(
            old_head, new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    T* pop() {
        RefCountedNode<T>* old_head;
        RefCountedNode<T>* next;
        do {
            old_head = head_.load(std::memory_order_acquire);
            if (old_head == nullptr) return nullptr;
            // 先增加引用，防止在 CAS 期间被其他线程回收
            old_head->add_ref();
            next = old_head->next.load(std::memory_order_relaxed);
        } while (!head_.compare_exchange_weak(
            old_head, next,
            std::memory_order_release,
            std::memory_order_relaxed));
        // old_head 已脱离链表，减少引用（可能触发 delete）
        T* result = new T(old_head->data);
        old_head->release();
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// 四、并发测试
// ─────────────────────────────────────────────────────────────────────────────

template <typename Stack>
void concurrent_test(const char* name, int num_threads, int ops_per_thread) {
    Stack stack;
    std::vector<std::thread> threads;
    std::atomic<int> success_push{0};
    std::atomic<int> success_pop{0};

    auto start = std::chrono::steady_clock::now();

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                int value = t * ops_per_thread + i;
                stack.push(value);
                success_push.fetch_add(1, std::memory_order_relaxed);

                auto ptr = stack.pop();
                if (ptr != nullptr) {
                    success_pop.fetch_add(1, std::memory_order_relaxed);
                    delete ptr;
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    auto end = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();

    printf("[%s] push=%d pop=%d 时间=%lldms\n",
           name, success_push.load(), success_pop.load(), (long long)ms);
}

// ─────────────────────────────────────────────────────────────────────────────
// 主函数
// ─────────────────────────────────────────────────────────────────────────────

int main() {
    printf("===== 无锁数据结构演示 =====\n\n");

    // 1. 单线程基本功能测试
    printf("--- 1. 单线程基本测试 ---\n");
    LockFreeStack<int> stack;
    for (int i = 0; i < 5; ++i) stack.push(i);
    while (!stack.empty()) {
        int* v = stack.pop();
        if (v) { printf("pop: %d\n", *v); delete v; }
    }

    // 2. ABA 问题演示
    printf("\n--- 2. ABA 问题演示 ---\n");
    demo_aba_problem();

    // 3. 并发压测
    printf("\n--- 3. 并发压测 ---\n");
    concurrent_test<LockFreeStack<int>>("LockFreeStack", 4, 10000);
    concurrent_test<RefCountedStack<int>>("RefCountedStack", 4, 10000);

    printf("\n===== 演示结束 =====\n");
    return 0;
}
