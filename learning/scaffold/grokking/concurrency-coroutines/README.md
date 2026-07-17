# Python 协程基础 (Coroutines)

## 简介

Python 协程基础演示 —— 生成器协程 (yield/send) / 手写调度器 / async/await 原生协程 / 协作式 vs 抢占式对比。

## 目录结构

```
concurrency-coroutines/
├── main.py    # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/concurrency-coroutines
make run

# 或直接运行
python3 main.py
```

## 涵盖内容

- 生成器协程：yield/send 模式
- 手写协作式调度器（事件循环）
- 手动上下文切换
- async/await 原生协程 (Python 3.5+)
- 协作式 vs 抢占式多任务对比
