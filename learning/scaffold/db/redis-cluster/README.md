# Redis 集群架构

## 概述

演示 Redis Cluster 核心机制：
- **Hash Slot**: 16384 槽自动分配
- **MOVED/ASK**: 客户端重定向
- **Gossip**: 节点发现与故障检测
- **故障切换**: 自动选主

## 编译运行

```bash
make
./redis_cluster
```

## 核心概念

| 概念 | 说明 |
|------|------|
| Hash Slot | 16384 个槽，键通过 CRC16 映射 |
| MOVED | 槽永久迁移，客户端更新映射 |
| ASK | 槽迁移中，允许临时访问旧节点 |
| Gossip | 节点间传播状态信息 |

## 文件结构

```
redis-cluster/
├── main.c
├── Makefile
├── README.md
└── NOTES.md
```
