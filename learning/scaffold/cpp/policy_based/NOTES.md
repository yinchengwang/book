# Policy 模式工程对照

## std::allocator 政策

C++ 标准库使用 allocator 作为内存分配策略：

```cpp
// 标准容器使用allocator作为Policy
template<typename T, typename Allocator = std::allocator<T>>
class vector {
    Allocator alloc_;  // 可替换的分配策略
};
```

## 项目中的 Policy 应用

### 1. Buffer Pool 分配策略

```cpp
// engineering/src/db/bufmgr.c
// 可以设计为 Policy-based

// 置换策略 Policy
typedef struct {
    void (*evict)(BufferPool*);
    bool (*can_evict)(BufferPool*, PageId);
} ReplacementPolicy;

// 具体策略
extern const ReplacementPolicy ClockSweepPolicy;
extern const ReplacementPolicy LRUPolicy;
extern const ReplacementPolicy MRUPolicy;
```

### 2. 日志策略

```cpp
// 日志级别 Policy
struct DebugLogging {
    static void log(const char* msg) { /* 详细日志 */ }
};

struct ErrorLogging {
    static void log(const char* msg) { /* 仅错误 */ }
};

template<typename LogPolicy>
class Logger {
    void info(const char* msg) { LogPolicy::log(msg); }
};
```

### 3. 锁策略

```cpp
// 数据库锁的 Policy 组合
struct SpinLockPolicy {
    void lock() { /* 自旋锁 */ }
    void unlock() { /* 解锁 */ }
};

struct MutexLockPolicy {
    std::mutex mtx_;
    void lock() { mtx_.lock(); }
    void unlock() { mtx_.unlock(); }
};

struct RWLockPolicy {
    std::shared_mutex mtx_;
    void readLock() { mtx_.lock_shared(); }
    void writeLock() { mtx_.lock(); }
};
```

### 4. 序列化策略

```cpp
// WAL 日志序列化 Policy
struct BinarySerializer {
    static void serialize(std::ostream& os, const LogEntry& e) {
        os.write(reinterpret_cast<const char*>(&e.type), sizeof(e.type));
        // 二进制序列化
    }
};

struct TextSerializer {
    static void serialize(std::ostream& os, const LogEntry& e) {
        os << e.type << " " << e.data << "\n";
    }
};
```

## Policy vs Strategy

| 特性 | Policy 模式 | Strategy 模式 |
|------|-------------|---------------|
| 选择时机 | 编译时 | 运行时 |
| 灵活性 | 低（固定后难以改变） | 高（可动态切换） |
| 性能 | 优（无虚函数开销） | 一般（虚函数调用） |
| 二进制大小 | 小（内联） | 大（虚表） |
| 适用场景 | 库设计、框架配置 | 业务逻辑、插件 |

## 标准库中的 Policy 示例

1. **std::sort 的比较器**：`std::less<T>`, `std::greater<T>`
2. **std::unique_ptr 的删除器**：`std::default_delete<T>`
3. **std::allocator**：`std::allocator<T>`
4. **std::char_traits**：`std::char_traits<char>`

## Loki 库

Andrei Alexandrescu 的 Loki 库是 Policy 模式的经典实现：

```cpp
// Loki 库的核心模式
template<template<class> class ThreadingModel,
         template<class> class CreationPolicy,
         class... OtherPolicies>
class Host : public ThreadingModel<Host>,
             public CreationPolicy<Host>,
             public OtherPolicies... {
    // Policy 组合
};
```
