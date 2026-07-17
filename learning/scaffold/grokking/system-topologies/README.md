# 网络拓扑设计

## 简介

系统设计中网络拓扑的基本类型和负载均衡策略，包括星型、环形、全网状拓扑的对比与容错分析。

## 目录结构

```
system-topologies/
├── main.py    # 网络拓扑演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-topologies
make run          # 运行演示
```

## 涵盖内容

- 星型/环形/全网状/部分网状拓扑对比
- 负载均衡: 轮询/加权/最少连接
- 故障模拟与容错分析
- 单点故障 (SPOF) 识别
