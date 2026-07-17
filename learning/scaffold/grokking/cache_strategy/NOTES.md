# 缓存策略 · 学习笔记

## 核心概念

1. **Cache Aside**：读时回填（cache miss → 读 DB → 回填）、写时删缓存（先写 DB 后删缓存）
2. **缓存穿透**：查询不存在的数据 → 布隆过滤器或空值缓存
3. **缓存击穿**：热 key 过期 → 互斥锁回填
4. **缓存雪崩**：大量 key 同时过期 → 过期时间加随机偏移
5. **Write Through**：同步写缓存 + DB，牺牲延迟换一致性
6. **Write Behind**：异步写 DB，吞吐高但宕机可能丢数据

## 缓存策略对比

| 策略 | 读延迟 | 写延迟 | 一致性 | 复杂度 |
|------|--------|--------|--------|--------|
| Cache Aside | 低 | 低 | 弱（可能读到旧数据） | 简单 |
| Read/Write Through | 低 | 高 | 强（同步写入） | 中等 |
| Write Behind | 极低 | 极快 | 最终一致 | 中等 |
| Refresh Ahead | 最低 | 高 | 强 | 复杂 |

## 工程对照

本项目的 `engineering/src/db/core/bufmgr.c` 实现了 Buffer Pool 页面缓存。
Buffer Pool 本质就是数据库自身的缓存系统，与本文演示的应用层缓存策略
（Redis）不同点在于：Buffer Pool 以页面为粒度（4KB/8KB），使用
Clock-Sweep 置换算法（近似 LRU），脏页通过 WAL 日志保证 crash-safe。
而本文演示的应用缓存策略是行级缓存，适合减数据库查询压力的通用缓存场景。
