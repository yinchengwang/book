# 延迟分析

## 简介

系统设计中延迟分析的核心概念，包括请求延迟分解、P99/P999 百分位计算和 SLA 验证方法。

## 目录结构

```
system-latency/
├── main.py    # 延迟分析演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-latency
make run          # 运行演示
```

## 涵盖内容

- 请求延迟分解: DNS/TCP/TLS/排队/处理各阶段
- P50/P90/P95/P99/P99.9/P99.99 计算
- SLA 验证: 是否满足延迟目标
- 抖动计算与长尾分析
