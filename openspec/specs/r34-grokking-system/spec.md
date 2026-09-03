# R34 Grokking 系统设计

## Purpose

为系统设计面试提供体系化的知识覆盖，包含：容量估算、延迟分析、高可用设计、缓存策略、CDN 原理、数据库分片、微服务架构、消息队列、网络拓扑、安全设计、共识算法（Raft/Paxos）、拜占庭容错（PBFT）、Gossip 协议、分布式锁等核心系统设计主题。每张卡以 Python 演示代码 + 工程笔记的形式呈现，强调与 `engineering/` 轨源码的工程对照。

## Requirements

### Requirement: R34 Grokking 系统设计栈

R34 系统设计栈 SHALL 包含以下 14 张卡：

| # | 卡 ID | 标题 | 难度 |
|---|-------|------|------|
| 1 | capacity | 容量估算：QPS/TPS/存储/带宽 | intermediate |
| 2 | latency | 延迟分析：P99/P999/SLA | intermediate |
| 3 | availability | 高可用：99.9/99.99/MTTF/MTTR | intermediate |
| 4 | caching | 缓存：CacheAside/WriteThrough/一致性哈希 | intermediate |
| 5 | cdn | CDN：内容分发/缓存策略/动态加速 | intermediate |
| 6 | databases | 数据库：主从复制/分库分表/读写分离 | intermediate |
| 7 | microservices | 微服务：服务拆分/API网关/服务发现 | intermediate |
| 8 | message_queues | 消息队列：MQ模式/顺序消息/幂等消费 | intermediate |
| 9 | topologies | 拓扑结构：星型/环形/网状/负载均衡 | basic |
| 10 | security | 安全：认证授权/HTTPS/加密存储 | intermediate |
| 11 | leader_election | 领导者选举：Paxos/Raft/租约/脑裂防护 | advanced |
| 12 | byzantine | 拜占庭容错：PBFT/视图变更/故障检测 | advanced |
| 13 | multicast | 多播：Gossip/流言协议/反熵 | intermediate |
| 14 | synchronization | 同步：分布式锁/乐观锁/悲观锁 | intermediate |

### Requirement: 每张卡必须包含的 4 个文件

每张卡 SHALL 在 `learning/scaffold/grokking/system-{id}/` 目录下包含：

1. **main.py** — Python 演示代码，注释良好的类/函数实现，~120 行
2. **Makefile** — 标准 Makefile（`python3 main.py`）
3. **README.md** — 中文介绍：简介、运行方法、涵盖内容
4. **NOTES.md** — 工程笔记：核心概念、关键算法、工程对照（≥100 字关联 engineering/ 源码）

### Requirement: 全局文件更新

- `statuses.json` SHALL 添加 14 张卡并标记为 done（done count 从 222 → 236）
- `items-registry.js` SHALL 添加 14 张卡到 ITEMS_REGISTRY（stack: "grok", quadrant: "systems"）
- `r34-progress.md` SHALL 记录 14 行进度条目

### Requirement: 工程对照要求

每张卡的 NOTES.md SHALL 包含 ≥100 字的工程对照小节，映射到 `engineering/` 轨对应实现。
