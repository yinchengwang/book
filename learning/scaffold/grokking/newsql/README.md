# NewSQL (新一代数据库)

## 简介

Grokking 数据库内核——NewSQL 篇。演示 TiDB/CockroachDB/HTAP。

## 目录结构

```
newsql/
├── main.py    # 演示代码
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
make run
```

## 涵盖内容

- TiDB 架构：PD + TiKV + TiDB Server
- CockroachDB：Raft 共识 + 自动分片
- HTAP 混合负载：行存 + 列存
- 单体 SQL vs 分布式 SQL
