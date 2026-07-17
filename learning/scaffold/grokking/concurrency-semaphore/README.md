# 并发 - 信号量

## 简介

Grokking 并发进阶——POSIX 信号量（Semaphore）。

信号量是 Dijkstra 提出的经典同步原语，使用 P（prolaag/等待）和 V（verhoog/释放）操作管理资源的访问。二元信号量可用作互斥锁，计数信号量用于控制多份资源的并发访问。

## 目录结构

```
concurrency-semaphore/
├── main.c    # 信号量演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-semaphore main.c
./concurrency-semaphore
```

## 涵盖内容

- sem_init / sem_wait / sem_post 基本用法
- 二元信号量作为互斥锁
- 计数信号量控制资源池并发数
- 信号量实现生产者-消费者模式
