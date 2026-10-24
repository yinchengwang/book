# C2-3 Graph/Spatial 并发与恢复 Proposal

## Why

差距分析 03/07 卷发现：Graph CSR 全文件零锁（`graph_csr.c`）+ `graph_csr_save`（`:206`）持久化缺 fsync、崩溃后 COO 未 compact 数据恢复路径未验证；PageRank 悬挂节点处理未验证（`graph_algorithms.c:678-902`）。Spatial 仅平面欧氏距离（grep haversine/spherical 零命中）——地理数据算错距离；R-Tree double 等值比较边界风险（`rtree.c:270`）；ST_* PostGIS 兼容函数缺失。

## What Changes

- CSR 双视图完成（C0-1 基础上）：查询走旧 CSR、写走 COO、compact 原子切换 + 启动时 COO 重放
- graph_csr_save 接 fsync（C0-2 统一策略）
- PageRank 悬挂节点处理修正 + 对拍 NetworkX 参考输出的单元测试
- Spatial geography 类型：经纬度存储 + Haversine/Vincenty 距离 + 度↔米转换
- ST_* 核心函数 10 个：ST_Distance/ST_Within/ST_Intersects/ST_Contains/ST_Buffer/ST_Union/ST_Area/ST_Length/ST_Centroid/ST_DWithin
- rtree.c:270 double 等值改 `<` + tie-breaker
- 图算法 +10：betweenness/closeness/katz/Louvain/三角计数/弱连通/Jaccard/度分布/环路检测/节点相似

## Capabilities

| 能力 | 交付 |
|------|------|
| CSR 并发安全 | 并发遍历+插入压力测试 |
| 崩溃恢复 | kill -9 后 COO 重放，图数据完整 |
| PageRank 精度 | 对拍 NetworkX（容差 1e-6） |
| 球面距离 | geography 类型两点距离与 Haversine 公式一致 |
| ST_* 函数 | 10 个核心函数 + 几何测试用例 |
| 图算法 | 17 个（7+10） |

## Impact

- 修改：graph_csr.c、graph_algorithms.c、spatial_geo.c、rtree.c、spatial_engine.c
- 新增：st_functions.c、graph_algorithms_ext.c、geography 类型
- 预计 8-10 个 commit
- 依赖：C0-1、C0-2
