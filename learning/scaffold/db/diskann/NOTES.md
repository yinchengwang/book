# DiskANN 与 engineering 实现对照笔记

> 对照 `engineering/src/db/index/vector_index/diskann/` 分析 DiskANN 工程实现细节

## 1. 搜索核心：diskann_search_candidates

`diskann_search.c` 第 210-280 行实现了候选搜索核心算法。流程如下：

```c
/* 初始化 visited 标记数组和候选池 */
visited = (bool *)calloc(index->n_total, sizeof(bool));
candidates = (diskann_scored_t *)malloc(capacity * sizeof(diskann_scored_t));

/* 从 frozen points（冻结点）作为种子出发，作为图的稳定入口点 */
for (i = 0; i < index->storage_params.frozen_point_count; ++i) {
    int32_t seed_id = index->frozen_points[i];
    visited[seed_id] = true;
    candidates[count].id = seed_id;
    candidates[count].distance = diskann_candidate_distance_from_query(...);
}
```

**与 main.c 的区别**：
- main.c 从 medoid 单一起点出发贪心搜索
- 工程实现从多个 frozen points 并行出发，使用优先队列（最小堆）选择最近未展开节点
- 使用 `diskann_heap_push/pop` 实现 O(log n) 的候选选择，避免线性扫描

## 2. PQ 距离加速

`diskann_search_candidates` 第 235-248 行支持 PQ 加速：

```c
if (diskann_pq_ready(index)) {
    /* 为查询向量计算 PQ 距离表 */
    distance_table = (float *)malloc(distance_table_size);
    quantizer_compute_distance_table(index->quantizer, index->metric,
                                     query, distance_table);

    /* 用 PQ 近似距离替代精确 L2 计算 */
    candidates[count].distance = diskann_candidate_distance_from_query(
        index, query, distance_table, neighbor_id);
}
```

**距离估算原理**：
- 查询向量 → 计算与各分段质心的距离，生成 lookup table
- 邻居节点用 PQ 编码 → 查表求和得到近似距离
- 最终重排阶段用精确距离重新排序

## 3. 图剪枝：diskann_robust_prune

`diskann_graph.c` 第 80-165 行实现了 **Robust Prune** 算法：

```c
/* 按距离从近到远排序候选池 */
qsort(pool, pool_count, sizeof(diskann_scored_t), diskann_compare_scored);

/* 多轮 alpha 递增剪枝 */
cur_alpha = 1.0f;
for (round = 0; round < rounds && chosen < index->index_size; ++round) {
    cur_alpha *= 1.2f;  /* 每轮 alpha 递增 1.2x */
    for (i = 0; i < pool_count; ++i) {
        if (!selected[i]) {
            result_ids[chosen++] = pool[i].id;
            selected[i] = true;
        }
        /* 对每个已选节点，遮挡距离过远的候选 */
        for (j = i + 1; j < pool_count; ++j) {
            float between = diskann_fast_l2_distance(index, pool[i].id, pool[j].id);
            if (cur_alpha * between <= pool[j].distance) {
                occlude_factor[j] = true;  /* 被遮挡，跳过 */
            }
        }
    }
}
```

**Vamana 剪枝核心思想**：
- 候选节点按与当前节点的举例排序
- 从最近的节点开始选择，将距离过近的节点互相遮挡
- alpha 参数控制遮挡半径（alpha=1.2 意味着距离 1.2 倍以内的节点会被遮挡）
- 保证最终每个节点的邻居数 ≤ index_size

## 4. 删除修复：diskann_repair_neighbors_after_delete

`diskann_graph.c` 第 250-320 行处理删除后的图修复：

```c
/* 候选池来源三部分：
 * 1. 当前节点的旧邻居
 * 2. 被删除节点的邻居
 * 3. 二跳邻居（邻居的邻居）
 */
diskann_build_repair_candidate_pool(index, node_id, victim_neighbors, ...);
diskann_robust_prune(index, node_id, pool, pool_count, result_ids, ...);
diskann_set_neighbors_for_node(index, node_id, result_ids, result_count);
```

**工程实现 vs. main.c**：
- main.c 演示了内存搜索和 SSD 回退的基本流程
- 工程实现还处理了动态删除后的图修复，保证删除后搜索质量不显著下降

## 5. 总结

| 特性 | main.c 演示 | diskann_*.c 工程实现 |
|------|-------------|---------------------|
| 搜索起点 | 单 medoid | 多 frozen points |
| 候选选择 | 数组线性扫描 | 二叉最小堆 O(log n) |
| 距离计算 | 全量 L2 | PQ 近似 + 精确重排 |
| 图剪枝 | 简化近邻 | Robust Prune + alpha 控制 |
| 删除处理 | 未实现 | 候选池修复 + 反向边维护 |
