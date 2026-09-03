# columnar-storage Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 通用列式存储

系统 SHALL 实现通用列式存储引擎，用于分析场景。

#### Scenario: 列式页面格式
- **WHEN** 存储分析数据
- **WHEN** 数据 SHALL 按列存储
- **THEN** 每个列 SHALL 有独立的存储文件或页面
- **THEN** 读取时只加载需要的列

#### Scenario: 列压缩
- **WHEN** 存储列数据
- **WHEN** 压缩 SHALL 被应用
- **THEN** Dictionary、Delta、RLE 压缩 SHALL 被支持
- **THEN** 存储空间 SHALL 被减少

#### Scenario: 列索引
- **WHEN** 在列上创建索引
- **WHEN** MinMax 索引 SHALL 被应用
- **THEN** 范围查询 SHALL 能跳过不相关块

---

### Requirement: 混合存储策略

系统 SHALL 支持行存和列存的混合存储。

#### Scenario: 行写列读
- **WHEN** 写入数据到行存
- **WHEN** 后台任务 SHALL 将数据转换为列存
- **THEN** 写入性能 SHALL 被保证
- **THEN** 分析查询 SHALL 使用列存

#### Scenario: 同步双写
- **WHEN** 配置同步双写
- **WHEN** 写入时 SHALL 同时写入行存和列存
- **THEN** 两边数据 SHALL 保持一致

#### Scenario: 延迟物化
- **WHEN** 执行分析查询
- **WHEN** 投影 SHALL 在扫描时延迟执行
- **THEN** 只读取需要的列
- **THEN** I/O SHALL 被减少

---

### Requirement: 列式聚合优化

系统 SHALL 实现列式存储的聚合优化。

#### Scenario: 列内聚合
- **WHEN** 执行 COUNT/SUM/AVG
- **WHEN** 在列存上
- **THEN** 聚合 SHALL 在列级别执行
- **THEN** 无需解压整个列

#### Scenario: 近似聚合
- **WHEN** 使用近似聚合（如 HyperLogLog）
- **WHEN** 在列存上
- **THEN** 近似结果 SHALL 被高效计算
- **THEN** 内存 SHALL 被节省

---

### Requirement: 列式数据导入

系统 SHALL 支持列式数据的批量导入。

#### Scenario: Parquet 导入
- **WHEN** 导入 Parquet 文件
- **WHEN** 数据 SHALL 被转换为列存格式
- **THEN** 列结构 SHALL 被保留
- **THEN** 压缩 SHALL 被应用

#### Scenario: CSV 导入优化
- **WHEN** 导入 CSV 文件
- **WHEN** 目标为列存
- **THEN** 直接按列写入
- **THEN** 无需先转为行存

