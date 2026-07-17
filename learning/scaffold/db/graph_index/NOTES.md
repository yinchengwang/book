# 工程参考笔记：HNSW 向量索引

本文档记录 HNSW 索引在 `engineering/src/db/index/vector_index/hnsw/` 中的工程实现要点，
供学习时对照参考。

## 1. 核心数据结构

### 1.1 扁平化邻接表

工程实现将所有层级的邻接信息存储在单一扁平数组 `nbs` 中，通过 `offsets` 数组定位每个节点的邻接区间。
关键定位宏如下（来自 `faiss_hnsw_utils.h`）：

```c
// 计算节点在某一层的邻接区间 [begin, end)
begin = offsets[no] + cum_nneighbor_per_level[layer_no];
end   = offsets[no] + cum_nneighbor_per_level[layer_no + 1];
```

其中 `cum_nneighbor_per_level` 是前缀和数组，例如 M=16 时：
- Layer 0 容量 = 2M = 32
- Layer 1+ 容量 = M = 16

这种设计避免了在每一层维护独立的邻接表，减少指针解引用开销。

### 1.2 MinimaxHeap

工程中用 `faiss_minimax_heap_t` 管理候选集，它是一个双语义堆：
- 按**最小距离** pop（优先扩展最近候选，即 Best-First）
- 同时能**自动拒绝**比堆内最差候选还差的新候选

这使得候选集始终保持在 efSearch 个最优值，不会有冗余。

### 1.3 批量距离计算

工程在搜索热路径上使用了 4 个一批的批量距离计算 `batch4_from_query`，
减少函数调用开销。在量化模式下走 ADC 查表（O(1)），否则走原始向量计算。

## 2. 插入流程

### 2.1 层高采样

每个新节点按指数分布随机分配层高：

```c
assign_probas[0] = 1 - (1/M)^(1/level_factor);  // L0 的概率最高
for (i = 1; i < N_layers; i++)
    assign_probas[i] = assign_probas[i-1] * (1/M)^(1/level_factor);
```

层数越高，概率越小，因此高层节点稀少，形成"高速通道"。

### 2.2 逐层插入

从高层到低层逐层插入，每层步骤：
1. 贪心下降找到当前层的入口点
2. 搜索 efConstruction 个最近邻居
3. 调用 `shrink_neighbor_list` 启发式选边
4. `connect_points` 双向连边

### 2.3 启发式选边（shrink_neighbor_list）

按距离排序候选队列后，从近到远依次检查：
只有当"没有已选邻居比当前候选更靠近查询点"时才保留。
这避免了邻居相互"遮挡"，保证了图的多样性和搜索覆盖率。

## 3. 搜索流程

### 3.1 高层贪心下降

从 `entry_point` 出发，在每一层遍历邻居找更近的点。
工程中用 4 个一批的批量距离计算减少开销：
每次遍历 4 个邻居，批量计算距离后比较更新 nearest。

### 3.2 第 0 层 Best-First

使用 MinimaxHeap 管理候选集：
1. 从堆中弹出最近候选
2. 遍历其所有邻居，计算距离
3. 尝试进入 top-k 结果堆，同时推入 MinimaxHeap
4. 当已处理的小距离候选数 >= efSearch 时，提前终止

提前终止条件：`if (worse_than_worst) break;`，
即当当前候选比结果集中最差的结果还差时，可以安全剪枝。

## 4. 存储设计

全内存索引，无磁盘持久化。所有数据保存在并行数组中：
- `vectors`：原始向量
- `codes`：PQ 编码向量
- `levels`：每个节点的层高
- `offsets`：邻接表偏移
- `nbs`：扁平邻接表
- `assign_probas`：层高概率分布表
- `cum_nneighbor_per_level`：每层邻居容量前缀和

空间估算（32 维向量，M=16，10 万节点）：
| 组件 | 大小 |
|------|------|
| vectors | 12.8 MB |
| codes (PQ) | 1.6 MB |
| nbs 邻接表 | ~19.2 MB |
| **合计** | **~34 MB** |

## 5. 参数调优

| 参数 | 说明 | 调优方向 |
|------|------|----------|
| M | 每层最大邻居数（底层 2M） | 越大召回越高，内存越大 |
| ef_construction | 构图搜索候选数 | 越大图质量越高，构图越慢 |
| ef_search | 搜索候选数 | 越大召回越高，搜索越慢 |

## 6. 关键文件

| 文件 | 职责 |
|------|------|
| `faiss_hnsw.h` | 主数据结构 `faiss_hnsw_t` 定义 |
| `faiss_hnsw_create.c` | 创建、销毁、参数管理 |
| `faiss_hnsw_insert.c` | 层高分配、图构建、启发式选边 |
| `faiss_hnsw_search.c` | 搜索（贪心下降+Best-First） |
| `faiss_hnsw_utils.h` | 邻接表定位宏、距离计算封装 |
| `faiss_visited_table.h` | visited 标记表 |
| `faiss_minimax_heap.h` | 双语义候选堆 |
