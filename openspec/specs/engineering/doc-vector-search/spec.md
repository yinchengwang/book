# doc-vector-search Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 文档向量检索

系统 SHALL 支持文档的语义向量搜索。

#### Scenario: 文档嵌入存储
- **WHEN** 插入文档时
- **WHEN** 文档包含文本内容
- **THEN** 文本 SHALL 可以被嵌入为向量
- **THEN** 向量 SHALL 与文档一起存储

#### Scenario: 语义搜索
- **WHEN** 执行语义搜索
- **WHEN** 提供查询文本
- **THEN** 查询文本 SHALL 被嵌入
- **THEN** 相似文档 SHALL 被返回

#### Scenario: 向量相似度排序
- **WHEN** 返回搜索结果
- **THEN** 文档 SHALL 按向量相似度排序
- **THEN** 最相似的文档 SHALL 在前面

---

### Requirement: BM25 + 向量混合检索

系统 SHALL 支持 BM25 与向量搜索的混合检索。

#### Scenario: 混合评分
- **WHEN** 执行混合搜索
- **THEN** BM25 分数 SHALL 被计算
- **THEN** 向量相似度 SHALL 被计算
- **THEN** 组合分数 SHALL 被生成

#### Scenario: 分数加权
- **WHEN** 配置权重（如 BM25 0.3, 向量 0.7）
- **THEN** 组合分数 SHALL 为 `0.3 * bm25 + 0.7 * vector`
- **THEN** 结果 SHALL 按组合分数排序

#### Scenario: RRF 融合
- **WHEN** 使用 Reciprocal Rank Fusion
- **WHEN** 多个结果列表融合
- **THEN** RRF 分数 SHALL 被计算
- **THEN** `score = Σ 1/(k + rank)` SHALL 被应用

---

### Requirement: 文档向量字段

系统 SHALL 支持文档的向量字段定义。

#### Scenario: 定义向量字段
- **WHEN** 创建集合
- **WHEN** 定义字段类型为 vector
- **THEN** 向量字段 SHALL 被支持
- **THEN** 维度 SHALL 被指定

#### Scenario: 自动嵌入
- **WHEN** 插入文档
- **WHEN** 配置自动嵌入
- **THEN** 文本字段 SHALL 自动生成向量
- **THEN** 向量 SHALL 被存储到向量字段

---

### Requirement: 向量搜索过滤

系统 SHALL 支持文档向量搜索与过滤条件组合。

#### Scenario: 元数据过滤
- **WHEN** 向量搜索时指定过滤条件
- **THEN** 过滤 SHALL 只返回符合条件的文档
- **THEN** 相似度 SHALL 在过滤后计算

#### Scenario: 范围过滤
- **WHEN** 按日期范围过滤
- **THEN** 只搜索指定时间范围内的文档

