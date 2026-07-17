# 消息队列设计

## 简介

系统设计中消息队列的核心模式，包括点对点/发布订阅模式、顺序消息和幂等消费的实现。

## 目录结构

```
system-message-queues/
├── main.py    # 消息队列演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-message-queues
make run          # 运行演示
```

## 涵盖内容

- 点对点模式: 队列模型
- 发布订阅模式: Topic 模型
- 顺序消息: 分区内严格有序
- 幂等消费: 消息去重机制
