# Python 多线程基础 (Python Threading)

## 简介

Python threading 模块基础演示 —— Thread 创建 / GIL 限制 / I/O 受益 / Lock / thread local。

## 目录结构

```
concurrency-python-threading/
├── main.py    # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/concurrency-python-threading
make run

# 或直接运行
python3 main.py
```

## 涵盖内容

- `threading.Thread` — 线程创建与 join
- GIL 限制：CPU 密集型任务不并行
- I/O 密集型任务受益于多线程
- `threading.Lock` — 互斥锁保证线程安全
- `threading.local()` — 线程局部存储
