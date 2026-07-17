# SQL 基础 (SQL Fundamentals)

## 简介

Grokking 数据库内核——SQL 基础篇。使用 SQLite 内存数据库演示核心 SQL 操作。

## 目录结构

```
sql_fundamentals/
├── main.py    # 演示代码（SELECT/JOIN/聚合/索引）
├── schema.sql # 数据库 Schema 定义
├── Makefile   # 构建配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
make run
# 或直接运行
python3 main.py
```

## 涵盖内容

- SELECT 投影与过滤
- INNER JOIN / LEFT JOIN 多表关联
- GROUP BY + 聚合函数（COUNT/AVG/SUM）
- HAVING 分组后过滤
- 子查询（标量子查询）
- 索引效果对比（全表扫描 vs 索引查找）
