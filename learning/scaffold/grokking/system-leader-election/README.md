# 领导者选举

## 简介

系统设计中分布式共识算法的核心概念，包括 Raft 领导者选举、Paxos 两阶段提交和脑裂防护机制。

## 目录结构

```
system-leader-election/
├── main.py    # 共识算法演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-leader-election
make run          # 运行演示
```

## 涵盖内容

- Raft 领导者选举: 任期/投票/多数派
- Raft 脑裂防护: 多数派原则
- Paxos 两阶段提交: Prepare/Accept
- 租约机制: 领导者有效性保证
