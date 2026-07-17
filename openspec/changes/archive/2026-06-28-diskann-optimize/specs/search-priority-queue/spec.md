## ADDED Requirements

### Requirement: 搜索展开使用二叉堆选择最近未展开节点

`diskann_search_candidates` 在 best-first 图搜索的每次迭代中，SHALL 使用基于数组的二叉最小堆选择下一个距离最小的未展开节点，而非遍历整个候选数组做线性扫描。

堆操作 SHALL 包括：
- `diskann_heap_push(heap, scored)` — 插入新候选，上滤维护堆序，O(log n)
- `diskann_heap_pop(heap)` — 弹出堆顶最小距离候选，下滤维护堆序，O(log n)
- `diskann_heap_peek(heap)` — 读取堆顶但不弹出，O(1)
- `diskann_heap_skip_expanded(heap)` — 跳过堆顶已展开节点（连续弹出直到找到未展开节点或堆空）

#### Scenario: 搜索过程中候选插入到堆中

- **WHEN** 搜索中遇到新邻居节点 `neighbor_id`，且该节点未被访问过
- **THEN** 系统将该节点封装为 `diskann_scored_t`（含 id、distance、expanded=false），调用 `diskann_heap_push` 插入二叉堆中

#### Scenario: 从堆中弹出最近未展开节点进行展开

- **WHEN** 搜索循环需要展开下一个节点，且堆非空
- **THEN** 系统从堆顶取出距离最小的候选节点，将其标记为 `expanded=true` 并展开其邻居

#### Scenario: 堆顶节点已展开时自动跳过

- **WHEN** 堆顶节点的 `expanded == true`（例如由其他路径展开）
- **THEN** 系统弹出该节点并丢弃，继续检查新的堆顶，直到找到未展开节点或堆空

#### Scenario: 堆空时终止搜索

- **WHEN** 堆中没有未展开节点（所有候选均已展开或堆为空）
- **THEN** 搜索循环正常终止，返回已收集的候选结果

### Requirement: 堆结构与现有候选数组兼容

`diskann_heap_t` SHALL 直接包装现有的 `diskann_scored_t *data` 数组，不要求额外内存分配。堆的元数据（size、capacity）存储在 `diskann_heap_t` 结构体中。

#### Scenario: 堆基于已有的候选数组初始化

- **WHEN** 搜索初始化阶段创建了 `candidates` 数组并填充了种子节点（frozen points）
- **THEN** 系统将 `candidates` 数组和 `capacity` 传入 `diskann_heap_t`，指定 `size = 种子节点数`，调用建堆操作（自底向上 heapify，O(n)）

#### Scenario: 堆容量不足时扩展失败

- **WHEN** 堆已满（`size == capacity`）且需要插入新候选
- **THEN** 系统返回错误码 `-1`，调用方可触发重新搜索或增大 capacity

### Requirement: PQ 距离表在堆操作中保持可用

当 PQ 启用时，二叉堆的排序依据 SHALL 与线性扫描模式完全一致（使用 `diskann_candidate_distance_from_query` 预计算的距离值）。堆不修改任何距离值。

#### Scenario: PQ 模式下堆插入使用预计算距离

- **WHEN** 系统启用 PQ 且有有效的 `distance_table`，搜索遇到新邻居
- **THEN** 系统调用 `diskann_candidate_distance_from_query(index, query, distance_table, neighbor_id)` 计算距离，然后将结果插入堆中
