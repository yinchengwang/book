# ts-functions Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: TIME_SERIES_AGG 函数

系统 SHALL 实现时序聚合函数。

#### Scenario: 时间窗口聚合
- **WHEN** 执行 `TIME_SERIES_AGG(timestamp, '1h', 'AVG')`
- **THEN** 数据 SHALL 按 1 小时窗口聚合
- **THEN** 每个窗口的 AVG SHALL 被计算

#### Scenario: 支持的窗口粒度
- **WHEN** 使用不同窗口粒度
- **THEN** '1m', '5m', '15m', '1h', '1d' SHALL 被支持

#### Scenario: 支持的聚合函数
- **WHEN** 使用聚合函数
- **THEN** AVG, SUM, MIN, MAX, COUNT SHALL 被支持
- **THEN** FIRST, LAST SHALL 被支持

---

### Requirement: FIRST/LAST 函数

系统 SHALL 实现 FIRST/LAST 聚合函数。

#### Scenario: 获取第一个值
- **WHEN** 执行 `FIRST(value, timestamp)`
- **THEN** 最早时间的值 SHALL 被返回

#### Scenario: 获取最后一个值
- **WHEN** 执行 `LAST(value, timestamp)`
- **THEN** 最晚时间的值 SHALL 被返回

---

### Requirement: 差分函数

系统 SHALL 实现时序差分函数。

#### Scenario: RATE 计算
- **WHEN** 执行 `RATE(value, timestamp)`
- **THEN** 单位时间变化率 SHALL 被计算

#### Scenario: DERIVATIVE 计算
- **WHEN** 执行 `DERIVATIVE(value, timestamp)`
- **THEN** 导数 SHALL 被计算

---

### Requirement: 分桶函数

系统 SHALL 实现数据分桶函数。

#### Scenario: HISTOGRAM
- **WHEN** 执行 `HISTOGRAM(value, nbuckets)`
- **THEN** 值分布 SHALL 被计算
- **THEN** 直方图 SHALL 被返回

#### Scenario: BUCKET
- **WHEN** 执行 `BUCKET(value, start, width)`
- **THEN** 值所属的桶 SHALL 被返回

---

### Requirement: 时间函数

系统 SHALL 实现时序相关的时间函数。

#### Scenario: 时间戳提取
- **WHEN** 使用 `DATE_TRUNC('hour', timestamp)`
- **THEN** 时间 SHALL 被截断到小时

#### Scenario: 时间差计算
- **WHEN** 使用 `EXTRACT(EPOCH FROM timestamp)`
- **THEN** 时间戳 SHALL 被转换为 epoch 秒

#### Scenario: 时区转换
- **WHEN** 使用 `timezone_convert(timestamp, 'UTC', 'Asia/Shanghai')`
- **THEN** 时区 SHALL 被正确转换

