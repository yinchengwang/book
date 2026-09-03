# 索引设计 (Index Design)

## 简介

Grokking 数据库内核——索引设计篇。演示 BTree/Hash/复合索引和最左前缀原则。

## 目录结构

```
index_design/
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

- BTree 索引：等值与范围查询
- Hash 索引：精确等值匹配
- 复合索引与最左前缀原则
- 覆盖索引避免回表
