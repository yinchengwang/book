## Purpose

Define requirements for drawio diagram generation and embedding in learning articles for database products (MySQL, Redis, etc.).

## Requirements

### Requirement: 架构图绘制规范

系统 SHALL 为每个数据库产品知识点绘制专业的 drawio 架构图，遵循以下规范：

1. 所有架构图必须以 drawio XML 格式保存为 `.drawio` 文件
2. 导出为 PNG 格式用于 MD 文章嵌入
3. 图片托管路径遵循：`data/illustrations/<product>/<topic>/<image>.drawio`
4. 每张图必须包含：标题、图例、关键步骤编号
5. 配色统一：MySQL 用蓝色系、Redis 用红色系、PostgreSQL 用深蓝色系

#### Scenario: 绘制 MySQL Buffer Pool 架构图
- **WHEN** 需要为 `db-buffer-pool` 知识点添加架构图
- **THEN** 生成 `data/illustrations/mysql/buffer-pool/buffer_pool_diagram.drawio` 和对应的 PNG

#### Scenario: 绘制 MySQL 行锁示意图
- **WHEN** 需要为 `db-locking` 知识点添加架构图
- **THEN** 生成包含 S锁/X锁、Record锁、Gap锁、Next-Key锁 的 drawio 图

### Requirement: 核心知识点图解覆盖

系统 SHALL 覆盖小林coding MySQL/Redis 所有核心知识点的图解：

MySQL 必须覆盖的图示（至少 20 张）：
1. Buffer Pool 内存分布图
2. Redo Log 写入流程
3. Undo Log 回滚机制
4. MVCC 读视图（ReadView）生成流程
5. 行锁类型（S锁/X锁）示意图
6. Record锁、Gap锁、Next-Key锁 示意图
7. 死锁产生和检测流程图
8. B+ 树索引页分裂过程图
9. 聚簇索引 vs 二级索引 对比图
10. LIMIT 优化分页图
11. COUNT 优化图
12. 索引失效场景图
13. 事务隔离级别对比图
14. binlog 写入流程
15. Two-Phase Commit 流程图
16. 行格式（ROW_FORMAT）对比图
17. Change Buffer 流程图
18. Checkpoint 机制图
19. 存储引擎架构对比图
20. SQL 执行流程（Parser → Optimizer → Executor）

Redis 必须覆盖的图示（至少 10 张）：
1. Redis 内存模型图
2. AOF/RDB 持久化流程图
3. 主从复制流程图
4. Sentinel 故障转移图
5. Cluster 分片图
6. 数据结构底层实现图（SDS/ZipList/IntSet/HT/QuickList）
7. 过期策略图
8. 缓存一致性方案图

#### Scenario: MySQL 图解完整性检查
- **WHEN** 所有 MySQL 知识点图解完成后
- **THEN** 至少 20 张 drawio 图存在于 `data/illustrations/mysql/` 目录

#### Scenario: Redis 图解完整性检查
- **WHEN** 所有 Redis 知识点图解完成后
- **THEN** 至少 10 张 drawio 图存在于 `data/illustrations/redis/` 目录

### Requirement: 图片质量要求

每张 drawio 图必须满足：
1. 使用 drawio 官方配色或主题色
2. 字体大小 ≥ 12pt，确保清晰可读
3. 图例齐全，关键元素有编号/标注
4. 导出 PNG 分辨率 ≥ 2x（适配 Retina 屏）
5. 文件大小 ≤ 500KB（合理压缩）

#### Scenario: 图片导出质量检查
- **WHEN** 一张 drawio 图导出为 PNG
- **THEN** 图片宽度 ≥ 800px，高度 ≥ 600px，文件大小 ≤ 500KB
