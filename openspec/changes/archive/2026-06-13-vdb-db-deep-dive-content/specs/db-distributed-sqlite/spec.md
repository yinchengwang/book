## ADDED Requirements

### Requirement: DB 分布式与嵌入式深度文章

DB 分布式与嵌入式的每篇深度文章 SHALL 覆盖以下知识点：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-sharding` | 数据分片与分区策略 | intermediate |
| `db-ha` | 高可用与故障转移 | advanced |
| `db-sqlite-arch` | SQLite 架构与嵌入存储 | basic |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：分布式文章 SHALL 额外包含

- 分片策略的一致性哈希/范围分片/哈希分片的对比
- 脑裂防护的 Fencing Token 机制
- SQLite 在嵌入式场景的架构取舍（无服务、VDBE、WAL 模式）

#### Scenario: 分布式文章覆盖

- **WHEN** 用户阅读 `db-sharding`
- **THEN** 文章 SHALL 包含 Range/Hash/一致性哈希三种分片策略的优缺点对比，以及分布式事务的跨分片查询挑战
