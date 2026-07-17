# 任务列表：mmdb-htap-evolution

> 多模态数据库 HTAP 演进计划
> 预估周期：43-56 周（11-14 个月）

---

## Phase 1: 核心基础设施增强 (4-6 周)

### 1.1 MVCC 多版本并发控制

- [x] 1.1.1 设计并实现 Tuple Header 扩展（xmin/xmax/ctid 字段）
- [x] 1.1.2 实现 TransactionId 分配器（递增分配）
- [x] 1.1.3 实现事务状态表管理（Active/Committed/Aborted）
- [x] 1.1.4 实现 ReadView 结构体（xmin/xmax/active_txs）
- [x] 1.1.5 实现 HeapTupleSatisfiesMVCC 可见性判断函数
- [x] 1.1.6 实现 READ COMMITTED 隔离级别（每个语句刷新 ReadView）
- [x] 1.1.7 实现 REPEATABLE READ 隔离级别（事务开始生成 ReadView）
- [x] 1.1.8 实现 SERIALIZABLE 隔离级别（谓词锁 + SSI 检测）
- [x] 1.1.9 实现 MVCC 与 WAL 集成（记录版本信息）
- [x] 1.1.10 实现 VACUUM 垃圾回收（过期版本清理 + Freeze）

### 1.2 锁管理器升级

- [x] 1.2.1 设计 LockTag 结构（relfilenode/blocknum/offset）
- [x] 1.2.2 实现行级锁获取（SHARE/EXCLUSIVE 模式）
- [x] 1.2.3 实现锁队列管理（FIFO 顺序）
- [x] 1.2.4 实现死锁检测（Wait-For Graph 环检测）
- [x] 1.2.5 实现锁超时回滚机制
- [x] 1.2.6 实现谓词锁（KeyRange 锁用于 SSI）
- [x] 1.2.7 实现 SIREAD 跟踪集合
- [x] 1.2.8 实现 GUC 参数（lock_timeout, deadlock_timeout）

### 1.3 数据完整性增强

- [x] 1.3.1 设计 Page Header checksum 字段
- [x] 1.3.2 实现写盘时 checksum 计算
- [x] 1.3.3 实现读盘时 checksum 校验
- [x] 1.3.4 统一各引擎的损坏检测接口
- [x] 1.3.5 实现 OPQ/RTree 损坏检测（已有部分）
- [x] 1.3.6 实现向量引擎损坏检测
- [x] 1.3.7 实现时序引擎损坏检测
- [x] 1.3.8 实现 WAL 回放一致性验证
- [x] 1.3.9 实现从 WAL 重建损坏页面的自修复机制

### 1.4 物化视图完善

- [x] 1.4.1 设计物化视图依赖图结构
- [x] 1.4.2 实现增量刷新机制（CDC 变更捕获）
- [x] 1.4.3 实现定时全量刷新调度器
- [x] 1.4.4 实现多视图链式依赖（拓扑排序 + 级联刷新）
- [x] 1.4.5 实现物化视图查询改写（自动识别可复用场景）
- [x] 1.4.6 编写 Phase 1 单元测试和集成测试

---

## Phase 2: SQL 层 + 网络协议 (6-8 周)

### 2.1 SQL 解析器

- [x] 2.1.1 实现词法分析器（Token 识别、位置跟踪）
- [x] 2.1.2 实现关键字表（SELECT, FROM, WHERE 等）
- [x] 2.1.3 实现递归下降语法分析器（SELECT/INSERT/UPDATE/DELETE）
- [x] 2.1.4 实现 DDL 语法解析（CREATE/DROP/ALTER TABLE）
- [x] 2.1.5 实现 JOIN 语法解析（INNER/LEFT/RIGHT/FULL）
- [x] 2.1.6 实现聚合语法解析（GROUP BY, HAVING, 窗口函数）
- [x] 2.1.7 实现子查询解析（标量/EXISTS/IN）
- [x] 2.1.8 实现语义分析器（表/列引用解析、类型推导）
- [x] 2.1.9 实现错误恢复机制

### 2.2 查询计划器

- [x] 2.2.1 设计逻辑算子（LogicalScan/Join/Agg/Project）
- [x] 2.2.2 实现 AST 到逻辑计划的转换
- [x] 2.2.3 设计物理算子（PhysSeqScan/IndexScan/HashJoin）
- [x] 2.2.4 实现逻辑计划到物理计划的转换
- [x] 2.2.5 实现统计信息收集（rowcount, ndistinct, histogram）
- [x] 2.2.6 实现代价估算函数
- [x] 2.2.7 实现谓词下推优化
- [x] 2.2.8 实现列裁剪优化
- [x] 2.2.9 实现连接顺序优化
- [x] 2.2.10 实现物化视图改写
- [x] 2.2.11 实现向量索引下推

### 2.3 查询执行器

- [x] 2.3.1 实现 Volcano 迭代器模型基类
- [x] 2.3.2 实现 SeqScan 算子
- [x] 2.3.3 实现 IndexScan 算子
- [x] 2.3.4 实现 Filter 算子
- [x] 2.3.5 实现 Projection 算子
- [x] 2.3.6 实现 HashJoin 算子
- [x] 2.3.7 实现 NestedLoopJoin 算子
- [x] 2.3.8 实现 HashAgg 算子
- [x] 2.3.9 实现 SortAgg 算子
- [x] 2.3.10 实现 Insert/Update/Delete 算子
- [x] 2.3.11 实现表达式求值引擎
- [x] 2.3.12 实现结果集管理

### 2.4 PostgreSQL Wire Protocol

- [x] 2.4.1 实现协议框架和状态机
- [x] 2.4.2 实现 StartupMessage 握手
- [x] 2.4.3 实现简单查询协议（Query 消息）
- [x] 2.4.4 实现 RowDescription/DataRow 消息
- [x] 2.4.5 实现 CommandComplete 消息
- [x] 2.4.6 实现扩展查询协议（Parse/Bind/Execute/Sync）
- [x] 2.4.7 实现类型系统（OID 映射、二进制编码）
- [x] 2.4.8 实现事务命令（BEGIN/COMMIT/ROLLBACK）
- [x] 2.4.9 实现 ErrorResponse 消息
- [x] 2.4.10 实现与 psql 客户端的兼容性测试

### 2.5 REST API

- [x] 2.5.1 实现轻量级 HTTP 服务器
- [x] 2.5.2 实现路由分发机制
- [x] 2.5.3 实现 POST /api/v1/query SQL 查询接口
- [x] 2.5.4 实现 GET /api/v1/collections 集合管理
- [x] 2.5.5 实现 POST /api/v1/collections 创建集合
- [x] 2.5.6 实现 DELETE /api/v1/collections/{name} 删除集合
- [x] 2.5.7 实现 POST /api/v1/vectors/{collection}/search 向量搜索
- [x] 2.5.8 实现 GET /api/v1/health 健康检查
- [x] 2.5.9 实现 GET /api/v1/metrics 指标查询

### 2.6 跨语言 SDK

- [x] 2.6.1 设计 Python SDK 连接管理
- [x] 2.6.2 实现 Python Cursor 和查询执行
- [x] 2.6.3 实现 Python 类型映射（int/str/float/list/dict）
- [x] 2.6.4 实现 Python 上下文管理器
- [x] 2.6.5 实现 Python 向量操作接口
- [x] 2.6.6 设计 JDBC Driver 架构
- [x] 2.6.7 实现 JDBC Connection 和 Statement
- [x] 2.6.8 实现 JDBC PreparedStatement
- [x] 2.6.9 实现 JDBC ResultSet 和 ResultSetMetaData
- [x] 2.6.10 编写 SDK 文档和示例代码

---

## Phase 3: 向量模型增强 (4-6 周)

### 3.1 DiskANN 索引

- [x] 3.1.1 研究 NSG 图构建算法
- [x] 3.1.2 实现 NSG 图构建器
- [x] 3.1.3 实现 DiskANN 搜索算法（贪婪搜索 + 图遍历）
- [x] 3.1.4 设计磁盘驻留向量存储格式
- [x] 3.1.5 实现 mmap 内存映射访问
- [x] 3.1.6 实现异步 I/O 预取
- [x] 3.1.7 实现索引持久化和加载
- [x] 3.1.8 实现增量索引更新

### 3.2 混合检索能力

- [x] 3.2.1 设计混合检索接口
- [x] 3.2.2 实现 Pre-filter 策略
- [x] 3.2.3 实现 Post-filter 策略
- [x] 3.2.4 实现自适应策略选择
- [x] 3.2.5 实现 VECTOR_SEARCH() SQL 函数
- [x] 3.2.6 实现丰富的过滤条件语法
- [x] 3.2.7 优化混合检索性能

### 3.3 量化增强

- [x] 3.3.1 实现 Scalar Quantization (SQ)
- [x] 3.3.2 实现 OPQ (Optimized Product Quantization)
- [x] 3.3.3 实现自适应量化参数选择
- [x] 3.3.4 集成量化与 DiskANN

### 3.4 分层索引

- [x] 3.4.1 设计分层索引策略
- [x] 3.4.2 实现 HNSW 内存索引（已有部分）
- [x] 3.4.3 实现 IVF-PQ SSD 索引
- [x] 3.4.4 实现 IVF 对象存储索引
- [x] 3.4.5 实现自动分层切换

---

## Phase 4: 时序模型增强 (4-6 周)

### 4.1 列式存储引擎

- [x] 4.1.1 设计时序列式页面格式
- [x] 4.1.2 实现时间分区管理
- [x] 4.1.3 实现 Delta 编码压缩
- [x] 4.1.4 实现 RLE 压缩
- [x] 4.1.5 实现 Gorilla 压缩算法
- [x] 4.1.6 实现 Bit-packing 压缩
- [x] 4.1.7 实现列式读取优化（块跳过、批量读取）

### 4.2 时序专用索引

- [x] 4.2.1 实现时间索引
- [x] 4.2.2 实现 Tag 倒排索引
- [x] 4.2.3 实现降采样索引

### 4.3 时序 SQL 函数

- [x] 4.3.1 实现 TIME_SERIES_AGG() 函数
- [x] 4.3.2 实现 FIRST/LAST 函数
- [x] 4.3.3 实现 RATE/DERIVATIVE 函数
- [x] 4.3.4 实现 HISTOGRAM/BUCKET 函数
- [x] 4.3.5 实现时间函数（DATE_TRUNC, EXTRACT）

### 4.4 连续聚合

- [x] 4.4.1 设计连续聚合架构
- [x] 4.4.2 实现变更数据捕获（CDC）
- [x] 4.4.3 实现增量计算
- [x] 4.4.4 实现自动降采样策略
- [x] 4.4.5 实现查询改写（自动使用物化视图）

---

## Phase 5: 图模型增强 (4-6 周)

### 5.1 属性图支持

- [x] 5.1.1 设计属性图数据模型
- [x] 5.1.2 实现顶点属性存储
- [x] 5.1.3 实现边属性存储
- [x] 5.1.4 实现标签系统
- [x] 5.1.5 实现图 Schema 定义和验证

### 5.2 图查询语言

- [x] 5.2.1 设计 Cypher 解析器
- [x] 5.2.2 实现 MATCH 子句
- [x] 5.2.3 实现 RETURN 子句
- [x] 5.2.4 实现 WHERE 子句
- [x] 5.2.5 实现可变边模式（*1..3, *）
- [x] 5.2.6 实现 GRAPH_TRAVERSE() SQL 扩展函数
- [x] 5.2.7 实现 GRAPH_SHORTEST_PATH() 函数

### 5.3 图索引

- [x] 5.3.1 实现标签索引
- [x] 5.3.2 实现属性索引
- [x] 5.3.3 实现向量属性索引

### 5.4 图算法库

- [x] 5.4.1 实现 BFS/DFS 遍历
- [x] 5.4.2 实现 Dijkstra 最短路径
- [x] 5.4.3 实现 PageRank 算法
- [x] 5.4.4 实现度中心性
- [x] 5.4.5 实现 Louvain 社区检测
- [x] 5.4.6 实现 GRAPH_PAGERANK() SQL 函数

---

## Phase 6: 文档模型增强 (4-6 周)

### 6.1 嵌套文档支持

- [x] 6.1.1 实现嵌套对象存储
- [x] 6.1.2 实现数组存储
- [x] 6.1.3 增强 JSONPath 查询（支持通配符和谓词）
- [x] 6.1.4 实现嵌套字段索引
- [x] 6.1.5 实现嵌套文档部分更新

### 6.2 文档向量检索

- [x] 6.2.1 设计文档向量字段
- [x] 6.2.2 实现文档嵌入存储
- [x] 6.2.3 实现语义搜索
- [x] 6.2.4 实现 BM25 + 向量混合检索
- [x] 6.2.5 实现分数加权融合和 RRF 融合

### 6.3 文档聚合框架

- [x] 6.3.1 实现词条聚合（term_agg）
- [x] 6.3.2 实现范围聚合（range_agg）
- [x] 6.3.3 实现直方图聚合
- [x] 6.3.4 实现统计聚合（stats_agg）
- [x] 6.3.5 实现百分位数聚合
- [x] 6.3.6 实现管道聚合

### 6.4 全文检索增强

- [x] 6.4.1 设计分词器插件接口
- [x] 6.4.2 实现默认分词器
- [x] 6.4.3 实现短语搜索和高亮
- [x] 6.4.4 实现同义词和停用词

---

## Phase 7: 空间 + KV + 树模型增强 (3-4 周)

### 7.1 空间模型增强

- [x] 7.1.1 实现几何对象类型（Point/Linestring/Polygon）
- [x] 7.1.2 实现 WKT 解析和序列化
- [x] 7.1.3 实现 ST_Distance 和 ST_Distance_Sphere
- [x] 7.1.4 实现 ST_Contains 和 ST_Within
- [x] 7.1.5 实现 ST_Intersects 和 ST_Buffer
- [x] 7.1.6 实现 QuadTree 索引

### 7.2 KV 模型增强

- [x] 7.2.1 实现有序集合数据结构
- [x] 7.2.2 实现 ZADD/ZRANGEBYSCORE/ZSCORE
- [x] 7.2.3 实现 ZUNIONSTORE/ZINTERSTORE
- [x] 7.2.4 实现毫秒级 TTL

### 7.3 树模型增强

- [x] 7.3.1 实现 TREE_ANCESTORS/TREE_DESCENDANTS
- [x] 7.3.2 实现 TREE_DEPTH/TREE_PATH
- [x] 7.3.3 实现 XPath/JSONPath 查询
- [x] 7.3.4 实现 TREE_LEVEL_PATH 函数

---

## Phase 8: 跨模型联合查询 + OLAP 增强 (6-8 周)

### 8.1 跨模型查询优化器

- [x] 8.1.1 设计跨模型执行计划结构
- [x] 8.1.2 实现异构模型 JOIN
- [x] 8.1.3 实现 Exchange 算子
- [x] 8.1.4 实现跨模型谓词下推
- [x] 8.1.5 实现跨模型列裁剪
- [x] 8.1.6 实现小表广播优化

### 8.2 向量化执行引擎

- [x] 8.2.1 设计列块数据结构
- [x] 8.2.2 实现 SIMD 距离计算（SSE/AVX）
- [x] 8.2.3 实现 SIMD 过滤
- [x] 8.2.4 实现向量化 Scan 算子
- [x] 8.2.5 实现向量化 HashJoin
- [x] 8.2.6 实现向量化 Aggregation

### 8.3 通用列式存储

- [x] 8.3.1 设计列存格式（Parquet 风格）
- [x] 8.3.2 实现 Dictionary/RLE/Bit-packing 压缩
- [x] 8.3.3 实现 MinMax 索引
- [x] 8.3.4 实现行写列读混合存储
- [x] 8.3.5 实现延迟物化优化

### 8.4 SQL 扩展函数统一化

- [x] 8.4.1 设计函数注册表
- [x] 8.4.2 统一所有模型的函数注册
- [x] 8.4.3 实现自动类型推导和绑定

---

## Phase 9: 分布式演进 (8-12 周)

### 9.1 分片与路由

- [x] 9.1.1 设计分片元数据结构
- [x] 9.1.2 实现 Hash 分片策略
- [x] 9.1.3 实现 Range 分片策略
- [x] 9.1.4 实现分片路由
- [x] 9.1.5 实现分片拓扑管理
- [x] 9.1.6 实现动态扩缩容

### 9.2 分布式事务

- [x] 9.2.1 实现两阶段提交（2PC）协调者
- [x] 9.2.2 实现两阶段提交参与者
- [x] 9.2.3 实现 SAGA 模式
- [x] 9.2.4 设计 TSO (Timestamp Oracle)
- [x] 9.2.5 实现分布式 MVCC
- [x] 9.2.6 实现分布式快照
- [x] 9.2.7 实现故障恢复

### 9.3 Raft 高可用

- [x] 9.3.1 实现 Raft 核心算法（选举、日志复制）
- [x] 9.3.2 实现成员变更（Joint Consensus）
- [x] 9.3.3 实现故障检测和自动切换
- [x] 9.3.4 实现线性一致性读（ReadIndex/LeaseRead）
- [x] 9.3.5 实现快照和恢复

### 9.4 多节点协调

- [x] 9.4.1 实现节点注册和发现
- [x] 9.4.2 实现全局锁服务
- [x] 9.4.3 实现领导者选举
- [x] 9.4.4 实现配置管理
- [x] 9.4.5 实现集群扩缩容

---

## 测试与文档

### 测试

- [x] T.1 Phase 1 单元测试（MVCC、锁）
- [x] T.2 Phase 1 集成测试
- [x] T.3 Phase 2 SQL 层测试
- [x] T.4 Phase 2 pgwire 兼容性测试
- [x] T.5 Phase 3-7 各模型功能测试
- [x] T.6 Phase 8 跨模型查询测试
- [x] T.7 Phase 9 分布式测试
- [x] T.8 性能基准测试

### 文档

- [x] D.1 SQL 语法参考文档
- [x] D.2 API 参考文档
- [x] D.3 部署指南
- [x] D.4 开发者指南
- [x] D.5 性能调优指南

---

## 里程碑

| 里程碑 | 目标时间 | 验收标准 |
|--------|----------|----------|
| M1: 单机 OLTP 基础 | Week 6 | MVCC + 锁 + 损坏检测可用 |
| M2: SQL + 网络协议 | Week 14 | 可用 psql 连接，基础 SQL 可用 |
| M3: 各模型增强 | Week 28 | 向量/时序/图/文档能力达到主流水平 |
| M4: 跨模型 + OLAP | Week 36 | 跨模型查询可用，向量化执行就绪 |
| M5: 分布式就绪 | Week 48 | 高可用和分布式能力可用 |

---

**最后更新**: 2026-07-08 (归档)

---

## Phase 4 完成进度：20/20 ✓

## Phase 5 完成进度：22/22 ✓

## Phase 6 完成进度：20/20 ✓

## Phase 7 完成进度：14/14 ✓

## Phase 8 完成进度：20/20 ✓

## Phase 9 完成进度：23/23 ✓
