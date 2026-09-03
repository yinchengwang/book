# doc-aggregations Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 桶聚合

系统 SHALL 实现文档桶聚合（Bucketing Aggregations）。

#### Scenario: 词条聚合
- **WHEN** 执行 `SELECT term_agg(field, size) FROM collection`
- **THEN** 字段值 SHALL 被分组统计
- **THEN** Top N 词条 SHALL 被返回

#### Scenario: 范围聚合
- **WHEN** 执行 `SELECT range_agg(price, [0, 100, 500, 1000]) FROM products`
- **THEN** 值 SHALL 被分到指定范围桶
- **THEN** 每个桶的计数 SHALL 被返回

#### Scenario: 直方图聚合
- **WHEN** 执行 `SELECT histogram_agg(price, interval => 50) FROM products`
- **THEN** 等宽直方图 SHALL 被生成
- **THEN** 每个柱子代表一个区间

#### Scenario: 日期直方图聚合
- **WHEN** 执行 `SELECT date_histogram_agg(timestamp, '1d') FROM events`
- **THEN** 按天分组的时间直方图 SHALL 被生成

---

### Requirement: 指标聚合

系统 SHALL 实现文档指标聚合（Metrics Aggregations）。

#### Scenario: AVG 聚合
- **WHEN** 执行 `SELECT avg_agg(price) FROM products`
- **THEN** 平均值 SHALL 被计算

#### Scenario: 统计聚合
- **WHEN** 执行 `SELECT stats_agg(price) FROM products`
- **THEN** count, sum, avg, min, max, std_deviation SHALL 被返回

#### Scenario: 百分位数聚合
- **WHEN** 执行 `SELECT percentiles_agg(price, [25, 50, 75, 95]) FROM products`
- **THEN** 指定百分位数的值 SHALL 被返回

---

### Requirement: 管道聚合

系统 SHALL 实现文档管道聚合（Pipeline Aggregations）。

#### Scenario: 桶排序
- **WHEN** 执行 `SELECT terms_agg(field).sort_by(agg => 'count', order => 'desc') FROM collection`
- **THEN** 桶 SHALL 按指定方式排序

#### Scenario: 桶过滤
- **WHEN** 执行 `SELECT terms_agg(field).filter(min_doc_count => 10) FROM collection`
- **THEN** 只保留符合条件的桶

#### Scenario: 嵌套管道
- **WHEN** 执行嵌套管道聚合
- **THEN** 外层聚合 SHALL 基于内层结果
- **THEN** 多层嵌套 SHALL 被支持

---

### Requirement: 聚合结果格式

系统 SHALL 支持聚合结果的格式化。

#### Scenario: JSON 格式返回
- **WHEN** 执行聚合查询
- **THEN** 结果 SHALL 以 JSON 格式返回
- **THEN** 结构 SHALL 反映聚合层级

#### Scenario: 分页桶结果
- **WHEN** 桶数量很大
- **THEN** 分页 SHALL 被支持
- **THEN** from, size 参数 SHALL 被应用

