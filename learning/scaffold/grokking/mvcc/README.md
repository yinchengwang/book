# MVCC (多版本并发控制)

## 简介

Grokking 数据库内核——MVCC 篇。演示版本链/ReadView/快照隔离。

## 目录结构

```
mvcc/
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

- 版本链：每个修改生成新版本，旧版本通过 Undo Log 可达
- ReadView：事务开始时的活跃事务快照
- 快照隔离：Reader 看到事务开始时的版本
- 写写冲突：MVCC 结合行锁序列化写操作
