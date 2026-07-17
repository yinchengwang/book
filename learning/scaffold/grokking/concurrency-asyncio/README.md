# Python asyncio 异步编程

## 简介

Python asyncio 异步 I/O 基础演示 —— async/await / create_task / gather / Queue / 同步 vs 异步对比。

## 目录结构

```
concurrency-asyncio/
├── main.py    # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/concurrency-asyncio
make run

# 或直接运行
python3 main.py
```

## 涵盖内容

- `async/await` 协程基本语法
- `asyncio.create_task` 并发执行任务
- `asyncio.gather` 等待多个任务完成
- `asyncio.Queue` 生产者-消费者模式
- `asyncio.sleep` 非阻塞等待
- 同步与异步执行耗时对比
