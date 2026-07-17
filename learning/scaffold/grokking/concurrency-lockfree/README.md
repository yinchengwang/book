# Lock-Free 无锁数据结构

## 简介

Grokking 并发编程进阶——Lock-Free 无锁数据结构。

使用 C11 原子操作（`<stdatomic.h>`）实现无锁栈，演示 CAS 操作、ABA 问题及标签指针缓解方案，并对比无锁计数器与互斥锁计数器的性能。

## 目录结构

```
concurrency-lockfree/
├── main.c    # 无锁数据结构演示代码
├── Makefile  # 构建配置
├── README.md # 本文件
└── NOTES.md  # 学习笔记
```

## 运行方法

```bash
# 编译
make

# 运行
make run

# 清理
make clean

# 或手动编译运行
gcc -std=c11 -Wall -Wextra -pthread -o main main.c
./main
```

## 涵盖内容

- 无锁栈：基于 CAS（atomic_compare_exchange_weak）实现
- ABA 问题：CAS 无法区分同一指针的不同状态
- 标签指针（Tagged Pointer）：在指针旁附加递增标签缓解 ABA
- 无锁计数器 vs 互斥锁计数器性能对比
