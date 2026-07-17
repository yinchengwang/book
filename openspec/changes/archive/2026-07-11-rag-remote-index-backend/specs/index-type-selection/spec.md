## ADDED Requirements

### Requirement: IndexType 枚举

系统 SHALL 提供 `IndexType` 枚举，允许用户在创建集合时选择索引算法。

#### Scenario: IndexType 常量定义
- **WHEN** 用户导入 SDK
- **THEN** `IndexType` 枚举 SHALL 包含以下常量：
  - `HNSW = 0`：基于图的近似最近邻搜索（高召回，适合 < 1M 向量）
  - `DISKANN = 1`：基于磁盘的近似最近邻搜索（适合大规模 > 10M）
  - `IVF = 2`：倒排文件索引（适合中等规模，内存可控）
  - `BRUTE_FORCE = 3`：暴力线性扫描（适合 < 10K 调试用）
  - `AUTO = 99`：由系统根据数据规模自动选择（默认值）

### Requirement: C 层枚举归一化

系统 SHALL 统一两套索引类型枚举，确保 VectorAPI 和 Selector 数值兼容。

#### Scenario: VectorIndexType 枚举补充
- **WHEN** 编译 Engineering 项目
- **THEN** `VectorIndexType`（vector_query.h）SHALL 包含 `VECTOR_INDEX_BRUTE_FORCE = 3`
- **THEN** 所有合法值 SHALL 与 Python SDK `IndexType` 数值一一对应

#### Scenario: 枚举映射函数
- **WHEN** VectorAPI 收到 `VectorIndexType` 值
- **THEN** `vector_index_type_convert()` SHALL 将其映射为 `vector_index_type_t` 供 Selector 使用
- **WHEN** 枚举值无法映射
- **THEN** 默认回退到 HNSW

### Requirement: REST API 透传 index_type

REST Server 创建集合时 SHALL 解析请求体中的 `index_type` 字段，透传到 `VectorCreateParams`。

#### Scenario: 创建集合时指定索引类型
- **WHEN** `POST /collections` 请求体包含 `"index_type": 0`
- **THEN** 响应 SHALL 包含 `"index_type": 0`
- **THEN** 后端 SHALL 使用 HNSW 索引

#### Scenario: 创建集合时省略 index_type
- **WHEN** `POST /collections` 请求体不包含 `index_type`
- **THEN** 后端 SHALL 使用 `AUTO`，由 Selector 后续决定

### Requirement: 集合信息暴露 index_type

REST Server SHALL 在集合详情中返回当前使用的索引类型。

#### Scenario: 查询集合详情
- **WHEN** `GET /collections/{name}`
- **THEN** 响应 SHALL 包含 `"index_type"` 字段
- **THEN** `"index_type"` SHALL 为 `VectorIndexType` 中定义的整数值
