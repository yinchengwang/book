// sanitizer 演示代码 - 展示各种内存错误及 sanitizer 检测能力
// 编译方式（Makefile）：
//   make asan    - AddressSanitizer（内存错误）
//   make msan    - MemorySanitizer（未初始化内存）
//   make tsan    - ThreadSanitizer（数据竞争）
//   make usan    - UndefinedBehaviorSanitizer（未定义行为）
//   make all     - 编译全部

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>

// ============================================================
// 1. AddressSanitizer (ASan) - 内存错误检测
// ============================================================

// 1.1 堆缓冲区溢出（Heap Buffer Overflow）
// ASan 检测：读取/写入超出 malloc 分配范围的内存
void demo_heap_overflow() {
    printf("\n--- 1.1 堆缓冲区溢出 ---\n");
    char *buf = (char *)malloc(10);
    strcpy(buf, "Hello");  // 正常
    printf("正常: %s\n", buf);
    buf[10] = 'X';         // ASan: heap-buffer-overflow
    printf("溢出: %c\n", buf[10]);
    free(buf);
}

// 1.2 栈缓冲区溢出（Stack Buffer Overflow）
// ASan 检测：写入超出局部数组范围
void demo_stack_overflow() {
    printf("\n--- 1.2 栈缓冲区溢出 ---\n");
    int arr[4] = {1, 2, 3, 4};
    arr[10] = 999;         // ASan: stack-buffer-overflow
    printf("arr[10] = %d\n", arr[10]);
}

// 1.3 全局缓冲区溢出（Global Buffer Overflow）
// ASan 检测：读写超出全局变量范围
int global_arr[4] = {1, 2, 3, 4};
void demo_global_overflow() {
    printf("\n--- 1.3 全局缓冲区溢出 ---\n");
    global_arr[10] = 999;  // ASan: global-buffer-overflow
    printf("global_arr[10] = %d\n", global_arr[10]);
}

// 1.4 使用后释放（Use After Free）
// ASan 检测：访问已 free 的内存
void demo_use_after_free() {
    printf("\n--- 1.4 使用后释放 ---\n");
    char *p = (char *)malloc(10);
    strcpy(p, "test");
    printf("释放前: %s\n", p);
    free(p);
    printf("释放后: %s\n", p);  // ASan: heap-use-after-free
}

// 1.5 双重释放（Double Free）
// ASan 检测：对同一块内存调用两次 free
void demo_double_free() {
    printf("\n--- 1.5 双重释放 ---\n");
    char *p = (char *)malloc(10);
    free(p);
    free(p);  // ASan: double-free
}

// 1.6 内存泄漏（Memory Leak）
// ASan 检测：分配后未释放的内存
void demo_memory_leak() {
    printf("\n--- 1.6 内存泄漏 ---\n");
    char *leaked = (char *)malloc(100);
    strcpy(leaked, "This memory is never freed");
    printf("泄漏内存: %s\n", leaked);
    // 故意不 free，ASan 会在程序结束时报告
}

// 1.7 返回栈地址（Return Stack Address）
// ASan 检测：返回局部变量的地址
void demo_return_stack_addr() {
    printf("\n--- 1.7 返回栈地址（故意制造错误）---\n");
    char local[] = "local string";
    char *ptr = local;  // 存储栈地址
    printf("返回前栈地址: %p, 内容: %s\n", (void*)ptr, ptr);
    // 正确做法：使用堆分配或静态存储
    // char* ptr = strdup("local string");  // 正确
}

// 1.8 空指针解引用（Null Pointer Dereference）
void demo_null_deref() {
    printf("\n--- 1.8 空指针解引用 ---\n");
    int *p = nullptr;
    *p = 42;  // ASan: 仍可能崩溃，但 ASan 可能检测不到所有情况
}

// 1.9 构造函数异常（Constructor Exception）
// 分配器正确性问题
struct ThrowingStruct {
    ThrowingStruct() { throw 1; }
};
void demo_ctor_exception() {
    printf("\n--- 1.9 构造函数异常 ---\n");
    try {
        ThrowingStruct ts;  // 构造失败，但内存已分配
    } catch (...) {}
    // ASan 可能报告泄漏，因为析构函数未调用
}

// 1.10 错位释放（Invalid Free / Mismatched Allocation）
// ASan 检测：malloc/new 配对错误
void demo_mismatched_free() {
    printf("\n--- 1.10 错位释放 ---\n");
    char *p = (char *)malloc(10);
    delete p;  // ASan: attempting free on address which did not point to start of an allocation
}

// ============================================================
// 2. MemorySanitizer (MSan) - 未初始化内存检测
// ============================================================

// 2.1 使用未初始化变量
void demo_uninit_memory() {
    printf("\n--- 2.1 使用未初始化变量 ---\n");
    int arr[5];
    arr[0] = 100;
    printf("arr[0]=%d arr[1]=%d\n", arr[0], arr[1]);  // MSan: use-of-uninitialized-value
}

// 2.2 从未初始化内存读取
void demo_uninit_read() {
    printf("\n--- 2.2 从未初始化内存读取 ---\n");
    char buf[32];
    // memset(buf, 0, sizeof(buf));  // 注释掉以模拟未初始化
    printf("未初始化: %s\n", buf);  // MSan: use-of-uninitialized-value
}

// 2.3 条件分支使用未初始化值
void demo_uninit_branch() {
    printf("\n--- 2.3 条件分支使用未初始化值 ---\n");
    int x;
    int *p = &x;
    if (*p > 0) {  // MSan: use-of-uninitialized-value
        printf("positive\n");
    } else {
        printf("non-positive\n");
    }
}

// 2.4 逻辑运算中的未初始化值
void demo_uninit_logical() {
    printf("\n--- 2.4 逻辑运算中的未初始化值 ---\n");
    int a, b = 1;
    int c = a && b;  // MSan: use-of-uninitialized-value
    printf("c = %d\n", c);
}

// 2.5 传递未初始化参数
void use_value(int x) { printf("value = %d\n", x); }
void demo_uninit_param() {
    printf("\n--- 2.5 传递未初始化参数 ---\n");
    int uninit;
    use_value(uninit);  // MSan: use-of-uninitialized-value
}

// ============================================================
// 3. ThreadSanitizer (TSan) - 数据竞争检测
// ============================================================

#include <thread>
#include <atomic>
#include <mutex>

std::atomic<int> counter{0};
int shared_var = 0;

// 3.1 经典数据竞争（两个线程同时读写共享变量）
void writer_thread() {
    for (int i = 0; i < 10000; i++) {
        shared_var = 42;  // TSan: data race
    }
}
void reader_thread() {
    for (int i = 0; i < 10000; i++) {
        int tmp = shared_var;  // TSan: data race
        if (tmp != 42) {
            printf("读取到错误值: %d\n", tmp);
        }
    }
}
void demo_data_race() {
    printf("\n--- 3.1 经典数据竞争 ---\n");
    std::thread t1(writer_thread);
    std::thread t2(reader_thread);
    t1.join();
    t2.join();
}

// 3.2 原子变量 vs 普通变量竞争
void demo_atomic_race() {
    printf("\n--- 3.2 原子变量 vs 普通变量竞争 ---\n");
    int normal_var = 0;
    std::thread t1([&]() {
        for (int i = 0; i < 10000; i++) {
            normal_var++;  // TSan: 即使使用 atomic 保护 counter，normal_var 仍会冲突
        }
    });
    std::thread t2([&]() {
        for (int i = 0; i < 10000; i++) {
            counter++;  // 原子操作，无竞争
        }
    });
    t1.join();
    t2.join();
    printf("normal_var = %d, counter = %d\n", normal_var, counter.load());
}

// 3.3 读-读-写竞争
void demo_read_write_race() {
    printf("\n--- 3.3 读-读-写竞争 ---\n");
    int data = 0;
    std::mutex mtx;
    // 错误示例：无锁保护
    std::thread writers[2];
    for (int i = 0; i < 2; i++) {
        writers[i] = std::thread([&]() {
            for (int j = 0; j < 1000; j++) {
                data = j;  // TSan: data race
            }
        });
    }
    std::thread reader([&]() {
        for (int i = 0; i < 1000; i++) {
            volatile int tmp = data;  // TSan: data race
            (void)tmp;
        }
    });
    for (auto &t : writers) t.join();
    reader.join();
}

// 3.4 锁顺序死锁检测（TSan 也可检测某些死锁模式）
#include <mutex>
std::mutex mtx1, mtx2;
void demo_potential_deadlock() {
    printf("\n--- 3.4 潜在死锁风险 ---\n");
    // TSan 不直接检测死锁，但通过竞态分析可发现锁使用模式问题
    // 此处演示正确的锁使用方式
    std::thread t1([]() {
        std::lock(mtx1, mtx2);  // 使用 lock 避免死锁
        std::lock_guard<std::mutex> l1(mtx1, std::adopt_lock);
        std::lock_guard<std::mutex> l2(mtx2, std::adopt_lock);
        printf("线程1持有两把锁\n");
    });
    std::thread t2([]() {
        std::lock(mtx1, mtx2);
        std::lock_guard<std::mutex> l1(mtx1, std::adopt_lock);
        std::lock_guard<std::mutex> l2(mtx2, std::adopt_lock);
        printf("线程2持有两把锁\n");
    });
    t1.join();
    t2.join();
}

// 3.5 释放后使用（线程视角）
void demo_race_after_free() {
    printf("\n--- 3.5 释放后使用 ---\n");
    int *ptr = new int(42);
    std::thread t1([&]() {
        delete ptr;  // TSan: race with t2
    });
    std::thread t2([&]() {
        printf("读取: %d\n", *ptr);  // TSan: race with t1
    });
    t1.join();
    t2.join();
}

// ============================================================
// 4. UndefinedBehaviorSanitizer (USan) - 未定义行为检测
// ============================================================

// 4.1 有符号整数溢出
void demo_signed_overflow() {
    printf("\n--- 4.1 有符号整数溢出 ---\n");
    int x = INT_MAX;
    x = x + 1;  // USan: signed-integer-overflow
    printf("溢出后: %d\n", x);
}

// 4.2 浮点数转换为整数（溢出）
void demo_float_nan() {
    printf("\n--- 4.2 浮点数 NaN 转整数 ---\n");
    double nan_val = 0.0 / 0.0;
    int x = (int)nan_val;  // USan: nan
    printf("NaN 转 int: %d\n", x);
}

// 4.3 除零
void demo_divide_by_zero() {
    printf("\n--- 4.3 除零 ---\n");
    int x = 10;
    int y = 0;
    int z = x / y;  // USan: division-by-zero
    printf("结果: %d\n", z);
}

// 4.4 空指针成员访问
struct S {
    int value;
};
void demo_null_member_access() {
    printf("\n--- 4.4 空指针成员访问 ---\n");
    S *p = nullptr;
    p->value = 42;  // USan: null-dereference
    printf("value = %d\n", p->value);
}

// 4.5 位移越界
void demo_shift_overflow() {
    printf("\n--- 4.5 位移越界 ---\n");
    int x = 1;
    x = x << 100;  // USan: shift-exponent-overflow
    printf("位移后: %d\n", x);
}

// 4.6 无效的位移基值
void demo_shift_base_overflow() {
    printf("\n--- 4.6 无效位移基值 ---\n");
    int x = -1;
    x = x << 2;  // USan: (部分情况下) invalid shift
    printf("位移后: %d\n", x);
}

// 4.7 非布尔值用于条件判断
void demo_invalid_bool() {
    printf("\n--- 4.7 非布尔值用于条件 ---\n");
    int *ptr = (int *)0x1;  // 非空但未对齐
    if (ptr) {  // 通常 OK，但某些平台可能有对齐问题
        printf("ptr 非空\n");
    }
}

// 4.8 返回不完整类型
struct Incomplete;  // 前向声明
Incomplete* return_incomplete() {
    return nullptr;  // OK
}
// Incomplete return_incomplete2() { return {}; }  // USan: incomplete type

// 4.9 alignas 违反
#include <memory>
void demo_alignment_violation() {
    printf("\n--- 4.9 对齐违反 ---\n");
    alignas(16) char buf[32];
    char *misaligned = buf + 1;
    int *int_ptr = (int *)misaligned;  // USan: alignment-assumption
    printf("对齐检查: %p\n", (void *)int_ptr);
}

// 4.10 函数指针违反
void demo_function_type_mismatch() {
    printf("\n--- 4.10 函数类型不匹配 ---\n");
    void (*func_ptr)() = (void (*)())demo_function_type_mismatch;
    // 正确调用
    func_ptr();
}

// ============================================================
// 5. 综合演示入口
// ============================================================

void run_demo(const char *name, void (*fn)()) {
    printf("\n========== %s ==========\n", name);
    try {
        fn();
    } catch (...) {
        printf("异常被捕获\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        std::string mode = argv[1];
        if (mode == "asan") {
            printf("AddressSanitizer 演示模式\n");
            run_demo("堆溢出", demo_heap_overflow);
            run_demo("栈溢出", demo_stack_overflow);
            run_demo("全局溢出", demo_global_overflow);
            run_demo("UAF", demo_use_after_free);
            run_demo("双重释放", demo_double_free);
            run_demo("内存泄漏", demo_memory_leak);
            run_demo("返回栈地址", demo_return_stack_addr);
            run_demo("错位释放", demo_mismatched_free);
        } else if (mode == "msan") {
            printf("MemorySanitizer 演示模式\n");
            run_demo("未初始化读取", demo_uninit_memory);
            run_demo("未初始化字符串", demo_uninit_read);
            run_demo("条件分支", demo_uninit_branch);
            run_demo("逻辑运算", demo_uninit_logical);
            run_demo("传递参数", demo_uninit_param);
        } else if (mode == "tsan") {
            printf("ThreadSanitizer 演示模式\n");
            run_demo("数据竞争", demo_data_race);
            run_demo("原子变量竞争", demo_atomic_race);
            run_demo("读写竞争", demo_read_write_race);
            run_demo("安全锁使用", demo_potential_deadlock);
            run_demo("释放后使用", demo_race_after_free);
        } else if (mode == "usan") {
            printf("UndefinedBehaviorSanitizer 演示模式\n");
            run_demo("有符号溢出", demo_signed_overflow);
            run_demo("NaN转换", demo_float_nan);
            run_demo("除零", demo_divide_by_zero);
            run_demo("空指针成员", demo_null_member_access);
            run_demo("位移越界", demo_shift_overflow);
            run_demo("对齐违反", demo_alignment_violation);
        }
    } else {
        printf("用法: %s <asan|msan|tsan|usan>\n", argv[0]);
        printf("  asan  - AddressSanitizer (内存错误)\n");
        printf("  msan  - MemorySanitizer (未初始化内存)\n");
        printf("  tsan  - ThreadSanitizer (数据竞争)\n");
        printf("  usan  - UndefinedBehaviorSanitizer (未定义行为)\n");
    }
    return 0;
}
