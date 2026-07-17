# 数据库架构设计

## 简介

系统设计中数据库架构的核心模式，包括主从复制、分库分表和读写分离的实现与对比。

## 目录结构

```
system-databases/
├── main.py    # 数据库设计演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-databases
make run          # 运行演示
```

## 涵盖内容

- 主从复制: 同步/异步复制与复制延迟
- 分库分表: 水平分片与 Hash 路由
- 读写分离: 读流量分发与一致性保证
- 数据分布均匀性分析
