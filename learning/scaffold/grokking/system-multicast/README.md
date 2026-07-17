# Gossip 多播协议

## 简介

系统设计中 Gossip 协议的核心概念，包括流言传播模型、反熵机制和最终一致性的实现。

## 目录结构

```
system-multicast/
├── main.py    # Gossip 协议演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-multicast
make run          # 运行演示
```

## 涵盖内容

- Gossip 传播模型: Push/Pull/Push-Pull
- 收敛性: 传播轮数与节点数的对数关系
- 最终一致性: 版本向量机制
- 反熵修复: Merkle Tree 比对
- Gossip vs Paxos/Raft 对比
