# 多模态数据库能力补齐方案（2026 版）

> 基于 `docs/multimodal-db-comparison-2026.md` 的 Gap 分析，设计分阶段实现方案。
> 覆盖 P0/P1/P2/P3 所有 Gap，按依赖关系排序。

---

## 一、核心依赖关系图

```
基础设施层（无依赖）
├── P0-1: MVCC 事务系统          ← 所有需要 ACID 的模态都依赖
├── P0-2: Raft 共识协议           ← 分布式分片的前置
└── P0-3: Binary Quantization    ← 向量模块内部优化，无外部依赖

基础设施层（有依赖）
├── P1-1: 分布式分片              ← 依赖 MVCC + Raft
├── P1-2: Cypher 查询语言         ← 独立，可先行
├── P1-3: 窗口函数 + CTE          ← 独立，可先行
├── P1-4: 图分析算法库            ← 独立，可先行
└── P1-5: 物化视图框架            ← 依赖窗口函数

功能增强层
├── P2-1: GPU 向量加速            ← 独立（CUDA）
├── P2-2: 聚合管道（文档）         ← 独立
└── P2-3: 空间分析函数            ← 独立

生态建设层
├── P3-1: DiskANN 磁盘索引        ← 依赖分布式分片
├── P3-2: GraphRAG 集成           ← 依赖 Cypher + 向量
├── P3-3: YANG/NETCONF 标准       ← 独立
└── P3-4: Column Family（KV）     ← 依赖 MVCC
```

---

## 二、实现阶段规划

### 阶段 1：基础设施先行（M1-M2，优先级 P0）

#### P0-1：MVCC 事务系统（4 周）

**为什么最优先**：KV/Graph/Relational 三个模态都依赖 ACID 事务，是分布式分片的必要基础。

**设计要点**：
```
核心组件：
1. Write-Ahead Log（WAL）扩展
   - 已有 db/wal.h，需扩展支持 MVCC 日志条目
   - 新增：事务开始/提交/回滚日志类型
   - 新增：多版本数据项的 Undo Log

2. 版本链管理
   - 每条数据记录附加版本号（递增的 transaction_id）
   - 新增版本：append-only，不修改原记录
   - 读取：根据 read_ts 选择可见版本
   - 清理：VACUUM 回收旧版本（参考 PostgreSQL）

3. 事务管理器
   - 全局事务计数器（txn_id）
   - 事务状态：Active/Committed/Aborted
   - 冲突检测：SSI（Serializable Snapshot Isolation）
   - 死锁检测：等待图 + 超时机制

4. 各模态适配
   - KV：所有写操作经过事务管理器
   - Graph：顶点/边操作支持事务
   - Relational：SQL 执行器集成 MVCC
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/txn.h` | 事务管理器接口 |
| `engineering/src/db/core/txn.c` | MVCC 实现（SSI） |
| `engineering/src/db/core/mvcc.c` | 版本链管理 |
| `engineering/src/db/core/undo.c` | Undo Log 管理 |

**验收标准**：
- [ ] 100 个并发事务无数据丢失
- [ ] 事务隔离级别：Read Committed / Repeatable Read / Serializable 可选
- [ ] 事务回滚后数据一致
- [ ] VACUUM 正确回收旧版本

**风险**：SSI 实现复杂度高，可降级为乐观 CAS（类似 FoundationDB）

---

#### P0-2：Raft 共识协议（4 周）

**为什么独立于 MVCC**：Raft 是分布式一致性的基础，可以和 MVCC 并行实现。

**设计要点**：
```
核心组件：
1. Raft 节点角色
   - Leader / Follower / Candidate
   - 选举机制：随机超时 + 多数票
   - 心跳：Leader 定期发送心跳维持权威

2. 日志复制
   - 日志条目：index + term + data
   - AppendEntries RPC：日志复制 + 心跳
   - 多数节点确认后提交

3. 持久化
   - Raft Log 持久化（WAL 兼容）
   - 快照：定期压缩日志，减少恢复时间

4. 成员变更
   - Joint Consensus 方案
   - 单节点增删（简化版）

5. 各模态适配
   - 存储层：Raft 复制单元（Partition/Collection）
   - 计算层：请求路由到 Leader
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/raft.h` | Raft 接口 |
| `engineering/src/db/core/raft/raft.c` | Raft 核心算法 |
| `engineering/src/db/core/raft/rpc.c` | Raft RPC |
| `engineering/src/db/core/raft/snapshot.c` | 快照管理 |
| `engineering/src/db/core/raft/membership.c` | 成员变更 |

**验收标准**：
- [ ] 3 节点集群，1 节点故障后自动恢复
- [ ] 写操作多数节点确认后成功
- [ ] Leader 切换时间 < 5 秒
- [ ] 日志复制正确（无数据丢失）

**风险**：Raft 实现复杂度高，建议参考 etcd Raft 库

---

#### P0-3：Binary Quantization（2 周）

**为什么独立**：向量模块内部优化，不影响其他模块，可最先实现。

**设计要点**：
```
核心组件：
1. 二值化量化
   - 阈值：均值 / 中位数 / 学习阈值
   - 位向量存储：每维 1 bit
   - Hamming 距离：bit 异或 + popcount

2. 距离度量扩展
   - 新增 METRIC_HAMMING 支持
   - 优化 popcount（AVX2 POPCNTQ）

3. 索引扩展
   - BinaryIVF：倒排文件 + 二值向量
   - BinaryHNSW：HNSW + 二值向量
   - BQ + HNSW 组合：内存 32x 压缩

4. API 扩展
   - mmdb_vector_quantize(bq_float32_to_binary)
   - mmdb_vector_search(metric=MMDB_METRIC_HAMMING)
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/vector/bq.h` | BQ 接口 |
| `engineering/src/db/vector/bq.c` | 二值化量化实现 |
| `engineering/test/db/vector/bq_test.cpp` | BQ 测试 |

**验收标准**：
- [ ] 内存压缩率：~32x（float32 → bit）
- [ ] 召回率：@10K 数据集 ≥ 0.90
- [ ] 与现有 HNSW/IVF 索引兼容

---

### 阶段 2：功能补齐（M3-M4，优先级 P1）

#### P1-1：分布式分片（4 周）

**前置条件**：P0-1（MVCC）+ P0-2（Raft）

**设计要点**：
```
核心组件：
1. 分片策略
   - Hash 分片：key % num_shards
   - Range 分片：key 范围区间
   - 模态可选：Vector（Hash）/ Graph（Hash）/ KV（Range）

2. 分片路由
   - Router 层：接收请求 → 解析 key → 路由到目标分片
   - 请求合并：多分片结果聚合

3. 分片迁移
   - 在线扩容：数据重平衡
   - 迁移协议：源节点 → 目标节点 → 元数据更新

4. 各模态适配
   - Vector：按 collection_id 分片
   - Graph：按顶点 ID 分片（边跟随主顶点）
   - KV：按 key 哈希分片
   - Relational：按行 ID 分片
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/shard.h` | 分片接口 |
| `engineering/src/db/core/shard/router.c` | 路由层 |
| `engineering/src/db/core/shard/placement.c` | 分片调度 |
| `engineering/src/db/core/shard/migration.c` | 迁移协议 |

**验收标准**：
- [ ] 3 分片集群，均匀分布
- [ ] 单分片故障不影响其他分片
- [ ] 扩容后数据重平衡正确

---

#### P1-2：Cypher 查询语言（4 周）

**为什么独立**：图数据库标准 API，不依赖 MVCC，可最先实现。

**设计要点**：
```
核心组件：
1. 词法/语法分析
   - 参考 openCypher 语法（EBNF）
   - 使用 Flex/Bison 实现
   - 关键语句：MATCH, WHERE, RETURN, WITH, OPTIONAL MATCH, UNION

2. 语义分析
   - AST → 逻辑计划
   - 变量绑定 / 类型推导
   - 模式匹配（Pattern Matching）

3. 查询优化
   - 索引下推：WHERE age > 30 → IndexScan
   - 边过滤早执行
   - 路径长度限制

4. 执行引擎
   - Volcano 迭代器适配
   - 算子：NodeScan, EdgeScan, Filter, Project, Aggregate
   - 结果返回：Cypher → JSON / CSV

5. 语法示例
   MATCH (a:Person)-[r:KNOWS]->(b:Person)
   WHERE a.age > 30 AND b.city = 'Beijing'
   RETURN a.name, b.name, r.since
   ORDER BY a.name
   LIMIT 10
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/parser/cypher.h` | Cypher 语法接口 |
| `engineering/src/db/parser/cypher.l` | Flex 词法 |
| `engineering/src/db/parser/cypher.y` | Bison 语法 |
| `engineering/src/db/parser/cypher_semantic.c` | 语义分析 |
| `engineering/src/db/executor/cypher_exec.c` | Cypher 执行器 |
| `engineering/test/db/graph/cypher_test.cpp` | Cypher 测试 |

**验收标准**：
- [ ] 支持 MATCH/WHERE/RETURN/WITH/OPTIONAL MATCH/UNION
- [ ] 支持聚合：COUNT, SUM, AVG, MIN, MAX
- [ ] 支持子查询和 WITH 管道
- [ ] 与现有 Graph API（CSR/COO）无缝集成

**风险**：Cypher 语法复杂，建议参考 Memgraph 的解析器实现

---

#### P1-3：窗口函数 + CTE（3 周）

**设计要点**：
```
窗口函数支持：
- 聚合窗口：SUM, AVG, COUNT, MIN, MAX OVER (PARTITION BY ... ORDER BY ...)
- 排名窗口：ROW_NUMBER, RANK, DENSE_RANK, NTILE
- 偏移窗口：LAG, LEAD, FIRST_VALUE, LAST_VALUE
- 帧定义：ROWS BETWEEN UNBOUNDED PRECEDING AND CURRENT ROW

CTE 支持：
- 非递归 CTE：WITH ... SELECT
- 递归 CTE：WITH RECURSIVE ... （用于树形遍历）

实现路径：
1. 在 expr.c 中新增窗口函数表达式类型
2. 在 planner.c 中添加窗口计划节点
3. 在 nodeWindow.c 中实现窗口算子
4. 在 optimizer.c 中支持窗口下推优化
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/sql/window.h` | 窗口函数接口 |
| `engineering/src/db/sql/nodeWindow.c` | 窗口算子 |
| `engineering/src/db/sql/cte.c` | CTE 处理 |
| `engineering/test/db/sql/window_test.cpp` | 窗口函数测试 |

**验收标准**：
- [ ] 支持 OVER (PARTITION BY / ORDER BY)
- [ ] 支持 ROWS/RANGE 帧
- [ ] 支持 WITH 递归（树形遍历）
- [ ] 与现有 SQL 执行器兼容

---

#### P1-4：图分析算法库（4 周）

**设计要点**：
```
算法分类（按优先级）：

核心算法（先行实现）：
1. 遍历算法
   - BFS（已实现）/ DFS（已实现）
   - 单源最短路径：Dijkstra / Bellman-Ford
   - 全源最短路径：Floyd-Warshall

2. 中心性算法
   - PageRank（已实现）
   - Degree Centrality
   - Betweenness Centrality（Brandess 算法）

3. 社区发现
   - Label Propagation
   - Louvain 算法
   - Connected Components（强/弱连通分量）

高级算法（后期实现）：
4. 图嵌入
   - Node2Vec
   - DeepWalk

5. 链接预测
   - Common Neighbors
   - Adamic-Adar

实现架构：
- 算法库独立：libgraph_algo.a
- 统一接口：graph_algo_execute(ctx, algo, params)
- 图存储适配层：CSR/COO → 算法输入格式
```

**新增文件**：
| 文件 | 说明 |
|------|------|
| `engineering/include/db/graph/algo.h` | 算法接口 |
| `engineering/src/db/graph/algo/traversal.c` | 最短路径 |
| `engineering/src/db/graph/algo/centrality.c` | 中心性算法 |
| `engineering/src/db/graph/algo/community.c` | 社区发现 |
| `engineering/test/db/graph/algo_test.cpp` | 算法测试 |

**验收标准**：
- [ ] Dijkstra 最短路径正确
- [ ] PageRank 收敛（误差 < 1e-6）
- [ ] Louvain 社区发现输出正确
- [ ] 算法性能：100K 顶点图 < 10 秒

---

#### P1-5：物化视图框架（2 周）

**前置条件**：P1-3（窗口函数）

**设计要点**：
```
核心组件：
1. 物化视图定义
   - CREATE MATERIALIZED VIEW mv1 AS SELECT ...

2. 刷新策略
   - 完整刷新：TRUNCATE + 重新计算
   - 增量刷新：基于增量数据更新
   - 定时刷新：cron 调度

3. 时序物化视图
   - 持续聚合：Sliding Window Aggregate
   - 时间分区：按天/周/月 物化

4. 查询重写
   - 自动识别可下推的查询
   - 重写为物化视图查询

实现路径：
1. 在 catalog 中注册物化视图
2. 执行器计算物化视图结果
3. 存储到独立表/Collection
4. 查询重写器拦截并重定向
```

---

### 阶段 3：性能飞跃（M5-M6，优先级 P2）

#### P2-1：GPU 向量加速（4 周）

**设计要点**：
```
支持索引：
- GPU_IVF_FLAT：暴力搜索，批量处理
- GPU_IVF_PQ：IVF + PQ，内存节省
- GPU_HNSW：HNSW 层间并行

CUDA 实现：
1. 内存管理
   - CUDA Unified Memory
   - Host <-> Device 数据传输

2. 距离计算
   - L2：cuBLAS / 自实现
   - IP/Cosine：向量点积
   - popcount：__popcntll（BBH）

3. IVF 搜索
   - 并行遍历多个倒排列表
   - 归并排序 Top-K

4. HNSW 搜索
   - 并行探索多个层
   - 并行候选队列

API 扩展：
- mmdb_vector_search_gpu(ctx, req, algo=GPU_IVF_PQ)
- mmdb_vector_build_gpu(ctx, algo=GPU_HNSW)
```

**验收标准**：
- [ ] GPU_IVF_PQ: 召回率 @10K ≥ 0.95
- [ ] GPU 搜索 QPS ≥ 50K（A100）
- [ ] 与 CPU 索引结果一致

---

#### P2-2：文档聚合管道（3 周）

**设计要点**：
```
管道阶段（参考 MongoDB Aggregation Pipeline）：
$match    → 过滤文档
$project  → 投影字段
$group    → 分组聚合
$sort     → 排序
$limit    → 限制数量
$skip     → 跳过
$unwind   → 展开数组
$lookup   → 关联查询（类似 JOIN）
$bucket   → 分桶
$facet    → 多管道并行

实现路径：
1. 新增 Aggregator 算子
2. 支持管道阶段解析
3. 优化：管道下推（$match 早执行）
4. 支持 $lookup 跨 Collection 查询
```

**验收标准**：
- [ ] 支持 $match/$project/$group/$sort/$limit/$skip
- [ ] 支持 $unwind 展开数组
- [ ] 支持 $lookup 跨 Collection JOIN
- [ ] 管道下推优化生效

---

#### P2-3：空间分析函数（3 周）

**设计要点**：
```
空间函数补齐（PostGIS ST_* 子集）：

几何构造：
- ST_Point(x, y)
- ST_LineString(points)
- ST_Polygon(ring)
- ST_Multi* 族

几何关系：
- ST_Intersects(a, b)
- ST_Contains(a, b)
- ST_Within(a, b)
- ST_Equals(a, b)
- ST_Touches(a, b)
- ST_Overlaps(a, b)

几何测量：
- ST_Distance(a, b)
- ST_Area(polygon)
- ST_Length(line)
- ST_Centroid(polygon)

几何操作：
- ST_Buffer(point, radius)
- ST_Union(a, b)
- ST_Intersection(a, b)
- ST_Difference(a, b)
- ST_ConvexHull(points)

坐标系统扩展：
- 新增 geography 类型（球面坐标）
- Haversine 距离计算
- ST_DWithin 支持球面距离
```

---

### 阶段 4：生态建设（M7+，优先级 P3）

#### P3-1：DiskANN 磁盘索引（4 周）

**前置条件**：P1-1（分布式分片）

**设计要点**：
```
核心组件：
1. Vamana 图索引
   - 贪心搜索 + 剪枝
   - 参数：Alpha（搜索半径）/ L（层数）

2. PQ 量化
   - 已有 mmdb_vector_quantize PQ
   - 优化：OPQ（方向对齐）

3. 磁盘存储
   - PQ 码本 + 原始向量 → SSD
   - mmap 映射，按需加载
   - 缓存：热点页面在内存

4. 过滤索引
   - Filtered Vamana
   - 元数据索引 + 向量索引 联动

性能目标：
- 十亿级向量（100GB+）
- SSD 搜索延迟 < 10ms
- 召回率 @1M ≥ 0.95
```

---

#### P3-2：GraphRAG 集成（3 周）

**前置条件**：P1-2（Cypher）+ 向量引擎

**设计要点**：
```
GraphRAG 架构：
1. 图构建
   - 从文档提取实体（NER）
   - 从文档提取关系（RE）
   - 存储到 Graph 引擎（CSR/COO）

2. 向量索引
   - 实体嵌入 → Vector 引擎
   - 关系嵌入 → Vector 引擎

3. 混合检索
   - 向量检索：找到相似实体
   - 图检索：扩展邻居实体（Cypher）
   - 融合排序：RRF / 加权

4. 答案生成
   - 上下文组装
   - LLM 调用（外部）
```

---

#### P3-3：YANG/NETCONF 标准（3 周）

**设计要点**：
```
实现 RFC 6020/7950 YANG 1.1：
1. YANG 解析器
   - 类型系统（int/uint/string/leafref/identity）
   - 路径表达式（XPath）
   - 数据验证

2. NETCONF 协议
   - RPC：get/get-config/edit-config
   - 通知：create/update/delete

3. 树形存储适配
   - 扩展现有 yang_engine
   - 支持 y Filters
   - 支持 operation attributes
```

---

#### P3-4：Column Family（KV）（2 周）

**前置条件**：P0-1（MVCC）

**设计要点**：
```
类似 RocksDB Column Family：
- 每个 Column Family 独立 WAL
- 独立 compaction 策略
- 独立 TTL 设置

使用场景：
- 多租户隔离（每个租户一个 CF）
- 冷热数据分离（热数据 CF + 冷数据 CF）
```

---

## 三、依赖关系汇总表

| 任务 | 前置依赖 | 独立程度 | 工期 |
|------|---------|---------|------|
| P0-1 MVCC | 无 | **完全独立** | 4 周 |
| P0-2 Raft | 无 | **完全独立** | 4 周 |
| P0-3 BQ | 无 | **完全独立** | 2 周 |
| P1-1 分片 | P0-1 + P0-2 | 强依赖 | 4 周 |
| P1-2 Cypher | 无 | **完全独立** | 4 周 |
| P1-3 窗口函数 | 无 | **完全独立** | 3 周 |
| P1-4 图算法 | 无 | **完全独立** | 4 周 |
| P1-5 物化视图 | P1-3 | 中等依赖 | 2 周 |
| P2-1 GPU 加速 | 无 | **完全独立** | 4 周 |
| P2-2 聚合管道 | 无 | **完全独立** | 3 周 |
| P2-3 空间函数 | 无 | **完全独立** | 3 周 |
| P3-1 DiskANN | P1-1 | 强依赖 | 4 周 |
| P3-2 GraphRAG | P1-2 | 中等依赖 | 3 周 |
| P3-3 YANG/NETCONF | 无 | **完全独立** | 3 周 |
| P3-4 Column Family | P0-1 | 弱依赖 | 2 周 |

---

## 四、最优并行路径

根据依赖关系，可按以下路径并行实施：

```
第 1 批（完全独立，可并行）：
├── P0-1 MVCC（4 周）
├── P0-2 Raft（4 周）
└── P0-3 BQ（2 周）
    ├── P1-2 Cypher（4 周）
    ├── P1-3 窗口函数（3 周）
    ├── P1-4 图算法（4 周）
    └── P2-1 GPU（4 周）
        ├── P2-2 聚合管道（3 周）
        ├── P2-3 空间函数（3 周）
        └── P3-3 YANG/NETCONF（3 周）
            ├── P1-1 分片（4 周）
            ├── P1-5 物化视图（2 周）
            ├── P3-2 GraphRAG（3 周）
            └── P3-4 Column Family（2 周）
                └── P3-1 DiskANN（4 周）
```

**理论最短工期**：~18 周（P0-1/P0-2/P0-3 并行 4 周 + 后续串行）

**推荐工期**：~30 周（考虑测试和调试缓冲）

---

## 五、风险与缓解

| 风险 | 概率 | 影响 | 缓解方案 |
|------|------|------|---------|
| MVCC SSI 实现复杂 | 高 | 高 | 降级为乐观 CAS（FoundationDB 风格） |
| Raft 正确性难以保证 | 高 | 高 | 参考 etcd Raft 库实现 |
| Cypher 语法覆盖不全 | 中 | 中 | 分阶段实现，MVP 仅支持核心语句 |
| GPU 兼容性问题 | 中 | 中 | 提供 CPU fallback |
| DiskANN 性能不达预期 | 中 | 中 | 先实现内存版，再加磁盘支持 |

---

## 六、OpenSpec 变更任务

基于上述方案，创建以下 OpenSpec 变更任务：

| 任务 | 说明 | 阶段 |
|------|------|------|
| T-M1 | P0-1 MVCC 事务系统 | M1 |
| T-M2 | P0-2 Raft 共识协议 | M1 |
| T-M3 | P0-3 Binary Quantization | M1 |
| T-M4 | P1-1 分布式分片 | M2 |
| T-M5 | P1-2 Cypher 查询语言 | M2 |
| T-M6 | P1-3 窗口函数 + CTE | M2 |
| T-M7 | P1-4 图分析算法库 | M2 |
| T-M8 | P2-1 GPU 向量加速 | M3 |
| T-M9 | P2-2 聚合管道 | M3 |
| T-M10 | P3-1 DiskANN 磁盘索引 | M4 |

---

*文档版本: 2026-08-25*
*基于: `docs/multimodal-db-comparison-2026.md` Gap 分析*
