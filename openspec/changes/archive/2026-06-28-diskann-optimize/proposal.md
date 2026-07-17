## Why

当前 DiskANN C 实现的核心算法路径存在三个关键瓶颈：(1) 搜索热路径采用 O(n) 线性扫描寻找最近未展开节点，而 openGauss 等成熟实现使用二分插入优先队列；(2) 增量插入将全体活跃节点作为候选池遍历，而非通过图搜索定位局部候选集（openGauss 的 Link 流程）；(3) RobustPrune 为单轮 alpha 剪枝，openGauss 的多轮递增 OccludeList 能产生更优的图质量。这些问题导致在大规模数据集（>10K 向量）上搜索延迟和构建质量均不理想。

## What Changes

- **搜索优先队列**：将 `diskann_search_candidates` 中的 O(n) 线性展开选择替换为基于最小堆的优先队列，使每次展开操作的复杂度从 O(n) 降至 O(log n)
- **增量插入 Link 流程**：用 `IterateToFixedPoint`（图搜索）替代 `diskann_incremental_insert_node` 中的全量候选池扫描，新节点通过图遍历发现近邻候选，无需遍历全体活跃节点
- **多轮 OccludeList 剪枝**：将单轮 alpha 剪枝升级为递增 alpha（1.0 → 目标值，步进 ×1.2）的多轮遮挡列表算法，提升图连通性质量
- **渐进搜索扩宽**：在 `diskann_index_search` 中支持候选集翻倍重新搜索机制，当初始搜索未返回足够结果时自动扩大搜索深度
- **快速 L2 距离计算**：利用 `su + sv - 2·dot` 公式优化精确距离计算路径，减少冗余向量模计算 **BREAKING**: `diskann_robust_prune` 函数签名变更（增加 occlude_alpha 和 rounds 参数）；`diskann_search_candidates` 内部实现完全重构

## Capabilities

### New Capabilities

- `search-priority-queue`: 基于最小堆的 best-first 搜索展开选择，包含堆插入、堆弹出、距离表集成。替换 `diskann_search.c:90-104` 的线性扫描逻辑。
- `incremental-vamana-link`: 增量 Vamana 图的 Link 插入流程 — 通过 IterateToFixedPoint 从冻结点出发进行图搜索，以搜索到的近邻为候选池做 Prune，最后通过 InterInsert 维护对称边。
- `multi-round-occlude-prune`: 多轮递增 alpha 的 RobustPrune，每轮使用 occlude_factor 数组跟踪遮挡状态，cur_alpha 从 1.0 按 1.2 倍率递增至目标 alpha。
- `progressive-search-widening`: 搜索时从小候选集开始，若返回结果不足则翻倍 `search_list_size` 后重新搜索，支持配置最大迭代轮次。

### Modified Capabilities

<!-- 无现有规范需要修改 -->

## Impact

| 受影响文件 | 变更类型 |
|-----------|---------|
| `src/index/vector_index/diskann/diskann_search.c` | 重写搜索展开循环（最小堆），新增渐进扩宽逻辑 |
| `src/index/vector_index/diskann/diskann_graph.c` | 重写 `diskann_robust_prune`（多轮 OccludeList），新增 `diskann_link`、`diskann_iterate_to_fixed_point`、`diskann_inter_insert` |
| `src/index/vector_index/diskann/diskann_insert.c` | 重构 `diskann_incremental_insert_node`，调用 Link 流程 |
| `src/index/vector_index/diskann/diskann_private.h` | 新增优先队列结构、opaque_list 上下文、函数声明，修改 `diskann_robust_prune` 签名 |
| `include/index/vector_index/diskann/diskann.h` | `diskann_index_search` 增加 `max_iterations` 参数 |
| `src/index/vector_index/diskann/diskann_build.c` | 适配 `diskann_robust_prune` 的新签名 |
| `src/index/vector_index/diskann/diskann_delete.c` | 适配 `diskann_robust_prune` 的新签名 |
| `test/index/vector_index/diskann/diskann_gtest.cpp` | 新增优先队列、Link 流程、多轮剪枝、渐进搜索测试用例 |
