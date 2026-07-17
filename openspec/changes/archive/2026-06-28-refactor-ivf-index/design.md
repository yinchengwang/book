## Context

当前 `faiss_ivf_t` 使用 `int32_t **inverted_lists` 裸数组管理两级聚类 (nlist × nlist2) 产生的倒排桶。搜索使用 `qsort` 对一级中心排序后遍历选中桶内全量向量，再用 `qsort` 截断 top-k。缺少 DirectMap 无法 reconstruct，缺少删除支持。

约束：
- 公共 API 保持不变，向后兼容所有已有调用点和测试
- 无外部运行时依赖
- C11 标准，GCC/Clang 可编译

## Goals / Non-Goals

**Goals:**
- Phase 1: 引入 InvertedLists 抽象层替代裸数组，解耦存储与算法
- Phase 1: 重构所有内部引用点（core/add/search/train/utils）
- Phase 2: 新增 DirectMap 支持 reconstruct() 和 remove_ids()
- Phase 3: 搜索重写 —— MinimaxHeap + 桶优先级 + 提前终止

**Non-Goals:**
- 不改变 KV 化量化（PQ）的编码/解码逻辑
- 不改变 K-Means 训练流程
- 不添加多线程并行搜索
- 不实现 openGauss 风格的页面存储模型

## Decisions

### D1: InvertedLists 采用纯数组存储（与 FAISS ArrayInvertedLists 一致）

**选择**: 每个桶维护独立的 `int32_t *ids` 和 `uint8_t *codes[]` 数组

**替代方案**:
- 连续大数组 + offsets: FAISS 自身的 HNSW 使用此方案，但倒排表插入顺序不确定，桶间不连续，连续数组需要额外重排，开销大于收益
- 链表: 每次访问都需指针跳转，缓存不友好

**理由**: 纯数组方案动态扩容简单（2× 扩容），批量读取缓存友好，与 FAISS ArrayInvertedLists 一致

### D2: 墓碑删除而非物理删除

**选择**: 将 vector id 设为 -1 作为墓碑，搜索时跳过

**替代方案**:
- 物理删除 + 移动尾部元素: 改变向量位置，DirectMap 需全局更新，O(n) 开销
- 真空清理定时压缩: 定期或手动触发，将墓碑后的元素前移

**理由**: 墓碑方案 O(1) 删除，DirectMap 条目直接失效即可，不影响其他条目索引

### D3: 搜索用 MinimaxHeap 替代 qsort + 全局 candidates 数组

**选择**: 利用已有的 `faiss_minimax_heap_t`（大顶堆）维护 top-k 结果，O(log k) 更新

**替代方案**:
- qsort 全局排序: 当前方案，O(n log n)，n 可达 n_total
- std::priority_queue: C++ 方案，不符合本项目 C 语言约束

**理由**: MinimaxHeap 已在本项目 HNSW 搜索中验证，堆顶自动拒绝比第 k 个更差的候选，内存从 O(n_total) 降至 O(k)

### D4: DirectMap 采用数组类型（非哈希）

**选择**: `Type 1` 数组映射，`direct_map[id] = (list_no << 32) | offset`

**替代方案**:
- Type 2 哈希表: 支持稀疏 ID，但增加内存和复杂度
- 不实现 DirectMap: 无法 reconstruct

**理由**: 当前向量 ID 从 0 连续递增，数组映射 O(1) 查找且无哈希冲突

### D5: 桶优先级按二级中心距离排序

**选择**: 计算 query 到所有二级中心的距离，取最近 nprobe 个桶优先扫描

**替代方案**:
- 先选一级簇再展开所有二级桶: 当前方案，不考虑二级中心距离
- 只选一个一级簇: 召回率太低

**理由**: 直接按二级中心距离排序避免了扫描大量无关桶，且与 FAISS IndexIVF 的 `scan_codes` 理念一致

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| 墓碑累积使桶膨胀、搜索变慢 | 提供 `compact()` 真空清理 API，用户按需调用 |
| InvertedLists 动态扩容 2× 可能浪费内存 | 扩容上限由 `nlist × nlist2` 缩放，训练后可预分配 |
| 桶优先级排序需计算所有二级中心距离 | 二级中心数 = nlist × nlist2，通常 ≤ 1024，开销可忽略 |
| DirectMap 数组大小 = n_total，大容量时内存显著 | 提供 type 选择，未来可扩展哈希表 |
