# Barrier 同步

## 简介

Grokking 并发原语——Barrier（屏障）同步机制演示。

Barrier 允许多个线程在某个汇合点同步，所有线程都到达后才能继续执行。常用于分阶段计算、并行归并、迭代算法等场景。

## 目录结构

```
concurrency-barrier/
├── main.c    # 分阶段同步演示代码
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
gcc -std=c11 -Wall -Wextra -pthread -o concurrency-barrier main.c
./concurrency-barrier
```

## 涵盖内容

- pthread_barrier_t 初始化和销毁
- pthread_barrier_wait 同步点
- 分阶段计算模型（多线程流水线）
- 与点对点同步（mutex + cond_var）的对比
