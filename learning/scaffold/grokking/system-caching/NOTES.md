# 缓存系统设计 学习笔记

## 核心概念

- **CacheAside**: 读时回填，写时淘汰——最常用模式，逻辑清晰
- **WriteThrough**: 写时同步更新缓存和 DB——一致性高但写延迟大
- **WriteBehind**: 异步写入 DB——高性能但可能丢数据
- **一致性哈希**: 带虚拟节点的哈希环，节点增减只影响 1/N 的键

## 缓存策略选择

| 模式 | 读性能 | 写性能 | 一致性 | 适用场景 |
|------|--------|--------|--------|----------|
| CacheAside | 高 | 中 | 最终 | 读多写少 |
| WriteThrough | 高 | 低 | 强 | 读写均衡 |
| WriteBehind | 高 | 高 | 弱 | 日志类 |

## 工程对照

`engineering/src/db/buf/bufmgr.c` 中实现的 Buffer Pool 是工程化缓存的经典案例——它使用 Hash 表（`buf_hash.c`）定位页面、Clock-Sweep 算法（`buf_clock.c`）进行页面置换、脏页列表跟踪修改。这个本地缓存模型与分布式缓存（如 Redis/Memcached）的区别在于：Buffer Pool 以磁盘页面为单位（8KB 默认），命中率依赖访问局部性。`engineering/include/db/buf.h` 中的 `buffer_pool_t` 结构和 `BUFFER_LOOKUP_HASH` 宏定义了缓存的接口抽象。`engineering/src/db/cache/` 目录下的缓存实现对应于系统设计中的"本地缓存"策略——与分布式缓存相比，本地缓存延迟更低（内存访问 vs 网络访问）但容量有限。实际工程中，数据库使用"Buffer Pool（本地）+ Redis（分布式）"的多级缓存架构，这与系统设计中的缓存分层策略一致。

## 面试要点

1. 缓存穿透: 查询不存在的数据 → 布隆过滤器
2. 缓存雪崩: 大量缓存同时过期 → 过期时间加随机偏移
3. 缓存击穿: 热点 key 过期 → 互斥锁/永不过期
