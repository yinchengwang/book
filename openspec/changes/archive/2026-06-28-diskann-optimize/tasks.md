## 1. 数据结构与内部头文件变更

- [x] 1.1 在 `diskann_private.h` 中新增 `diskann_heap_t` 结构体（`data/capacity/size`），声明 `diskann_heap_push`、`diskann_heap_pop`、`diskann_heap_peek`、`diskann_heap_skip_expanded`、`diskann_heapify` 五个堆操作函数
- [x] 1.2 在 `struct diskann` 中新增 `float *norms` 数组字段，用于存储每个向量的预计算平方范数
- [x] 1.3 修改 `diskann_robust_prune` 声明，新增 `int32_t occlude_rounds` 参数
- [x] 1.4 新增 `diskann_link_node`、`diskann_iterate_to_fixed_point`、`diskann_inter_insert` 函数声明
- [x] 1.5 修改 `diskann_index_search` 声明，新增 `int32_t max_iterations` 参数

## 2. 搜索优先队列（search-priority-queue）

- [x] 2.1 在 `diskann_utils.c` 中实现二叉堆 ops：`diskann_heap_push`（上滤）、`diskann_heap_pop`（下滤）、`diskann_heap_peek`（读取堆顶）、`diskann_heapify`（自底向上建堆）
- [x] 2.2 在 `diskann_utils.c` 中实现 `diskann_heap_skip_expanded`：连续弹出直到堆顶为未展开节点或堆空
- [x] 2.3 重构 `diskann_search.c` 中 `diskann_search_candidates` 的搜索展开循环（当前 90-104 行）：初始化 `diskann_heap_t` 包装 `candidates` 数组 → 种子节点入堆 → heapify 建堆 → while 循环调用 `heap_skip_expanded` + `heap_pop` 获取下一展开节点
- [x] 2.4 保持 visited 数组和 PQ 距离表集成逻辑不变，仅替换节点选择机制

## 3. 多轮 OccludeList 剪枝（multi-round-occlude-prune）

- [x] 3.1 在 `diskann_graph.c` 中修改 `diskann_robust_prune`：当 `occlude_rounds == 0` 时自动计算轮数 = `ceil(log(index->alpha) / log(1.2f))`；当 `occlude_rounds > 0` 时使用显式值
- [x] 3.2 实现多轮循环体：每轮重新 init `occlude_factor`（calloc），`cur_alpha` 从 1.0 按 1.2 递增，最后一轮 clamp 到 `index->alpha`
- [x] 3.3 合并多轮选中的节点（使用位图去重），传递到回填阶段
- [x] 3.4 保留回填（saturate graph）逻辑：所有轮次执行完毕后，若结果数 < `index_size` 则从候选池补足

## 4. 增量 Vamana Link 流程（incremental-vamana-link）

- [x] 4.1 在 `diskann_graph.c` 中新增 `diskann_iterate_to_fixed_point`：从 frozen points 作为种子 → best-first 图搜索（使用二叉堆）→ 收集 visited 节点到候选池 → 以 `build_search_list_size` 为展开上限
- [x] 4.2 在 `diskann_graph.c` 中新增 `diskann_inter_insert`：遍历 `result_ids`，对每个邻居检查 `diskann_has_neighbor` → 未满时直接追加 → 已满时收集旧邻居与新边重新 `diskann_robust_prune`
- [x] 4.3 在 `diskann_insert.c` 中新增 `diskann_link_node`：编排 `iterate_to_fixed_point → robust_prune → inter_insert` 三步流程
- [x] 4.4 重构 `diskann_incremental_insert_node`：移除全量 `diskann_append_unique_candidate` 循环，改为调用 `diskann_link_node`

## 5. 渐进搜索扩宽（progressive-search-widening）

- [x] 5.1 修改 `diskann_index_search` 签名，新增 `int32_t max_iterations` 参数
- [x] 5.2 实现扩宽循环：初始 `effective_l` 计算 → while 循环（轮次 < max_iterations 且 result_count < k）→ 翻倍 `effective_l`（clamp 到 `active_count`）→ 重新调用 `diskann_search_candidates`
- [x] 5.3 实现多轮搜索结果合并去重：按 id 去重，保留距离更小的版本
- [x] 5.4 `max_iterations <= 0` 时保持单次搜索行为，确保向后兼容

## 6. 快速 L2 距离计算

- [x] 6.1 在 `diskann_create.c` 的 `diskann_index_create` 中为 `norms` 数组分配初始容量
- [x] 6.2 在 `diskann_insert.c` 的向量添加路径中（`diskann_index_insert`、`diskann_index_add`），计算新向量的平方范数并存入 `norms[new_id]`
- [x] 6.3 在 `diskann_utils.c` 中新增 `diskann_fast_l2_distance(index, id_a, id_b)`：返回 `sqrt(norms[a] + norms[b] - 2*dot(a,b))`，搜索比较路径省略 `sqrt()`
- [x] 6.4 在 `diskann_search.c` 和 `diskann_graph.c` 的 L2 精确距离计算处替换为快速版本
- [x] 6.5 在 `diskann_persist.c` 的保存/加载中持久化 `norms` 数组（新增 `DISKANN_PAGE_NORM` 页面类型或追加到向量段）

## 7. 调用方适配

- [x] 7.1 适配 `diskann_build.c` 中所有 `diskann_robust_prune` 调用，传递 `occlude_rounds = 0`
- [x] 7.2 适配 `diskann_delete.c` 中 `diskann_repair_neighbors_after_delete` 的 `diskann_robust_prune` 调用，传递 `occlude_rounds = 0`
- [x] 7.3 适配 `diskann_graph.c` 中 `diskann_add_reverse_edge` 的 `diskann_robust_prune` 调用（若需保留兼容路径）
- [x] 7.4 确保 `diskann_incremental_insert_node` 内部不再直接调用旧版 `diskann_robust_prune`

## 8. 测试

- [x] 8.1 二叉堆单元测试：push/pop 堆序验证、heapify 正确性、skip_expanded 行为、空堆/满堆边界条件
- [x] 8.2 多轮 OccludeList 单元测试：不同 alpha 值的轮数计算验证、与旧版单轮行为等价性（occlude_rounds=1）、高 alpha 多轮产出不同结果
- [x] 8.3 Link 流程集成测试：单节点增量插入后图连通性、InterInsert 反向边正确性、已满邻居重新剪枝后边数不超限
- [x] 8.4 渐进搜索扩宽测试：max_iterations=1 与旧版结果一致、首轮不足时翻倍触发、max_iterations=0 向后兼容
- [x] 8.5 端到端召回率回归测试：使用标准数据集（如 SIFT10K），对比优化前后的 recall@10、QPS
- [x] 8.6 快速 L2 距离精度验证：快速版本与精确 L2 距离的数值误差在 `1e-6` 以内
