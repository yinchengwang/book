# 高可用设计

## 简介

系统设计中高可用架构的核心概念，包括可用性级别计算、MTTF/MTTR、冗余策略对比和级联可用性分析。

## 目录结构

```
system-availability/
├── main.py    # 高可用设计演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-availability
make run          # 运行演示
```

## 涵盖内容

- 可用性级别: 90%~99.9999% 与年停机时间换算
- MTTF/MTTR 计算与可用性公式
- 冗余策略: 单机/主备/双活/多活
- 级联可用性: 服务链整体可用性计算
