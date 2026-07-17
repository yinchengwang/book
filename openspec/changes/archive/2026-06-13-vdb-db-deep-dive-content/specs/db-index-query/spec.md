## ADDED Requirements

### Requirement: DB 索引与查询引擎深度文章

DB 索引与查询引擎的每篇深度文章 SHALL 覆盖以下知识点（选取核心 ~4 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `db-btree-idx` | B+树索引原理 | basic |
| `db-btree-impl` | B+树实现与优化 | intermediate |
| `db-optimizer` | 查询优化器原理 | intermediate |
| `db-executor` | 查询执行引擎 | intermediate |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：索引/查询文章 SHALL 额外包含

- SQL 查询到索引选择的完整链路（SQL → 解析 → 优化 → 执行 → 存储）
- EXPLAIN 输出解读示例
- 常见慢查询场景及索引优化策略

#### Scenario: 索引文章覆盖

- **WHEN** 用户阅读 `db-btree-idx` 和 `db-btree-impl`
- **THEN** B+树文章 SHALL 从数据结构定义讲到 InnoDB 的工程实现（页分裂/合并/ICP/MRR）
