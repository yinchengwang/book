# Milvus IVF 索引

## 学习目标

- 理解 Milvus 中 IVF 索引的实现方式
- 掌握 IVF 参数配置和调优方法

## IVF 原理

Milvus 中的 IVF（Inverted File）索引基于 Faiss 的 IVF 实现，但在集群环境下增加了分布式支持：

```mermaid
flowchart TD
    TRAIN[训练: K-Means 聚类<br/>在 Milvus 中自动触发] --> CENT[N 个中心点]
    CENT --> ASSIGN[向量分配到最近的中心]
    ASSIGN --> BUILD[建立倒排列表<br/>每个中心一个列表]

    Q[查询向量] --> FIND[查找最近的 nprobe 个中心]
    FIND --> SCAN[扫描这些中心的倒排列表]
    SCAN --> RES[Top-K 结果]
```

## 参数配置

```python
from pymilvus import Collection

# IVF 索引参数
index_params = {
    "metric_type": "L2",
    "index_type": "IVF_FLAT",   # 或 IVF_PQ, IVF_SQ8
    "params": {"nlist": 1024}
}

# 搜索参数
search_params = {
    "metric_type": "L2",
    "params": {"nprobe": 10}
}

collection.create_index("embedding", index_params)
collection.search(query_vectors, "embedding", search_params, limit=10)
```

## IVF 系列变体

```mermaid
graph TD
    IVF[IVF 索引族] --> IVF_FLAT[IVF_FLAT<br/>不压缩向量<br/>精度最高]
    IVF --> IVF_PQ[IVF_PQ<br/>乘积量化压缩<br/>内存最低]
    IVF --> IVF_SQ8[IVF_SQ8<br/>标量量化 8-bit<br/>折中方案]

    IVF_FLAT --> M1[内存: 向量原大小<br/>精度: 高]
    IVF_PQ --> M2[内存: 压缩 80-90%<br/>精度: 中]
    IVF_SQ8 --> M3[内存: 压缩 75%<br/>精度: 中高]
```

## 参数调优

| 参数 | 说明 | 推荐值 |
|------|------|--------|
| nlist | 聚类中心数 | 4×sqrt(N) |
| nprobe | 搜索访问中心数 | 10-50 |
| nbits (IVF_PQ) | 量化位数 | 8 |

```mermaid
graph LR
    A[nprobe 调优] --> B[nprobe=1<br/>最快但精度低]
    A --> C[nprobe=10<br/>推荐起点]
    A --> D[nprobe=100<br/>接近暴力搜索精度]
    B --> E[召回率 ~60%]
    C --> F[召回率 ~90%]
    D --> G[召回率 ~99%]
```

## 要点总结

- Milvus 的 IVF 索引基于 Faiss，封装为分布式版本
- 三种变体：IVF_FLAT（高精度）、IVF_PQ（低内存）、IVF_SQ8（折中）
- nlist 控制训练时的聚类数，nprobe 控制搜索粒度
- 索引构建在 IndexNode 上异步执行

## 思考题

1. 为什么 nlist 推荐设置为 4×sqrt(N)？这个公式的由来？
2. IVF_PQ 中 PQ 量化对精度的影响有多大？如何选择合适的 m 值？
3. Milvus 中索引构建是异步的，在索引构建完成前如何保证搜索可用？