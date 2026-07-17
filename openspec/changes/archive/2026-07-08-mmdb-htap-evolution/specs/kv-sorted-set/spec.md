# 规格：kv-sorted-set

## ADDED Requirements

### Requirement: 有序集合支持

系统 SHALL 支持有序集合（Sorted Set）数据类型。

#### Scenario: 创建有序集合
- **WHEN** 插入有序集合元素
- **WHEN** 指定分数（score）
- **THEN** 元素 SHALL 按分数排序存储
- **THEN** 相同分数 SHALL 按字典序排序

#### Scenario: 添加元素
- **WHEN** 执行 `ZADD key score member`
- **THEN** 元素 SHALL 被添加或更新
- **THEN** 分数 SHALL 被记录

#### Scenario: 获取排名
- **WHEN** 执行 `ZRANK key member`
- **THEN** 成员按分数升序的排名 SHALL 被返回
- **WHEN** 执行 `ZREVRANK key member`
- **THEN** 成员按分数降序的排名 SHALL 被返回

#### Scenario: 获取分数
- **WHEN** 执行 `ZSCORE key member`
- **THEN** 成员的分数 SHALL 被返回

---

### Requirement: 范围查询

系统 SHALL 支持按分数范围查询。

#### Scenario: ZRANGEBYSCORE
- **WHEN** 执行 `ZRANGEBYSCORE key min max`
- **THEN** 分数在指定范围内的元素 SHALL 被返回
- **THEN** 结果 SHALL 按分数升序

#### Scenario: ZREVRANGEBYSCORE
- **WHEN** 执行 `ZREVRANGEBYSCORE key max min`
- **THEN** 分数在指定范围内的元素 SHALL 被返回
- **THEN** 结果 SHALL 按分数降序

#### Scenario: 带分数获取
- **WHEN** 执行 `ZRANGEBYSCORE key min max WITHSCORES`
- **THEN** 元素和分数 SHALL 一起被返回

#### Scenario: 分页查询
- **WHEN** 执行 `ZRANGEBYSCORE key min max LIMIT offset count`
- **THEN** 分页结果 SHALL 被返回

---

### Requirement: 集合操作

系统 SHALL 支持有序集合的集合操作。

#### Scenario: ZCARD
- **WHEN** 执行 `ZCARD key`
- **THEN** 集合元素数量 SHALL 被返回

#### Scenario: ZCOUNT
- **WHEN** 执行 `ZCOUNT key min max`
- **THEN** 指定分数范围内的元素数量 SHALL 被返回

#### Scenario: ZRANGE
- **WHEN** 执行 `ZRANGE key start stop`
- **THEN** 指定索引范围的元素 SHALL 被返回

#### Scenario: ZUNIONSTORE
- **WHEN** 执行 `ZUNIONSTORE destkey numkeys key1 key2`
- **THEN** 多个集合的并集 SHALL 被计算并存入目标键

#### Scenario: ZINTERSTORE
- **WHEN** 执行 `ZINTERSTORE destkey numkeys key1 key2`
- **THEN** 多个集合的交集 SHALL 被计算并存入目标键

---

### Requirement: TTL 增强

系统 SHALL 支持有序集合的 TTL 功能。

#### Scenario: 设置 TTL
- **WHEN** 执行 `EXPIRE key seconds`
- **THEN** 键的过期时间 SHALL 被设置
- **WHEN** 执行 `PEXPIRE key milliseconds`
- **THEN** 毫秒级精度 SHALL 被支持

#### Scenario: 查询 TTL
- **WHEN** 执行 `TTL key`
- **THEN** 剩余生存时间（秒）SHALL 被返回
- **WHEN** 执行 `PTTL key`
- **THEN** 毫秒级精度 SHALL 被返回

#### Scenario: 持久化键
- **WHEN** 执行 `PERSIST key`
- **THEN** TTL SHALL 被移除
- **THEN** 键 SHALL 变为永久
