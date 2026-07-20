# 核心算法 — IVF 倒排索引

## 学习目标

- 理解 IVF（Inverted File Index）算法的原理
- 掌握 IVF 在 Faiss 中的实现和参数调优

## 原理

IVF 通过聚类将向量空间划分为 Voronoi 单元，搜索时只访问最近的几个单元：

```mermaid
graph TD
    subgraph "训练阶段"
        DATA[训练数据] --> KMEANS[K-Means 聚类<br/>N 个中心点]
        KMEANS --> CENT[中心点集合<br/>centroids]
        CENT --> ASSIGN[每个向量分配到<br/>最近的中心]
        ASSIGN --> LISTS[构建倒排列表<br/>每个中心一个列表]
    end

    subgraph "搜索阶段"
        Q[查询向量] --> NEAR[找到最近的<br/>nprobe 个中心]
        NEAR --> SCAN[扫描这些中心的<br/>倒排列表]
        SCAN --> RES[Top-K 结果]
    end
```

### 算法流程

```mermaid
flowchart TD
    TRAIN[训练: K-Means 聚类] --> BUILD[构建倒排索引]
    BUILD --> SEARCH[搜索阶段]

    SEARCH --> Q[查询向量 q]
    Q --> QUANT[量化器: 找到最近的 c 个中心]
    QUANT --> LIST[遍历最近 c 个中心的倒排列表]
    LIST --> COMP[计算 q 与列表中每个向量的距离]
    COMP --> HEAP[维护 Top-K 堆]
    HEAP --> RES[返回结果]
```

### 参数

- **nlist**：聚类中心数量，控制倒排列表数
- **nprobe**：搜索时访问的中心数，控制精度-速度权衡

## nprobe 的影响

```mermaid
graph LR
    subgraph "nprobe=1"
        Q1[查询] --> C1[最近中心]
        C1 --> L1[扫描该中心列表]
    end

    subgraph "nprobe=4"
        Q4[查询] --> C4[最近 4 个中心]
        C4 --> L4[扫描 4 个列表]
    end

    subgraph "nprobe=nlist"
        QN[查询] --> ALL[所有中心]
        ALL --> FULL[扫描所有列表<br/>≈ 暴力搜索]
    end
```

精度-速度曲线：

| nprobe | 速度提升 | 召回率 |
|--------|---------|--------|
| 1 | 100x | ~60% |
| 10 | 10x | ~90% |
| 100 | 1x | ~99% |

## Faiss 实现

```python
import faiss
import numpy as np

d = 128
nlist = 100  # 聚类中心数

# 创建量化器 (用于搜索时确定最近中心)
quantizer = faiss.IndexFlatL2(d)

# 创建 IVF 索引
index = faiss.IndexIVFFlat(quantizer, d, nlist, faiss.METRIC_L2)

# 训练
xb = np.random.random((50000, d)).astype('float32')
index.train(xb)
index.add(xb)

# 搜索
index.nprobe = 10  # 搜索时访问 10 个中心
xq = np.random.random((10, d)).astype('float32')
D, I = index.search(xq, k=5)
```

## 内存与性能

```mermaid
graph TB
    subgraph "IndexIVFFlat 内存构成"
        CENT[中心向量: nlist × d × 4B] --> M1[小]
        LISTS[倒排列表: 存储原始向量] --> M2[与 IndexFlat 相同]
        IDS[ID 列表: 存储向量 ID] --> M3[nlist 个列表头]
    end
```

| 索引类型 | 内存 | 搜索速度 | 精度 |
|---------|------|---------|------|
| IndexFlat | N × D × 4B | 慢 | 精确 |
| IndexIVFFlat | N × D × 4B + 中心 | 快 10-100x | ~90%+ |
| IndexIVFPQ | N × code_size | 快 10-100x | ~80-95% |

## 要点总结

- IVF 通过 K-Means 聚类 + 倒排列表实现快速近似搜索
- nlist 控制训练时的聚类数，nprobe 控制搜索时的访问范围
- IndexIVFFlat 不压缩向量，精度损失来自 nprobe 限制
- 速度和精度通过 nprobe 参数连续调节

## 思考题

1. nlist 设置过大或过小各有什么影响？
2. IVF 训练时 K-Means 的迭代次数对搜索结果有什么影响？
3. 如果数据分布极不均匀，IVF 的倒排列表长度差异很大，如何优化？