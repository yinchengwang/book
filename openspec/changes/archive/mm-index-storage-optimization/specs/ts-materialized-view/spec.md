# 时序物化视图规范

## ADDED Requirements

### Requirement: 物化视图定义

时序存储引擎 SHALL 支持预计算聚合结果的物化视图。

#### Scenario: 创建物化视图
- **WHEN** 调用 `ts_mview_create(index, name, granularity, func)`
- **THEN** 系统 SHALL 创建物化视图定义
- **AND** 系统 SHALL 分配存储空间
- **AND** 系统 SHALL 返回视图句柄

#### Scenario: 物化视图刷新
- **WHEN** 调用 `ts_mview_refresh(mview)`
- **THEN** 系统 SHALL 计算聚合结果
- **AND** 系统 SHALL 存储到物化视图
- **AND** 系统 SHALL 更新刷新时间戳

### Requirement: 物化视图类型

系统 SHALL 支持多种聚合函数的物化视图。

#### Scenario: 创建 AVG 物化视图
- **WHEN** 创建 granularity=HOUR, func=AVG 的物化视图
- **THEN** 系统 SHALL 预计算每小时的平均值
- **AND** 查询物化视图 SHALL 直接返回结果

#### Scenario: 创建 MIN/MAX 物化视图
- **WHEN** 创建 MIN 物化视图
- **THEN** 系统 SHALL 跟踪每时间粒度的最小值
- **AND** 创建 MAX 物化视图
- **AND** 系统 SHALL 跟踪每时间粒度的最大值

#### Scenario: 创建百分位物化视图
- **WHEN** 创建 percentile=95 的物化视图
- **THEN** 系统 SHALL 预计算每时间粒度的 P95 值
- **AND** 物化视图 SHALL 包含 min, max, p50, p95, p99

### Requirement: 物化视图查询

物化视图 SHALL 支持快速聚合查询。

#### Scenario: 查询物化视图
- **WHEN** 调用 `ts_mview_query(mview, start_ts, end_ts, results)`
- **THEN** 系统 SHALL 从物化数据直接读取
- **AND** 无需解压原始数据
- **AND** 查询性能 SHALL 优于实时聚合

#### Scenario: 物化视图自动刷新
- **WHEN** 新数据到达且当前时间粒度已过去
- **THEN** 系统 SHALL 自动刷新对应粒度的物化视图
- **AND** 刷新 SHALL 在后台执行
- **AND** 不阻塞写入操作

### Requirement: 物化视图存储格式 SHALL 遵循标准二进制布局

```
mview_<name>.bin:
  - header: magic, version, name, granularity, func, count
  - MviewRecord[0..n-1]:
    - timestamp: int64_t (对齐到粒度边界)
    - value: double (聚合结果)
    - count: uint32_t (参与聚合的点数)

mview_meta.bin:
  - view_count: uint32_t
  - views[0..n-1]:
    - name: 视图名
    - granularity: uint8_t
    - func: uint8_t
    - last_refresh: int64_t
```

#### Scenario: 保存物化视图
- **WHEN** 调用 `ts_mview_save(mview)`
- **THEN** 系统 SHALL 将物化数据写入文件
- **AND** 系统 SHALL 更新元数据

#### Scenario: 加载物化视图
- **WHEN** 调用 `ts_mview_load(path)`
- **THEN** 系统 SHALL 读取物化数据
- **AND** 系统 SHALL 重建视图结构
