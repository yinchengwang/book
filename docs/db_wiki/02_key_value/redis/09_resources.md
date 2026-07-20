# Redis 学习资源

## 学习目标

- 收集 Redis 学习的优质资源
- 提供源码阅读路径

## 官方资源

- **GitHub**：[redis/redis](https://github.com/redis/redis)
- **官方文档**：[redis.io/docs](https://redis.io/docs/)
- **命令参考**：[redis.io/commands](https://redis.io/commands/)
- **Redis 官方博客**：[redis.io/blog](https://redis.io/blog/)

## 源码阅读路径

```
src/
├── ae.c / ae.h           # 事件循环核心
├── anet.c / anet.h       # 网络抽象层
├── networking.c          # 网络 IO 和协议解析
├── server.c / server.h   # 服务器主逻辑
├── db.c                  # 数据库操作
├── object.c              # 对象系统
├── t_string.c            # String 类型实现
├── t_hash.c              # Hash 类型实现
├── t_list.c              # List 类型实现
├── t_set.c               # Set 类型实现
├── t_zset.c              # ZSet 类型实现
├── sds.c / sds.h         # 动态字符串
├── ziplist.c             # 压缩列表
├── dict.c / dict.h       # 哈希表
├── quicklist.c           # 快速列表
├── skiplist.c            # 跳表
├── rdb.c / rdb.h         # RDB 持久化
├── aof.c / aof.h         # AOF 持久化
├── replication.c         # 主从复制
├── sentinel.c            # 哨兵
├── cluster.c / cluster.h # 集群
├── redis-benchmark.c     # 基准测试
├── latency.c             # 延迟监控
├── lazyfree.c            # 异步删除
└── modules/              # 模块系统
```

## 推荐书籍

| 书名 | 作者 | 适合人群 |
|------|------|---------|
| 《Redis 设计与实现》 | 黄健宏 | 初学者，源码分析 |
| 《Redis 开发与运维》 | 付磊 | 运维人员 |
| 《Redis 深度历险》 | 钱文品 | 进阶，核心原理 |

## 推荐项目

| 项目 | 说明 | 链接 |
|------|------|------|
| redis-rs | Rust 实现的 Redis | [mitsuhiko/redis-rs](https://github.com/mitsuhiko/redis-rs) |
| KeyDB | 多线程 Redis 兼容 | [Snapchat/KeyDB](https://github.com/Snapchat/KeyDB) |
| RediSearch | 全文搜索模块 | [RediSearch/RediSearch](https://github.com/RediSearch/RediSearch) |
| RedisBloom | 布隆过滤器模块 | [RedisBloom/RedisBloom](https://github.com/RedisBloom/RedisBloom) |

## 学习路径建议

1. **入门**：阅读《Redis 设计与实现》，理解核心数据结构
2. **深入**：阅读源码中 `ae.c` 事件循环和 `dict.c` 哈希表
3. **实践**：搭建主从 + 哨兵集群，验证故障转移
4. **进阶**：研究集群模式，理解 Gossip 协议和 Hash Slot
5. **扩展**：学习 Redis 模块系统，开发自定义模块

## 要点总结

- 官方文档是学习 Redis 命令的最佳资源
- 源码阅读建议从事件循环和数据结构开始
- 推荐《Redis 设计与实现》作为系统学习教材
- 实践是最好的学习方式，建议搭建集群验证