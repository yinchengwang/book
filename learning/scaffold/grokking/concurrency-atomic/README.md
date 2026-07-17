# 原子操作

## 简介

Grokking 并发原语——C11 `<stdatomic.h>` 原子操作演示。

原子操作是无锁编程的基础，提供了不必使用互斥锁即可安全操作共享数据的手段。C11 标准引入了 `<stdatomic.h>`，为跨平台原子编程提供了语言级支持。

## 目录结构

```
concurrency-atomic/
├── main.c    # 原子操作演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-atomic main.c
./concurrency-atomic
```

## 涵盖内容

- atomic_int 原子递增 vs mutex 保护递增的性能对比
- atomic_compare_exchange_strong (CAS) 实现无锁栈
- atomic_flag 自旋锁
- lock-free 数据结构基础
