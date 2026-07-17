# 分布式同步

## 简介

系统设计中分布式同步的核心机制，包括分布式锁、乐观锁 (CAS) 和悲观锁 (2PL) 的实现与对比。

## 目录结构

```
system-synchronization/
├── main.py    # 同步机制演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-synchronization
make run          # 运行演示
```

## 涵盖内容

- 分布式锁: Redis SET NX + 租约
- 乐观锁: CAS 版本号 + 重试
- 悲观锁: 两阶段锁 (2PL)
- 锁策略选择指南
