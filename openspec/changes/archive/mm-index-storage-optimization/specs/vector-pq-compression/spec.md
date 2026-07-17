# 向量量化压缩规范

## ADDED Requirements

### Requirement: PQ 量化压缩

向量存储引擎 SHALL 支持 Product Quantization (PQ) 量化压缩，将高维向量压缩为紧凑编码。

#### Scenario: 创建 PQ 量化器
- **WHEN** 调用 `pq_quantizer_create(dim, M, nbits)`
- **THEN** 系统 SHALL 将向量分割为 M 个子向量
- **AND** 系统 SHALL 为每个子空间训练码书（k = 2^nbits 个中心点）
- **AND** 系统 SHALL 返回量化器句柄

#### Scenario: 量化向量
- **WHEN** 调用 `pq_quantize(quantizer, vector, code)`
- **THEN** 系统 SHALL 将向量分割为 M 个子向量
- **AND** 对每个子向量，系统 SHALL 找到最近的码书中心
- **AND** 系统 SHALL 将中心索引编码为 nbits 位存储到 code

#### Scenario: 计算量化距离
- **WHEN** 调用 `pq_compute_distance(quantizer, query, codes, n, distances)`
- **THEN** 系统 SHALL 使用 ADIS (Asymmetric Distance Computation)
- **AND** 系统 SHALL 返回 n 个距离值

### Requirement: OPQ 优化

系统 SHALL 支持 Optimized PQ (OPQ)，通过旋转子空间减少量化误差。

#### Scenario: 训练 OPQ
- **WHEN** 调用 `opq_train(vectors, n, dim, M)`
- **THEN** 系统 SHALL 迭代优化旋转矩阵 R 和码书
- **AND** 系统 SHALL 返回优化后的量化器
- **AND** 量化误差 SHALL 小于标准 PQ

### Requirement: 量化搜索

量化索引 SHALL 支持高效的最近邻搜索。

#### Scenario: PQ 索引搜索
- **WHEN** 调用 `pq_index_search(index, query, k, results)`
- **THEN** 系统 SHALL 在粗量化器上搜索候选集
- **AND** 系统 SHALL 对候选集使用 ADIS 重排
- **AND** 系统 SHALL 返回 top-k 最近邻

#### Scenario: IVF-PQ 索引
- **WHEN** 创建 IVF-PQ 索引
- **THEN** 系统 SHALL 首先将向量聚类为 nlist 个簇
- **AND** 每个簇 SHALL 使用独立的 PQ 编码
- **AND** 搜索时 SHALL 只扫描相关簇

### Requirement: 量化参数配置

系统 SHALL 支持可配置的量化参数。

#### Scenario: 配置量化参数
- **WHEN** 创建量化器时
- **THEN** 系统 SHALL 支持 M=8, 16, 32, 48, 64
- **AND** 系统 SHALL 支持 nbits=4, 6, 8, 12
- **AND** 默认配置 SHALL 为 M=48, nbits=8

### Requirement: 量化文件格式

量化后的数据 SHALL 使用标准格式存储。

```
PQ Code File:
  - header: magic, version, n, dim, M, nbits
  - codes: uint8_t[n * (M * nbits / 8)]
  - codebook: float[M * k * (dim / M)]

粗量化器文件:
  - centers: float[nlist * dim]
  - list_offsets: int32_t[nlist + 1]
```

#### Scenario: 保存量化索引
- **WHEN** 调用 `pq_index_save(index, path)`
- **THEN** 系统 SHALL 将量化参数写入头文件
- **AND** 系统 SHALL 保存所有 PQ 编码
- **AND** 系统 SHALL 保存码书

#### Scenario: 加载量化索引
- **WHEN** 调用 `pq_index_load(index, path)`
- **THEN** 系统 SHALL 读取并验证头文件
- **AND** 系统 SHALL 加载码书
- **AND** 系统 SHALL 将编码数据映射到内存
