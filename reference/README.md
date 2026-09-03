# Reference 目录说明

本目录按数据库数据模型组织只读参考源码，不参与工程构建。

## 目录结构

- `relational/`：关系型数据库，包括传统 RDBMS 与嵌入式关系型数据库。
- `distributed-sql/`：分布式事务型、NewSQL、HTAP 与云原生数据库。
- `key-value/`：键值数据库与 LSM 存储系统。
- `document/`：文档数据库。
- `columnar/`：列式分析数据库与 OLAP 引擎。
- `time-series/`：时序数据库。
- `search/`：搜索数据库与全文检索引擎。
- `vector/`：向量数据库与向量检索库。
- `graph/`：图数据库。
- `stream/`：事件流与流数据库系统。
- `embedded/`：嵌入式数据库与本地分析引擎。
- `extension/`：数据库扩展、索引库与存储插件。
- `benchmark/`：数据库、检索或性能基准项目。

## 元数据规范

每个项目应在 `catalog.yml` 中维护以下字段：

- `name`：项目目录名
- `model`：模型目录
- `tier`：`core`、`representative`、`innovative`
- `repository`：官方仓库地址
- `mirror`：实际克隆地址
- `license`：开源许可证
- `status`：`active`、`maintained`、`legacy`
- `notes`：技术亮点与用途说明

## 分阶段克隆与维护

- `catalog.yml` 是源码清单，`fetch-manifest.txt` 是本地可直接消费的克隆清单。
- 首次全量克隆前先确认磁盘容量，避免一次性拉取所有大型数据库源码。
- 可按模型目录分批克隆，例如先拉取 `relational/` 与 `key-value/`，再补充 `vector/` 与 `graph/`。
- `stream/` 中的部分系统不应被误算为传统数据库产品。
- `extension/` 与 `benchmark/` 属于生态项目，不计入完整数据库产品数量。
