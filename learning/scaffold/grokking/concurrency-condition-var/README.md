# 并发 - 条件变量

## 简介

Grokking 并发进阶——POSIX 条件变量（Condition Variable）。

条件变量允许线程在某个条件满足之前阻塞等待，避免了忙等待（busy waiting）的 CPU 浪费。典型的应用场景是生产者-消费者问题中的缓冲区满/空等待。

## 目录结构

```
concurrency-condition-var/
├── main.c    # 条件变量与生产者-消费者演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-condition-var main.c
./concurrency-condition-var
```

## 涵盖内容

- pthread_cond_wait / pthread_cond_signal 生产者-消费者模式
- pthread_cond_broadcast 通知所有等待线程
- 有界缓冲区（环形缓冲区）实现
- 正确的 mutex + cond_var 使用模式
