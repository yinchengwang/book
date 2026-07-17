# 缓存系统设计

## 简介

系统设计中缓存策略的核心模式，包括 CacheAside、WriteThrough 和一致性哈希的实现与对比。

## 目录结构

```
system-caching/
├── main.py    # 缓存系统演示代码
├── Makefile   # 运行配置
├── README.md  # 本文件
└── NOTES.md   # 学习笔记
```

## 运行方法

```bash
cd learning/scaffold/grokking/system-caching
make run          # 运行演示
```

## 涵盖内容

- CacheAside 模式: 旁路缓存读写策略
- WriteThrough 模式: 穿透写
- 一致性哈希: 带虚拟节点的哈希环
- 缓存命中率与节点增删影响
