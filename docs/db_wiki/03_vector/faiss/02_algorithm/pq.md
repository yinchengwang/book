# 核心算法 — PQ 乘积量化

## 学习目标

- 理解乘积量化（Product Quantization）的原理
- 掌握 PQ 在 Faiss 中的实现和参数配置

## 原理

乘积量化将高维向量分割为多个子空间，分别量化后拼接编码：

```mermaid
graph LR
    subgraph "原始向量 128 维"
        V[128 维向量<br/>4 字节/维 = 512 字节]
    end

    subgraph "分割为 8 个子空间"
        V --> S1[0-15 维]
        V --> S2[16-31 维]
        V --> S3[32-47 维]
        V --> S4[48-63 维]
        V --> S5[64-79 维]
        V --> S6[80-95 维]
        V --> S7[96-111 维]
        V --> S8[112-127 维]
    end

    subgraph "每个子空间量化"
        S1 --> C1[K-Means 256 中心<br/>= 1 字节]
        S2 --> C2[1 字节]
        S3 --> C3[1 字节]
        S8 --> C8[1 字节]
    end

    subgraph "编码结果"
        C1 --> CODE[8 字节<br/>压缩比: 512/8 = 64x]
    end
```

### 算法流程

```mermaid
flowchart TD
    TRAIN[训练阶段] --> SPLIT[将 D 维向量<br/>分割为 M 个子空间]
    SPLIT --> CLUSTER[每个子空间<br/>K-Means 聚类, 256 中心]
    CLUSTER --> CODEBOOK[得到 M 个码本<br/>每个码本 256 个中心]

    ENCODE[编码阶段] --> QUANT[每个子空间<br/>找到最近的中心]
    QUANT --> COMP[组合 M 个 1 字节编码<br/>= M 字节压缩向量]

    SEARCH[搜索阶段] --> Q_PRE[查询向量按相同方式分割]
    Q_PRE --> Q_CODE[每个子空间<br/>计算到 256 中心的距离]
    Q_CODE --> TABLE[生成长度 M×256 的<br/>距离查找表]
    TABLE --> ADC[使用 ADC 计算近似距离<br/>查表求和]
    ADC --> RES[Top-K 结果]
```

## 参数选择

- **M**：子空间数（编码字节数）
- **nbits**：每个子空间的量化位数（通常 8）

```mermaid
graph TD
    M[M 参数选择] --> M_SMALL[M=4<br/>4 字节/向量<br/>压缩比 128x]
    M --> M_MEDIUM[M=8<br/>8 字节/向量<br/>压缩比 64x]
    M --> M_LARGE[M=16<br/>16 字节/向量<br/>压缩比 32x]
    M --> M_HUGE[M=32<br/>32 字节/向量<br/>压缩比 16x]

    M_SMALL --> Q_LOW[精度较低]
    M_MEDIUM --> Q_MED[精度中等]
    M_LARGE --> Q_HIGH[精度高]
    M_HUGE --> Q_VHIGH[精度很高]
```

## PQ 搜索: SDC vs ADC

```mermaid
graph LR
    subgraph "SDC 对称距离计算"
        V[查询向量<br/>量化编码] --> COMP[编码对编码<br/>查表距离]
        DB[库向量<br/>量化编码] --> COMP
    end

    subgraph "ADC 非对称距离计算"
        Q_RAW[查询向量<br/>原始值] --> COMP2[原始对编码<br/>精确距离]
        DB_CODE[库向量<br/>量化编码] --> COMP2
        COMP2 --> NOTE[精度更高<br/>Faiss 默认使用]
    end
```

## Faiss 实现

```python
import faiss
import numpy as np

d = 128
m = 8  # 8 字节编码

# 纯 PQ 索引
index = faiss.IndexPQ(d, m, faiss.METRIC_L2)

# 训练
xb = np.random.random((50000, d)).astype('float32')
index.train(xb)
index.add(xb)

# 搜索
xq = np.random.random((10, d)).astype('float32')
D, I = index.search(xq, k=5)

# IVF + PQ 组合
quantizer = faiss.IndexFlatL2(d)
index_ivfpq = faiss.IndexIVFPQ(quantizer, d, nlist=100, m=m, nbits=8)
index_ivfpq.train(xb)
index_ivfpq.add(xb)
index_ivfpq.nprobe = 10
D, I = index_ivfpq.search(xq, k=5)
```

## PQ 内存压缩

| 索引类型 | 内存/向量 | 1M 向量内存 |
|---------|-----------|------------|
| IndexFlat | 512字节（128维） | 512MB |
| IndexPQ (m=8) | 8字节 | 8MB |
| IndexPQ (m=16) | 16字节 | 16MB |
| IndexIVFPQ (m=8) | 8字节 + IVF头 | ~10MB |

## 要点总结

- PQ 将高维空间分割为子空间分别量化，大幅压缩向量
- 搜索时使用 ADC（非对称距离计算），保持较高精度
- PQ 编码长度 M 控制压缩比和精度之间的权衡
- IVF + PQ 组合（IndexIVFPQ）是 Faiss 最常用的高配方案

## 思考题

1. PQ 的 M 参数选择与向量维度 D 之间有什么关系？为什么 M 需要能整除 D？
2. ADC 比 SDC 精度更高，原因是什么？
3. PQ 量化对哪些数据分布特别有效？对哪些分布效果较差？