# 时序分段索引规范

## ADDED Requirements

### Requirement: 时间分段索引

时序存储引擎 SHALL 实现基于时间范围的分段索引。

#### Scenario: 创建分段索引
- **WHEN** 调用 `ts_segment_index_create(path, seg_size)`
- **THEN** 系统 SHALL 创建索引文件和数据文件目录
- **AND** 系统 SHALL 初始化段元数据数组
- **AND** 系统 SHALL 返回索引句柄

#### Scenario: 追加数据点
- **WHEN** 调用 `ts_segment_append(index, timestamp, value)`
- **THEN** 系统 SHALL 定位所属的时间段
- **AND** 如段不存在则创建新段
- **AND** 系统 SHALL 将数据追加到段的压缩缓冲区

#### Scenario: 段自动切换
- **WHEN** 当前段数据点数达到 seg_size
- **THEN** 系统 SHALL 触发段压缩
- **AND** 系统 SHALL 创建新段继续写入
- **AND** 系统 SHALL 更新索引元数据

### Requirement: 段稀疏索引

每个段 SHALL 维护稀疏索引以加速时间范围查询。

#### Scenario: 时间范围查询
- **WHEN** 调用 `ts_segment_query(index, start_ts, end_ts, results)`
- **THEN** 系统 SHALL 使用稀疏索引定位目标段
- **AND** 系统 SHALL 对匹配的段解压并过滤
- **AND** 系统 SHALL 返回结果数据点

#### Scenario: 段元数据查询
- **WHEN** 需要定位时间戳所属段时
- **THEN** 系统 SHALL 使用二分查找稀疏索引
- **AND** 系统 SHALL 返回段元数据（文件偏移、点数范围）

### Requirement: 分段文件格式 SHALL 遵循标准二进制布局

```
segment_index.bin:
  - header: magic, version, seg_count, seg_size
  - SegmentMeta[0..n-1]:
    - start_ts: int64_t
    - end_ts: int64_t
    - file_offset: uint64_t
    - point_count: uint32_t
    - compressed_size: uint32_t
    - min_value: double
    - max_value: double

segments/seg_0000.tsd:
  - Block[0]: Gorilla 压缩块
  - Block[1]: Gorilla 压缩块
  - ...
```

#### Scenario: 持久化段数据
- **WHEN** 调用 `ts_segment_save(index)`
- **THEN** 系统 SHALL 压缩当前未完成的段
- **AND** 系统 SHALL 将压缩数据写入段文件
- **AND** 系统 SHALL 更新索引文件

#### Scenario: 加载段索引
- **WHEN** 调用 `ts_segment_load(path)`
- **THEN** 系统 SHALL 读取索引文件构建段元数据
- **AND** 系统 SHALL 按需加载段数据（不是全部）
- **AND** 系统 SHALL 支持延迟加载

### Requirement: TTL 保留策略

系统 SHALL 支持基于 TTL 的数据清理。

#### Scenario: 过期数据清理
- **WHEN** 调用 `ts_segment_expire(index, retention_days)`
- **THEN** 系统 SHALL 删除早于 retention_days 的段
- **AND** 系统 SHALL 更新索引元数据
- **AND** 系统 SHALL 释放磁盘空间
