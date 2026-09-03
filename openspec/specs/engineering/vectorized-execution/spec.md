# vectorized-execution Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: 向量化执行概述

系统 SHALL 实现向量化执行引擎，提升分析查询性能。

#### Scenario: 列批处理
- **WHEN** 执行向量化算子
- **WHEN** 数据 SHALL 按列批次处理
- **THEN** 一批数据 SHALL 被一起处理
- **THEN** 批次大小 SHALL 可配置（如 1024 行）

#### Scenario: 火山模型改进
- **WHEN** 在向量化模式下
- **WHEN** 算子 SHALL 返回列块而非单行
- **THEN** 减少函数调用开销
- **THEN** 提高 CPU 缓存命中率

---

### Requirement: SIMD 加速

系统 SHALL 实现 SIMD 指令加速。

#### Scenario: SSE/AVX 指令支持
- **WHEN** 执行向量化操作
- **WHEN** 平台支持 SIMD
- **THEN** SIMD 指令 SHALL 被使用
- **THEN** 多数据并行处理

#### Scenario: 距离计算 SIMD 加速
- **WHEN** 计算向量距离
- **WHEN** 使用 SIMD
- **THEN** 多个距离 SHALL 同时计算
- **THEN** 性能 SHALL 大幅提升

#### Scenario: 过滤 SIMD 加速
- **WHEN** 执行条件过滤
- **WHEN** 使用 SIMD
- **THEN** 多个条件 SHALL 同时判断
- **THEN** 性能 SHALL 大幅提升

---

### Requirement: 向量化算子

系统 SHALL 实现向量化版本的算子。

#### Scenario: 向量化 Scan
- **WHEN** 执行向量化扫描
- **WHEN** 一批数据 SHALL 被同时扫描
- **THEN** 过滤 SHALL 在列级别执行
- **THEN** 匹配的行 SHALL 被标记

#### Scenario: 向量化 HashJoin
- **WHEN** 执行向量化哈希连接
- **WHEN** 构建阶段 SHALL 使用 SIMD
- **THEN** 探测阶段 SHALL 使用 SIMD
- **THEN** 连接性能 SHALL 被提升

#### Scenario: 向量化 Aggregation
- **WHEN** 执行向量化聚合
- **WHEN** 分组键 SHALL 被哈希
- **WHEN** 聚合 SHALL 在列块上执行
- **THEN** 性能 SHALL 被提升

---

### Requirement: 代码生成（可选）

系统 SHALL 支持可选的 JIT 代码生成。

#### Scenario: 表达式 JIT 编译
- **WHEN** 执行复杂表达式
- **WHEN** JIT 编译被启用
- **THEN** 表达式 SHALL 被编译为机器码
- **THEN** 执行性能 SHALL 被提升

#### Scenario: 算子融合
- **WHEN** JIT 编译时
- **WHEN** 相邻算子 SHALL 被融合
- **THEN** 减少中间结果物化
- **THEN** 减少内存访问

---

### Requirement: 向量化性能指标

系统 SHALL 提供向量化性能监控。

#### Scenario: 吞吐量监控
- **WHEN** 监控系统
- **THEN** 行处理吞吐量 SHALL 被记录
- **THEN** MB/s 处理速度 SHALL 被计算

#### Scenario: SIMD 利用率
- **WHEN** 监控系统
- **THEN** SIMD 指令使用率 SHALL 被记录
- **THEN** 帮助识别性能瓶颈

