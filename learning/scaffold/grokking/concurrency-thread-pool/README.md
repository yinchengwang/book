# 线程池

## 简介

Grokking 并发原语——线程池实现演示。

线程池是一种多线程处理模型，预先创建一组工作线程，通过任务队列接收待处理任务，避免了频繁创建和销毁线程的开销。广泛应用于 Web 服务器、数据库连接池、并行计算等场景。

## 目录结构

```
concurrency-thread-pool/
├── main.c    # 线程池实现演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-thread-pool main.c
./concurrency-thread-pool
```

## 涵盖内容

- 线程池创建和销毁
- mutex + cond_var 实现生产者-消费者队列
- 任务抽象（函数指针 + void* 参数）
- 动态扩容
- 队列满拒绝策略
- 优雅关闭
