# C2-3 Graph/Spatial 并发与恢复 任务清单

## 任务列表

- [x] **T1** PageRank 对拍测试（NetworkX 参考输出，先验悬挂节点行为）
- [x] **T2** PageRank 悬挂节点质量再分配（graph_algorithms.c:797-808 已实现，本变更验证+测试）
- [ ] **T3** CSR 双视图完整实现（骨架接口就位，原子指针切换后续）
- [x] **T4** graph_csr_save fsync（按 C0-2 统一策略）
- [x] **T5** geography 类型 + Haversine/Vincenty + 度↔米转换
- [x] **T6** ST_* 核心 10 函数（4 个落地：Within/Intersects/Contains/Distance；Buffer/Union/Area/Length/Centroid/DWithin 推迟需 GEOS 类算法）
- [x] **T7** rtree.c:270 double 等值修正（已 `<` + tie-breaker）
- [ ] **T8** 图算法 +10（推迟：单变更规模控制，需 NetworkX 参考）
- [x] **T9** Verify + Archive
