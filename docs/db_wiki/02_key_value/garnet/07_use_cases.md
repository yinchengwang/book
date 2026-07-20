# Garnet 使用场景与实验

## 学习目标

- 掌握 Garnet 的部署和使用
- 通过实验验证 Garnet 的性能

## 环境准备

```bash
# 使用 Docker 部署 Garnet
docker run -d --name garnet-test \
  -p 6379:6379 \
  ghcr.io/microsoft/garnet

# 或使用 .NET 直接运行
git clone https://github.com/microsoft/garnet
cd garnet
dotnet run --project main/GarnetServer
```

## 使用场景

```python
import redis

# Garnet 完全兼容 Redis 协议
# 可以直接使用 redis-py 客户端

client = redis.Redis(host='localhost', port=6379)

# 基本操作
client.set('key', 'value')
value = client.get('key')

# 数据结构
client.hset('hash', 'field', 'value')
client.lpush('list', 'item')
client.sadd('set', 'member')
client.zadd('zset', {'member': 1.0})

# 批量操作
pipe = client.pipeline()
pipe.set('k1', 'v1')
pipe.set('k2', 'v2')
pipe.execute()
```

## 实验：性能测试

```bash
# 使用 redis-benchmark
redis-benchmark -h localhost -p 6379 \
  -t set,get,lpush,lpop,sadd \
  -n 100000 -c 50 -P 10

# 使用 Garnet 自带的基准测试
dotnet run --project benchmark/Benchmark

# 对比 Redis
# 在相同硬件上运行相同的 benchmark
```

## 要点总结

- Docker 或 .NET 直接运行
- 完全兼容 redis-py 客户端
- 适合已使用 Redis 的 .NET 生态团队
- MIT 许可证，商业友好

## 思考题

1. Garnet 在 .NET 环境下相比 Redis 的性能优势有多大？
2. Garnet 的集群模式如何配置和使用？
3. Garnet 是否支持 Redis 的 Lua 脚本和事务？