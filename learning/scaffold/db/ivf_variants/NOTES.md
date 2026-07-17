# IVF 变体工程对照笔记

## 概述

本笔记对照 `engineering/src/db/index/vector_index/ivf/` 与 `ivf_pq/` 中的实际实现，分析 IVF 家族索引的工程细节。

## IVF 核心架构（ivf/）

### 两级聚类设计

`faiss_ivf_private.h` 中定义的 `faiss_ivf` 结构采用两级聚类：
- `primary_centroids`: 一级中心数组 [nlist × dims]
- `secondary_centroids`: 二级中心数组 [nlist × nlist2 × dims]

这种设计将粗粒度空间划分（nlist 个一级簇）与细粒度搜索（nprobe × nlist2 个桶）结合，在相近的索引大小下获得更好的搜索精度/速度折中。

### 生命周期管理

`faiss_ivf_core.c` 定义了完整的索引生命周期：
- `faiss_ivf_index_create()`: 三步分配（主结构 → 训练结果 → quantizer）
- `faiss_ivf_index_reset()`: 清空数据但保留配置
- `faiss_ivf_index_drop()`: 释放所有资源

### 墓碑删除机制

`faiss_ivf_index_remove_ids()` 采用墓碑策略而非物理删除：
1. 通过 DirectMap 查询 id → (list_no, offset)
2. 在倒排桶中标记 id = -1
3. 清除 DirectMap 映射
4. `n_total` 递减

墓碑在 `faiss_ivf_index_compact()` 时被物理移除，避免频繁内存移动。

## IVF-PQ 实现（ivf_pq/）

### Product Quantization 编码

`ivf_pq.c` 中的 `ivf_pq_add()` 实现了完整的 PQ 编码流程：
1. 向量路由到最近簇
2. 调用 `quantizer_encode()` 生成 PQ 码
3. 存储 id + cluster + pq_code

### ADC 距离计算

搜索时使用 Asymmetric Distance Computation（`ivf_pq_search()`）：
- `quantizer_compute_distance_table()`: 预计算查询向量到所有码字中心的距离
- `quantizer_adc_distance()`: 查表累加各分段的距离

### 文件持久化

`ivf_pq_save()` / `ivf_pq_load()` 实现索引的二进制序列化，包含魔数、版本头和聚类中心数据。

## 关键数据结构对照

| 组件 | 工程结构体 | 核心字段 |
|------|-----------|---------|
| IVF 主结构 | faiss_ivf | primary_centroids, secondary_centroids, invlists |
| 倒排桶 | faiss_inverted_lists_t | lists 数组，存储 id 和 code |
| IVF-PQ | ivf_pq_index_t | centroids, lists, pq (quantizer) |
| 量化参数 | ivf_pq_quantization_params_t | pq_m (分段数), pq_bits (每段位数) |

## 真实源码路径

- IVF 核心实现: `engineering/src/db/index/vector_index/ivf/faiss_ivf_core.c`
- IVF 内部结构: `engineering/src/db/index/vector_index/ivf/faiss_ivf_private.h`
- IVF 倒排桶: `engineering/src/db/index/vector_index/ivf/faiss_inverted_lists.c`
- IVF-PQ 实现: `engineering/src/db/index/vector_index/ivf_pq/ivf_pq.c`
- IVF-HNSW: `engineering/src/db/index/vector_index/ivf_hnsw/`
