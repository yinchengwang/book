# 事务 (Transaction)

## 简介

Grokking 数据库内核——事务篇。演示 ACID 特性和并发问题。

## 目录结构

```
transaction/
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

- 原子性（Atomicity）：事务回滚
- 隔离性（Isolation）：脏读、不可重复读
- 隔离级别：READ UNCOMMITTED ~ SERIALIZABLE
- 持久性（Durability）：WAL 日志
