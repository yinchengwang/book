# Reference 目录模型化重构 规格

## 能力: reference-by-model

Reference 目录按数据模型分类组织。

### ADDED Requirements

#### Requirement: 模型目录结构
`reference/` SHALL 包含以下 13 个模型子目录：
- relational, distributed-sql, key-value, document, columnar
- time-series, search, vector, graph, stream
- embedded, extension, benchmark

每个模型子目录下 SHALL 按项目名再分目录：`reference/<model>/<project>/`。

#### Requirement: 元数据清单
`reference/catalog.yml` SHALL 列出所有子模块的元数据（url / branch / description），共 74 项。

#### Requirement: 分阶段克隆
`reference/fetch-manifest.txt` SHALL 提供分阶段克隆指引：
- 第一阶段：核心必备
- 第二阶段：特色与生态

#### Scenario: 定位关系型参考
- **WHEN** 用户需要查找关系型数据库参考实现
- **THEN** 路径 `reference/relational/` 下应有 postgres/mysql/sqlite3 等子目录

#### Scenario: 添加新子模块
- **WHEN** 用户添加新项目到某个模型目录
- **THEN** 同步在 `catalog.yml` 中追加元数据条目

## 验收

- 71/71 子模块已克隆完成
- 13 个模型目录全部建立
- catalog.yml 与 fetch-manifest.txt 内容一致