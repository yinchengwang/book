# 范式 (Normalization)

## 简介

Grokking 数据库内核——数据库范式篇。演示 1NF/2NF/3NF/BCNF 规范化和反规范化。

## 目录结构

```
normalization/
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

- 1NF：原子列、消除重复组
- 2NF：消除部分函数依赖
- 3NF：消除传递函数依赖
- BCNF：每个决定子都是超键
- 反规范化：用冗余换性能
