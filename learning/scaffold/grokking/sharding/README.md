# 分库分表 (Sharding)

## 简介

Grokking 数据库内核——分库分表篇。演示水平分片/垂直分片/路由策略。

## 目录结构

```
sharding/
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

- 水平分片（Hash 分片）
- 垂直分片（按列拆分）
- 范围分片（按时间/Q）
- 跨分片查询与聚合
- 分片键选择原则
