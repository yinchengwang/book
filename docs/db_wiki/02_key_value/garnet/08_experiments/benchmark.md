# Garnet 动手实验

## 学习目标

- 掌握 Garnet 的部署和基本操作
- 通过实验验证 Garnet 的 Redis 兼容性

## 环境准备

```bash
# 方法 1: Docker 部署
docker run -d --name garnet-test \
  -p 6379:6379 \
  ghcr.io/microsoft/garnet

# 方法 2: .NET 直接运行
git clone https://github.com/microsoft/garnet.git
cd garnet
dotnet run --project main/GarnetServer

# 验证连接
redis-cli -h localhost -p 6379 PING
```

## 实验 1：Redis 兼容性验证

```bash
# 连接 Garnet
redis-cli

# 基本命令
SET key "hello"
GET key
INCR counter
EXISTS key
DEL key
TYPE key

# Hash
HSET user:1 name "alice"
HGET user:1 name
HDEL user:1 name

# List
LPUSH list "a" "b" "c"
RPUSH list "d"
LPOP list
LLEN list

# 过期
EXPIRE key 10
TTL key
```

## 实验 2：性能测试

```bash
# 使用 redis-benchmark
redis-benchmark -h localhost -p 6379 \
  -t set,get,lpush,lpop,sadd,spop \
  -n 100000 -c 50

# 使用 Garnet 自带的基准测试
cd garnet
dotnet run --project benchmark/Benchmark -- --help
```

## 要点总结

- Docker 或 .NET 两种部署方式
- 完全兼容 Redis 命令
- 自带基准测试工具
- 适合 .NET 生态团队