# Redis 动手实验

## 学习目标

- 掌握 Redis 的安装和基本操作
- 通过实验验证 Redis 核心特性

## 环境准备

```bash
# Docker 启动 Redis
docker run -d --name redis-test -p 6379:6379 redis:7.2

# 进入容器
docker exec -it redis-test redis-cli
```

## 实验 1：基本数据结构操作

```bash
# String
SET key "hello"
GET key
STRLEN key
APPEND key " world"

# Hash
HSET user:1001 name "alice" age 30
HGETALL user:1001
HINCRBY user:1001 age 1

# List
LPUSH tasks "task1" "task2" "task3"
RPUSH tasks "task4"
LPOP tasks
LRANGE tasks 0 -1

# Set
SADD tags "redis" "database" "cache"
SMEMBERS tags
SISMEMBER tags "redis"

# ZSet
ZADD leaderboard 100 "user:1" 90 "user:2" 80 "user:3"
ZREVRANGE leaderboard 0 2 WITHSCORES
ZREVRANK leaderboard "user:1"
```

## 实验 2：过期策略观察

```bash
# 设置过期键
SETEX session:token 5 "abc123"
TTL session:token  # 查看剩余时间
# 等待 5 秒后
GET session:token  # 返回 nil

# 内存淘汰策略查看
CONFIG GET maxmemory-policy
# config get maxmemory

# 主动过期
EXPIRE key 10
PEXPIRE key 10000  # 毫秒
```

## 实验 3：持久化验证

```bash
# 查看持久化配置
CONFIG GET save
CONFIG GET appendonly
CONFIG GET appendfsync

# 手动触发 RDB
BGSAVE
# 查看 RDB 文件
# ls -la /data/dump.rdb

# 手动触发 AOF 重写
BGREWRITEAOF

# 模拟故障恢复
# 1. 写入数据
# 2. docker restart redis-test
# 3. 检查数据是否恢复
```

## 实验 4：主从复制

```bash
# 启动从节点
docker run -d --name redis-slave -p 6380:6379 redis:7.2 \
  redis-server --slaveof 172.17.0.1 6379

# 在从节点验证
redis-cli -p 6380
ROLE  # 查看角色
GET key  # 可以读取主节点数据

# 验证写阻塞
SET slave_key "test"  # 从节点写操作会报错
```

## 实验 5：并发与原子性

```bash
# 模拟并发计数器
# 开启两个终端，同时执行
INCR counter

# 使用 Lua 保证原子性
EVAL "local c = redis.call('incr', KEYS[1]); if c == 1 then redis.call('expire', KEYS[1], 10) end; return c" 1 counter

# 事务
MULTI
SET key1 "value1"
SET key2 "value2"
EXEC
```

## 实验结果记录

| 实验项目 | 预期结果 | 实际结果 |
|---------|---------|---------|
| String 操作 | 成功读写 | |
| 过期键自动删除 | 5 秒后返回 nil | |
| 持久化恢复 | 重启后数据仍在 | |
| 从节点只读 | 写操作报错 | |
| Lua 原子性 | 并发下计数正确 | |

## 要点总结

- 使用 Docker 快速搭建 Redis 环境
- 通过 CLI 直接验证各项特性
- 主从复制验证读写分离行为
- 事务和 Lua 脚本保证原子性

## 思考题

1. 在实验 5 中，不使用 Lua 脚本如何实现"原子递增+设置过期时间"？
2. 主从复制的延迟如何测量？在高写入场景下如何控制数据一致性？