# 内存序 (Memory Order)

## 简介

Grokking 并发编程进阶——内存序（Memory Order）概念演示。

通过可编译的 C 代码展示 `<stdatomic.h>` 中不同内存序的行为差异：relaxed、acquire/release、sequential consistency，以及非原子变量的编译器优化陷阱。

## 目录结构

```
concurrency-memory-order/
├── main.c    # 内存序概念演示代码
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

- memory_order_relaxed：仅原子性，无顺序约束
- memory_order_acquire / memory_order_release：生产者-消费者同步
- memory_order_seq_cst：全局顺序一致性（安全默认值）
- CPU 重排序与内存序的关系
- 非原子变量的编译器优化陷阱
