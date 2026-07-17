/**
 * GDB 调试演示代码
 * 包含多种有意设计的 bug，用于学习 GDB 调试技巧
 */

#include <iostream>
#include <vector>
#include <string>

// ============================================================
// 全局变量区
// ============================================================

// BUG 1: 全局计数器递增错误 - 应该是 ++counter，但写成了 counter++
int global_counter = 0;

// BUG 2: 全局数组，用于演示越界访问
const int ARRAY_SIZE = 5;
int global_array[ARRAY_SIZE] = {10, 20, 30, 40, 50};

// ============================================================
// BUG 1: 全局计数器递增错误
// ============================================================

#ifdef ENABLE_BUG
// 有 bug 版本：表达式返回原值，counter 永远是 0
int increment_counter_bug() {
    global_counter = global_counter++;
    return global_counter;
}
#else
// 修复版本：正确递增
int increment_counter_fixed() {
    return ++global_counter;
}
#endif

// ============================================================
// BUG 2: 数组越界访问
// ============================================================

int access_array_out_of_bounds(int index) {
    // BUG: 没有边界检查，当 index >= ARRAY_SIZE 或 index < 0 时越界
    std::cout << "  访问数组索引 " << index << ": " << global_array[index] << std::endl;
    return global_array[index];
}

// ============================================================
// BUG 3: 递归函数 base case 错误
// ============================================================

#ifdef ENABLE_BUG
// BUG 版本：base case 是 n == 0，但递归调用传入 n-1 后永不等于 0
// 当 n 为负数时，会无限递归直到栈溢出
long long factorial_bug(int n) {
    if (n == 0) {
        return 1;  // base case
    }
    return n * factorial_bug(n - 1);
}
#else
// 修复版本：正确的 base case
long long factorial_fixed(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial_fixed(n - 1);
}
#endif

// ============================================================
// BUG 4: 线程竞态条件 (C++17)
// ============================================================

#ifdef ENABLE_BUG
#include <thread>
#include <atomic>

std::atomic<int> shared_value{0};

// 两个线程同时修改 shared_value，没有同步
void increment_shared() {
    for (int i = 0; i < 1000; ++i) {
        // 竞态：read-modify-write 不是原子操作
        int temp = shared_value.load();
        temp = temp + 1;
        shared_value.store(temp);
    }
}

void race_condition_demo() {
    std::cout << "\n=== 线程竞态演示 ===" << std::endl;
    shared_value = 0;
    
    std::thread t1(increment_shared);
    std::thread t2(increment_shared);
    
    t1.join();
    t2.join();
    
    std::cout << "预期值: 2000, 实际值: " << shared_value.load() << std::endl;
    std::cout << "(如果值小于 2000，说明存在竞态条件)" << std::endl;
}
#endif

// ============================================================
// 主函数：演示各个 bug
// ============================================================

void demo_counter_bug() {
    std::cout << "\n=== 全局计数器递增 bug ===" << std::endl;
    global_counter = 0;
    
#ifdef ENABLE_BUG
    std::cout << "调用 5 次 increment_counter_bug():" << std::endl;
    for (int i = 0; i < 5; ++i) {
        int result = increment_counter_bug();
        std::cout << "  第 " << (i + 1) << " 次调用，counter = " << result << std::endl;
    }
#else
    std::cout << "调用 5 次 increment_counter_fixed():" << std::endl;
    for (int i = 0; i < 5; ++i) {
        int result = increment_counter_fixed();
        std::cout << "  第 " << (i + 1) << " 次调用，counter = " << result << std::endl;
    }
#endif
}

void demo_array_out_of_bounds() {
    std::cout << "\n=== 数组越界访问 bug ===" << std::endl;
    std::cout << "数组大小: " << ARRAY_SIZE << std::endl;
    
    // 正常访问
    std::cout << "正常访问索引 0-4:" << std::endl;
    for (int i = 0; i < ARRAY_SIZE; ++i) {
        access_array_out_of_bounds(i);
    }
    
    // 越界访问 (BUG 触发点)
    std::cout << "\n越界访问索引 10 (超出数组范围):" << std::endl;
    access_array_out_of_bounds(10);  // 未定义行为
    
    // 负索引越界
    std::cout << "\n越界访问索引 -1 (负数索引):" << std::endl;
    access_array_out_of_bounds(-1);  // 未定义行为
}

void demo_recursive_bug() {
    std::cout << "\n=== 递归函数 base case bug ===" << std::endl;
    
#ifdef ENABLE_BUG
    // BUG: 传入负数会无限递归
    std::cout << "尝试计算 factorial_bug(-1):" << std::endl;
    std::cout << "  (这会导致无限递归直到栈溢出)" << std::endl;
    // factorial_bug(-1);  // 取消注释会导致段错误
    std::cout << "  已注释，实际调用会崩溃" << std::endl;
#else
    std::cout << "正确计算 factorial_fixed(5) = " << factorial_fixed(5) << std::endl;
#endif
}

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "GDB 调试演示程序" << std::endl;
    std::cout << "编译选项: " 
#ifdef ENABLE_BUG
              << "ENABLE_BUG (包含 bug)"
#else
              << "修复版本"
#endif
              << std::endl;
    std::cout << "========================================" << std::endl;
    
    // 演示各种 bug
    demo_counter_bug();
    demo_array_out_of_bounds();
    demo_recursive_bug();
    
#ifdef ENABLE_BUG
#ifdef __cpp_lib_atomic
    race_condition_demo();
#endif
#endif
    
    std::cout << "\n程序正常结束。" << std::endl;
    return 0;
}
