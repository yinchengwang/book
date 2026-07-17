# IVF-PQ 倒排量化索引

## Purpose

实现 IVF-PQ (Inverted File with Product Quantization) 索引，结合倒排索引和乘积量化，实现大规模向量压缩存储和高效检索。

## Requirements

### Requirement: IVF-PQ 索引基础操作

IVF-PQ 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `ivf_pq_index_t *ivf_pq_create(int nlist, int pq_m, int pq_bits)` | 创建索引 |
| 训练 | `int ivf_pq_train(ivf_pq_index_t *idx, int n, const float *vectors)` | 训练聚类和码书 |
| 添加向量 | `int ivf_pq_add(ivf_pq_index_t *idx, int n, const float *vectors, int *ids)` | 添加向量 |
| 搜索 | `int ivf_pq_search(ivf_pq_index_t *idx, const float *query, int k, int *ids, float *distances)` | K 近邻搜索 |
| 重排 | `int ivf_pq_rerank(ivf_pq_index_t *idx, int n, const float *vectors, int k, int *ids, float *distances)` | 重排精排 |
| 保存 | `int ivf_pq_save(ivf_pq_index_t *idx, const char *path)` | 保存索引 |
| 加载 | `ivf_pq_index_t *ivf_pq_load(const char *path)` | 加载索引 |
| 销毁 | `void ivf_pq_destroy(ivf_pq_index_t *idx)` | 释放资源 |

### Requirement: IVF-PQ 核心数据结构

```c
// IVF-PQ 索引
typedef struct ivf_pq_index {
    int nlist;                    // 聚类中心数
    int nprobe;                   // 查询探针数（可配置）
    int pq_m;                     // PQ 子空间数
    int pq_bits;                  // PQ 每子空间比特数
    int pq_k;                     // PQ 码书大小 (2^pq_bits)
    int dims;                     // 向量维度

    // 聚类
    float *centroids;             // [nlist, dims] 聚类中心
    void *invlists;               // 倒排列表

    // PQ 量化器
    quantizer_t *pq;              // PQ 量化器

    // 元数据
    int ntotal;                   // 向量总数
    size_t code_size;             // 单向量编码大小
} ivf_pq_index_t;

// PQ 编码向量
typedef struct ivf_pq_code {
    int cluster;                  // 聚类 ID
    uint8_t *pq_code;            // [pq_m] PQ 编码
} ivf_pq_code_t;
```

### Requirement: 存储结构

```
┌─────────────────────────────────────────────────────────────┐
│                        IVF-PQ 存储结构                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────┐                                            │
│  │  Header     │  nlist, nprobe, pq_m, pq_bits, dims        │
│  └─────────────┘                                            │
│          │                                                  │
│          ▼                                                  │
│  ┌─────────────┐                                            │
│  │  Centroids  │  [nlist, dims] float32                      │
│  │  聚类中心   │                                            │
│  └─────────────┘                                            │
│          │                                                  │
│          ▼                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  List 0    │  │  List 1    │  │  List N-1  │           │
│  │  [id,code] │  │  [id,code] │  │  [id,code] │           │
│  │  [id,code] │  │  [id,code] │  │            │           │
│  │  ...       │  │  ...       │  │            │           │
│  └─────────────┘  └─────────────┘  └─────────────┘           │
│       │               │               │                      │
│       └───────────────┴───────────────┘                      │
│                       │                                      │
│                       ▼                                      │
│              PQ 压缩后的向量                                  │
│              [pq_m bytes per vector]                        │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### Requirement: 搜索流程

IVF-PQ 搜索 SHALL 按以下流程执行：

```
1. 计算 Query 到所有聚类中心的距离
2. 选择最近的 nprobe 个聚类
3. 遍历选中聚类的倒排列表
4. 对列表中的每个向量：
   a. 获取 PQ 编码
   b. 用 ADC (Asymmetric Distance Computation) 计算距离
      dist ≈ ||q - PQ(code)||
5. 维护大小为 k 的最小堆
6. 返回 top-k 结果
```

### Requirement: 性能参数

| 参数 | 说明 | 典型值 |
|------|------|--------|
| `nlist` | 聚类数 | 1024 |
| `nprobe` | 探针数 | 64 |
| `pq_m` | PQ 子空间数 | 16 |
| `pq_bits` | 每子空间比特数 | 8 |

#### Scenario: 大规模向量检索

- **WHEN** 用户有 1000 万个 128 维向量
- **AND** 使用 IVF-PQ(1024, 16, 8)
- **THEN** 存储大小约为原始的 1/8（PQ 压缩）
- **AND** 搜索时间约为 Flat 的 1/16（nprobe=64/nlist=1024）

#### Scenario: 精度-速度权衡

- **WHEN** 用户调高 `nprobe`（如 256）
- **THEN** 召回率提高，搜索时间增加
- **AND** 调低 `pq_bits`（如 4）
- **THEN** 存储更小，精度下降
