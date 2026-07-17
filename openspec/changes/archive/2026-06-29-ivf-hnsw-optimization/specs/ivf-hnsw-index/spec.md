# IVF-HNSW 索引规格

## Purpose

定义 HNSW-IVF 复合索引的功能需求、接口规范和行为约束。

## ADDED Requirements

### Requirement: HNSW-IVF 索引创建

HNSW-IVF 索引 SHALL 通过 `faiss_ivf_hnsw_index_create()` 函数创建，接受参数结构体包含所有可配置项。

#### Scenario: 基础创建

- **WHEN** 调用 `faiss_ivf_hnsw_index_create()` 且参数合法
- **THEN** 返回有效的索引指针，索引状态为未训练

#### Scenario: 参数校验失败

- **WHEN** 调用 `faiss_ivf_hnsw_index_create()` 且参数非法（nlist ≤ 0, dims ≤ 0 等）
- **THEN** 返回 NULL，不分配资源

### Requirement: HNSW-IVF 训练流程

HNSW-IVF 索引 SHALL 在训练阶段依次执行：一级 K-Means、二级 K-Means、HNSW 图构建、量化器训练（可选）。

#### Scenario: 完整训练流程

- **WHEN** 对未训练的索引调用 `faiss_ivf_hnsw_index_train()` 且训练向量数量充足
- **THEN** 索引状态变为已训练，一级中心、二级中心、HNSW 图均已构建

#### Scenario: 训练后禁止重训

- **WHEN** 对已训练的索引调用 `faiss_ivf_hnsw_index_train()`
- **THEN** 返回错误码 -1，索引状态保持不变

### Requirement: HNSW-IVF 搜索流程

HNSW-IVF 索引 SHALL 在搜索阶段使用 HNSW 替代暴力扫描找 nprobe 个一级簇。

#### Scenario: HNSW 粗排搜索

- **WHEN** 对已训练的索引调用 `faiss_ivf_hnsw_index_search()` 且 nprobe ≤ nlist
- **THEN** 搜索流程使用 HNSW 找到最近的 nprobe 个一级簇

#### Scenario: nprobe 超过 nlist

- **WHEN** nprobe 参数大于 nlist
- **THEN** 自动将 nprobe 修正为 nlist

### Requirement: HNSW-IVF 二级桶展开

搜索到 nprobe 个一级簇后，索引 SHALL 展开每个一级簇下的所有二级桶。

#### Scenario: 二级桶按距离排序

- **WHEN** 展开二级桶时
- **THEN** 二级桶按其中心到查询向量的距离升序排列

### Requirement: HNSW-IVF 量化支持

HNSW-IVF 索引 SHALL 支持 PQ/SQ/LVQ/RQ 量化，量化应用于桶内向量而非中心点。

#### Scenario: 量化模式搜索

- **WHEN** 索引启用量化且调用搜索
- **THEN** 使用 ADC（Asymmetric Distance Computation）查表计算距离

### Requirement: HNSW-IVF 参数配置

所有 HNSW-IVF 参数 SHALL 支持运行时配置，包括 nprobe、ef_search、max_assignments。

#### Scenario: 更新 nprobe

- **WHEN** 调用 `faiss_ivf_hnsw_index_set_nprobe()` 设置新值
- **THEN** 下次搜索使用新的 nprobe 值

#### Scenario: 更新 ef_search

- **WHEN** 调用 `faiss_ivf_hnsw_index_set_ef_search()` 设置新值
- **THEN** 下次搜索使用新的 ef_search 值

### Requirement: HNSW-IVF 生命周期管理

索引 SHALL 支持创建、训练、添加、搜索、重置、销毁的完整生命周期。

#### Scenario: 重置索引

- **WHEN** 调用 `faiss_ivf_hnsw_index_reset()`
- **THEN** 清空所有已添加向量，保留训练结果（中心点、HNSW 图）

#### Scenario: 销毁索引

- **WHEN** 调用 `faiss_ivf_hnsw_index_drop()`
- **THEN** 释放所有资源，索引不可再使用

### Requirement: HNSW-IVF 向量添加

索引 SHALL 支持批量添加向量，添加时自动分配到对应的桶。

#### Scenario: 添加向量

- **WHEN** 对已训练的索引调用 `faiss_ivf_hnsw_index_add()`
- **THEN** 向量被分配到最近的桶，并记录到倒排列表

#### Scenario: 未训练时添加

- **WHEN** 对未训练的索引调用 `faiss_ivf_hnsw_index_add()`
- **THEN** 返回错误码 -1，不添加向量
