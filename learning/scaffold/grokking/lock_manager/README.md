# 锁管理器 (Lock Manager)

## 简介

Grokking 数据库内核——锁管理器篇。演示行锁/表锁/意向锁和死锁检测。

## 目录结构

```
lock_manager/
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

- 行锁 vs 表锁
- 意向锁（IS/IX/S/X）锁兼容矩阵
- 死锁产生与检测
- 锁超时与死锁避免策略
