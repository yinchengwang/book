# 并发 - 互斥锁

## 简介

Grokking 并发进阶——POSIX 互斥锁（Mutex）与读写锁。

互斥锁是最基本的线程同步原语，用于保护共享数据不被多个线程同时修改。读写锁在此基础上进一步区分读操作和写操作，提高并发度。

## 目录结构

```
concurrency-mutex/
├── main.c    # 互斥锁与读写锁演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-mutex main.c
./concurrency-mutex
```

## 涵盖内容

- pthread_mutex_lock / pthread_mutex_unlock 保护共享数据
- ABBA 死锁场景演示
- pthread_rwlock_t 读写锁（多读单写）
- 读者-写者问题
