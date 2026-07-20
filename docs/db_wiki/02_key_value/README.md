# 键值数据库横向对比

## 分类概述

键值数据库是最简单的 NoSQL 类型，以 Key-Value 对形式存储数据，提供 O(1) 时间复杂度的读写性能。本分类涵盖从内存缓存到持久化存储、从单节点到分布式集群的各种架构，服务于缓存、会话、消息队列等场景。

## 库一览

- **Redis** - 全功能内存数据结构服务器，字符串/哈希/列表/集合/Sorted Set
- **Dragonfly** - Redis 兼容的高性能内存数据库，多线程架构，支持垂直扩展
- **Etcd** - 云原生配置存储和服务发现，Raft 共识，Kubernetes 后端
- **NATS** - 轻量级消息系统和键值存储，简单发布/订阅，嵌入式部署
- **Garnet** - Microsoft 开源高性能缓存库，Redis 协议兼容，TSl/ASP.NET 优化

## 功能对比表

| 维度 | Redis | Dragonfly | etcd | NATS | Garnet |
|------|-------|-----------|------|------|--------|
| 编程语言 | C | C++ | Go | Go | C# |
| 开源协议 | BSD | BSL 1.1 | Apache 2.0 | Apache 2.0 | MIT |
| 首次发布 | 2009 | 2022 | 2013 | 2010 | 2024 |
| GitHub Stars | 64k+ | 23k+ | 48k+ | 14k+ | 9k+ |
| 架构模型 | 主从/集群 | 多线程 | 分布式 Raft | 发布/订阅 | 缓存库 |
| 持久化 | RDB/AOF | RDB/AOF | Write Ahead Log | 可选持久化 | 可选持久化 |
| 集群模式 | Sentinel/Cluster | 原生集群 | Raft 联盟 | 超球/叶节点 | 单机/集群 |
| 一致性 | 最终一致 | 最终一致 | 线性一致性 | At-Most-Once | 弱一致 |
| 协议 | Redis Protocol | Redis Protocol | gRPC | NATS Protocol | Redis Protocol |
| 内存效率 | 高 | 更高 | 中 | 高 | 高 |
| 线程模型 | 单线程/IO多路复用 | 多线程 | 多线程 Raft | 轻量级协程 | 多线程 |
| 一句话定位 | 全功能内存数据结构库 | 云规模 Redis 替代 | 分布式配置中心 | 轻量消息系统 | .NET 高性能缓存 |

## 选型指南

- **缓存场景**：推荐 Redis（生态成熟）、Dragonfly（更高性能）
- **配置中心/服务发现**：推荐 etcd（K8s 原生支持）
- **消息队列/事件流**：推荐 NATS（轻量低延迟）
- **.NET 生态缓存**：推荐 Garnet（ASP.NET 深度集成）
- **通用 KV 存储**：推荐 Redis（功能最全）

## 学习路径

1. **先学 Redis** — 最流行的 KV 数据库，理解数据结构命令和持久化
2. **再学 etcd** — 分布式一致性协议实践，理解 Raft 共识
3. **然后学 NATS** — 发布/订阅模式，理解消息语义
4. **最后学 Dragonfly/Garnet** — 高级性能和特定生态优化

## 关联项目

- `redis/` — Redis 核心数据结构手写实现
- `self_made/` — KV 数据结构练习
