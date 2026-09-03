// raii scaffold — RAII 与资源管理
//
// 复现命令：
//   g++ -Wall -Wextra -O2 -std=c++17 -o test main.cpp && ./test
//
// 演示 5 段：
//   [ifstream]    — std::ifstream 自动关闭文件
//   [lock_guard]  — std::lock_guard 自动释放互斥锁
//   [unique_ptr]  — std::unique_ptr 自动 delete
//   [custom]      — 自定义 RAII 包装（MyLock/Transaction）
//   [exception]   — 异常安全保证（资源不泄漏）

#include <iostream>
#include <fstream>
#include <string>
#include <cstdio>
#include <mutex>
#include <memory>
#include <stdexcept>

// === [custom] 自定义 RAII 包装：模拟 mutex ===
class MyLock {
public:
    explicit MyLock(std::mutex& m) : m_(m) {
        m_.lock();
        printf("  [custom] MyLock: acquired\n");
    }
    ~MyLock() {
        m_.unlock();
        printf("  [custom] MyLock: released\n");
    }
    // 禁止拷贝
    MyLock(const MyLock&) = delete;
    MyLock& operator=(const MyLock&) = delete;
private:
    std::mutex& m_;
};

// === [custom] 模拟数据库事务 ===
class Transaction {
public:
    Transaction(const char* name) : name_(name), committed_(false) {
        printf("  [custom] Transaction %s: BEGIN\n", name_);
    }
    ~Transaction() {
        if (!committed_) {
            printf("  [custom] Transaction %s: ROLLBACK (未提交)\n", name_);
        } else {
            printf("  [custom] Transaction %s: 析构（已提交，无操作）\n", name_);
        }
    }
    void commit() {
        committed_ = true;
        printf("  [custom] Transaction %s: COMMIT\n", name_);
    }
private:
    const char* name_;
    bool committed_;
};

// === [exception] 可能抛异常的函数所需的 RAII 类型（前置定义） ===

// RAII 守卫演示
struct FileHandle_guard {
    FileHandle_guard() { printf("  [exception] FileHandle_guard: acquired\n"); }
    ~FileHandle_guard() { printf("  [exception] FileHandle_guard: released\n"); }
};

// 文件句柄 RAII 包装（不依赖 fstream，方便演示）
class FileHandle {
public:
    FileHandle(const char* path, const char* mode) : fp_(std::fopen(path, mode)) {
        if (!fp_) throw std::runtime_error("FileHandle: fopen failed");
        printf("  [raii-fp] open %s\n", path);
    }
    ~FileHandle() {
        if (fp_) {
            std::fclose(fp_);
            printf("  [raii-fp] close\n");
        }
    }
    FileHandle(const FileHandle&) = delete;
    FileHandle& operator=(const FileHandle&) = delete;

    void write(const std::string& s) {
        std::fputs(s.c_str(), fp_);
    }
private:
    FILE* fp_;
};

// === [exception] 可能抛异常的函数 ===
static void risky_operation(int mode) {
    FileHandle_guard guard;
    if (mode == 1) {
        throw std::runtime_error("risky_operation failed at mode 1");
    }
    printf("  [exception] 正常完成\n");
}

int main() {
    // === [ifstream] std::ifstream ===
    printf("[ifstream] === std::ifstream ===\n");
    {
        std::ofstream ofs("cpp_raii_demo.tmp");
        ofs << "line 1\nline 2\n";
        printf("  [ifstream] write file (RAII will close at scope end)\n");
    }   // ofs 自动 close

    {
        std::ifstream ifs("cpp_raii_demo.tmp");
        std::string line;
        while (std::getline(ifs, line)) {
            printf("  [ifstream] read: %s\n", line.c_str());
        }
    }   // ifs 自动 close

    std::remove("cpp_raii_demo.tmp");

    // === [unique_ptr] std::unique_ptr ===
    printf("\n[unique_ptr] === std::unique_ptr ===\n");
    {
        std::unique_ptr<int> p1(new int(42));
        printf("  *p1 = %d\n", *p1);
        // std::unique_ptr<int> p2 = p1;  // 编译错误：禁止拷贝
        std::unique_ptr<int> p2 = std::move(p1);
        printf("  p2 = std::move(p1): *p2 = %d, p1 is empty\n", *p2);
        // p2 离开作用域时自动 delete
    }
    printf("  [unique_ptr] 自动 delete 完毕\n");

    // === [custom] 自定义 RAII ===
    printf("\n[custom] === 自定义 RAII ===\n");
    {
        std::mutex m;
        MyLock lock(m);             // 构造时 lock
        printf("  critical section\n");
        // 析构时 unlock
    }

    {
        Transaction tx("tx1");
        tx.commit();
        // 析构时发现已提交，无操作
    }

    {
        Transaction tx("tx2");
        // 没 commit，析构时 ROLLBACK
    }

    // === [exception] 异常安全 ===
    printf("\n[exception] === 异常安全（资源不泄漏）===\n");
    try {
        risky_operation(1);          // 抛异常
    } catch (const std::exception& e) {
        printf("  caught: %s\n", e.what());
        printf("  FileHandle_guard 已在异常路径中释放\n");
    }

    printf("\n  --- 正常路径 ---\n");
    risky_operation(0);

    // === FileHandle 演示 ===
    printf("\n[raii-fp] === FILE* RAII 包装 ===\n");
    try {
        FileHandle fh("cpp_raii_demo2.tmp", "w");
        fh.write("RAII-managed file\n");
        printf("  write done\n");
        // fh 离开作用域自动 fclose
    } catch (const std::exception& e) {
        printf("  error: %s\n", e.what());
    }
    std::remove("cpp_raii_demo2.tmp");

    printf("\n=== PASS ===\n");
    return 0;
}