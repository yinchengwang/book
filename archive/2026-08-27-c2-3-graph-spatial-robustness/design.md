# C2-3 Graph/Spatial 并发与恢复 设计文档

## 设计目标

修复 03/07 卷识别的 Graph/Spatial 缺陷：CSR 并发与持久化恢复、Spatial 球面距离、ST_* 函数空缺。

## 方案

### 1. PageRank 悬挂节点处理（T1-T2）

`graph_algorithms.c:678-902` `pagerank_new` 现有实现可能未正确处理悬挂节点（无出边的节点）。修正：
- 在迭代前计算 total dangling mass
- 每次迭代将 dangling mass 均匀分配到所有节点（standard PageRank 公式）
- 收敛条件：|old_rank - new_rank| < ε

### 2. CSR 双视图（T3）

C0-1 已提供并发锁骨架。本变更完成：
- 遍历持旧 CSR 快照指针（`csr_storage` 主指针）
- 写入走 COO（`coo_entries`），达到容量后 `csr_compact` 重建
- 压力测试：4 reader + 1 writer 并发

### 3. fsync + COO 重放（T4）

- `graph_csr_save` 加 fsync（C0-2 统一策略）
- 启动恢复：读 CSR 文件 → 看是否有未 compact 的 COO 记录 → 重放

### 4. geography 类型（T5）

新增 `geography_t` 类型：经纬度 + Haversine 距离 + Vincenty 大圆距离 + 度↔米转换系数。

### 5. ST_* 函数（T6）

10 个核心 ST 函数：Distance/Within/Intersects/Contains/Buffer/Union/Area/Length/Centroid/DWithin。Buffer/Union 由 JTS 替代？**自研**：基于 bounding box 简化实现，精度足够基本 GIS 应用。
