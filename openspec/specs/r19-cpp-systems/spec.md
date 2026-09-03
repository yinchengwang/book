# R19 C++ 系统级特性 — 能力规格

## 概述

R19 沉淀 C++ 系统级编程的核心能力，覆盖内存模型、并发编程、资源管理三大主题。

## 10 张卡能力矩阵

| 卡 ID | 主题 | 核心概念 | 难度 |
|-------|------|----------|------|
| memory_model | 内存模型 | 内存布局（栈/堆/静态区）、内存序（seq_cst/acquire/release/relaxed） | basic |
| thread | 线程基础 | thread 创建/join/detach、mutex、thread_local、this_thread | basic |
| atomic | 原子操作 | atomic 类型、CAS 循环、内存序、ABA 问题 | intermediate |
| cond_var | 条件变量 | condition_variable、wait/notify、生产者消费者模型 | intermediate |
| lockfree | 无锁数据结构 | lock-free 栈/队列、CAS 原子操作、内存回收策略 | intermediate |
| memory_pool | 内存池 | 定长/变长池、Slab 分配、对象池模式 | basic |
| parallel_stl | 并行 STL | execution::par/seq/parallel、parallel_sort、任务调度 | basic |
| allocator | 分配器 | allocator 概念、池分配器、区域分配器、pmr | intermediate |
| smart_ptr | 智能指针 | unique_ptr/shared_ptr/weak_ptr、自定义 deleter、循环引用 | basic |
| raii | RAII 模式 | RAII 封装、异常安全、作用域守卫、lock_guard | basic |

## 能力规格

### 1. 内存模型（memory_model）

**学完能做的**：
- 绘制进程的内存布局图（栈增长方向、堆向上增长）
- 解释 5 种内存序的语义差异：seq_cst / acquire / release / consume / relaxed
- 对比 Linux 内存屏障（mb/smp_mb/smp_wmb/smp_rmb）与 C++ 内存序的对应关系

### 2. 线程基础（thread）

**学完能做的**：
- 用 `std::thread` 创建线程，调用 `join()` 或 `detach()`
- 用 `std::mutex` + `lock_guard` 保护临界区
- 用 `thread_local` 声明线程局部变量
- 获取线程 ID：`std::this_thread::get_id()`

### 3. 原子操作（atomic）

**学完能做的**：
- 使用 `std::atomic<int>` 实现线程安全的计数器
- 正确选择内存序（release-acquire 配对使用）
- 用 `compare_exchange_weak/strong` 实现 CAS 循环
- 识别并处理 ABA 问题

### 4. 条件变量（cond_var）

**学完能做的**：
- 用 `std::condition_variable` + `wait()` 实现阻塞等待
- 实现生产者-消费者模型（多生产者多消费者）
- 处理伪唤醒（使用谓词而非裸 wait）

### 5. 无锁数据结构（lockfree）

**学完能做的**：
- 用 atomic + CAS 实现 lock-free 栈
- 分析 ABA 问题的成因与解决方案（tagged pointer / 引用计数）
- 了解内存回收策略（hazard pointer / RCU）

### 6. 内存池（memory_pool）

**学完能做的**：
- 实现定长内存池（预先分配 chunks，按块发放）
- 实现 Slab 分配器（对象大小固定，减少碎片）
- 理解对象池模式（复用已分配对象）

### 7. 并行 STL（parallel_stl）

**学完能做的**：
- 使用 `std::execution::par` 并行化算法
- 用 `std::parallel_sort` 加速排序
- 理解任务调度与工作窃取队列

### 8. 分配器（allocator）

**学完能做的**：
- 实现自定义 `std::allocator` 满足 STL 容器
- 实现池分配器（固定大小，减少系统调用）
- 使用 `std::pmr::memory_resource` 实现内存资源管理

### 9. 智能指针（smart_ptr）

**学完能做的**：
- 使用 `unique_ptr` 管理独占资源（移动语义转移所有权）
- 使用 `shared_ptr` + `weak_ptr` 管理共享资源（打破循环引用）
- 实现自定义删除器（管理 FILE*/socket 等资源）
- 使用 `std::make_unique` / `std::make_shared`（异常安全）

### 10. RAII 模式（raii）

**学完能做的**：
- 用 RAII 封装文件操作（`std::fstream` 自动 close）
- 用 `std::lock_guard` / `std::unique_lock` 管理锁
- 实现自定义 RAII 类（构造 lock，析构 unlock）
- 验证异常安全：资源在异常路径正确释放

## 工程对照

### C++ vs C 内存管理对比

| C 写法 | C++ 等价 |
|--------|----------|
| `malloc/free` | `unique_ptr` / `shared_ptr` |
| `open/close` | RAII 封装（如 `std::fstream`） |
| 手动引用计数 | `shared_ptr` 自动引用计数 |
| 循环引用泄漏 | `weak_ptr` 打破循环 |

### 工程中的应用

- `bufmgr.c`：缓冲区页面管理 → 可用 `unique_ptr<Page>` 替代手动 free
- `mvcc_version.c`：版本链插入 → 可用 CAS 原子操作
- `concurrency.c`：锁保护 → 可用 `lock_guard` RAII 封装

## 验收标准

1. 10 张卡的 `main.cpp` 均能编译运行（g++ -std=c++17）
2. 10 张卡的 `NOTES.md` 均包含工程对照（≥100 字）
3. `statuses.json` 中 10 张卡状态均为 `done`
4. `r19-progress.md` 记录 10 行 TBD-deferred 审计日志
5. 双轨编译（engineering + learning）均通过 CMake 配置
