## ADDED Requirements

### Requirement: DB 存储引擎深度文章

DB 存储引擎的每篇深度文章 SHALL 覆盖以下知识点（选取核心 ~5 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-page-block` | 数据页与块结构 | basic |
| `db-buffer-pool` | Buffer Pool 缓存管理 | intermediate |
| `db-wal` | WAL 预写日志原理 | intermediate |
| `db-lsm` | LSM-Tree 原理与实现 | advanced |
| `db-compaction` | Compaction 与空间回收 | intermediate |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：存储引擎文章 SHALL 额外包含

- 数据结构的图示描述（页布局、链表结构、分层组织）
- IO 路径分析（几次磁盘读写、随机 vs 顺序）
- 核心参数的合理取值范围和设置原则
- 与 InnoDB/RocksDB 等真实引擎的对应关系

#### Scenario: 存储引擎文章完整性

- **WHEN** 用户阅读存储引擎系列文章
- **THEN** 每篇文章 SHALL 说明该知识点在真实数据库（MySQL InnoDB / RocksDB）中的对应实现和配置参数
