# 消息队列设计 学习笔记

## 核心概念

- **点对点 (Queue)**: 每条消息只被一个消费者消费，适合任务分发
- **发布订阅 (Topic)**: 消息广播给所有订阅者，适合事件通知
- **顺序消息**: 同一分区/队列内保证消息顺序，Kafka 按 partition 有序
- **幂等消费**: 处理重复消息时不影响业务状态，通过去重表/消息 ID

## 消息队列选型

| 特性 | Kafka | RabbitMQ | RocketMQ |
|------|-------|----------|----------|
| 模型 | 分区日志 | Queue/Exchange | Topic/Queue |
| 顺序 | 分区内有序 | 单队列有序 | 分区内有序 |
| 延迟 | 毫秒级 | 微秒级 | 毫秒级 |
| 吞吐 | 百万/s | 万/s | 十万/s |
| 持久化 | 磁盘日志 | 内存+磁盘 | 磁盘 |

## 工程对照

`engineering/src/redis/` 目录下的 Redis 核心数据结构（SDS 字符串、跳跃表、字典、链表、压缩列表）中的 Redis 列表（linkedlist/ziplist/quicklist）本身是实现消息队列的基础——BLPOP/BRPOP 命令从列表头部阻塞弹出消息，LPUSH/RPUSH 堆积消息。`engineering/include/redis/redis.h` 中定义的 `redis_object` 结构和 `redis_db` 抽象提供了多数据类型的统一接口，这对应于消息队列中"不同消息模型"（Queue/Topic/Stream）的统一抽象。`engineering/src/db/lock/` 中的锁机制可以用于实现消息的幂等消费——消费者在处理消息前获取分布式锁，处理完成后记录已消费的消息 ID。`engineering/src/db/wal/wal.c` 中的 WAL 日志的顺序写入策略与消息队列的顺序写模式一致——顺序追加写入磁盘以获得最佳写入性能。

## 面试要点

1. 消息队列解决了"耦合"问题，但引入了"一致性"问题
2. 至少一次投递（at-least-once）需要幂等消费配合
3. 消息堆积是常见问题——需要监控消费速度与积压量
