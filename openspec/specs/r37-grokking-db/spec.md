---
name: r37-grokking-db
title: Grokking DB 内核
description: Grokking 数据库内核——14 张卡覆盖 SQL 基础/范式/索引设计/查询优化/事务/锁管理器/MVCC/复制/分库分表/CAP 定理/NoSQL/NewSQL/缓存策略/备份恢复
status: stable
tags:
  - learning
  - grokking
  - database
---

# R37 Grokking DB 内核

## 能力规格

### 1. SQL 基础
**编码实现**：`learning/scaffold/grokking/sql_fundamentals/main.py`  
使用 SQLite 演示 SELECT 投影与过滤、INNER/LEFT JOIN 多表关联、GROUP BY 聚合、子查询、索引效果对比。包含 `schema.sql` DDL 定义。

### 2. 范式
**编码实现**：`learning/scaffold/grokking/normalization/main.py`  
演示 1NF（原子列）、2NF（消除部分依赖）、3NF（消除传递依赖）、BCNF（决定子超键）、反规范化（用冗余换性能）。

### 3. 索引设计
**编码实现**：`learning/scaffold/grokking/index_design/main.py`  
演示 BTree 索引等值与范围查询、Hash 索引精确匹配、复合索引最左前缀原则、覆盖索引避免回表。

### 4. 查询优化
**编码实现**：`learning/scaffold/grokking/query_optimization/main.py`  
演示 EXPLAIN QUERY PLAN 解读、索引覆盖 vs 回表、LIKE 模糊查询优化、函数包裹列导致索引失效、多条件复合索引优化。

### 5. 事务
**编码实现**：`learning/scaffold/grokking/transaction/main.py`  
演示 ACID 原子性回滚、脏读与不可重复读并发问题、四种隔离级别、WAL 持久性。

### 6. 锁管理器
**编码实现**：`learning/scaffold/grokking/lock_manager/main.py`  
演示行锁 vs 表锁并发度、意向锁兼容矩阵、死锁产生与检测、锁超时与死锁避免策略。

### 7. MVCC
**编码实现**：`learning/scaffold/grokking/mvcc/main.py`  
演示版本链（Undo Log 保留旧版本）、ReadView 事务快照、快照隔离写写冲突。

### 8. 复制
**编码实现**：`learning/scaffold/grokking/replication/main.py`  
演示异步复制、半同步复制、GTID 全局事务标识、复制延迟问题。

### 9. 分库分表
**编码实现**：`learning/scaffold/grokking/sharding/main.py`  
演示 Hash 水平分片、垂直分片列拆分、范围分片、跨分片查询聚合、分片键选择原则。

### 10. CAP 定理
**编码实现**：`learning/scaffold/grokking/cap_theorem/main.py`  
演示 CP 策略（分区时放弃可用性）、AP 策略（分区时放弃一致性）、CA 策略、PACELC 扩展。

### 11. NoSQL
**编码实现**：`learning/scaffold/grokking/nosql/main.py`  
演示 KV 存储（Redis）、文档数据库（MongoDB）、列族存储（HBase/Cassandra）、图数据库（Neo4j）。

### 12. NewSQL
**编码实现**：`learning/scaffold/grokking/newsql/main.py`  
演示 TiDB 计算存储分离架构、CockroachDB Raft 共识、HTAP 行存列存混合、单体 vs 分布式 SQL 对比。

### 13. 缓存策略
**编码实现**：`learning/scaffold/grokking/cache_strategy/main.py`  
演示 Cache Aside 旁路缓存、缓存穿透/击穿/雪崩、Write Through/Behind、TTL/LRU/LFU 淘汰策略。

### 14. 备份恢复
**编码实现**：`learning/scaffold/grokking/backup_recovery/main.py`  
演示冷备停服复制、热备 WAL 日志一致性、全量与增量备份对比、PITR 时间点恢复演练。
