# HNSW 算法原理：面试必备知识

> 本文档深入解析 HNSW（Hierarchical Navigable Small World）向量索引算法，
> 分析核心参数影响、对比其他算法，是面试准备的必备资料。

## 目录

1. [HNSW 基础原理](#1-hnsw-基础原理)
2. [HNSW 核心算法](#2-hnsw-核心算法)
3. [HNSW 参数调优](#3-hnsw-参数调优)
4. [HNSW vs 其他算法](#4-hnsw-vs-其他算法)
5. [ANN 评测标准](#5-ann-评测标准)

---

## 1. HNSW 基础原理

### 1.1 什么是 HNSW

**HNSW (Hierarchical Navigable Small World)** 是一种基于图的高效近似最近邻搜索算法，通过构建多层跳跃搜索图实现快速检索。

```
┌─────────────────────────────────────────────────────────────┐
│                     HNSW 核心思想                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  多层结构：                                                   │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ Level 3:    ○────────○                                 │ │
│  │            /         \                                 │ │
│  │ Level 2:  ○───○───○───○───○                           │ │
│  │          /           \                                 │ │
│  │ Level 1: ○─○─○─○─○─○─○─○─○─○─○─○─○─○─○─○─○─○─○        │ │
│  │ Level 0: (完整图，所有数据点)                           │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  搜索过程：从高层向低层贪心下降                               │
│  插入过程：随机决定插入层数，高层连接稀疏，快速定位            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 核心特点

| 特性 | 说明 |
|------|------|
| **分层图结构** | 多层图，上层稀疏下层稠密 |
| **Skip List 类似** | 高层快速定位，低层精确搜索 |
| **无索引构建参数** | M 是唯一关键参数 |
| **增量构建** | 支持增量添加向量 |
| **内存占用大** | 需要存储所有邻居关系 |

### 1.3 与其他算法对比

| 算法 | 构建速度 | 查询速度 | 内存占用 | 召回率 |
|------|----------|----------|----------|--------|
| **HNSW** | 中等 | 最快 | 大 | 高 |
| **IVF** | 快 | 中等 | 中等 | 中等 |
| **PQ** | 快 | 快 | 小 | 低 |
| **LSH** | 快 | 慢 | 大 | 低 |
| **DiskANN** | 慢 | 快 | 中等 | 高 |

---

## 2. HNSW 核心算法

### 2.1 多层图构建

```c
/**
 * HNSW 插入算法
 *
 * 1. 随机决定向量插入的层数 L
 *    - 层数越高概率越低（指数衰减）
 *    - 公式: P(level > L) = exp(-level / lambda)
 *
 * 2. 从最高层开始，逐层贪心搜索插入位置
 * 3. 在每层建立双向连接（连接 M 个最近邻）
 *
 * @param hnsw HNSW 索引
 * @param vec 待插入向量
 * @param vec_id 向量ID
 */
int hnsw_insert(hnsw_t *hnsw, const float *vec, int vec_id) {
    int max_level = hnsw->max_level;

    // 1. 随机选择插入层数（几何分布）
    int level = geometric_distribution(hnsw->level_lambda);

    // 2. 从最高层开始搜索
    int entry_point = hnsw->entry_point;
    int current_level = max_level;

    // 3. 逐层贪心下降找到插入点
    while (current_level > level) {
        entry_point = greedy_search(hnsw, vec, entry_point, current_level);
        current_level--;
    }

    // 4. 从选中的层开始，逐层插入
    for (int l = level; l >= 0; l--) {
        // 贪心搜索找到该层的最近邻
        int nearest = greedy_search(hnsw, vec, entry_point, l);

        // 获取该层的 M 个最近邻
        vector_t *neighbors = search_k_nearest(hnsw, vec, nearest, l, hnsw->M);

        // 建立双向连接
        for (int i = 0; i < neighbors.size(); i++) {
            hnsw_connect(hnsw, vec_id, neighbors[i], l);
        }

        entry_point = nearest;
    }

    hnsw->n_total++;
    return 0;
}
```

### 2.2 层数选择（几何分布）

```c
/**
 * 几何分布采样
 *
 * HNSW 使用几何分布决定向量插入层数：
 * P(level = L) = (1 - p) * p^L
 *
 * 其中 p 通常设为 exp(-1/ML)，ML 是 max_level
 *
 * 这种设计确保：
 * - 大部分向量插入到底层（level=0）
 * - 少量向量插入到高层
 * - 高层图稀疏，便于快速导航
 */
int sample_level(float p) {
    int level = 0;
    while (uniform_random() > p && level < MAX_LEVEL) {
        level++;
    }
    return level;
}
```

### 2.3 贪心搜索算法

```c
/**
 * 贪心搜索（Greedy Search）
 *
 * 在某一层图上，从入口点开始贪心地向目标向量靠近。
 *
 * 算法：
 * 1. 从入口点开始
 * 2. 找到入口点的邻居中比当前更近的点
 * 3. 如果找到更近的点，移动到该点
 * 4. 重复直到没有更近的邻居
 *
 * @param hnsw HNSW 索引
 * @param query 查询向量
 * @param entry 入口点
 * @param level 搜索层
 * @return 该层最近的点
 */
int greedy_search(hnsw_t *hnsw,
                  const float *query,
                  int entry,
                  int level) {
    int current = entry;
    float d_current = distance(hnsw, query, current);

    bool changed = true;
    while (changed) {
        changed = false;

        // 遍历当前点的所有邻居
        for (int neighbor : hnsw->neighbors[current][level]) {
            float d = distance(hnsw, query, neighbor);

            if (d < d_current) {
                current = neighbor;
                d_current = d;
                changed = true;
            }
        }
    }

    return current;
}
```

### 2.4 搜索剪枝优化

```c
/**
 * 搜索剪枝：ef_construction 参数
 *
 * ef_construction 控制搜索过程中维护的候选集大小。
 * 更大的 ef_construction 意味着更精确的搜索，但构建更慢。
 *
 * @param hnsw HNSW 索引
 * @param query 查询向量
 * @param entry 入口点
 * @param k 返回 k 个最近邻
 * @param ef_search ef_search 参数
 */
void *hnsw_search(hnsw_t *hnsw,
                  const float *query,
                  int k,
                  int ef_search) {
    // 1. 初始化结果堆（最大堆，维护 top-k）
    minimax_heap_t *results = create_minimax_heap(k);

    // 2. 初始化候选队列
    minimax_heap_t *candidates = create_minimax_heap(ef_search);

    // 3. 初始化访问标记
    visited_set_t *visited = create_visited_set(hnsw->n_total);

    // 4. 从最高层开始贪心下降
    int level = hnsw->max_level;
    int ep = hnsw->entry_point;

    while (level > 0) {
        ep = greedy_search(hnsw, query, ep, level);
        level--;
    }

    // 5. 从底层开始搜索
    minimax_heap_push(candidates, ep, 0);

    while (!minimax_heap_empty(candidates)) {
        // 取出最远的候选点
        int c = minimax_heap_pop_max(candidates);
        float d_c = minimax_heap_get_max_distance(candidates);

        // 检查是否可以剪枝
        // 如果最远候选点的距离已经大于结果堆中最大距离，可以停止
        if (d_c > minimax_heap_get_max_distance(results)) {
            break;
        }

        // 遍历邻居
        for (int neighbor : hnsw->neighbors[c][0]) {
            if (visited_check(visited, neighbor)) {
                continue;
            }
            visited_mark(visited, neighbor);

            float d = distance(hnsw, query, neighbor);

            // 如果比结果堆中最大距离小，加入结果
            if (d < minimax_heap_get_max_distance(results) ||
                results.size() < k) {
                minimax_heap_push(results, neighbor, d);
                minimax_heap_push(candidates, neighbor, d);
            }
        }
    }

    return extract_results(results);
}
```

### 2.5 搜索流程图

```
┌─────────────────────────────────────────────────────────────┐
│                     HNSW 搜索流程                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  输入: query, k, ef_search                                   │
│                                                              │
│  ┌─────────┐                                                │
│  │ 初始化   │                                                │
│  │ results │ ← 最大堆，维护 top-k 结果                       │
│  │ candidates│← 最大堆，维护候选点                           │
│  └────┬────┘                                                │
│       │                                                      │
│       ▼                                                      │
│  ┌─────────────┐                                            │
│  │ 从最高层开始 │                                            │
│  │ 贪心下降到   │                                            │
│  │ 第1层       │                                            │
│  └──────┬──────┘                                            │
│         │                                                   │
│         ▼                                                   │
│  ┌─────────────────────────────┐                            │
│  │ Layer 0 搜索 (ef_search)     │                            │
│  │                             │                            │
│  │ 1. 取出最远候选点 c          │                            │
│  │ 2. 如果 dist(c) > min_dist  │                            │
│  │    停止搜索                  │                            │
│  │ 3. 遍历 c 的邻居             │                            │
│  │ 4. 更新 results 和 candidates│                            │
│  │ 5. 重复直到候选集为空        │                            │
│  └──────────────┬────────────────┘                            │
│                 │                                            │
│                 ▼                                            │
│  ┌─────────────────────────────┐                            │
│  │ 返回 results 中的 top-k     │                            │
│  └─────────────────────────────┘                            │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. HNSW 参数调优

### 3.1 核心参数

| 参数 | 默认值 | 说明 | 影响 |
|------|--------|------|------|
| **M** | 16 | 每层连接数 | 平衡精度与内存 |
| **efConstruction** | 200 | 构建时候选集大小 | 影响构建质量 |
| **efSearch** | - | 搜索时候选集大小 | 影响搜索精度 |
| **maxLevel** | - | 最大层数 | 通常自动计算 |
| **ml** | 1/log(1/p) | 层衰减参数 | 影响层分布 |

### 3.2 M 参数影响分析

```python
"""
M 参数对 HNSW 性能的影响

M 控制每层中每个节点的连接数：
- M 增大：召回率↑，内存↑，构建时间↑
- M 减小：召回率↓，内存↓，构建时间↓
"""

import matplotlib.pyplot as plt
import numpy as np

# 模拟 M 对召回率的影响
M_values = [4, 8, 16, 24, 32, 48, 64]
recall_at_M = [0.82, 0.91, 0.96, 0.98, 0.99, 0.995, 0.998]

# 模拟 M 对内存的影响（归一化）
memory_at_M = [m / 4 for m in M_values]  # 线性关系

# 模拟 M 对构建时间的影响
build_time_at_M = [m ** 1.5 for m in M_values]
build_time_at_M = [t / build_time_at_M[0] for t in build_time_at_M]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

# 召回率 vs M
ax1.plot(M_values, recall_at_M, 'b-o')
ax1.set_xlabel('M (connections per node)')
ax1.set_ylabel('Recall@10')
ax1.set_title('Recall vs M')
ax1.grid(True)

# 内存 vs M
ax2.plot(M_values, memory_at_M, 'r-s')
ax2.set_xlabel('M (connections per node)')
ax2.set_ylabel('Memory (normalized)')
ax2.set_title('Memory vs M')
ax2.grid(True)

plt.tight_layout()
plt.show()

print("""
结论:
- M=16: 平衡点，召回率>95%，内存可控
- M=32: 高召回场景，内存增加2倍
- M=64: 极高召回场景，内存增加4倍
""")
```

### 3.3 ef_construction 参数影响

```
┌─────────────────────────────────────────────────────────────┐
│              ef_construction 对构建质量的影响                │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ef_construction 越小：                                      │
│    - 搜索空间受限                                            │
│    - 可能错过更近的邻居                                      │
│    - 构建速度快                                              │
│    - 召回率可能降低                                          │
│                                                              │
│  ef_construction 越大：                                      │
│    - 搜索更彻底                                              │
│    - 找到更优的邻居                                          │
│    - 构建速度慢                                              │
│    - 召回率提高                                              │
│                                                              │
│  推荐值：                                                    │
│    - 小数据集（<1M）: 100-200                                │
│    - 中数据集（1M-10M）: 200-400                             │
│    - 大数据集（>10M）: 400+                                  │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 3.4 ef_search 参数影响

```c
/**
 * ef_search 对搜索精度的影响
 *
 * ef_search 控制搜索时维护的候选集大小。
 * - ef_search >= k: 确保能找到 top-k
 * - ef_search >> k: 提高召回率，但增加搜索时间
 *
 * 推荐：
 * - 精确搜索：ef_search = k
 * - 高召回搜索：ef_search = k * 10 或更高
 */
typedef struct {
    int k;           // 请求的最近邻数
    int ef_search;   // 搜索候选集大小
    float recall;    // 实际召回率
    float qps;       // 查询吞吐量
} search_result_t;

search_result_t benchmark_ef_search(hnsw_t *hnsw, int k, int ef) {
    // 测量召回率和 QPS
    // ...
}
```

### 3.5 召回率 vs 性能 trade-off

```
┌─────────────────────────────────────────────────────────────┐
│              召回率 vs 性能 Trade-off                        │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   召回率                                                      │
│   100% │●                                                    │
│       │  ●                                                   │
│   99% │    ●                                                 │
│       │       ●                                              │
│   95% │          ●●●●●                                       │
│       │                ●●●●●●●●●●●●                          │
│   90% │                           ●●●●●●●●●●●●●●●             │
│       │                                              QPS     │
│       └──┬──────┬──────┬──────┬──────┬──────┬──────→         │
│          10     50    100    200    500   1000              │
│                                                              │
│   推荐配置：                                                  │
│   ┌────────────────────────────────────────────────────────┐ │
│   │ 场景          │ M  │ efS │ 召回率 │ QPS   │ 内存       │ │
│   ├──────────────┼────┼─────┼────────┼───────┼────────────┤ │
│   │ 实时推荐     │ 16 │ 100 │ ~95%   │ 10k+  │ 适中       │ │
│   │ 语义搜索     │ 32 │ 200 │ ~99%   │ 5k    │ 较大       │ │
│   │ 精确搜索     │ 64 │ 500 │ ~99.9% │ 1k    │ 很大       │ │
│   └────────────────────────────────────────────────────────┘ │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. HNSW vs 其他算法

### 4.1 HNSW vs IVF-PQ

```
┌─────────────────────────────────────────────────────────────┐
│                   HNSW vs IVF-PQ                            │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   维度           │ HNSW              │ IVF-PQ                │
│   ───────────────┼───────────────────┼─────────────────────  │
│   架构           │ 分层图            │ 倒排索引 + 量化        │
│   精度           │ 高                │ 中等（量化损失）        │
│   速度           │ 极快              │ 快                     │
│   内存           │ 大                │ 小（量化压缩）          │
│   构建速度       │ 中等              │ 快                     │
│   支持删除       │ 困难              │ 支持                   │
│   增量更新       │ 支持              │ 受限                   │
│                                                              │
│   适用场景：                                                  │
│   - HNSW: 小规模高精度（<100M向量）                          │
│   - IVF-PQ: 大规模低内存（>100M向量）                        │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 HNSW vs DiskANN

```
┌─────────────────────────────────────────────────────────────┐
│                   HNSW vs DiskANN                           │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   维度           │ HNSW              │ DiskANN               │
│   ───────────────┼───────────────────┼─────────────────────  │
│   存储位置       │ 内存              │ 磁盘（内存+SSD）        │
│   精度           │ 高                │ 高                     │
│   延迟           │ <10ms             │ <50ms                  │
│   QPS            │ 高                │ 中等                   │
│   数据规模       │ <100M             │ >1B                    │
│   内存占用       │ 大                │ 小（~1字节/向量）       │
│   SSD 读取       │ 无                │ 需要                   │
│                                                              │
│   DiskANN 优势：                                             │
│   1. 支持超大规模向量（TB级）                                │
│   2. 内存占用极小                                            │
│   3. 可扩展到单机多盘                                        │
│                                                              │
│   HNSW 优势：                                                │
│   1. 延迟更低（纯内存）                                      │
│   2. QPS 更高                                               │
│   3. 实现简单                                                │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### 4.3 向量索引选型指南

```python
"""
向量索引选型决策树

Q1: 数据规模？
├── < 1M 向量 → HNSW
├── 1M - 100M → HNSW 或 IVF-HNSW
└── > 100M → DiskANN 或 IVF-PQ

Q2: 内存限制？
├── 充足内存 → HNSW
├── 受限内存 → IVF-PQ
└── 极小内存 → DiskANN

Q3: 精度要求？
├── 高精度 → HNSW
└── 可接受精度损失 → IVF-PQ

Q4: 是否需要更新？
├── 静态数据 → 任意算法
└── 动态更新 → HNSW（部分支持）
"""

def select_index(scenario):
    data_size, memory_gb, recall_target = scenario

    if data_size < 1_000_000:
        if recall_target > 0.95:
            return "HNSW (M=16, efS=200)"
        else:
            return "HNSW (M=12, efS=100)"

    elif data_size < 100_000_000:
        if memory_gb > 64:
            return "HNSW (M=32, efS=400)"
        else:
            return "IVF-HNSW (nlist=4096)"

    else:
        if recall_target > 0.95:
            return "DiskANN"
        else:
            return "IVF-PQ (m=128)"
```

### 4.4 混合索引策略

```c
/**
 * IVF-HNSW 混合索引
 *
 * 结合 IVF 的聚类能力和 HNSW 的精确搜索：
 * 1. 先用 IVF 找到候选聚类
 * 2. 在候选聚类内用 HNSW 精确搜索
 */
typedef struct {
    int nlist;           // 聚类数量
    float *centroids;    // 聚类中心
    hnsw_t *hnsw_per_cluster;  // 每个聚类内的 HNSW
} ivf_hnsw_index_t;

int ivf_hnsw_search(ivf_hnsw_index_t *index,
                    const float *query,
                    int k,
                    int nprobe) {
    // 1. 找到最近的 nprobe 个聚类
    int *clusters = find_nearest_clusters(index, query, nprobe);

    // 2. 在每个聚类内搜索
    min_heap_t results = create_min_heap(k);

    for (int i = 0; i < nprobe; i++) {
        hnsw_search(index->hnsw_per_cluster[clusters[i]],
                    query, k, results);
    }

    return extract_top_k(results, k);
}
```

---

## 5. ANN 评测标准

### 5.1 常用评测指标

| 指标 | 公式 | 说明 |
|------|------|------|
| **Recall@K** | \|S∩G\|/K | S=返回集合，G=真实集合 |
| **QPS** | 查询数/秒 | 吞吐量 |
| **Latency** | P50/P99延迟 | 响应时间 |
| **内存** | GB | 索引内存占用 |
| **构建时间** | 秒/百万向量 | 构建效率 |

### 5.2 Recall@K 计算

```python
"""
Recall@K 计算

Recall@K = |Retrieved_K ∩ GroundTruth_K| / K

示例：
- 查询 q 的真实最近邻: [A, B, C, D, E]
- 索引返回: [A, B, F, G, H]
- Recall@5 = |{A, B} ∩ {A, B, C, D, E}| / 5 = 2/5 = 0.4
"""

def recall_at_k(retrieved, ground_truth, k):
    retrieved_k = set(retrieved[:k])
    gt_k = set(ground_truth[:k])
    return len(retrieved_k & gt_k) / k

def mean_recall_at_k(results, k):
    """计算平均召回率"""
    return sum(r['recall'] for r in results) / len(results)
```

### 5.3 评测数据集

| 数据集 | 规模 | 维度 | 说明 |
|--------|------|------|------|
| **SIFT1M** | 1M | 128 | 图像特征 |
| **GIST1M** | 1M | 960 | 图像特征 |
| **DEEP1B** | 1B | 96 | 深度学习特征 |
| **SPACEV1B** | 1B | 100 | 文本向量 |
| **Cohere** | 50M | 768 | 文本向量 |

### 5.4 性能基准

```
┌─────────────────────────────────────────────────────────────┐
│                  ANN Benchmark 结果示例                      │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│   数据集: SIFT1M (1M, 128维)                                │
│   硬件: 32核, 256GB RAM                                     │
│                                                              │
│   算法         │ Recall@10 │ QPS    │ 内存   │ 构建时间      │
│   ─────────────┼───────────┼────────┼────────┼─────────────  │
│   HNSW(M=16)   │  96.5%    │ 5000   │ 1.2GB  │ 30s           │
│   HNSW(M=32)   │  99.1%    │ 3000   │ 2.1GB  │ 45s           │
│   IVF-PQ       │  85.0%    │ 8000   │ 0.4GB  │ 15s           │
│   IVF-HNSW     │  94.0%    │ 6000   │ 0.8GB  │ 25s           │
│   LSH          │  70.0%    │ 2000   │ 1.5GB  │ 10s           │
│                                                              │
│   数据集: DEEP1B (1B, 96维)                                 │
│   硬件: 64核, 1TB RAM, NVMe SSD                             │
│                                                              │
│   算法         │ Recall@10 │ QPS    │ 内存   │ 构建时间      │
│   ─────────────┼───────────┼────────┼────────┼─────────────  │
│   DiskANN      │  95.0%    │ 1000   │ 100GB  │ 2h            │
│   IVF-PQ       │  80.0%    │ 3000   │ 50GB   │ 1h            │
│   ScaNN        │  92.0%    │ 2000   │ 60GB   │ 1.5h          │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 附录：面试高频问题

### Q1: HNSW 的时间复杂度是多少？

**答**：构建 O(N log N)，搜索 O(log N)。其中 N 是向量数量，M 是常数因子。

### Q2: HNSW 为什么比其他图算法更快？

**答**：关键在于多层结构和贪心搜索。高层图稀疏，搜索快速定位到相关区域；低层图稠密，保证搜索精度。

### Q3: M 参数对性能的影响是什么？

**答**：M 增大 → 召回率↑、内存↑、构建时间↑。M 是最重要的调参参数，需要在精度和资源之间权衡。

### Q4: ef_search 和 ef_construction 的区别？

**答**：ef_construction 用于构建阶段，控制邻居选择的候选集大小；ef_search 用于查询阶段，控制搜索的候选集大小。

### Q5: HNSW 有什么局限性？

**答**：1) 内存占用大（需要存储邻居关系）；2) 不支持高效删除（删除后需要重建）；3) 增量更新效率低。

### Q6: 什么时候选择 IVF-PQ 而不是 HNSW？

**答**：当数据规模很大（>10M）、内存受限、可以接受一定精度损失时，IVF-PQ 更合适。

---

*文档版本: v1.0*
*最后更新: 2026-07-12*
