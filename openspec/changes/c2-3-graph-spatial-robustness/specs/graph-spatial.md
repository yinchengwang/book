# Graph/Spatial 球面与 ST 函数规范（新增）

## 目的

消除 Graph 持久化恢复、Spatial 球面距离、ST_* 函数空缺等缺陷。

## 要求

### REQ-1：PageRank 悬挂节点

`pagerank_new` 必须实现标准悬挂节点质量再分配：迭代前计算无出边节点质量之和，每次迭代按 1/N 分配到所有节点。

### REQ-2：CSR 双视图

CSR 存储支持遍历（持旧快照）与写入（COO 缓冲 + compact 重建）并发。压力测试 4 reader + 1 writer 无死锁。

### REQ-3：fsync + COO 重放

`graph_csr_save` 必须 fsync（`db_fsync`）；启动时若 CSR 文件存在但有未 compact 的 COO 记录，按顺序重放。

### REQ-4：geography 类型

`geography_t` 携带经纬度（lat/lon double）；提供 Haversine 与 Vincenty 距离函数；度↔米转换（lat 1°≈111000 m）。

### REQ-5：ST_* 核心 10 函数

`ST_Distance / ST_Within / ST_Intersects / ST_Contains / ST_Buffer / ST_Union / ST_Area / ST_Length / ST_Centroid / ST_DWithin` 自研实现（基础 GIS 能力）。
