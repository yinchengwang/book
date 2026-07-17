# ts-columnar Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 时序列式存储

系统 SHALL 实现时序数据专用的列式存储引擎。

#### Scenario: 列式页面格式
- **WHEN** 存储时序数据
- **THEN** 数据 SHALL 按列存储
- **THEN** 每个时间序列 SHALL 独立存储

#### Scenario: 时间分区
- **WHEN** 写入时序数据
- **THEN** 数据 SHALL 按时间分块
- **THEN** 块大小 SHALL 可配置（如按天/按月）

#### Scenario: 块元数据
- **WHEN** 创建时间块
- **THEN** 元数据 SHALL 包含：start_time, end_time, num_points, min/max values

---

### Requirement: 时序压缩算法

系统 SHALL 实现时序数据压缩算法。

#### Scenario: Delta 编码
- **WHEN** 压缩相邻值
- **WHEN** 值变化平滑
- **THEN** Delta 编码 SHALL 被应用
- **THEN** 存储差分值而非原始值

#### Scenario: RLE 压缩
- **WHEN** 存在连续重复值
- **THEN** Run-Length Encoding SHALL 被应用
- **THEN** 存储 (值, 重复次数) 对

#### Scenario: Gorilla 压缩
- **WHEN** 使用 Gorilla 压缩算法
- **WHEN** 时序数据具有时间相关性
- **THEN** 异或 + 熵编码 SHALL 被应用
- **THEN** 压缩率可达 10x 以上

#### Scenario: Bit-packing
- **WHEN** 存储小范围整数
- **THEN** 紧凑位存储 SHALL 被应用
- **THEN** 减少存储空间

---

### Requirement: 压缩统计

系统 SHALL 提供压缩效果统计。

#### Scenario: 压缩率查询
- **WHEN** 查询压缩统计
- **THEN** 原始大小 SHALL 被记录
- **THEN** 压缩后大小 SHALL 被记录
- **THEN** 压缩比 SHALL 被计算

#### Scenario: 自动压缩触发
- **WHEN** 数据量达到阈值
- **THEN** 自动压缩 SHALL 被触发
- **THEN** 后台任务 SHALL 执行

---

### Requirement: 列式读取

系统 SHALL 实现高效的列式读取。

#### Scenario: 列裁剪
- **WHEN** 查询只涉及部分列
- **THEN** 只读取需要的列
- **THEN** I/O SHALL 被减少

#### Scenario: 块跳过
- **WHEN** 查询特定时间范围
- **THEN** 不相关时间块 SHALL 被跳过
- **THEN** 基于元数据的块过滤

#### Scenario: 批量读取
- **WHEN** 读取列数据
- **THEN** 批量读取 SHALL 被优化
- **THEN** 减少随机 I/O

---

### Requirement: 时序索引

系统 SHALL 实现时序数据索引。

#### Scenario: 时间索引
- **WHEN** 创建时间索引
- **THEN** 时间到块 SHALL 的映射 SHALL 被维护
- **THEN** 范围查询 SHALL 能快速定位

#### Scenario: Tag 索引
- **WHEN** 为 tag 列创建索引
- **THEN** 倒排索引 SHALL 被构建
- **THEN** 按 tag 查询 SHALL 高效

#### Scenario: 降采样索引
- **WHEN** 创建物化视图
- **THEN** 预计算的聚合 SHALL 被存储
- **THEN** 高精度查询 SHALL 聚合低精度数据

---

### Requirement: 写入优化

系统 SHALL 实现高效的时序数据写入。

#### Scenario: 批量写入
- **WHEN** 批量写入数据点
- **THEN** 批量缓冲 SHALL 被应用
- **THEN** 减少 I/O 次数

#### Scenario: WAL 写入
- **WHEN** 写入数据
- **THEN** WAL SHALL 被先写入
- **THEN** 崩溃恢复 SHALL 被支持

#### Scenario: 内存缓冲
- **WHEN** 写入数据先到内存
- **THEN** 内存缓冲 SHALL 被使用
- **THEN** 定期刷盘 SHALL 被执行

