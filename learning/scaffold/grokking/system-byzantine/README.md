# 拜占庭容错

## 简介

系统设计中拜占庭容错的概念，包括 PBFT 三阶段协议、视图变更机制和故障检测方法。

## 目录结构

```
system-byzantine/
├── main.py    # PBFT 演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-byzantine
make run          # 运行演示
```

## 涵盖内容

- PBFT 三阶段: Pre-Prepare/Prepare/Commit
- 拜占庭节点容忍: 3f+1=N
- 视图变更: 主节点故障切换
- 故障检测: 超时/水线/检查点
