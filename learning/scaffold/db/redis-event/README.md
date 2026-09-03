# Redis 事件循环与 Reactor 模式

## 概述

演示 Redis 的事件驱动架构，包括：
- **Reactor 模式**：单线程事件分发
- **I/O 多路复用**：select/poll/epoll 模型对比
- **定时器管理**：最小堆 + 时间轮算法

## 编译运行

```bash
make
./redis_event
# 或
make run
```

## 输出示例

```
========================================
   Redis 事件循环与 Reactor 模式
========================================

[event] ===== Redis 事件循环 =====
[event] 创建事件循环 setsize=1024
...
```

## 核心概念

| 概念 | 说明 |
|------|------|
| Reactor | 事件分发器，单线程循环处理所有 I/O |
| FileEvent | 文件描述符事件（读/写/屏障）|
| TimeEvent | 定时器事件（一次性/周期性）|
| AE | ae.c - Redis 事件循环实现 |

## 文件结构

```
redis-event/
├── main.c      # Reactor 模式演示
├── Makefile    # gcc 编译配置
├── README.md   # 本文件
└── NOTES.md    # 工程对照笔记
```
