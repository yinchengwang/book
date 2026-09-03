# 死锁

## 简介

哲学家就餐问题、死锁四条件演示及资源排序避免策略。

## 目录结构

```
deadlock/
├── main.c    # 演示代码
├── Makefile  # 运行配置
├── README.md # 本文件
└── NOTES.md  # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/deadlock
make run
make check
```

## 涵盖内容

- 死锁四条件: 互斥/持有等待/不可剥夺/循环等待
- 哲学家就餐: 经典死锁复现
- 资源排序: 打破循环等待
- 死锁检测: 等待图 (WFG) 检测
