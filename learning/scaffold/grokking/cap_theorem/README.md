# CAP 定理 (CAP Theorem)

## 简介

Grokking 数据库内核——CAP 定理篇。模拟网络分区下的 CP/AP 策略取舍。

## 目录结构

```
cap_theorem/
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

- CP 策略：分区时放弃可用性保证一致性
- AP 策略：分区时放弃一致性保证可用性
- CA 策略：假设无分区（单机）
- PACELC 扩展定理
