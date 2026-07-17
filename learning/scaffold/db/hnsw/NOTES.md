# HNSW 索引对照笔记

> 对照 `engineering/src/db/index/vector_index/hnsw/` 分析 HNSW 实现细节

## 1. 工程实现架构

### 文件结构

```
engineering/src/db/index/vector_index/hnsw/
├── faiss_hnsw.h          # 主头文件，定义 faiss_hnsw_t 结构体
├── faiss_hnsw_utils.h    # 工具函数：邻接表定位、距离计算
├── faiss_hnsw_create.c   # 创建/销毁索引
├── faiss_hnsw_insert.c   # 向量存储、层高分配、图构建
├── faiss_hnsw_search.c   # 搜索（贪心下降 + Best-First）
├── faiss_hnsw_delete.c   # 删除索引
├── faiss_hnsw_utils.c    # 辅助工具实现
└── DESIGN.md             # 设计文档
```

### 核心数据结构

`faiss_hnsw_t` 是索引主结构体，包含以下关键字段：

- `vectors`：原始向量数组 `float[n_total × dims]`
- `codes`：PQ 编码向量 `uint8[n_total × code_size]`
- `levels`：每个节点的层高 `int32[n_total]`
- `offsets`：每个节点在 `nbs` 中的起始偏移
- `nbs`：扁平化邻接表 `int32[nb_size]`
- `cum_nneighbor_per_level`：每层邻居容量的前缀和
- `entry_pointer`：搜索入口节点 ID
- `max_level`：图中最大层高

## 2. 扁平化邻接表设计

### 布局原理

所有层级的邻接信息存储在单一扁平数组 `nbs` 中，通过 `offsets` 和 `cum_nneighbor_per_level` 定位：

```
offsets[node] ────────────┬──────────────────────┬──────────────────────
                          │  Level 0 neighbors    │  Level 1 neighbors
                          │  (最多 2M 个)          │  (最多 M 个)

nbs:  [n0,n1,...│n2,n3,..│-1,-1,...│n4,...│-1,...│ ... ]
              │         │         │
              │         │         └── 上层邻居
              │         └── M个槽(底层2M)
              └── cum_nneighbor_per_level[layer_no] 定位
```

### 邻接区间定位

```c
// faiss_hnsw_utils.h 中的核心宏
#define faiss_cum_nb_neighbors(index, layer_no) \
    (index)->cum_nneighbor_per_level[layer_no]

// 读取节点 no 在 layer_no 层的邻居
begin = offsets[no] + cum_nneighbor_per_level[layer_no];
end   = offsets[no] + cum_nneighbor_per_level[layer_no + 1];
```

## 3. 层高采样

### 指数分布

节点层高由指数分布决定：

```c
// faiss_hnsw_insert.c
double faiss_rand_float() {
    return rand() / (double)RAND_MAX;
}

int32_t faiss_random_level(faiss_hnsw_t *index) {
    double f = faiss_rand_float();
    // 对概率累积表查表
    for (int level = 0; level < index->assign_probas_size; level++) {
        if (f < index->assign_probas[level]) {
            return level;
        }
        f -= index->assign_probas[level];
    }
    return index->assign_probas_size - 1;
}
```

`assign_probas` 是预计算的层高概率累积表，与 `M` 参数相关。

## 4. 搜索算法

### 高层贪心下降

```c
// faiss_hnsw_insert.c: faiss_hnsw_add_vector
for (; level > pt_level; level--) {
    for (;;) {
        int32_t prev_nearest = nearest;
        // 遍历 nearest 在 level 层的所有邻居
        // 计算到查询点的距离
        // 更新 nearest 和 d_nearest
        if (nearest == prev_nearest) {
            break;  // 本层已收敛
        }
    }
}
```

### 第 0 层 Best-First 搜索

使用 `faiss_minimax_heap_t` 候选堆管理：
- 按最小距离 pop（取最近候选优先扩展）
- 自动拒绝比堆内最差还差的新候选

## 5. 插入算法

### 核心流程

1. **`faiss_hnsw_vector_storage`**：将新向量追加到 `vectors` 数组
2. **`faiss_hnsw_prepare_quantized_storage`**：量化编码（可选）
3. **`faiss_hnsw_prepare_level_tab`**：为新节点分配层高 + 扩展邻接数组
4. **`faiss_hnsw_vector_algo_insert`**：逐层插入

### 选边策略（shrink_neighbor_list）

启发式选边避免邻居相互"遮挡"：

```c
for each (v1_id, dist_v1_q) in candidates:  // 按距离排序
    good = true
    for each v2_id in output:
        if distance(v2, v1) < dist_v1_q:
            good = false; break  // v2 遮挡了 v1
    if good:
        output.append(v1_id)
```

## 6. 与本演示程序的区别

| 特性 | 工程实现 | main.c 演示 |
|------|---------|-------------|
| 邻接表 | 扁平化数组 + 前缀和定位 | 简化：共用数组 |
| 搜索 | MinimaxHeap Best-First | 简化：贪心下降 |
| 选边 | shrink_neighbor_list 启发式 | 简化：只连最近邻 |
| 量化 | 支持 PQ 量化 | 不支持 |
| 距离计算 | 批量 4 个候选计算 | 单个计算 |
| 持久化 | 无 | 无 |

## 7. 关键文件对照

| 本演示 | 工程实现 |
|--------|----------|
| `hnsw_create` | `faiss_hnsw_index_create` |
| `hnsw_insert` | `faiss_hnsw_index_add` |
| `hnsw_search` | `faiss_hnsw_index_search` |
| `greedy_search_layer` | `greedy_update_nearest` (内联) |
| `sample_level` | `faiss_random_level` |

## 8. 参考链接

- 设计文档：`engineering/src/db/index/vector_index/hnsw/DESIGN.md`
- 核心实现：`engineering/src/db/index/vector_index/hnsw/faiss_hnsw_insert.c`
- 工具函数：`engineering/src/db/index/vector_index/hnsw/faiss_hnsw_utils.h`
