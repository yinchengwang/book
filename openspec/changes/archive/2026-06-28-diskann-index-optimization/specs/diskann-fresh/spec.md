# FreshDiskANN 增量更新规格

## Purpose

定义 FreshDiskANN 的行为规范，支持动态区/静态区分分离的增量更新。

## 术语说明

| 术语 | 说明 |
|------|------|
| Frozen Index | 静态区，已持久化的主索引 |
| Fresh Index | 动态区，内存中的新增数据 |
| Merge | 合并，将动态区合并到静态区 |
| Merge Threshold | 合并阈值，触发合并的条件 |

## ADDED Requirements

### Requirement: FreshDiskANN 配置

FreshDiskANN SHALL 通过独立的 `diskann_fresh_config_t` 配置。

```c
typedef struct diskann_fresh_config {
    bool enabled;                    // 是否启用 Fresh 模式
    int32_t fresh_capacity;        // 动态区容量
    int32_t merge_threshold;        // 触发合并的阈值
} diskann_fresh_config_t;
```

#### Scenario: 默认配置

- **WHEN** 创建配置时未设置任何参数
- **THEN** `enabled = false`
- **AND** `fresh_capacity = 100000` (10 万)
- **AND** `merge_threshold = 80000` (8 万)

#### Scenario: 启用 Fresh 模式

- **WHEN** `enabled = true`
- **THEN** 系统准备动态区
- **AND** `fresh_capacity >= 1`

### Requirement: 动态区管理

动态区 SHALL 支持向量插入。

#### Scenario: 动态区插入

- **WHEN** Fresh 模式启用且动态区未满
- **AND** 调用 `diskann_index_insert()`
- **THEN** 向量添加到动态区
- **AND** 动态区计数 +1

#### Scenario: 动态区容量

- **WHEN** 动态区已有 `fresh_capacity` 个向量
- **AND** 尝试插入新向量
- **THEN** 返回容量已满错误
- **OR** 触发自动合并

#### Scenario: 动态区图构建

- **WHEN** 向量添加到动态区
- **THEN** 在动态区内执行增量图链接
- **AND** 动态区保持独立图结构

### Requirement: 合并触发

当动态区达到合并阈值 SHALL 触发合并。

#### Scenario: 自动合并

- **WHEN** `enabled = true` 且 `merge_threshold = 80000`
- **AND** 动态区计数 >= 80000
- **THEN** 自动触发合并操作

#### Scenario: 手动合并

- **WHEN** 调用 `diskann_fresh_merge()`
- **THEN** 立即执行合并操作
- **AND** 返回合并状态

#### Scenario: 合并优先级

- **WHEN** 动态区达到阈值但正在进行搜索
- **THEN** 合并可以延后到搜索完成
- **OR** 在后台线程执行合并

### Requirement: 合并流程

合并 SHALL 按以下步骤执行。

#### Scenario: 合并步骤 1 - 持久化

- **WHEN** 开始合并
- **THEN** 将动态区的向量写入磁盘
- **AND** 更新磁盘文件

#### Scenario: 合并步骤 2 - 图更新

- **WHEN** 向量已持久化
- **THEN** 更新静态区图结构
- **AND** 使用增量构建方式添加动态区节点

#### Scenario: 合并步骤 3 - 清空动态区

- **WHEN** 图更新完成
- **THEN** 清空动态区的向量
- **AND** 重置动态区计数

#### Scenario: 合并步骤 4 - 更新入口点

- **WHEN** 合并完成
- **THEN** 更新搜索入口点
- **AND** 确保新向量可被搜索到

### Requirement: 搜索流程

搜索 SHALL 同时访问动态区和静态区。

#### Scenario: 双区搜索

- **WHEN** Fresh 模式启用
- **AND** 动态区有数据
- **THEN** 搜索同时访问：
  - 动态区（内存搜索）
  - 静态区（磁盘搜索）

#### Scenario: 结果合并

- **WHEN** 两个区域都返回候选
- **THEN** 合并候选结果
- **AND** 按距离排序返回 top-k

#### Scenario: 动态区搜索延迟

- **WHEN** 动态区有数据
- **THEN** 动态区搜索延迟 < 静态区磁盘 IO 延迟
- **AND** 动态区结果优先

### Requirement: 持久化

FreshDiskANN SHALL 支持完整的持久化。

#### Scenario: 保存状态

- **WHEN** 调用 `diskann_index_save()`
- **THEN** 保存静态区、动态区（如果有）、合并元数据
- **AND** 保存动态区向量和图结构

#### Scenario: 加载状态

- **WHEN** 调用 `diskann_index_load()`
- **THEN** 加载所有数据
- **AND** 恢复动态区/静态区分离状态

### Requirement: 配置验证

`diskann_fresh_config_validate()` SHALL 验证配置有效性。

#### Scenario: 无效容量

- **WHEN** `fresh_capacity <= 0`
- **THEN** `validate()` 返回 0

#### Scenario: 无效阈值

- **WHEN** `merge_threshold > fresh_capacity`
- **THEN** `validate()` 返回 0

#### Scenario: 有效配置

- **WHEN** `fresh_capacity = 100000, merge_threshold = 80000`
- **THEN** `validate()` 返回 1

### Requirement: 内存管理

动态区 SHALL 合理管理内存。

#### Scenario: 动态区内存占用

- **WHEN** `fresh_capacity = 100000` 且 `dims = 128`
- **THEN** 动态区向量占用约 `100000 × 128 × 4 = 51.2 MB`
- **AND** 动态区图结构额外占用内存

#### Scenario: 合并内存峰值

- **WHEN** 合并执行时
- **THEN** 内存峰值不应超过 `2 × fresh_capacity × dims × sizeof(float)`
