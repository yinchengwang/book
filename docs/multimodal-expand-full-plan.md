# 多模态数据库扩展完整计划（2026-08-25 统一版）

> 本文档基于此前讨论内容整理，目标是将“多模态能力扩展 + 统一执行层 + 编译隔离 + 分布式能力 + SQL 兼容策略”合并为一份统一计划。

---

## 一、总体目标

将现有数据库扩展为**可裁剪、可演进的多模态数据库内核**，核心目标包括：

1. 支持所有行业主流数据模态
2. 新增流式数据（Stream）与分析型列存（Columnar）能力
3. 增强组合能力：时空、知识图谱/RDF、稀疏向量
4. 构建**统一执行层**，前端语言可插拔，后端引擎可插拔
5. 实现**编译时模态隔离**，支持小型化构建
6. 引入**独立分布式层**，不做单模态内聚式分布式
7. 明确 SQL 兼容策略：标准内核 + 方言层 + 函数映射

---

## 二、架构原则

### 2.1 分层架构

最终系统按 6 层组织：

1. 接入层（前端语言 / API）
2. 统一执行层
3. 分布式层
4. 引擎适配层
5. 存储层
6. 公共基础设施层

### 2.2 可插拔原则

- 前端语言可插拔：SQL、PostgreSQL 方言、MySQL 方言、GQL/Cypher、Stream API、分析 SQL 等
- 执行层尽量统一，仅在语义差异处做方言选项
- 引擎后端按模态插拔
- 分布式能力横切，不绑定到单个模态

### 2.3 编译隔离原则

每个模态和主要子能力都通过编译开关控制：

- 顶层模态开关：`MMDB_ENABLE_*`
- 分布式开关：`MMDB_ENABLE_DISTRIBUTED*`
- 子能力开关：`ENABLE_*`

目标：

- 最小单机 SQL-only 构建
- 最小单机某模态构建
- 全功能分布式构建

---

## 三、扩展方向汇总

### 3.1 新增顶级模型

1. `MODEL_STREAM`
2. `MODEL_COLUMNAR`

### 3.2 组合/扩展能力

1. 时空组合：Spatial + Timeseries
2. 知识图谱/RDF：Graph 扩展
3. 稀疏向量：Vector 扩展

### 3.3 横切能力

1. 统一执行层
2. 分布式能力
3. SQL 兼容方言层

---

## 四、统一执行层设计

### 4.1 统一逻辑计划

目标组件：

- Filter
- Project
- Aggregate
- Sort
- Join
- Limit/Offset
- CTE/Subquery
- Scan
- Insert/Update/Delete
- Set Operation（UNION/INTERSECT/EXCEPT）

### 4.2 统一物理算子框架

基础算子：

- SeqScan
- IndexScan
- HashJoin
- NestedLoopJoin
- Sort/TopN
- HashAggregate
- StreamAggregate

扩展算子（按模态插拔）：

- ANNOperator
- GraphTraverse
- TimeseriesAgg
- ColumnarScan/ColumnarAgg
- StreamWindowOperator

### 4.3 统一中间表示

各前端语言解析后先生成统一 IR/逻辑计划，再进入统一优化和执行。

---

## 五、SQL 兼容策略

### 5.1 总策略

采用“标准 SQL 内核 + 可插拔方言层 + 函数注册表”方式，不追求完全兼容所有数据库 SQL。

### 5.2 四层模型

1. SQL 核心语法层
2. SQL 方言模式层
3. SQL 兼容函数层
4. 前端解析层

### 5.3 支持目标

优先级：

- P0：ANSI SQL 核心子集
- P1：PostgreSQL 常见语法/函数兼容
- P2：MySQL 常见语法/函数兼容
- P3：分析 SQL（DuckDB/ClickHouse/Presto 风格）方言

### 5.4 执行层策略

执行层尽量统一，仅保留少量方言参数：

- 空值语义
- 布尔转换
- 类型隐式转换
- 分页行为
- 字符串大小写敏感性

### 5.5 兼容边界

明确对外声明：

- 核心 SQL 高度兼容
- 常见查询可移植
- 不承诺所有边缘语义 100% 一致
- 不在第一阶段完整兼容 PL/pgSQL / 存储过程

---

## 六、分布式能力设计

### 6.1 分布式定位

分布式能力作为独立横切层，不内聚到某个模态内部。

### 6.2 子层拆分

1. 协调层：成员管理、Leader 选举、健康检查、配置分发
2. 分片层：ShardMap、路由、分片迁移、split/merge
3. 复制层：Raft、日志复制、快照、成员变更
4. 分布式查询层：Scatter-Gather、跨节点聚合、跨节点 Join、结果归并

### 6.3 模态接入方式

本地模态引擎提供本地能力，分布式层将其包装为分布式引擎。

### 6.4 编译开关

```text
MMDB_ENABLE_DISTRIBUTED
MMDB_ENABLE_DISTRIBUTED_RAFT
MMDB_ENABLE_DISTRIBUTED_SHARD
MMDB_ENABLE_DISTRIBUTED_QUERY
```

---

## 七、新增模态具体规划

### 7.1 MODEL_STREAM

目标能力：

- 流式写入
- 批量写入
- 消费者读取
- 订阅推送
- 窗口聚合
- 事件日志持久化
- Offset 管理

### 7.2 MODEL_COLUMNAR

目标能力：

- 列式存储
- 字典编码
- 压缩（ZSTD/位包装等）
- 列式扫描
- 向量化聚合
- 投影下推
- 聚合下推

### 7.3 时空组合能力

目标能力：

- Spatial + Timeseries 组合查询
- 轨迹存储
- 时间范围 + 空间范围联合过滤
- 时空索引

### 7.4 知识图谱/RDF

目标能力：

- Triple Store
- RDF 三元组索引
- SPARQL 子集
- 属性图与 RDF 映射

### 7.5 稀疏向量

目标能力：

- Sparse vector
- Sparse + Dense 混合检索
- BM25/向量混合检索
- 稀疏索引结构

---

## 八、编译隔离设计

### 8.1 开关体系

顶层模态开关：

- `MMDB_ENABLE_RELATIONAL`
- `MMDB_ENABLE_KV`
- `MMDB_ENABLE_GRAPH`
- `MMDB_ENABLE_VECTOR`
- `MMDB_ENABLE_TIMESERIES`
- `MMDB_ENABLE_DOCUMENT`
- `MMDB_ENABLE_SPATIAL`
- `MMDB_ENABLE_TREE`
- `MMDB_ENABLE_STREAM`
- `MMDB_ENABLE_COLUMNAR`

子能力开关：

- `ENABLE_SPATIAL_TIMESERIES`
- `ENABLE_GRAPH_RDF`
- `ENABLE_VECTOR_SPARSE`
- `ENABLE_VECTOR_ANN`
- `ENABLE_GRAPH_ALGO`
- `ENABLE_DOC_AGG`
- `ENABLE_STREAM_WINDOW`
- `ENABLE_COLUMNAR_COMPRESS`

分布式开关：

- `MMDB_ENABLE_DISTRIBUTED`
- `MMDB_ENABLE_DISTRIBUTED_RAFT`
- `MMDB_ENABLE_DISTRIBUTED_SHARD`
- `MMDB_ENABLE_DISTRIBUTED_QUERY`

### 8.2 构建 Profile

1. Minimal Profile（单机小型化）
2. Full Profile（全模态 + 分布式）

### 8.3 代码组织建议

```text
engineering/
  src/
    core/
    model/
      relational/
      kv/
      graph/
      vector/
      timeseries/
      document/
      spatial/
      yang/
      stream/
      columnar/
    distributed/
```

CMake：

- `core/` 始终编译
- `model/<xxx>/` 按开关编译
- `distributed/` 按分布式开关编译

---

## 九、落地阶段建议

### 阶段 A：架构骨架与隔离机制

先不动业务，优先完成：

- 统一执行层抽象
- 分布式层骨架
- SQL dialect 骨架
- CMake 隔离机制
- 编译配置头生成
- 模态注册机制

### 阶段 B：真新模型落地

优先：

1. `MODEL_STREAM`
2. `MODEL_COLUMNAR`

### 阶段 C：组合能力补齐

1. 时空
2. RDF
3. 稀疏向量

### 阶段 D：SQL 方言补齐

1. PostgreSQL 常见兼容
2. MySQL 常见兼容
3. 分析 SQL 方言

### 阶段 E：分布式能力补齐

1. Raft
2. Shard
3. 分布式查询
4. 分布式元数据

---

## 十、验证方式

### 10.1 编译验证

- SQL-only 单机构建成功
- Vector-only 单机构建成功
- Graph-only 单机构建成功
- 全模态单机构建成功
- 分布式全模态构建成功

### 10.2 功能验证

- 各模态基础 CRUD
- 统一执行层查询
- Stream 写入与消费
- Columnar 扫描与聚合
- 时空联合查询
- RDF 基本查询
- 稀疏向量检索

### 10.3 SQL 兼容验证

- 标准 SQL 用例通过
- PG 方言常见用例通过
- MySQL 方言常见用例通过
- 分析 SQL 常见用例通过

### 10.4 分布式验证

- 多节点集群启动
- Leader 选举
- 分片路由
- 节点故障恢复
- 跨节点查询

---

## 十一、实施建议

### 11.1 推荐优先级

1. 架构与隔离
2. Stream + Columnar
3. SQL 方言层
4. 组合能力（时空/RDF/稀疏向量）
5. 分布式能力

### 11.2 风险控制

- 不在第一阶段承诺完全兼容 PostgreSQL/MySQL
- 分布式先做最小可用 Raft + Shard
- 组合能力先做可用版本，不追求高阶优化

---

## 十二、结论

本次讨论确认的统一计划是：

1. 构建可裁剪多模态数据库内核
2. 新增 Stream 和 Columnar
3. 补齐时空、RDF、稀疏向量
4. 建立统一执行层
5. SQL 通过标准内核 + 方言层实现渐进兼容
6. 分布式作为独立横切层
7. 编译隔离贯穿全部能力

该计划可作为后续统一设计文档和任务规划的基线。
