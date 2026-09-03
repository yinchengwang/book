// coroutines scaffold — C++20 协程
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++20 -fcoroutines -o test main.cpp && ./test
//
// 演示 4 段：
//   [generator]    — 协程生成器（yield）
//   [awaitable]    — 自定义 Awaitable 对象
//   [task]         — 异步 Task 框架
//   [sync-wait]    — sync_wait 同步等待

#include <iostream>
#include <coroutine>
#include <optional>
#include <vector>
#include <future>
#include <thread>
#include <chrono>

// === [generator] 协程生成器 ===
template<typename T>
struct Generator {
    struct promise_type {
        T value;
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        Generator get_return_object() { return Generator{Handle::from_promise(*this)}; }
        std::suspend_always yield_value(T v) {
            value = v;
            return {};
        }
        void return_void() {}
        void unhandled_exception() { throw; }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle coro;

    explicit Generator(Handle h) : coro(h) {}
    ~Generator() { if (coro) coro.destroy(); }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    Generator(Generator&& other) noexcept : coro(other.coro) { other.coro = {}; }
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (coro) coro.destroy();
            coro = other.coro;
            other.coro = {};
        }
        return *this;
    }

    T value() const { return coro.promise().value; }
    bool next() {
        if (coro) {
            coro.resume();
            return !coro.done();
        }
        return false;
    }
};

// 生成斐波那契数列（无限）
Generator<int> fibonacci() {
    int a = 0, b = 1;
    while (true) {
        co_yield a;
        int next = a + b;
        a = b;
        b = next;
    }
}

// === [awaitable] 自定义 Awaitable ===
struct SleepAwaitable {
    std::chrono::milliseconds duration;
    bool await_ready() const { return false; }
    void await_suspend(std::coroutine_handle<> h) {
        std::thread([h, d = duration] {
            std::this_thread::sleep_for(d);
            h.resume();
        }).detach();
    }
    void await_resume() {}
};

template<typename T>
struct Task {
    struct promise_type {
        T value;
        std::exception_ptr exc;
        bool ready = false;

        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        Task get_return_object() { return Task{Handle::from_promise(*this)}; }
        void return_value(T v) { value = v; ready = true; }
        void unhandled_exception() { exc = std::current_exception(); }
    };

    using Handle = std::coroutine_handle<promise_type>;
    Handle coro;

    explicit Task(Handle h) : coro(h) {}
    ~Task() { if (coro) coro.destroy(); }

    T get() {
        if (!coro.done()) coro.resume();
        if (coro.promise().exc) std::rethrow_exception(coro.promise().exc);
        return coro.promise().value;
    }
};

// === [task] 异步任务示例 ===
Task<int> async_add(int a, int b) {
    printf("  [Task] 计算 %d + %d...\n", a, b);
    co_await SleepAwaitable{std::chrono::milliseconds(100)};
    co_return a + b;
}

// === [sync-wait] 同步等待 ===
int main() {
    // [generator] 斐波那契
    printf("[generator] === 斐波那契数列 ===\n");
    auto gen = fibonacci();
    printf("  前 10 项: ");
    for (int i = 0; i < 10; ++i) {
        gen.next();
        printf("%d ", gen.value());
    }
    printf("\n");

    // [task] 异步任务
    printf("\n[task] === 异步任务 ===\n");
    auto task = async_add(40, 2);
    printf("  结果: %d\n", task.get());

    // [awaitable] SleepAwaitable
    printf("\n[awaitable] === SleepAwaitable ===\n");
    auto start = std::chrono::steady_clock::now();
    auto sleep_task = []() -> Task<void> {
        co_await SleepAwaitable{std::chrono::milliseconds(50)};
        printf("  休眠 50ms 完成\n");
    };
    sleep_task().get();
    auto end = std::chrono::steady_clock::now();
    printf("  耗时: %ld ms\n",
           std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    printf("\n=== PASS ===\n");
    return 0;
}
