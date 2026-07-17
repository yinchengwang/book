## Context

当前 DiskANN C 实现的搜索路径（`diskann_search.c:90-104`）使用 O(n) 线性扫描在候选数组中寻找最近未展开节点，这在大候选集（>1000 个已访问节点）时成为主要瓶颈。增量插入（`diskann_incremental_insert_node`）将全体活跃节点作为候选池（O(n²) 复杂度），而非像 openGauss 那样通过图搜索定位近邻。RobustPrune 为单轮 alpha 剪枝，未采用 openGauss 的多轮递增 OccludeList 算法。

参考实现：openGauss 的 `NeighborPriorityQueue`（二分插入）、`DiskAnnGraph::Link()`（IterateToFixedPoint → PruneNeighbors → InterInsert）、`DiskAnnGraph::OccludeList()`（多轮递增剪枝）。

## Goals / Non-Goals

**Goals:**

- 将搜索热路径的展开选择从 O(n) 降至 O(log n)，提升 >5K 向量规模的搜索吞吐
- 增量插入通过图搜索（而非全量扫描）定位候选集，使单条插入的复杂度从 O(n²·d) 降至 O(L·D·d)（L=搜索列表大小，D=节点度）
- 多轮 OccludeList 剪枝产生更优的图连通性，提升召回率
- 渐进搜索扩宽避免无效的大候选集搜索，同时保证不足时自动补充

**Non-Goals:**

- 不改变持久化文件格式（保存/加载兼容现有格式）
- 不改动量化子系统（PQ 编码/解码/距离表保持不变）
- 不引入新的外部依赖
- 不实现并行图构建（openGauss 的 bgworker 模式）
- 不实现 Master/Slave 节点模型（openGauss 的原地更新机制）

## Decisions

### D1: 优先队列选择数组二叉堆而非配对堆

**选择**: 在 `diskann_scored_t` 数组之上实现标准二叉最小堆（上滤/下滤），新增 `diskann_heap_t` 结构包装 `data/capacity/size`。

**备选方案**: (a) 配对堆 — 理论更优但实现复杂度高，C 语言手动管理节点内存增加出错风险；(b) 维持数组 + `qsort` 批排序 — 按固定间隔排序而非每次插入排序，但展开节点的选择精度下降。

**理由**: 二叉堆实现简单（~50 行 C），所有操作 O(log n)，与现有的 `diskann_scored_t` 结构完全兼容，无需额外分配节点对象。openGauss 在有 `std::vector` 的 C++ 环境下选择了二分插入数组，我们在纯 C 环境下二叉堆是更自然的选择。

### D2: Link 流程集成到 diskann_insert.c

**选择**: 新增 `diskann_link_node(diskann_t *index, int32_t node_id)` 函数，内部依次调用:
1. `diskann_iterate_to_fixed_point(index, node_id, pool, &pool_count)` — 从冻结点出发图搜索，收集搜索路径上的节点作为候选
2. `diskann_robust_prune(index, node_id, pool, pool_count, result_ids, &result_count)` — 多轮剪枝
3. `diskann_inter_insert(index, node_id, result_ids, result_count)` — 对每个出边邻居检查是否需要添加反向边

`diskann_incremental_insert_node` 改为调用此函数。

**备选方案**: 将 Link 逻辑放在 `diskann_graph.c` 中 — 合理但会使调用链跨文件变复杂。

**理由**: 增量插入的入口在 `diskann_insert.c`，保持调用链的局部性。图搜索和剪枝的实现在 `diskann_graph.c`，Link 只是把它们编排起来。

### D3: IterateToFixedPoint 搜索候选池而非路径

**选择**: `diskann_iterate_to_fixed_point` 执行标准的 best-first 图搜索（使用 D1 的二叉堆），返回搜索过程中遍历到的所有节点作为候选池，而非仅返回未展开节点的集合。

**备选方案**: (a) 搜索到底（直到无未展开节点），候选池 = 所有 visited 节点 → 候选太多，后续 prune 开销大；(b) 只返回搜索路径上的节点 → 候选太少，可能丢失好邻居。

**理由**: 沿用现有的 `diskann_search_candidates` 逻辑作为基础，使用 `search_list_size = build_search_list_size` 控制搜索深度。openGauss 的 `IterateToFixedPoint` 也是 explore 到底直到 `!has_unexpanded_node()`，但通过 `so->nexpextedCandidates` 限制队列大小。我们使用 `search_list_size` 上限来控制。

### D4: OccludeList 多轮递增 alpha

**选择**: 将 `diskann_robust_prune` 修改为支持多轮递增 alpha:
```
cur_alpha = 1.0f
while cur_alpha <= target_alpha && result_count < max_degree:
    occlude_factor[i] = true 重新初始化
    for each unoccluded candidate (sorted by distance):
        选中，标记被其遮挡的候选
    cur_alpha *= 1.2f
```

函数签名新增 `occlude_rounds` 参数（0 表示自动计算 = ceil(log(target_alpha)/log(1.2))）。

**备选方案**: (a) 保持单轮 alpha 不变 → 已知图质量不如多轮；(b) 固定 3 轮 → 不够灵活。

**理由**: openGauss 的做法已被 Microsoft 原版 DiskANN 论文验证。递增 alpha 从宽松到严格，早期轮次允许更多边保证连通性，后期轮次收紧控制度数。1.2 倍率是论文推荐值。

### D5: 渐进搜索扩宽在 diskann_index_search 层实现

**选择**: 在 `diskann_index_search` 中添加 while 循环：初始 `search_list_size` 较小（如 k*2），每轮翻倍直至返回足够结果或达到 `max_iterations` 上限。`max_iterations <= 0` 时沿用现有的一次性搜索行为（向后兼容）。

**备选方案**: 直接在 `diskann_search_candidates` 内部实现 — 但该函数是纯内部 API，改动返回语义会影响调用方。

**理由**: 扩宽逻辑涉及对返回结果数量的判断和重新搜索决策，放在公开 API 层更自然。内部函数保持单一职责（一次搜索）。

### D6: 快速 L2 距离使用预计算平方范数

**选择**: 在 `struct diskann` 中新增 `float *norms` 数组，在插入/构建时预计算每个向量的平方范数 `sum(x[i]²)`。L2 距离计算改为 `sqrt(norms[a] + norms[b] - 2*dot(a,b))`。搜索时大部分距离只需要比较相对大小，省略 `sqrt()`。

**备选方案**: 动态计算平方范数 — 每次距离计算都遍历维度，无性能收益。

**理由**: 这是标准 Vamana 实现（包括 openGauss `ComputeL2DistanceFast`）使用的优化。对高维向量的搜索吞吐提升显著。

## Risks / Trade-offs

- **[R1] 二叉堆修改 `diskann_scored_t` 数组时破坏展开状态** → 堆操作只影响未展开节点的顺序，`expanded` 标记不变。展开节点无需出堆（通过 `expanded` 标记跳过），避免额外维护成本。openGauss 的做法相同：`_cur` 指针直接跳过已展开节点。
- **[R2] 多轮 OccludeList 增加剪枝耗时** → 剪枝只在构建/增量插入时调用，不在搜索热路径上。实际增量插入中，候选池大小受 `build_search_list_size` 限制（通常 ≤ 200），多轮开销可忽略。
- **[R3] 快速 L2 需要额外 `n_total * sizeof(float)` 内存存储范数** → 对于 100K 向量仅增加 ~400KB，远小于向量存储本身。
- **[R4] `diskann_robust_prune` 签名变更破坏 ABI** → 所有内部调用方（build/insert/delete）在同一变更中同步修改；公开 API `diskann.h` 不暴露此函数。
- **[R5] 渐进扩宽增加最坏情况延迟** → 通过 `max_iterations` 上限（默认 3）控制。第一轮搜索通常已满足大部分查询，额外轮次仅在对高召回有严格需求时触发。
