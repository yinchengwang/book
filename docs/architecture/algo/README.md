# 通用算法库 (algo-prod) - 架构设计

本文档提供 algo-prod 通用算法库的架构设计，涵盖距离计算、量化器、K-Means 聚类、基础数据结构等核心模块。

---

## 项目信息

- **名称**: algo-prod 通用算法库
- **类型**: 向量检索基础设施 + 通用数据结构与算法
- **语言**: C11
- **构建系统**: CMake 3.20+

---

## 1. 子系统架构概览

algo-prod 算法库作为向量索引系统的底层基础设施，提供距离计算、向量量化、聚类算法和基础数据结构四大核心能力。

```mermaid
flowchart TB
    subgraph "上层索引系统"
        HNSW[HNSW 索引]
        IVF[IVF 索引]
        DISKANN[DiskANN 索引]
        BM25[BM25 文本索引]
    end

    subgraph "algo-prod 算法库"
        subgraph "距离计算模块"
            DIST[distance.h<br/>距离度量统一接口]
            SIMD[SIMD 加速<br/>L2/Cosine/IP]
        end

        subgraph "量化器模块"
            QUANT[quantization.h<br/>统一量化接口]
            PQ[PQ 乘积量化]
            LVQ[LVQ 局部自适应]
            SQ[SQ 标量量化]
            RQ[RQ 残差量化]
        end

        subgraph "聚类模块"
            KMEANS[kmeans.h<br/>K-Means 聚类]
        end

        subgraph "数据结构模块"
            DICT[dict.h<br/>分词词典]
            LIST[list.h<br/>双向链表]
            MAP[map.h<br/>哈希表]
            STACK[stack.h<br/>栈]
            QUEUE[queue.h<br/>队列]
            PQ_DS[priority_queue.h<br/>优先队列]
            SORT[sort.h<br/>排序算法]
            BS[binary_search.h<br/>二分查找]
        end
    end

    %% 索引依赖关系
    HNSW --> DIST
    HNSW --> QUANT
    IVF --> DIST
    IVF --> QUANT
    IVF --> KMEANS
    DISKANN --> DIST
    DISKANN --> QUANT
    BM25 --> DICT

    %% 模块内部依赖
    QUANT --> PQ
    QUANT --> LVQ
    QUANT --> SQ
    QUANT --> RQ
    QUANT --> DIST
    KMEANS --> DIST
```

---

## 2. 距离计算模块

距离计算是向量索引系统的核心基础，所有近似最近邻搜索算法（HNSW、IVF、DiskANN）都需要通过距离度量判断向量间的相似性。本模块屏蔽不同距离度量的实现差异，并提供统一的接口。

### 2.1 距离度量类型

```mermaid
classDiagram
    class distance_metric_t {
        <<enumeration>>
        +DISTANCE_METRIC_L2_SQUARED = 0
        +DISTANCE_METRIC_COSINE = 1
        +DISTANCE_METRIC_INNER_PRODUCT = 2
        +DISTANCE_METRIC_HAMMING = 3
    }

    class distance_compute {
        +float compute(metric, lhs, rhs, dims)
        +float compute_indexed(metric, vectors, dims, id1, id2)
        +float compute_from_query(metric, query, vectors, dims, id)
        +void compute_batch4_from_query(metric, query, vectors, dims, id1~id4, dis1~dis4)
    }

    class distance_l2sqr {
        +float distance_l2sqr(lhs, rhs, dims)
        +float distance_l2sqr_indexed(vectors, dims, id1, id2)
        +float distance_l2sqr_from_query(query, vectors, dims, id)
        +void distance_l2sqr_batch4_from_query(query, vectors, dims, id1~id4, dis1~dis4)
    }

    distance_compute --> distance_metric_t : 使用
    distance_l2sqr --> distance_metric_t : L2_SQUARED 特化
```

**距离度量说明**:

| 度量类型 | 适用场景 | 数学定义 |
|----------|----------|----------|
| L2_SQUARED | 通用向量检索，HNSW/IVF 默认距离 | `||lhs - rhs||_2^2` |
| COSINE | 文本/语义向量，归一化后等价于 L2 | `1 - (lhs . rhs) / (||lhs|| * ||rhs||)` |
| INNER_PRODUCT | 推荐系统、最大内积搜索 (MIPS) | `lhs . rhs` |
| HAMMING | 二值向量、哈希签名 | 对应位不同的数量 |

### 2.2 距离计算流程

```mermaid
sequenceDiagram
    participant Caller as 调用者 (索引模块)
    participant Dist as distance_compute
    participant Impl as 具体实现

    Caller->>Dist: distance_compute(metric, lhs, rhs, dims)
    Dist->>Dist: 校验 metric 是否有效
    alt L2_SQUARED
        Dist->>Impl: 调用距离_l2sqr 实现
        Impl->>Impl: 逐维计算差值平方和
        Impl-->>Dist: 返回平方距离
    else COSINE
        Dist->>Impl: 计算点积 + 双向量范数
        Impl-->>Dist: 返回余弦距离
    else INNER_PRODUCT
        Dist->>Impl: 计算点积，取负值
        Impl-->>Dist: 返回内积距离
    else HAMMING
        Dist->>Impl: 逐位比较
        Impl-->>Dist: 返回汉明距离
    end
    Dist-->>Caller: 返回 float 距离值
```

**批处理接口**: `distance_compute_batch4_from_query` 一次计算查询向量与 4 个基向量的距离，利用 SIMD 和循环展开减少函数调用开销，在 HNSW 搜索的关键路径上提升性能。

---

## 3. 量化器模块

量化器通过降低向量精度来减少内存占用和加速距离计算，是 IVF、DiskANN 等索引支持大规模数据集的核心依赖。

### 3.1 量化器架构

```mermaid
classDiagram
    class quantizer_config_t {
        +int32_t dims
        +quantization_type_t type
        +int32_t pq_subquantizers
        +int32_t pq_bits
        +int32_t lvq_bits
        +int32_t sq_bits
        +int32_t rq_pq_bits
        +int32_t rq_residual_bits
    }

    class quantization_type_t {
        <<enumeration>>
        +QUANTIZATION_TYPE_NONE = 0
        +QUANTIZATION_TYPE_PQ = 1
        +QUANTIZATION_TYPE_LVQ = 2
        +QUANTIZATION_TYPE_SQ = 3
        +QUANTIZATION_TYPE_RQ = 4
    }

    class quantizer_t {
        <<opaque>>
        +quantizer_config_t config
        +bool trained
        +void *impl_data
    }

    class quantizer_API {
        +quantizer_create(config) quantizer_t
        +quantizer_train(q, n, vectors) int
        +quantizer_encode(q, vector, code) int
        +quantizer_encode_batch(q, n, vectors, codes) int
        +quantizer_compute_distance_table(q, metric, query, table) int
        +quantizer_adc_distance(q, code, table) float
        +quantizer_code_size(q) int
        +quantizer_export_model(q, codebooks, count, size) int
        +quantizer_create_from_model(config, size, codebooks, count) quantizer_t
        +quantizer_save(q, path) int
        +quantizer_load(path) quantizer_t
        +quantizer_drop(q) void
        +quantizer_enable_opq(q) int
    }

    class PQ_Impl {
        +pq_init(q) int
        +pq_train(q, n, vectors) int
        +pq_encode(q, vector, code) int
        +pq_compute_distance_table(q, metric, query, table) int
        +pq_adc_distance(q, code, table) float
    }

    class LVQ_Impl {
        +lvq_init(q) void
        +lvq_encode(q, vector, code) int
        +lvq_compute_distance_table(q, metric, query, table) int
        +lvq_adc_distance(q, code, table) float
    }

    class SQ_Impl {
        +sq_init(q) int
        +sq_train(q, n, vectors) int
        +sq_encode(q, vector, code) int
        +sq_compute_distance_table(q, metric, query, table) int
        +sq_adc_distance(q, code, table) float
    }

    class RQ_Impl {
        +rq_init(q) int
        +rq_train(q, n, vectors) int
        +rq_encode(q, vector, code) int
        +rq_compute_distance_table(q, metric, query, table) int
        +rq_adc_distance(q, code, table) float
    }

    quantizer_config_t --> quantization_type_t
    quantizer_t --> quantizer_config_t
    quantizer_t ..> quantizer_API : 公开接口
    quantizer_API --> PQ_Impl : 转发 NONE/PQ
    quantizer_API --> LVQ_Impl : 转发 LVQ
    quantizer_API --> SQ_Impl : 转发 SQ
    quantizer_API --> RQ_Impl : 转发 RQ
```

### 3.2 量化流程

```mermaid
sequenceDiagram
    participant Index as 索引构建
    participant Quant as quantizer_API
    participant Impl as 具体量化器
    participant Storage as 存储层

    %% 训练阶段
    rect rgb(240, 248, 255)
        Note over Index,Storage: 阶段 1: 训练 (仅 PQ/SQ/RQ 需要)
        Index->>Quant: quantizer_train(q, n, vectors)
        Quant->>Impl: pq_train / sq_train / rq_train
        alt PQ
            Impl->>Impl: K-Means 聚类各子空间
            Impl->>Impl: 生成码本 (M x K 个码心)
        else SQ
            Impl->>Impl: 统计全局 min/max
        else RQ
            Impl->>Impl: 两级训练 (PQ + 残差)
        else LVQ
            Note over Impl: 无需训练，跳过
        end
        Impl-->>Quant: 训练完成
        Quant-->>Index: 返回 0 (成功)
    end

    %% 编码阶段
    rect rgb(255, 250, 240)
        Note over Index,Storage: 阶段 2: 批量编码
        Index->>Quant: quantizer_encode_batch(q, n, vectors, codes)
        Quant->>Impl: pq_encode_batch / sq_encode_batch 等
        Impl->>Impl: 逐向量编码为码字
        Impl-->>Quant: 编码完成
        Quant-->>Index: 返回码字数组
        Index->>Storage: 存储码字到磁盘
    end

    %% 搜索阶段
    rect rgb(240, 255, 240)
        Note over Index,Storage: 阶段 3: 搜索 ADC 距离计算
        Index->>Quant: quantizer_compute_distance_table(q, metric, query, table)
        Quant->>Impl: 计算距离表
        alt PQ
            Impl->>Impl: 计算 query 到各子空间码心的距离
        else SQ/LVQ
            Impl->>Impl: 直接存储 query 向量
        end
        Impl-->>Quant: 返回距离表
        loop 遍历候选码字
            Index->>Quant: quantizer_adc_distance(q, code, table)
            Quant->>Impl: 查表 + 求和
            Impl-->>Quant: 返回近似距离
        end
    end
```

### 3.3 量化类型对比

| 量化类型 | 压缩率 | 召回率 | 训练成本 | 适用场景 | 码字大小 |
|----------|--------|--------|----------|----------|----------|
| **PQ** | 高 (8x~32x) | 中 (85%~95%) | 高 (K-Means) | 内存受限、大规模索引 | `ceil(M * bits / 8)` |
| **LVQ** | 中 (4x~8x) | 高 (95%~99%) | 无 | DiskANN、SSD 索引 | `8 + dims * bits / 8` |
| **SQ** | 高 (4x~8x) | 中 (80%~90%) | 低 (min/max) | 快速原型、简单量化 | `8 + dims * bits / 8` |
| **RQ** | 最高 (16x~64x) | 低 (70%~85%) | 高 (两级) | 极端压缩、粗排 | `M + ceil(M * residual_bits / 8)` |

**关键设计决策**:

1. **统一接口**: 所有量化器通过 `quantizer_t` 不透明指针和统一 API 暴露，索引模块无需感知具体实现
2. **ADC (Asymmetric Distance Computation)**: 查询时使用原始向量，数据库向量使用码字，减少量化误差
3. **距离表预计算**: PQ/SQ 在搜索前预计算查询向量到码心的距离表，O(1) 查表加速
4. **OPQ 优化**: 可选的 PCA 旋转，使各子空间方差均衡，提升 PQ 精度

---

## 4. K-Means 聚类模块

K-Means 聚类是 IVF 索引的核心依赖，用于将向量空间划分为 K 个聚类中心（倒排桶），实现粗量化。

### 4.1 K-Means 参数结构

```mermaid
classDiagram
    class KMeansParams {
        +int n_clusters 聚类中心数量
        +int n_init 随机初始化次数
        +int max_iter 最大迭代次数
        +double tol 收敛阈值
        +int verbose 日志级别
        +int random_state 随机种子
        +const char *init 初始化方法 ("random" / "k-means++")
        +const char *algorithm 算法 ("lloyd" / "elkan")
        +int n_samples 样本数量
        +int n_features 特征维度
        +const double *data 输入数据 [n_samples][n_features]
        +double *cluster_centers 输出聚类中心 [n_clusters][n_features]
        +int *labels 输出标签 [n_samples]
        +double inertia 惯性 (SSE)
        +int n_iter 实际迭代次数
        +int success 是否成功
    }

    class Kmeans_API {
        +void Kmeans(KMeansParams *params) 执行聚类
        +void KmeansFree(KMeansParams *params) 释放资源
    }

    Kmeans_API --> KMeansParams : 参数传递
```

### 4.2 K-Means 执行流程

```mermaid
flowchart TB
    Start([开始]) --> Init[初始化参数校验]
    Init --> Method{选择初始化方法}
    
    Method -->|k-means++| KMeansPP[k-means++ 初始化<br/>概率采样 + 距离加权]
    Method -->|random| Random[随机选择 K 个点作为初始中心]
    
    KMeansPP --> Lloyd[Lloyd 迭代]
    Random --> Lloyd
    
    subgraph Lloyd[Lloyd 迭代循环]
        Assign[分配阶段<br/>每个点归属最近中心]
        Update[更新阶段<br/>重新计算聚类中心]
        Converge{收敛判断<br/>中心移动 < tol?}
        Assign --> Update
        Update --> Converge
        Converge -->|否| Assign
    end
    
    Converge -->|是| Output[输出结果<br/>cluster_centers + labels + inertia]
    Output --> End([结束])
    
    style Lloyd fill:#f0f8ff,stroke:#4169e1
```

---

## 5. 基础数据结构模块

本模块提供通用的数据结构和算法，供上层模块（索引、量化器、应用层）使用。

### 5.1 容器结构概览

```mermaid
classDiagram
    class list_t {
        <<双向链表>>
        +list_node_t *head
        +list_node_t *tail
        +size_t size
        +size_t element_size
    }
    
    class list_node_t {
        +void *value
        +list_node_t *prev
        +list_node_t *next
    }
    
    class map_t {
        <<哈希表>>
        +size_t key_size
        +size_t value_size
        +map_hash_fn hash
        +map_equals_fn equals
    }
    
    class map_entry_t {
        +void *key
        +void *value
    }
    
    class stack_t {
        <<动态数组栈>>
        +void *data
        +size_t size
        +size_t capacity
        +size_t element_size
    }
    
    class queue_t {
        <<环形缓冲区队列>>
        +void *data
        +size_t head
        +size_t tail
        +size_t size
        +size_t capacity
    }
    
    class pq_t {
        <<优先队列 (二叉堆)>>
        +void *data
        +size_t size
        +size_t capacity
        +size_t element_size
        +int (*compare)(a, b)
    }
    
    class dict_t {
        <<分词词典>>
        +trie_node_t *trie
        +dict_hmm_model_t *hmm
        +set_t stop_words
        +map_t synonyms
    }
    
    class token_t {
        +char *text
        +int32_t byte_start
        +int32_t byte_length
        +bool is_ascii
    }
    
    list_t --> list_node_t : 包含
    map_t --> map_entry_t : 迭代
    dict_t --> token_t : 分词输出
```

### 5.2 排序算法体系

```mermaid
classDiagram
    class sort_algorithm_t {
        <<enumeration>>
        +SORT_ALGORITHM_BUBBLE = 0
        +SORT_ALGORITHM_SELECTION = 1
        +SORT_ALGORITHM_INSERTION = 2
        +SORT_ALGORITHM_SHELL = 3
        +SORT_ALGORITHM_MERGE = 4
        +SORT_ALGORITHM_QUICK = 5
        +SORT_ALGORITHM_HEAP = 6
        +SORT_ALGORITHM_COUNTING_INT32 = 7
        +SORT_ALGORITHM_BUCKET_INT32 = 8
        +SORT_ALGORITHM_RADIX_INT32 = 9
    }
    
    class sort_API {
        +int sort_dispatch(algorithm, base, count, element_size, compare)
        +int sort_bubble(...)
        +int sort_selection(...)
        +int sort_insertion(...)
        +int sort_shell(...)
        +int sort_merge(...)
        +int sort_quick(...)
        +int sort_heap(...)
        +int sort_counting_int32(array, count)
        +int sort_bucket_int32(array, count)
        +int sort_radix_int32(array, count)
    }
    
    sort_API --> sort_algorithm_t : 选择算法
```

**排序算法选择指南**:

| 算法 | 稳定性 | 时间复杂度 | 空间复杂度 | 适用场景 |
|------|--------|------------|------------|----------|
| Bubble | 稳定 | O(n²) | O(1) | 教学、极小规模 |
| Selection | 不稳定 | O(n²) | O(1) | 实现简单、交换成本高 |
| Insertion | 稳定 | O(n²)，最好 O(n) | O(1) | 小数组、近乎有序 |
| Shell | 不稳定 | O(n log n) ~ O(n²) | O(1) | 中等规模、原地排序 |
| Merge | 稳定 | O(n log n) | O(n) | 需要稳定性、可预测性能 |
| Quick | 不稳定 | 平均 O(n log n)，最坏 O(n²) | O(log n) | 通用排序、平均最快 |
| Heap | 不稳定 | O(n log n) | O(1) | 需要上界、原地排序 |
| Counting | 稳定 | O(n + k) | O(k) | 值域小的整数 |
| Bucket | 不稳定 | 平均 O(n + k) | O(n + k) | 均匀分布整数 |
| Radix | 稳定 | O(d(n + r)) | O(n + r) | 定长整数 |

### 5.3 二分查找接口

```mermaid
classDiagram
    class binary_search_API {
        +bool binary_search(base, count, element_size, key, compare, index)
        +size_t binary_search_lower_bound(base, count, element_size, key, compare)
        +size_t binary_search_upper_bound(base, count, element_size, key, compare)
    }
```

**接口说明**:
- `binary_search`: 标准二分查找，返回是否存在及位置
- `lower_bound`: 返回第一个 ≥ key 的位置
- `upper_bound`: 返回第一个 > key 的位置

---

## 6. 模块间依赖关系

algo-prod 算法库作为基础设施，为上层索引系统提供核心能力。

```mermaid
flowchart TB
    subgraph "应用层"
        APP[应用程序]
    end
    
    subgraph "索引层"
        HNSW_IDX[HNSW 索引]
        IVF_IDX[IVF 索引]
        DISKANN_IDX[DiskANN 索引]
        BM25_IDX[BM25 文本索引]
    end
    
    subgraph "algo-prod 算法库"
        DIST[距离计算]
        QUANT[量化器]
        KMEANS[K-Means]
        DS[数据结构]
        SORT[排序]
        DICT[分词词典]
    end
    
    %% 应用层依赖
    APP --> HNSW_IDX
    APP --> IVF_IDX
    APP --> DISKANN_IDX
    APP --> BM25_IDX
    
    %% 索引层依赖算法库
    HNSW_IDX --> DIST
    HNSW_IDX --> QUANT
    IVF_IDX --> DIST
    IVF_IDX --> QUANT
    IVF_IDX --> KMEANS
    DISKANN_IDX --> DIST
    DISKANN_IDX --> QUANT
    BM25_IDX --> DICT
    
    %% 算法库内部依赖
    QUANT --> DIST
    KMEANS --> DIST
    KMEANS --> DS
    QUANT --> KMEANS
    
    %% 数据结构通用依赖
    HNSW_IDX --> DS
    IVF_IDX --> DS
    DISKANN_IDX --> DS
    
    style DIST fill:#e6f3ff,stroke:#4169e1
    style QUANT fill:#fff0e6,stroke:#ff8c00
    style KMEANS fill:#e6ffe6,stroke:#228b22
    style DS fill:#ffe6f0,stroke:#c71585
```

**依赖说明**:

1. **距离计算**: 所有向量索引的核心依赖，无外部依赖
2. **量化器**: 依赖距离计算，内部可选依赖 K-Means（PQ/RQ）
3. **K-Means**: 依赖距离计算，用于 IVF 聚类和 PQ 码本训练
4. **数据结构**: 无外部依赖，被索引层广泛使用
5. **分词词典**: BM25 文本索引专用，支持中文分词

---

## 7. 关键代码位置

| 模块 | 头文件 | 源文件 | 说明 |
|------|--------|--------|------|
| **距离计算** | `engineering/include/algo-prod/distance/distance.h` | `engineering/src/algo-prod/distance/distance.c` | L2/Cosine/IP/Hamming 距离 |
| **量化器统一接口** | `engineering/include/algo-prod/quantization/quantization.h` | `engineering/src/algo-prod/quantization/quantization.c` | 量化器 API |
| **PQ 量化** | `engineering/include/algo-prod/quantization/pq.h` | `engineering/src/algo-prod/quantization/pq.c` | 乘积量化 |
| **LVQ 量化** | `engineering/include/algo-prod/quantization/lvq.h` | `engineering/src/algo-prod/quantization/lvq.c` | 局部自适应量化 |
| **SQ 量化** | `engineering/include/algo-prod/quantization/sq.h` | `engineering/src/algo-prod/quantization/sq.c` | 标量量化 |
| **RQ 量化** | `engineering/include/algo-prod/quantization/rq.h` | `engineering/src/algo-prod/quantization/rq.c` | 残差量化 (RabitQ) |
| **K-Means** | `engineering/include/algo-prod/Kmeans/kmeans.h` | `engineering/src/algo-prod/Kmeans/kmeans.c` | K-Means 聚类 |
| **分词词典** | `engineering/include/algo-prod/dict/dict.h` | `engineering/src/algo-prod/dict/dict_*.c` | 中文分词 + HMM |
| **双向链表** | `engineering/include/algo-prod/list/list.h` | `engineering/src/algo-prod/list/list.c` | 双向链表 |
| **哈希表** | `engineering/include/algo-prod/map/map.h` | `engineering/src/algo-prod/map/map.c` | 哈希表 |
| **栈** | `engineering/include/algo-prod/stack/stack.h` | `engineering/src/algo-prod/stack/stack.c` | 动态数组栈 |
| **队列** | `engineering/include/algo-prod/queue/queue.h` | `engineering/src/algo-prod/queue/queue.c` | 环形缓冲区队列 |
| **优先队列** | `engineering/include/algo-prod/priority_queue/priority_queue.h` | `engineering/src/algo-prod/priority_queue/priority_queue.c` | 二叉堆优先队列 |
| **排序** | `engineering/include/algo-prod/sort/sort.h` | `engineering/src/algo-prod/sort/sort.c` | 10 种排序算法 |
| **二分查找** | `engineering/include/algo-prod/binary_search/binary_search.h` | `engineering/src/algo-prod/binary_search/binary_search.c` | 二分查找 |

---

## 相关文档

- [AGENTS.md](../../../AGENTS.md) - 项目构建指南
- [CLAUDE.md](../../../CLAUDE.md) - 项目说明文档
- [索引系统架构](../index/README.md) - 向量索引架构设计
- [存储架构](../../storage-architecture.md) - 存储引擎详细说明