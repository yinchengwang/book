## ADDED Requirements

### Requirement: DB Redis 子系统深度文章

DB Redis 子系统的每篇深度文章 SHALL 覆盖以下知识点：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-redis-event` | Redis 事件驱动模型 | basic |
| `db-redis-object` | Redis 对象系统 | intermediate |
| `db-redis-persist` | Redis 持久化 RDB/AOF | intermediate |
| `db-redis-cluster` | Redis 集群与哨兵 | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：Redis 文章 SHALL 额外包含

- 源码级分析（aeEventLoop / redisObject / RDB 格式等关键数据结构）
- 与 MySQL/其他 KV 存储的对比视角
- 配置参数的最佳实践

#### Scenario: Redis 文章完整性

- **WHEN** 用户阅读 `db-redis-event`
- **THEN** 文章 SHALL 包含 aeEventLoop 的事件处理流程、File Event 与 Time Event 的协作、单线程高性能的核心设计原理
