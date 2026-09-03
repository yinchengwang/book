# 缓存策略 (Cache Strategy)

## 简介

Grokking 数据库内核——缓存策略篇。演示 Cache Aside/Write Through/雪崩。

## 目录结构

```
cache_strategy/
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

- Cache Aside 旁路缓存（读回填、写删缓存）
- 缓存穿透 / 击穿 / 雪崩
- Write Through 同步写 + Write Behind 异步刷盘
- TTL、LRU、LFU 淘汰策略
