# sharding Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 数据分片

系统 SHALL 实现数据分片，支持水平扩展。

#### Scenario: Hash 分片
- **WHEN** 使用 Hash 分片策略
- **WHEN** `SHARD BY HASH(shard_key)`
- **THEN** 分片键 SHALL 被哈希
- **THEN** 哈希值 SHALL 决定数据所在分片

#### Scenario: Range 分片
- **WHEN** 使用 Range 分片策略
- **WHEN** `SHARD BY RANGE(shard_key)`
- **THEN** 分片键值 SHALL 被范围划分
- **THEN** 相邻值 SHALL 在同一分片

#### Scenario: 分片副本
- **WHEN** 配置副本因子
- **WHEN** 每个分片 SHALL 有多个副本
- **THEN** 数据 SHALL 被复制到多个节点
- **THEN** 高可用 SHALL 被支持

---

### Requirement: 分片路由

系统 SHALL 实现请求路由到正确分片。

#### Scenario: 单分片查询路由
- **WHEN** 查询可以定位到单个分片
- **WHEN** 根据分片键计算目标分片
- **THEN** 请求 SHALL 被直接路由到该分片

#### Scenario: 全分片查询路由
- **WHEN** 查询涉及所有分片
- **WHEN** 无分片键过滤
- **THEN** 请求 SHALL 被广播到所有分片

#### Scenario: 路由结果聚合
- **WHEN** 查询多个分片
- **THEN** 各分片结果 SHALL 被聚合
- **THEN** 最终结果 SHALL 被返回

---

### Requirement: 分片管理

系统 SHALL 实现分片拓扑管理。

#### Scenario: 分片拓扑维护
- **WHEN** 集群状态变化
- **THEN** 分片拓扑 SHALL 被更新
- **THEN** 路由信息 SHALL 被同步

#### Scenario: 动态扩缩容
- **WHEN** 增加节点
- **WHEN** 现有分片 SHALL 被重新平衡
- **THEN** 数据 SHALL 被迁移到新节点

#### Scenario: 分片故障处理
- **WHEN** 分片节点故障
- **WHEN** 副本 SHALL 被提升为主副本
- **THEN** 服务 SHALL 继续可用

---

### Requirement: 分片配置

系统 SHALL 支持分片配置。

#### Scenario: 创建分片集合
- **WHEN** 执行 `CREATE COLLECTION users SHARD BY HASH(user_id) WITH (replicas=3)`
- **THEN** 分片集合 SHALL 被创建
- **THEN** 分片数和副本数 SHALL 被配置

#### Scenario: 分片键选择
- **WHEN** 选择分片键
- **THEN** 应选择高选择性的字段
- **THEN** 避免热点分片

