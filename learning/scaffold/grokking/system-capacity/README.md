# 容量估算

## 简介

系统设计中容量估算的基本方法，包括 QPS/TPS 计算、存储估算和带宽估算。适合面试中的"估算"类问题（Design a URL Shortener、Design Twitter 等）。

## 目录结构

```
system-capacity/
├── main.py    # 容量估算演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-capacity
make run          # 运行演示
make check        # 检查 Python 版本并运行
```

## 涵盖内容

- QPS/TPS 计算: 按 DAU 和用户行为估算
- 存储估算: 按单条数据大小和日增量
- 带宽估算: 按响应大小和 QPS 计算网络带宽
- 网络设备选型: 对比千兆/万兆/25G/100G 网卡
