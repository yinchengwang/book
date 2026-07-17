# 文档 BM25 打分规范

## ADDED Requirements

### Requirement: BM25 打分算法

文档索引 SHALL 实现 BM25 算法进行相关性打分。

#### Scenario: 计算 BM25 分数
- **WHEN** 调用 `bm25_score(index, query_terms, doc_id)`
- **THEN** 系统 SHALL 计算文档长度正规化
- **AND** 系统 SHALL 计算每个词项的 TF 和 IDF
- **AND** 系统 SHALL 应用 BM25 公式计算总分

#### Scenario: BM25 参数配置
- **WHEN** 创建 BM25 评分器时
- **THEN** 系统 SHALL 支持配置 k1 参数（默认 1.2）
- **AND** 系统 SHALL 支持配置 b 参数（默认 0.75）
- **AND** 系统 SHALL 支持配置平均文档长度

### Requirement: BM25 公式实现

BM25 打分 SHALL 遵循标准公式：

```
score(D, Q) = Σ IDF(qi) × (tf(qi, D) × (k1 + 1)) / (tf(qi, D) + k1 × (1 - b + b × |D|/avgdl))

IDF(qi) = log((N - n(qi) + 0.5) / (n(qi) + 0.5))
```

#### Scenario: IDF 计算
- **WHEN** 计算词项 IDF 时
- **THEN** 系统 SHALL 使用改进的 IDF 公式
- **AND** 系统 SHALL 缓存已计算的 IDF 值
- **AND** IDF SHALL 考虑文档频率

#### Scenario: 多词项查询
- **WHEN** 查询包含多个词项时
- **THEN** 系统 SHALL 分别计算每个词项的 BM25 分数
- **AND** 系统 SHALL 求和得到最终分数
- **AND** 系统 SHALL 按分数降序排列结果

### Requirement: BM25 扩展

系统 SHALL 支持 BM25 的扩展变体。

#### Scenario: BM25F 打分
- **WHEN** 调用 `bm25f_score(index, query, doc_id, field_weights)`
- **THEN** 系统 SHALL 按字段权重计算加权 TF
- **AND** 系统 SHALL 应用 BM25F 公式

#### Scenario: BM25+ 打分
- **WHEN** 使用 BM25+ 变体时
- **THEN** 系统 SHALL 添加 delta 参数（默认 1.0）
- **AND** 系统 SHALL 避免极端查询词导致的分数为零

### Requirement: BM25 预计算

系统 SHALL 支持预计算文档的 BM25 相关统计。

#### Scenario: 预计算文档统计
- **WHEN** 调用 `bm25_precompute(index)`
- **THEN** 系统 SHALL 计算每个文档的长度
- **AND** 系统 SHALL 计算平均文档长度
- **AND** 系统 SHALL 缓存文档频率信息

#### Scenario: 批量打分
- **WHEN** 调用 `bm25_batch_score(index, query_terms, doc_ids, scores, n)`
- **THEN** 系统 SHALL 批量计算多个文档的 BM25 分数
- **AND** 系统 SHALL 使用 SIMD 优化计算
- **AND** 系统 SHALL 返回排序后的结果

### Requirement: BM25 缓存

系统 SHALL 缓存频繁使用的 BM25 中间结果。

#### Scenario: IDF 缓存
- **WHEN** 多次查询使用相同词项时
- **THEN** 系统 SHALL 缓存已计算的 IDF 值
- **AND** 后续查询 SHALL 直接使用缓存值

#### Scenario: 缓存淘汰
- **WHEN** 缓存达到容量限制时
- **THEN** 系统 SHALL 使用 LRU 淘汰策略
- **AND** 系统 SHALL 保留高频查询的缓存项
