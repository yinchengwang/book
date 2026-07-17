# 查询优化 (Query Optimization)

## 简介

Grokking 数据库内核——查询优化篇。演示 EXPLAIN 计划解读和慢查询优化。

## 目录结构

```
query_optimization/
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

- EXPLAIN QUERY PLAN 查询计划解读
- 索引覆盖 vs 回表
- LIKE %pattern% 慢查询优化
- 函数包裹列导致的索引失效
- 多条件组合的复合索引优化
