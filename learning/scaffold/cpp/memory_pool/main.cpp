/**
 * @file main.cpp
 * @brief 内存池演示：定长内存池、对象池、内存竞技场分配器
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdlib>

// ============================================================================
// 1. 定长内存池
// ============================================================================
class FixedSizePool {
public:
    FixedSizePool(size_t blk, size_t n = 128) : blk_(blk), cnt_(0) {
        mem_ = static_cast<char*>(std::malloc(blk_ * n));
        for (size_t i = 0; i < n; ++i) free_.push_back(mem_ + i * blk_);
        cnt_ = n;
    }
    ~FixedSizePool() { std::free(mem_); }
    void* Alloc() { if (free_.empty()) return nullptr; --cnt_; auto p = free_.back(); free_.pop_back(); return p; }
    void Free(void* p) { ++cnt_; free_.push_back(static_cast<char*>(p)); }
    size_t FreeCount() const { return cnt_; }
private: size_t blk_; char* mem_; std::vector<void*> free_; size_t cnt_;
};

// ============================================================================
// 2. 对象池
// ============================================================================
template<typename T> class ObjectPool {
public:
    ObjectPool(size_t n = 16) : active_(0) { Expand(n); }
    ~ObjectPool() { for (auto* x : all_) delete x; }
    T* Acquire() {
        T* o;
        if (free_.empty()) { o = new T(); all_.push_back(o); }
        else { o = free_.back(); free_.pop_back(); }
        ++active_; return o;
    }
    void Release(T* o) { if (o) { free_.push_back(o); --active_; } }
    size_t Active() const { return active_; }
private:
    void Expand(size_t n) { for (size_t i = 0; i < n; ++i) { auto* x = new T(); free_.push_back(x); all_.push_back(x); } }
    std::vector<T*> free_, all_; size_t active_;
};

class TestObj {
public:
    TestObj() : id_(0), data_(nullptr) {}
    explicit TestObj(int id) : id_(id), data_(new char[64]()) {}
    ~TestObj() { delete[] data_; }
    int id_; char* data_;
};

// ============================================================================
// 3. 内存竞技场
// ============================================================================
class Arena {
public:
    explicit Arena(size_t sz = 4096) : cap_(sz), off_(0) { buf_ = static_cast<char*>(std::malloc(cap_)); }
    ~Arena() { std::free(buf_); }
    void* Alloc(size_t sz, size_t align = 8) {
        size_t al = (off_ + align - 1) & ~(align - 1);
        size_t need = al + sz;
        if (need > cap_) { size_t nc = cap_ * 2; while (nc < need) nc *= 2;
            char* nb = static_cast<char*>(std::malloc(nc)); std::memcpy(nb, buf_, off_); std::free(buf_); buf_ = nb; cap_ = nc; }
        void* p = buf_ + al; off_ = need; return p;
    }
    void Reset() { off_ = 0; }
    size_t Used() const { return off_; }
private: char* buf_; size_t cap_, off_;
};

// ============================================================================
// 性能测试
// ============================================================================
template<typename F> double Bench(const char* n, F&& f, int k = 100000) {
    auto s = std::chrono::high_resolution_clock::now(); f(k);
    auto e = std::chrono::high_resolution_clock::now();
    double ms = std::chrono::duration<double, std::milli>(e - s).count();
    std::cout << "  " << n << ": " << ms << " ms\n";
    return ms;
}

// ============================================================================
// 主函数
// ============================================================================
int main() {
    std::cout << "=== 内存池演示 ===\n\n";

    // [1] 定长内存池
    std::cout << "[1] FixedSizePool\n";
    { FixedSizePool p(sizeof(int) * 100, 64);
      void* a = p.Alloc(); void* b = p.Alloc();
      p.Free(a); p.Free(b);
      std::cout << "  空闲块: " << p.FreeCount() << " / 64\n"; }

    // [2] 对象池
    std::cout << "\n[2] ObjectPool<T>\n";
    { ObjectPool<TestObj> pool(4);
      pool.Acquire(); auto* o2 = pool.Acquire(); pool.Acquire();
      std::cout << "  活跃: " << pool.Active() << "\n";
      pool.Release(o2);
      std::cout << "  归还后活跃: " << pool.Active() << "\n"; }

    // [3] 内存竞技场
    std::cout << "\n[3] Arena\n";
    { Arena a(1024);
      a.Alloc(sizeof(int) * 10);
      a.Alloc(sizeof(double) * 3);
      std::cout << "  已使用: " << a.Used() << " 字节\n";
      a.Reset();
      std::cout << "  Reset后: " << a.Used() << " 字节\n"; }

    // [4] 性能对比
    std::cout << "\n[4] 性能对比\n";
    std::cout << "  固定大小分配:\n";
    Bench("malloc/free", [](int n) { std::vector<void*> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.push_back(std::malloc(64));
        for (auto p : v) std::free(p); });
    FixedSizePool fp(64, 100000);
    Bench("FixedSizePool", [&fp](int n) { std::vector<void*> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.push_back(fp.Alloc());
        for (auto p : v) fp.Free(p); });
    std::cout << "  对象分配:\n";
    Bench("new/delete", [](int n) { std::vector<TestObj*> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.push_back(new TestObj(i));
        for (auto p : v) delete p; });
    ObjectPool<TestObj> op(100000);
    Bench("ObjectPool", [&op](int n) { std::vector<TestObj*> v; v.reserve(n);
        for (int i = 0; i < n; ++i) v.push_back(op.Acquire());
        for (auto p : v) op.Release(p); });

    std::cout << "\n=== 完成 ===\n";
    return 0;
}
