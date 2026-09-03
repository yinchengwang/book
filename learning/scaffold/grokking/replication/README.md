# 复制 (Replication)

## 简介

Grokking 数据库内核——数据复制篇。演示主从复制/半同步/GTID。

## 目录结构

```
replication/
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

- 异步复制（Async Replication）
- 半同步复制（Semi-Sync Replication）
- GTID（全局事务标识符）
- 复制延迟（Replication Lag）
