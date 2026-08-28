# C3-5 各模态功能面补齐 Proposal

## Why

差距分析 8 个深挖卷和 9 个未注册引擎核对列出的功能面缺口（无 TOAST/无 FSM/无 Hash 索引/无 BRIN/无 GiST/ST_* 子集/几何类型不全/Cypher 标准子集/图算法不足/KV CAS/Watch/Multi/宽表抽象/列式引擎独立等）。本变更聚合各模态的纯功能补齐（不涉及正确性修复，那部分在 C1-1/2/3 与 C2-1/3）。

## What Changes

**Relational**：
- B+Tree 页式化（复用 Buffer Pool，叶子链支持范围扫描）
- 索引种类 +Hash（等值）+BRIN（时序大表），GiST/GIN 延后
- TOAST 大元组外存（对接 C3-1 Blob）
- FSM 空闲空间映射
- 聚合管道（Document）+ $lookup/$unwind/$bucket/$facet（共用）
- Extended Query 协议（pgwire）
- Window Function 增强（更多 frame types）

**Graph**：
- Cypher 核心测试套件覆盖：变长路径 `*1..n`、OPTIONAL MATCH、UNWIND
- 属性 B-Tree 二级索引（复用 index/btree）
- 注册 MODEL_RDF、MODEL_SPATIOTEMPORAL 候选

**Timeseries**：
- 标签倒排 + 基数上限保护
- 持续后台聚合刷新（窗口级增量）

**Document**：
- B-Tree 二级索引 + TTL 索引
- $lookup（跨集合）/$unwind/$facet/$bucket 聚合阶段
- Hybrid 与 C3-4 多模态对象打通

**Spatial**：
- R*Tree 分裂策略（重插入降低重叠）
- +MultiPoint/MultiLineString/MultiPolygon/GeometryCollection

**KV**：
- kv_cas（compare-and-swap）+ kv_txn（begin/put/commit）
- kv_comparator_t 注入点（自定义排序）
- 跨 CF 原子写（WriteBatch）
- 宽表第一期：wide_row_get/put/范围扫描（row_key + column + ts 版本）

**Tree**：
- 注册 MODEL_TREE 已存在
- PostgreSQL ltree 兼容层（PostgreSQL 兼容）+ 操作符

## Capabilities

| 能力 | 交付 |
|------|------|
| 索引扩展 | B+Tree 页式化 + Hash + BRIN |
| TOAST | 大元组自动外存 Blob |
| 图算法扩充 | +10 算法（C2-3 同步） |
| Cypher 子集 | 变长路径/OPTIONAL/UNWIND |
| KV CAS/Watch | 完整 ACID 事务基础 |
| 宽表第一期 | row_key + column 多版本基础 |

## Impact

- 修改面广（8 个模态）
- 新增：wide_column/ 子目录
- 预计 15-20 个 commit（建议按模态拆 C3-5a/b/c 实际执行）
- 依赖：C0-*、C1-*、C2-*、C3-1、C3-4
