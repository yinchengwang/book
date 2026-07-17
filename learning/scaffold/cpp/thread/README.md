# C++ std::thread 基础

## 概念

**std::thread** 是 C++11 引入的线程管理类，提供了创建、管理和同步线程的完整能力。

## 核心 API

### 1. 构造函数与基本操作

| 操作 | 说明 |
|------|------|
| `std::thread t(func, args...)` | 创建线程，立即开始执行 |
| `t.join()` | 阻塞等待线程结束，回收资源 |
| `t.detach()` | 分离线程，使其在后台独立运行 |
| `t.joinable()` | 检查线程是否可 join/detach |
| `t.get_id()` | 获取线程 ID |

### 2. 线程 ID

| API | 说明 |
|-----|------|
| `std::this_thread::get_id()` | 获取当前线程 ID |
| `std::thread::id` | 线程 ID 类型 |

**线程 ID 的用途**：
- 调试时标识线程
- 作为线程局部存储的键
- 比较两个操作是否在同一线程

### 3. 线程局部存储

```cpp
thread_local int counter = 0;
```

- 每个线程有独立的变量副本
- 线程结束时自动销毁
- 适用于避免锁竞争的场景

## join vs detach

| 特性 | join() | detach() |
|------|--------|----------|
| 主线程行为 | 阻塞等待 | 不阻塞 |
| 资源回收 | join 后自动回收 | 线程结束时自动回收 |
| 安全性 | 线程必定完成 | 线程可能继续运行 |
| 适用场景 | 需要等待结果 | 独立后台任务 |

**注意**：
- 线程只能 join 或 detach 一次
- joinable() == false 时不能调用

## 线程函数

线程函数可以是：
- 普通函数
- 函数对象（仿函数）
- Lambda 表达式
- 成员函数（需绑定 this）

```cpp
// 函数
std::thread t1(func, arg);

// Lambda
std::thread t2([&]() { /* ... */ });

// 成员函数
std::thread t3(&MyClass::method, &obj);
```

## 与 MVCC 的关联

工程代码 `engineering/src/db/concurrency/mvcc_snapshot.c` 中，MVCC 快照管理虽然没有直接使用 `std::thread`，但涉及并发控制的线程模式：

| 模式 | 学习代码 | 工程代码 |
|------|---------|---------|
| 线程局部状态 | `thread_local` | 活跃事务列表 (xip_list) |
| 线程标识 | `std::thread::id` | `mvcc_txn_id_t` |
| 线程安全操作 | `std::mutex` | 原子操作 + 锁 |
| 线程同步 | `join()` | WAL 检查点同步 |

## 参考资料

- [cppreference: std::thread](https://en.cppreference.com/w/cpp/thread/thread)
- [C++ 并发编程实战](https://www.cplusplus.com/reference/thread/)
