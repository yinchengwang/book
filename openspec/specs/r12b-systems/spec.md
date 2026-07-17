# R12b 系统编程 Spec

## 概述

R12b C++ 系统级编程，包含 8 张核心卡片，覆盖并发编程、内存管理、并行算法的关键技能点。

## 卡片清单

| 卡 ID | 名称 | 核心概念 | 工程对照 |
|-------|------|----------|----------|
| thread | 线程基础 | std::thread/join/detach/thread_local | mvcc_snapshot.c 线程模式 |
| atomic | 原子操作 | memory_order/acquire/release/seq_cst/CAS | mvcc_version.c 原子操作 |
| cond_var | 条件变量 | 生产者-消费者/虚假唤醒/predicate | wal_buf.c 条件变量 |
| lockfree | 无锁数据结构 | CAS 循环/ABA 问题/Hazard Pointer | mvcc_gc.c 回收策略 |
| memory_model | 内存模型 | happens-before/内存屏障/synchronizes-with | Linux mb/smp_mb 对照 |
| sanitizers | 内存检测 | ASan/MSan/TSan/USan 原理与使用 | CMakeLists.txt sanitizer 配置 |
| memory_pool | 内存池 | 固定大小池/对象池/arena 分配器 | bufmgr.c Buffer Pool |
| parallel_stl | 并行 STL | execution::seq/par/par_unseq | sort.c SIMD 并行 |

## 学习路径

```
thread → atomic → cond_var → lockfree → memory_model → sanitizers → memory_pool → parallel_stl
```

## 技术栈

- C++17 标准库
- std::thread / std::atomic / std::condition_variable
- std::execution (parallel STL)
- Sanitizers: ASan, MSan, TSan, USan

## 验收标准

- [x] 8 张卡 scaffold 完成（main.cpp + Makefile + README.md + NOTES.md）
- [x] 8 张卡编译通过
- [x] statuses.json 中 8 张卡状态为 done
- [x] r12b-progress.md 创建（8 行）
- [x] NOTES.md 均包含工程对照（≥100 字）

## 归档时间

2026-07-12
