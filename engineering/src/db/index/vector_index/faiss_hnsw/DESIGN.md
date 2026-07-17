# faiss_hnsw 设计文档

## 概述

faiss_hnsw 是内存版 HNSW 索引实现，参考 FAISS `HNSW.cpp` 重写，使用 C11 语言实现，无外部依赖。

## 整体架构

```
┌─────────────────────────────────────────────────────────────┐
│                       faiss_hnsw_t                          │
├─────────────────────────────────────────────────────────────┤
│  向量存储：float *vectors（连续内存数组）                    │
│  图结构：levels / offsets / neighbors（flattened 邻居数组）  │
│  量化编码：codes + code_size（可选 SQ/PQ/LVQ/RaBitQ）       │
│  搜索辅助：MinimaxHeap + VisitedTable                         │
│  随机数：MT19937（seed=12345，与 FAISS 一致）               │
└─────────────────────────────────────────────────────────────┘
```

## 核心数据结构

```mermaid
erDiagram
    faiss_hnsw_t {
        int32_t dims "向量维度"
        int32_t M "每层最大邻居数"
        int32_t ef_construction "建图搜索宽度"
        int32_t ef_search "搜索宽度"
        int32_t n_total "已插入向量数"
        int32_t max_level "最大层号"
        int32_t entry_point "入口节点"
        float-star vectors "原始向量数组"
        uint8_t-star codes "量化编码数组"
        int32_t code_size "编码字节数"
        int32_t-star levels "每个节点层高"
        int32_t-star offsets "邻居数组偏移"
        int32_t-star neighbors "扁平化邻接表"
        float-star assign_probas "层分配概率表"
        int32_t-star cum_nneighbor "每层累计邻居数"
        quantizer_t-star quantizer "量化器实例"
        faiss_hnsw_mt_state_t rng_state "MT19937 随机状态"
        uint8_t-star delete_bitmap "删除位图"
    }
```

## 搜索流程

```mermaid
flowchart TD
    A[开始] --> B[从 entry_point 开始]
    B --> C{当前层 > 0?}
    C -->|是| D[贪心下降: 遍历当前节点邻居]
    D --> E[找到该层最近邻居]
    E --> F[下降到下一层]
    F --> C
    C -->|否| G[底层 level 0: Best-First 搜索]
    G --> H[初始化 MinimaxHeap + VisitedTable]
    H --> I[从堆中 pop 最近候选]
    I --> J[遍历邻居，计算距离]
    J --> K[更新 top-k 结果堆]
    K --> L{候选堆空或提前终止?}
    L -->|否| I
    L -->|是| M[选择排序取 top-k]
    M --> N[返回结果]
```

## 插入流程

```mermaid
flowchart TD
    A[开始] --> B[random_level 分配层高]
    B --> C[扩展 vectors/levels/offsets/neighbors 数组]
    C --> D[从高层到低层逐层插入]
    D --> E{当前层 >= 0?}
    E -->|是| F[在该层搜索 ef_construction 个候选]
    F --> G[shrink_neighbor_list 启发式选边]
    G --> H[双向连接: 新节点 ↔ 选中邻居]
    H --> I[下降一层]
    I --> E
    E -->|否| J{新层高 > max_level?}
    J -->|是| K[更新 entry_point 为新节点]
    J -->|否| L[完成]
    K --> L
```

## 扁平化邻接表布局

HNSW 的关键设计：所有层级的邻接信息存储在**单一扁平数组 `neighbors`** 中。

```
offsets[node]  ────────────┬──────────────────────┬──────────────────────
                          │  Level 0 neighbors    │  Level 1 neighbors
                          │  (最多 2M 个)          │  (最多 M 个)
                          │                       │
neighbors:  [n0,n1,...│n2,n3,..│-1,-1,...│n4,...│-1,...│ ... ]
                │         │         │
                │         │         └── 上层邻居
                │         └── M个槽(底层2M)
                └── cum_nneighbor[layer_no] 定位
```

读取节点 `no` 在 `layer_no` 层的邻居：

```c
begin = offsets[no] + cum_nneighbor[layer_no];
end   = offsets[no] + cum_nneighbor[layer_no + 1];
// neighbors[begin .. end) 即为该节点在该层的邻居列表
```

## 量化集成

| 量化类型 | 说明 | code_size |
|---------|------|-----------|
| QUANTIZATION_TYPE_NONE | 原始浮点向量 | 0 |
| QUANTIZATION_TYPE_SQ | 标量量化（8-bit） | dims |
| QUANTIZATION_TYPE_PQ | 乘积量化 | pq_m × ceil(pq_bits/8) |
| QUANTIZATION_TYPE_LVQ | 学习向量量化 | 由量化器配置 |
| QUANTIZATION_TYPE_RQ | RaBitQ 量化 | 由量化器配置 |

量化路径：
- **插入时**：调用 `quantizer_encode()` 将浮点向量编码为 `codes`
- **搜索时**：预计算 ADC 距离表（`quantizer_compute_distance_table`），后续只需 O(1) 查表

## 与 FAISS 的差异

| 特性 | FAISS HNSW | faiss_hnsw |
|------|-----------|------------|
| 语言 | C++17 | C11 |
| 随机数 | std::mt19937 | MT19937 手写实现 |
| 堆 | std::priority_queue | MinimaxHeap 手写实现 |
| 访问表 | 位图 | VisitedTable 手写实现 |
| 距离计算 | FAISS 距离函数 | 内联 L2/Cosine 实现 |
| 接口风格 | 类方法 | 函数指针 + 结构体 |
| 内存管理 | RAII | 手动 malloc/free |

## 文件结构

| 文件 | 职责 |
|------|------|
| `faiss_hnsw.h` | 公共 API 头文件（不透明指针） |
| `faiss_hnsw_internal.h` | 内部结构体定义 |
| `faiss_hnsw_create.c` | 创建、销毁、参数管理 |
| `faiss_hnsw_insert.c` | 向量存储、层高分配、图构建 |
| `faiss_hnsw_search.c` | 搜索（贪心下降 + Best-First） |
| `faiss_hnsw_search_layer.c` | 单层搜索算法 |
| `faiss_hnsw_level.c` | 层高分配概率计算 |
| `faiss_hnsw_minimax_heap.c` | MinimaxHeap 实现 |
| `faiss_hnsw_visited_table.c` | VisitedTable 实现 |
| `faiss_hnsw_quantize.c` | 量化相关辅助函数 |
| `faiss_hnsw_stubs.c` | 占位实现（未完成功能） |

## API 接口

```c
// 创建索引
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims,
    int32_t ef_construction, distance_metric_t metric,
    quantization_type_t quant_type);

// 批量插入向量
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n,
    const float *vectors);

// 搜索最近邻
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query,
    int32_t k, int32_t ef_search, float *distances, int32_t *ids);

// 销毁索引
void faiss_hnsw_index_drop(faiss_hnsw_t *index);
```

## 已知限制

- 不支持删除操作（需重建索引）
- 不支持持久化（全内存，进程重启后需重建）
- 无批量搜索优化（单 query 搜索，高并发需上层并行）