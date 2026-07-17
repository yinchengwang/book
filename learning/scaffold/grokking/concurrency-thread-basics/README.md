# 并发 - 线程基础

## 简介

Grokking 并发入门——POSIX 线程基础操作。

线程是操作系统能够进行运算调度的最小单位，被包含在进程之中，是进程中的实际运作单位。多个线程可以共享进程的资源（地址空间、文件描述符等），但也因此引入了同步问题。

## 目录结构

```
concurrency-thread-basics/
├── main.c    # 线程基础演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-thread-basics main.c
./concurrency-thread-basics
```

## 涵盖内容

- pthread_create / pthread_join 基本用法
- 向线程函数传递参数
- 从线程获取返回值
- 竞态条件（Race Condition）演示
