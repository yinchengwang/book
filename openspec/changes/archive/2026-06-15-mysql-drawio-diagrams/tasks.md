# 实施任务清单

## 1. 基础设施准备

- [x] 1.1 创建 `data/illustrations/` 目录结构
- [x] 1.2 创建 MySQL 子目录：`illustrations/mysql/buffer-pool/`、`illustrations/mysql/lock/`、`illustrations/mysql/mvcc/`、`illustrations/mysql/log/`、`illustrations/mysql/index/`、`illustrations/mysql/transaction/`、`illustrations/mysql/architecture/`、`illustrations/mysql/performance/`、`illustrations/mysql/executor/`
- [x] 1.3 创建 Redis 子目录：`illustrations/redis/memory-model/`、`illustrations/redis/persistence/`、`illustrations/redis/cluster/`、`illustrations/redis/data-structure/`、`illustrations/redis/replication/`
- [x] 1.4 配置 `learn.html` CSS 中的 `.diagram-block` 样式（确认居中、max-width、移动端适配）

## 2. MySQL 核心架构图（20 张）

### 2.1 SQL 执行流程（1 张）
- [x] 2.1.1 绘制 MySQL SQL 执行流程图：Connection Pool → Parser → Optimizer → Executor → Storage Engine

### 2.2 存储引擎架构（2 张）
- [x] 2.1.2 绘制 InnoDB 架构图：Buffer Pool / Log Buffer / Index Cache / Data Dictionary
- [x] 2.1.3 绘制 InnoDB vs MyISAM 对比图

### 2.3 Buffer Pool（2 张）
- [x] 2.1.4 绘制 Buffer Pool 内存分布图：Free List / Flush List / LRU List / Freelist / Hash Table
- [x] 2.1.5 绘制 Page 内部结构图：Page Header / File Header / Record 数据 / Infimum/SUPREMUM

### 2.4 行锁与事务锁（3 张）
- [x] 2.1.6 绘制 S锁/X锁 示意图（共享锁 vs 排他锁）
- [x] 2.1.7 绘制 Record锁 / Gap锁 / Next-Key锁 对比图
- [x] 2.1.8 绘制死锁产生和检测流程图

### 2.5 MVCC（2 张）
- [x] 2.1.9 绘制 ReadView 生成流程图
- [x] 2.1.10 绘制 MVCC 可见性判断流程图

### 2.6 Redo Log（2 张）
- [x] 2.1.11 绘制 Redo Log 写入流程图（Buffer → File）
- [x] 2.1.12 绘制 Redo Log + Undo Log 协同流程图

### 2.7 Undo Log（1 张）
- [x] 2.1.13 绘制 Undo Log 回滚机制图

### 2.8 B+ 树索引（3 张）
- [x] 2.1.14 绘制聚簇索引 vs 二级索引 对比图
- [x] 2.1.15 绘制索引页分裂过程图（插入满 → 分裂 → 父节点更新）
- [x] 2.1.16 绘制 LIMIT 分页优化图（深度分页 vs 游标分页）

### 2.9 事务与 ACID（2 张）
- [x] 2.1.17 绘制 Two-Phase Commit 流程图
- [x] 2.1.18 绘制 binlog 写入流程图

### 2.10 性能优化（2 张）
- [x] 2.1.19 绘制索引失效场景图（类型转换、函数计算、LIKE %前缀等）
- [x] 2.1.20 绘制 COUNT 优化图

## 3. Redis 核心架构图（10 张）

### 3.1 内存模型（1 张）
- [x] 3.1.1 绘制 Redis 内存模型图：对象系统 / 内存分配器 / 内存淘汰策略

### 3.2 数据结构（2 张）
- [x] 3.1.2 绘制 SDS（Simple Dynamic String）结构图
- [x] 3.1.3 绘制 QuickList 结构图（ziplist + linkedlist）

### 3.3 持久化（2 张）
- [x] 3.1.4 绘制 AOF 持久化流程图（rewrite / append）
- [x] 3.1.5 绘制 RDB 持久化流程图（fork / COW / save）

### 3.4 主从复制（1 张）
- [x] 3.1.6 绘制主从复制流程图（FULLRESYNC / PSYNC）

### 3.5 Sentinel（1 张）
- [x] 3.1.7 绘制 Sentinel 故障转移流程图

### 3.6 Cluster（2 张）
- [x] 3.1.8 绘制 Cluster 16384 slot 分片图
- [x] 3.1.9 绘制 Cluster gossip 协议通信图

### 3.7 其他（1 张）
- [x] 3.1.10 绘制 Redis 过期策略图（惰性删除 + 定期删除）

## 4. 图片嵌入 MD 文章

### 4.1 MySQL 文章嵌入
- [x] 4.1.1 更新 `db-buffer-pool.md`，嵌入 Buffer Pool 内存分布图
- [x] 4.1.2 更新 `db-locking.md`，嵌入 S锁/X锁、Record锁、Gap锁 对比图
- [x] 4.1.3 更新 `db-mvcc-principle.md`，嵌入 MVCC 可见性判断图
- [x] 4.1.4 更新 `db-redo-log-detail.md`，嵌入 Redo Log 写入流程图
- [x] 4.1.5 更新 `db-undo-log.md`，嵌入 Undo Log 回滚机制图
- [x] 4.1.6 更新 `db-btree-idx.md`，嵌入聚簇索引 vs 二级索引对比图
- [x] 4.1.7 更新 `db-sql-parser.md`，嵌入 SQL 执行流程图
- [x] 4.1.8 更新 `db-executor.md`，嵌入 Executor 执行计划图
- [x] 4.1.9 更新 `db-optimizer.md`，嵌入 Optimizer 优化规则图
- [x] 4.1.10 更新 `db-deadlock.md`，嵌入死锁检测流程图
- [x] 4.1.11 更新 `db-wal.md`，嵌入 Redo Log + Undo Log 协同图
- [x] 4.1.12 更新 `db-storage-overview.md`，嵌入 InnoDB 架构图
- [x] 4.1.13 补充嵌入：`db-btree-idx.md` 增加页分裂 + 索引失效图
- [x] 4.1.14 补充嵌入：`db-executor.md` 增加 LIMIT 分页 + COUNT 优化图
- [x] 4.1.15 补充嵌入：`db-redo-log-detail.md` 增加 binlog 写入 + 二阶段提交图
- [x] 4.1.16 补充嵌入：`db-storage-overview.md` 增加 InnoDB vs MyISAM 对比图

### 4.2 Redis 文章嵌入
- [x] 4.2.1 更新 `redis-interview.md`，嵌入 Redis 整体架构图
- [x] 4.2.2 更新 `what-is-redis.md`，嵌入 Redis 内存模型图
- [x] 4.2.3 更新 `redis-aof.md`，嵌入 AOF 持久化流程图
- [x] 4.2.4 更新 `redis-rdb.md`，嵌入 RDB 持久化流程图
- [x] 4.2.5 更新 `redis-master-slave.md`，嵌入主从复制流程图
- [x] 4.2.6 补充嵌入：`db-redis-cluster.md` 增加 slot 分片 + Gossip + Sentinel 图
- [x] 4.2.7 补充嵌入：`db-redis-persist.md` 增加过期策略图
- [x] 4.2.8 补充嵌入：`db-redis-object.md` 增加 SDS + QuickList 图

## 5. 测试验证

- [x] 5.1 验证所有 drawio 图片可正常渲染（27 张均含有效 mxGraphModel）
- [x] 5.2 验证移动端图片自适应（.diagram-block img max-width: 90% + @media 断点）
- [x] 5.3 验证图片加载性能（最大 9.3KB，远低于 500KB 上限）
- [x] 5.4 验证 file:// 协议下图片路径正确（所有相对路径均可解析）
