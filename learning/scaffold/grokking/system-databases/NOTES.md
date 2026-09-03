# 数据库架构设计 学习笔记

## 核心概念

- **主从复制**: 主库写入、从库读取，异步可能导致复制延迟
- **分库分表**: 水平拆分（Sharding），按 Hash/范围/列表路由
- **读写分离**: 写走主、读走从，可扩展读能力
- **强一致性读**: 关键读取需要从主库读

## 分片策略对比

| 策略 | 路由方式 | 优点 | 缺点 |
|------|----------|------|------|
| Hash 分片 | key % N | 分布均匀 | 扩缩容困难 |
| 范围分片 | 按值范围 | 范围查询高效 | 热点问题 |
| 列表分片 | 按枚举值 | 区域隔离 | 数据倾斜 |
| 一致性哈希 | 哈希环 | 扩缩容影响小 | 路由复杂 |

## 工程对照

`engineering/src/db/` 中实现了完整的 PostgreSQL 风格数据库存储引擎。`engineering/include/db/storage_engine.h` 的 `storage_engine_t` 结构定义了多模态存储的统一接口，`engineering/src/db/heapam/heapam.c` 实现了堆表存储（主表数据），`engineering/src/db/index/btreeam.c` 实现了 BTree 索引存储。`engineering/include/db/catalog.h` 中的 Catalog（`pg_database`、`pg_class`、`pg_attribute`）系统表对应于分库分表中"元数据管理"的需求——路由信息、表结构、分片键都需 Catalog 管理。`engineering/src/db/sql/sql_exec.c` 中的 SQL 执行器处理跨表的查询，它通过 `catalog_get_table()` 和 `catalog_get_index()` 获取元数据，这与分库分表中"全局路由表"的概念一致——每个查询先查路由表确定数据所在的分片。`engineering/src/db/lock/lock.c` 实现了行级锁和表级锁，这对应读写分离中的"写锁互斥"问题。

## 面试要点

1. 分库分表后跨分片 JOIN 和分布式事务是最难解决的问题
2. 主从复制延迟可能导致读到过期数据（读写分离的代价）
3. 优先考虑读写分离+缓存，最后才考虑分库分表
