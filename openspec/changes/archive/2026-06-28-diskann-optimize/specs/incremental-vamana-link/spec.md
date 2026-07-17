## ADDED Requirements

### Requirement: Link 流程编排 — IterateToFixedPoint → Prune → InterInsert

系统 SHALL 提供 `diskann_link_node(diskann_t *index, int32_t node_id)` 函数，按以下顺序执行增量插入：
1. 调用 `diskann_iterate_to_fixed_point` 进行图搜索，收集候选池
2. 调用 `diskann_robust_prune` 对候选池执行多轮剪枝，确定最终出边
3. 调用 `diskann_inter_insert` 对每个出边邻居检查并维护反向边

`diskann_incremental_insert_node` SHALL 改为调用 `diskann_link_node`。

#### Scenario: 增量插入一个节点进入已有图

- **WHEN** 图已构建（`index->built == true`），对节点 `node_id` 调用 `diskann_link_node`
- **THEN** 系统通过图搜索找到候选邻居，剪枝后设置该节点的出边，并为所有出边邻居补上反向边

#### Scenario: 只插入一个节点时跳过 Link

- **WHEN** 图中活跃节点数 ≤ 1 时调用 `diskann_link_node`
- **THEN** 系统直接返回成功（无需建立边），与现有行为一致

### Requirement: IterateToFixedPoint — 图搜索收集候选池

`diskann_iterate_to_fixed_point(index, node_id, pool, &pool_count)` SHALL 执行 best-first 图搜索：从 frozen points 作为种子出发，使用最小堆选择最近未展开节点，展开其邻居，直到达到 `search_list_size` 上限或堆中无未展开节点。搜索过程中访问到的所有节点（包括已展开和未展开）均收集到候选池中。

搜索参数 SHALL 使用 `index->build_search_list_size` 作为 `search_list_size` 上界。

#### Scenario: 从冻结点出发搜索图

- **WHEN** 调用 `diskann_iterate_to_fixed_point`，frozen points 存在且有效
- **THEN** 系统将 frozen points 作为种子入堆，开始 best-first 图遍历

#### Scenario: 冻结点均被删除时回退到活跃入口

- **WHEN** 所有 frozen points 已被逻辑删除
- **THEN** 系统使用 `entry_point` 作为唯一种子节点，若 `entry_point` 也无效则返回错误

#### Scenario: 搜索深度受 search_list_size 限制

- **WHEN** 图搜索展开节点数达到 `build_search_list_size`
- **THEN** 系统停止展开并将当前堆中的未展开节点一并纳入候选池

### Requirement: InterInsert — 对称边维护

`diskann_inter_insert(index, node_id, result_ids, result_count)` SHALL 遍历 `node_id` 的每个出边邻居 `result_ids[i]`，对该邻居检查是否已有从邻居指向 `node_id` 的反向边；若没有则尝试添加。若邻居的出边已满（达到 `index_size`），SHALL 将该邻居的旧邻居与新边一起重新执行 `diskann_robust_prune`。

此逻辑提取自 `diskann_add_reverse_edge`，但批量处理而非逐个调用。

#### Scenario: 目标邻居未满时直接添加反向边

- **WHEN** 邻居 `to_id` 的当前出边数 < `index_size`，且尚未包含 `node_id`
- **THEN** 系统直接将 `node_id` 追加到 `to_id` 的出边列表

#### Scenario: 目标邻居已满时重新剪枝

- **WHEN** 邻居 `to_id` 的出边已满（达到 `index_size`）
- **THEN** 系统将 `to_id` 的现有出边（去重）+ `node_id` 作为候选池，重新调用 `diskann_robust_prune` 确定新的出边列表

#### Scenario: 跳过已存在的反向边

- **WHEN** 邻居 `to_id` 的出边列表中已包含 `node_id`
- **THEN** 系统不做任何修改，直接跳过
