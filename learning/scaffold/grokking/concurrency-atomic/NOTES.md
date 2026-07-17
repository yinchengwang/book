# 原子操作学习笔记

## 核心概念

原子操作是不可中断的操作，要么全部执行完成，要么完全不执行。C11 `<stdatomic.h>` 提供了一组原子类型和操作，允许开发者编写跨平台的无锁代码。

**三种性能层级**（从快到慢）：
1. **普通内存访问**：无同步开销，但线程不安全
2. **原子操作**：CPU 指令级保证（如 x86 `LOCK CMPXCHG`），无需 OS 介入
3. **互斥锁**：涉及 OS 调度器调用，可能引发上下文切换

## 原子操作对比

| 操作 | 函数 | 用途 |
|------|------|------|
| 读 | atomic_load | 原子读取 |
| 写 | atomic_store | 原子写入 |
| 交换 | atomic_exchange | 原子交换 |
| CAS | atomic_compare_exchange_strong | 比较并交换 |
| CAS 弱 | atomic_compare_exchange_weak | 允许伪失败，循环内更高效 |
| 原子递增 | atomic_fetch_add | 原子 +1 |
| 测试置位 | atomic_flag_test_and_set | 测试并设置标志（自旋锁核心） |

## 工程对照

本项目的 `engineering/src/db/index/hash/cceh/cceh_hash_core.c`（CCEH 可扩展哈希索引）中大量使用了 C11 原子操作：

1. **Epoch 同步**：`g_cceh_epoch_registry_latch` 是一个 `atomic_flag`，用作 CCEH 的全局自旋锁，保护 epoch 注册表。其使用模式 `atomic_flag_test_and_set_explicit` 配合 `memory_order_acquire` 完全等价于本演示中的自旋锁实现——区别仅在于 CCEH 显式指定了内存序，在 x86 上默认是 `memory_order_seq_cst`，而 `memory_order_acquire` 对 x86 是免费操作（x86 的 load 天然具有 acquire 语义）。

2. **后端状态缓存**：`g_cceh_tls_backend_state` 是一个 `atomic_uint`，缓存确定的 CCEH 持久化后端类型。使用 `atomic_load_explicit` + `memory_order_acquire` 和 `atomic_store_explicit` + `memory_order_release` 实现了线程安全的延迟初始化——这比使用 mutex 或 `pthread_once` 开销更低。

3. **锁管理器兼容层**：`engineering/include/db/lock.h` 中，为 C11 和 C++11 提供了统一的原子类型别名（`atomic_int_t`、`atomic_uint_t`），应用了条件编译技巧：优先使用 C++ `std::atomic`，回退到 C11 `<stdatomic.h>`。这种抽象使得锁管理器代码不必关心底层是 C 还是 C++。

本演示实现的 CAS 无锁栈是工程中 lock-free 数据结构的基础模式。相比 mutex 保护的栈，CAS 栈在高竞争下可能因 ABA 问题产生 bug，工程实践通常结合 hazard pointer 或 epoch-based reclamation（如 CCEH 的做法）来解决内存回收问题。

## 面试要点

1. Atomic 比 Mutex 快在"无系统调用"——用户态即可完成同步
2. CAS 循环（LL/SC 或 CMPXCHG）是无锁编程的核心构建块
3. ABA 问题是无锁数据结构最隐蔽的陷阱
4. `atomic_flag` 是 C11 标准保证的无锁类型（所有平台上都 lock-free）
