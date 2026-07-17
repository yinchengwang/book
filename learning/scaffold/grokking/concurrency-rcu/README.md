# RCU (Read-Copy-Update)

## 简介

Grokking 并发编程进阶——RCU（Read-Copy-Update）机制。

RCU 是一种读侧无锁的同步机制，广泛用于 Linux 内核。读侧临界区无需加锁，写侧通过"复制-更新-宽限期回收"三步完成安全更新。

## 目录结构

```
concurrency-rcu/
├── main.c    # RCU 概念演示代码
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

- RCU 核心模式：读侧 wait-free、写侧 Copy-Update
- 宽限期（Grace Period）概念：等待所有读者退出
- Epoch 机制模拟：全局 epoch + 每线程活跃计数器
- 原子指针更新：atomic_store / atomic_exchange
