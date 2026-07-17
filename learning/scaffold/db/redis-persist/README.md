# Redis 持久化

## 概述

演示 Redis 三种持久化模式：
- **RDB**: 定时快照，二进制格式
- **AOF**: 命令日志，增量备份
- **混合持久化**: RDB 前缀 + AOF 增量

## 编译运行

```bash
make
./redis_persist
```

## 模式对比

| 模式 | 优点 | 缺点 |
|------|------|------|
| RDB | 恢复快、体积小 | 可能丢失数据 |
| AOF | 丢失少 | 恢复慢、文件大 |
| 混合 | 恢复快+丢失少 | 兼容性 |

## 文件结构

```
redis-persist/
├── main.c
├── Makefile
├── README.md
└── NOTES.md
```
