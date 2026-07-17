## ADDED Requirements

### Requirement: 向量 DB 知识条目迁移到 vdb 栈

`data/items-registry.js` 中当前 `stack:"db"` 的条目，按内容类型将向量数据库相关条目迁移到 `stack:"vdb"`。

#### Scenario: 条目迁移范围
- **WHEN** 迁移完成
- **THEN** 向量索引相关条目（db-vector-basic, db-hnsw, db-ivf-families, db-graph-index, db-scann, db-ann-eval, db-hybrid-search 等）的 `stack` 字段值为 `"vdb"`
- **AND** 量化压缩相关条目（db-pq-quant, db-quantization, db-scalar-quant, db-pq）的 `stack` 字段值为 `"vdb"`
- **AND** Milvus 相关条目（db-milvus-arch, db-milvus-segment, db-milvus-index, db-milvus-search）的 `stack` 字段值为 `"vdb"`
- **AND** 磁盘索引条目（db-diskann）的 `stack` 字段值为 `"vdb"`
- **AND** 传统数据库条目（存储/B+树/事务/Redis/ES/分片/共识等）的 `stack` 字段保持 `"db"`

#### Scenario: 条目数量校验
- **WHEN** 迁移完成后
- **THEN** `Object.entries(ITEMS_REGISTRY).filter(e => e[1].stack === "vdb").length >= 15`（向量 DB 条目不少于 15 个）
- **AND** `Object.entries(ITEMS_REGISTRY).filter(e => e[1].stack === "db").length >= 35`（传统 DB 条目不少于 35 个）

### Requirement: 新增 SQLite 知识条目

在 items-registry.js 中新增 SQLite 相关知识点条目。

#### Scenario: SQLite 条目存在
- **WHEN** 注册中心初始化完成
- **THEN** `ITEMS_REGISTRY` 中包含至少 1-2 个 `stack:"db"`、包含 "SQLite" 相关标签的条目
- **AND** 这些条目在雷达图/看板中正常显示

### Requirement: 条目象限合理性

迁移后的 vdb 条目象限归属反映向量数据库的学习路径。

#### Scenario: 象限合理性
- **WHEN** 迁移完成后
- **THEN** vdb 条目的 `quadrant` 字段值经过审查，确保向量基础概念归 `language`，索引算法归 `systems`，量化压缩相关归 `algorithms`，工程系统归 `engineering`
- **AND** 与设计文档中 `QUADRANT_LABELS.vdb` 的定义对齐
