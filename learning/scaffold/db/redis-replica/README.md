# Redis 主从复制

## 概述

演示 Redis 主从复制机制：
- **PSYNC**: 部分同步，减少网络开销
- **Replication Buffer**: 主从复制缓冲区
- **Sentinel**: 故障检测与自动切换

## 编译运行

```bash
make
./redis_replica
```

## 复制流程

1. **首次连接**: 全量同步 (FULLRESYNC)
2. **断线重连**: 增量同步 (CONTINUE) 或全量同步
3. **故障切换**: Sentinel 自动选主

## 文件结构

```
redis-replica/
├── main.c
├── Makefile
├── README.md
└── NOTES.md
```
