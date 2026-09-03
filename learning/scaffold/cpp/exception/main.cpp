// exception scaffold — 异常处理
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [basic]    — try/catch/throw 基本机制
//   [custom]   — 自定义异常类（继承 std::exception）
//   [unwind]   — 栈展开（stack unwinding）+ RAII 析构
//   [noexcept] — noexcept 关键字（性能优化）
//   [catchall] — catch(...) 兜底

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstdio>
#include <vector>

// === [custom] 自定义异常类 ===
class MyError : public std::exception {
public:
    explicit MyError(const std::string& msg) : msg_(msg) {}
    const char* what() const noexcept override {
        return msg_.c_str();
    }
private:
    std::string msg_;
};

// === [unwind] RAII 守卫：异常时自动清理 ===
class FileGuard {
public:
    FileGuard(const char* name) : name_(name) {
        printf("  [unwind] FileGuard: open %s\n", name_);
    }
    ~FileGuard() {
        printf("  [unwind] FileGuard: close %s\n", name_);
    }
private:
    const char* name_;
};

// === [basic] 可能抛出异常的函数 ===
static double divide(double a, double b) {
    if (b == 0) {
        throw std::runtime_error("division by zero");
    }
    return a / b;
}

// === [noexcept] 承诺不抛出异常的函数 ===
static int safe_add(int a, int b) noexcept {
    return a + b;            // int 加法不会抛
}

// === 嵌套调用 + 栈展开 ===
static void inner_func(bool throw_it) {
    FileGuard g("inner_resource");
    printf("  [unwind] inner_func running\n");
    if (throw_it) {
        throw MyError("inner_func failed");
    }
    printf("  [unwind] inner_func success\n");
}

static void outer_func(bool throw_it) {
    FileGuard g("outer_resource");
    printf("  [unwind] outer_func running\n");
    inner_func(throw_it);
    printf("  [unwind] outer_func success\n");
}

int main() {
    // === [basic] try/catch/throw ===
    printf("[basic] === try / catch / throw ===\n");
    try {
        double r = divide(10.0, 0.0);
        printf("  result = %.2f\n", r);
    } catch (const std::exception& e) {
        printf("  caught: %s\n", e.what());
    }

    try {
        double r = divide(10.0, 3.0);
        printf("  result = %.2f (no throw)\n", r);
    } catch (...) {
        printf("  catch-all\n");
    }

    // === [custom] 自定义异常类 ===
    printf("\n[custom] === 自定义异常类 ===\n");
    try {
        throw MyError("something bad happened");
    } catch (const std::exception& e) {
        printf("  caught MyError: %s\n", e.what());
    }

    // === [unwind] 栈展开 + RAII 析构 ===
    printf("\n[unwind] === 栈展开 ===\n");
    try {
        outer_func(true);           // 触发异常
    } catch (const std::exception& e) {
        printf("  caught at main: %s\n", e.what());
    }
    printf("  [unwind] 异常后正常继续\n");

    printf("\n  --- 无异常的路径 ---\n");
    try {
        outer_func(false);
    } catch (...) {}

    // === [noexcept] noexcept ===
    printf("\n[noexcept] === noexcept 不抛异常 ===\n");
    int sum = safe_add(3, 4);
    printf("  safe_add(3, 4) = %d\n", sum);
    // 若 safe_add 真抛了，std::terminate() 直接终止
    printf("  noexcept(true): 编译器可优化（不展开栈）\n");
    printf("  noexcept(false): 默认行为（可能抛）\n");

    // === [catchall] catch(...) 兜底 ===
    printf("\n[catchall] === catch(...) 兜底 ===\n");
    try {
        throw 42;                  // 抛 int（非 std::exception）
    } catch (const std::exception& e) {
        printf("  caught std::exception: %s\n", e.what());
    } catch (...) {
        printf("  catch-all caught non-exception\n");
    }

    printf("\n=== PASS ===\n");
    return 0;
}