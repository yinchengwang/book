# CDN 设计

## 简介

系统设计中内容分发网络的核心概念，包括三层节点架构、缓存策略、动态加速和关键指标。

## 目录结构

```
system-cdn/
├── main.py    # CDN 设计演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-cdn
make run          # 运行演示
```

## 涵盖内容

- CDN 三层架构: 边缘节点/中间源/源站
- 缓存策略: TTL/强制刷新/预热
- Cache-Control 头部配置
- 动态加速 (DSA) 原理
