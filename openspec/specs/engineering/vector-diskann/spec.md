# vector-diskann Specification

## Purpose
TBD - created by archiving change mmdb-htap-evolution. Update Purpose after archive.
## Requirements
### Requirement: DiskANN 索引概述

系统 SHALL 实现 DiskANN 磁盘友好的向量索引。

#### Scenario: NSG 图构建
- **WHEN** 构建 DiskANN 索引
- **WHEN** 使用 Navigable Spilling Graph (NSG) 图结构
- **THEN** 索引 SHALL 支持磁盘存储
- **THEN** 查询 SHALL 只需少量磁盘 I/O

#### Scenario: 内存映射支持
- **WHEN** 配置使用 mmap
- **THEN** 索引数据 SHALL 通过内存映射访问
- **THEN** 操作系统 SHALL 管理页面置换

#### Scenario: 异步 I/O
- **WHEN** 执行向量搜索
- **THEN** I/O SHALL 使用异步预取
- **THEN** 减少 I/O 等待时间

---

### Requirement: DiskANN 索引参数

系统 SHALL 支持 DiskANN 索引的可配置参数。

#### Scenario: 邻居数配置
- **WHEN** 创建索引
- **WHEN** 设置 `L` (最大邻居数)
- **THEN** 构建时 SHALL 使用指定邻居数

#### Scenario: 搜索宽度配置
- **WHEN** 设置 `search_L` (搜索宽度)
- **THEN** 查询时 SHALL 使用指定宽度
- **THEN** 影响查询精度和速度

#### Scenario: 构造参数配置
- **WHEN** 设置 `construction_L` (构造宽度)
- **THEN** 构建时 SHALL 使用指定宽度

---

### Requirement: 磁盘驻留向量存储

系统 SHALL 支持向量数据存储在磁盘上。

#### Scenario: 向量分页存储
- **WHEN** 存储向量数据
- **THEN** 向量 SHALL 被分页存储
- **THEN** 每页 SHALL 包含固定数量的向量

#### Scenario: 页面缓存
- **WHEN** 访问向量数据
- **THEN** 热点页面 SHALL 被缓存
- **THEN** LRU 策略 SHALL 被使用

#### Scenario: 直接 I/O
- **WHEN** 配置使用直接 I/O
- **THEN** 绕过操作系统页面缓存
- **THEN** 减少内存复制

---

### Requirement: DiskANN 搜索

系统 SHALL 实现 DiskANN 搜索算法。

#### Scenario: 贪婪搜索
- **WHEN** 执行向量搜索
- **THEN** 贪婪搜索 SHALL 被用于图遍历
- **THEN** 找到近似最近邻

#### Scenario: 搜索剪枝
- **WHEN** 搜索过程中
- **THEN** 不相关分支 SHALL 被剪枝
- **THEN** 减少搜索范围

#### Scenario: 结果排序
- **WHEN** 返回搜索结果
- **THEN** 结果 SHALL 按距离升序排列
- **THEN** 最近的向量 SHALL 在前面

---

### Requirement: 分层索引策略

系统 SHALL 实现热/温/冷数据的分层索引。

#### Scenario: 热数据索引（HNSW）
- **WHEN** 数据量 < 100 万向量
- **WHEN** 配置使用内存模式
- **THEN** HNSW 索引 SHALL 被使用
- **THEN** 全部在内存中

#### Scenario: 温数据索引（IVF-PQ）
- **WHEN** 数据量 100 万 - 1000 万
- **WHEN** 配置使用 SSD 模式
- **THEN** IVF-PQ 索引 SHALL 被使用
- **THEN** 索引在内存，数据在 SSD

#### Scenario: 冷数据索引（IVF）
- **WHEN** 数据量 > 1000 万
- **WHEN** 配置使用对象存储模式
- **THEN** IVF 索引 SHALL 被使用
- **THEN** 支持超大规模数据

---

### Requirement: 索引持久化

系统 SHALL 支持索引的持久化和加载。

#### Scenario: 索引保存
- **WHEN** 调用保存索引 API
- **THEN** 索引 SHALL 被序列化到磁盘
- **THEN** 图结构 SHALL 被保留

#### Scenario: 索引加载
- **WHEN** 调用加载索引 API
- **THEN** 索引 SHALL 从磁盘恢复
- **THEN** 可继续进行搜索

#### Scenario: 增量索引
- **WHEN** 新增向量
- **THEN** 增量更新 SHALL 被支持
- **THEN** 无需完全重建索引

---

### Requirement: 量化压缩

系统 SHALL 支持 DiskANN 与量化压缩结合。

#### Scenario: PQ 压缩
- **WHEN** 启用 Product Quantization
- **THEN** 向量 SHALL 被压缩为 PQ 码
- **THEN** 存储空间 SHALL 大幅减少

#### Scenario: SQ 压缩
- **WHEN** 启用 Scalar Quantization
- **THEN** 浮点数 SHALL 被量化为整数
- **THEN** 减少内存和存储需求

