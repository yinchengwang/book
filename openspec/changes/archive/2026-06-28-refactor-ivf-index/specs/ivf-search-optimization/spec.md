## ADDED Requirements

### Requirement: 用 MinimaxHeap 维护 top-k 结果
系统 SHALL 使用 `faiss_minimax_heap_t` 大顶堆（堆顶为第 k 个最近距离，即最差结果）管理搜索过程中的 top-k 候选。
新候选若距离大于等于堆顶且堆已满（k 个结果），SHALL 直接丢弃（O(1) 比较）。
若优于堆顶，SHALL 替换堆顶并下滤重建堆性质（O(log k)）。

#### Scenario: 结果集未满时添加
- **WHEN** 当前结果数 < k，任意距离的候选都直接加入
- **THEN** 候选正确插入，堆大小增加 1

#### Scenario: 结果集已满且新候选更优
- **WHEN** 已有 k 个结果（堆顶距离=3.5），新候选距离=2.0
- **THEN** 堆顶被替换为 2.0，堆重建后新的最差结果在堆顶

#### Scenario: 结果集已满但新候选更差
- **WHEN** 已有 k 个结果（堆顶距离=3.5），新候选距离=5.0
- **THEN** 候选被丢弃，结果集不变

### Requirement: 桶按二级中心距离排序
系统 SHALL 计算 query 到所有二级中心的距离，选取距离最近的 nprobe 个桶优先扫描。
桶的访问顺序 SHALL 按其中心与 query 的距离升序排列。

#### Scenario: 多桶按距离排序
- **WHEN** query 到三个二级中心的距离分别为 [2.0, 5.0, 1.5]，nprobe=2
- **THEN** 优先扫描距离 1.5 的桶，其次扫描距离 2.0 的桶，距离 5.0 的桶不扫描

#### Scenario: nprobe 大于实际桶数
- **WHEN** 总桶数 = 4，nprobe = 8
- **THEN** 所有 4 个桶均按距离排序后扫描

### Requirement: 提前终止搜索
当已处理的候选数量足够且当前桶的最短可能距离大于 top-k 结果中最差距离时，系统 SHALL 停止继续扫描剩余的桶。

#### Scenario: 桶距离大于最差结果时跳过
- **WHEN** top-k 堆顶（最差）= 0.8，下一个待扫描桶的中心距离 = 1.2
- **THEN** 该桶被跳过，后续更远的桶也不扫描

#### Scenario: 桶距离小于最差结果时继续
- **WHEN** top-k 堆顶（最差）= 1.5，下一个待扫描桶的中心距离 = 0.9
- **THEN** 该桶正常扫描

### Requirement: 消除全局 candidates 数组
搜索过程 SHALL 不再分配 n_total 大小的 candidates/distances 临时数组。
临时内存使用量 SHALL 仅与 k、nprobe、桶内最大向量数相关。

#### Scenario: 搜索内存占用验证
- **WHEN** n_total=100000，k=10，执行搜索
- **THEN** 不再出现 100000 × sizeof(int32_t) 和 100000 × sizeof(float) 的临时分配
