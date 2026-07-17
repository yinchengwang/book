# R36 Grokking 并发编程

## Purpose

为并发编程面试提供体系化的知识覆盖，包含：线程基础、互斥锁、条件变量、信号量、屏障/栅栏、原子操作与 CAS、线程池、C++ 线程（std::thread/future/packaged_task）、Python threading（含 GIL 分析）、asyncio 异步并发、生成器协程、RCU（Read-Copy-Update）、无锁算法（ABA 问题/内存回收）、内存序（memory_order/CPU 重排）等核心并发编程主题。每张卡以 C/C++/Python 演示代码 + 工程笔记的形式呈现，强调与 `engineering/` 轨源码的工程对照。

## Requirements

### Requirement: R36 Grokking 并发编程栈

R36 并发编程栈 SHALL 包含以下 14 张卡：

| # | 卡 ID | 标题 | 语言 | 难度 |
|---|-------|------|------|------|
| 1 | thread_basics | 线程基础（pthread 创建/同步/竞态条件） | C | basic |
| 2 | mutex | 互斥锁（pthread_mutex/死锁/读写锁） | C | basic |
| 3 | condition_var | 条件变量（生产者消费者/条件等待/广播） | C | intermediate |
| 4 | semaphore | 信号量（sem_init/wait/post/资源控制） | C | intermediate |
| 5 | barrier | 屏障（pthread_barrier/阶段同步） | C | intermediate |
| 6 | atomic | 原子操作（atomic/CAS/无锁队列） | C | intermediate |
| 7 | thread_pool | 线程池（固定/动态/拒绝策略） | C | intermediate |
| 8 | cpp_threading | C++ 线程（std::thread/future/packaged_task） | C++ | intermediate |
| 9 | python_threading | Python 线程（threading/GIL/线程安全） | Python | basic |
| 10 | asyncio | 异步并发（async/await/asyncio） | Python | intermediate |
| 11 | coroutines | 协程（生成器协程/调度器/上下文切换） | Python | advanced |
| 12 | rcu | RCU（读侧临界区/宽限期） | C | advanced |
| 13 | lockfree | 无锁算法（无锁栈/ABA/内存回收） | C | advanced |
| 14 | memory_order | 内存序（memory_order/编译器优化/CPU重排） | C | advanced |

### Requirement: 每张卡必须包含的 4 个文件

每张卡 SHALL 在 `learning/scaffold/grokking/concurrency-{id}/` 目录下包含：

1. **main.c/main.cpp/main.py** — 注释良好的演示代码，~100-140 行
2. **Makefile** — 标准 Makefile（C 卡用 `gcc -std=c11 -pthread`，C++ 卡用 `g++ -std=c++17 -pthread`，Python 卡用 `python3`）
3. **README.md** — 中文介绍：简介、运行方法、涵盖内容
4. **NOTES.md** — 工程笔记：核心概念、关键算法、工程对照（≥100 字关联 engineering/ 源码）

### Requirement: 全局文件更新

- `r36-progress.md` SHALL 记录 14 行进度条目
- R36 为学习轨变更，items-registry.js 和 statuses.json 不修改（learning 轨豁免）

### Requirement: 工程对照要求

每张卡的 NOTES.md SHALL 包含 ≥100 字的工程对照小节，映射到 `engineering/` 轨对应实现。
