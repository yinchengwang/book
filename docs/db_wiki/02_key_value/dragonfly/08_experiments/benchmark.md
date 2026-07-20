# Dragonfly 动手实验

## 学习目标

- 掌握 Dragonfly 的部署和基本操作
- 通过实验对比 Dragonfly 与 Redis 的性能

## 环境准备

```bash
# Docker 部署 Dragonfly
docker run -d --name dragonfly-test \
  -p 6379:6379 \
  docker.dragonflydb.io/dragonflydb/dragonfly

# 验证连接
redis-cli -h localhost -p 6379 PING
# 应返回 PONG
```

## 实验 1：基本操作验证

```bash
# 连接 Dragonfly
redis-cli

# 基本命令
SET name "dragonfly"
GET name
INCR counter
INCR counter

# 数据结构
HSET user:1 name "alice" age 30
HGETALL user:1
LPUSH queue "task1" "task2"
LRANGE queue 0 -1

# 过期
SETEX session 10 "test"
TTL session
```

## 实验 2：性能对比

```bash
# 使用 redis-benchmark 测试
# 测试 SET
redis-benchmark -h localhost -p 6379 -t set -n 100000 -c 50

# 测试 GET
redis-benchmark -h localhost -p 6379 -t get -n 100000 -c 50

# 测试 Pipeline
redis-benchmark -h localhost -p 6379 -t set,get -n 100000 -c 50 -P 10

# 对比测试
# 在另一台机器或同一机器上启动 Redis 做对比
```

## 实验结果记录

| 测试项 | Dragonfly | Redis | 备注 |
|--------|-----------|-------|------|
| SET (50 连接) | | | |
| GET (50 连接) | | | |
| Pipeline (P=10) | | | |
| 多线程 | | | 多核优势 |

## 要点总结

- Docker 一键部署，零配置
- 完全兼容 Redis 协议
- 使用 redis-benchmark 即可测试
- 多核机器上性能优势明显