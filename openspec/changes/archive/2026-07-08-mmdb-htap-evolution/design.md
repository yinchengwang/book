# 技术设计文档：mmdb-htap-evolution

## Context

### 背景

当前多模态数据库已实现 8 种数据模型的存储引擎（关系、KV、向量、时序、图、文档、空间、树），但在 HTAP（混合事务/分析处理）能力上存在明显不足：

- **事务能力受限**：单写者模型，缺少 MVCC，读写互相阻塞
- **查询接口分散**：各引擎独立 API，无统一查询层
- **跨模型能力缺失**：无法进行"向量+关系"、"图+时序"等异构查询
- **协议兼容性差**：缺乏标准网络协议，无法使用业界生态工具
- **各模型能力差距**：相比商业产品（如 Milvus、TimescaleDB、Neo4j），向量、时序、图等模型的查询能力仍有差距

### 约束条件

- **全自研原则**：所有组件均为参考业界实现后自行实现，不直接使用第三方库
- **渐进式演进**：不影响现有功能，逐步引入新能力
- **学习导向**：通过自研深入理解业界最佳实践
- **嵌入式+服务双模式**：同一核心引擎支持两种部署模式

### 利益相关方

- 数据库内核开发者：需要深入理解 HTAP 架构
- 应用开发者：需要标准 SQL 接口和跨语言 SDK
- DevOps：需要高可用和运维工具

## Goals / Non-Goals

**Goals:**

1. **HTAP 能力**：实现 MVCC 并发控制，支持 RC/SI/SSI 隔离级别，打破单写者限制
2. **统一 SQL 层**：实现完整 SQL 解析、优化、执行，支持跨模型查询
3. **标准协议兼容**：实现 PostgreSQL Wire Protocol，支持 psql、JDBC、Python driver 等标准客户端
4. **各模型能力增强**：向量/时序/图/文档等模型达到业界主流水平
5. **跨模型联合查询**：支持异构模型 JOIN 和联合查询
6. **分布式扩展**：支持水平扩展和高可用部署
7. **嵌入式+服务双模式**：同一核心引擎支持两种部署模式

**Non-Goals:**

1. 不追求完全兼容 PostgreSQL 语法（只支持核心 SQL 特性）
2. 不支持分布式 2PC 之外的其他分布式事务协议（短期内）
3. 不实现完整的 ACID 以外的高级特性（如 XA 事务）
4. 不实现完整的图数据库 GQL 标准（只支持 openCypher 子集）
5. 不实现完整的 TimescaleDB 所有功能（只实现核心时序能力）

## Decisions

### 决策 1: SQL 解析器采用手写递归下降解析器

**选择**：自研递归下降解析器（Recursive Descent Parser）

**理由**：
- 相比 Yacc/Bison：代码可控，便于学习和调试，错误信息友好
- 相比 Lemon：更符合"全自研"原则
- 相比 ANTLR：减少外部依赖，降低学习曲线

**参考实现**：
- PostgreSQL parser（复杂但完整）
- SQLite parser（相对简单，易于理解）
- 手写词法分析器 + 递归下降语法分析器

**替代方案考虑**：
- ❌ Yacc/Bison：语法简单但生成代码难维护
- ❌ ANTLR：功能强大但外部依赖，脱离"自研"原则
- ❌ Lemon (SQLite)：相对简单但文档较少

---

### 决策 2: 执行引擎采用 Volcano 迭代器模型

**选择**：Volcano 迭代器模型（Pull-based）

**理由**：
- 业界标准：PostgreSQL、Spark、Flink 等均采用此模型
- 易于理解：Init → Exec → End 三阶段，清晰直观
- 易于优化：算子组合灵活，便于优化器改写
- 参考丰富：大量论文和开源实现可参考

**参考实现**：
- PostgreSQL executor（经典实现）
- Apache Arrow Compute（现代列式执行）
- ClickHouse（向量化+迭代器混合）

**替代方案考虑**：
- ❌ Push-based 模型：不适配向量化执行
- ❌ Vectorized Volcano：作为 Phase 8 的增强方向

---

### 决策 3: 网络协议选择 PostgreSQL Wire Protocol

**选择**：PostgreSQL Wire Protocol（pgwire）作为主要协议

**理由**：
- 生态完善：所有语言的 PostgreSQL driver 可直接使用
- 工具链成熟：psql、pgAdmin、DBeaver 等直接可用
- 学习价值：协议设计优雅，实现过程可深入理解数据库协议
- 兼容性广：BI 工具、ORM、迁移工具均支持

**参考实现**：
- PostgreSQL 官方协议文档（权威定义）
- pgjdbc 源码（Java 客户端视角）
- cockroachdb/cockroach 源码（Go 服务端实现参考）

**替代方案考虑**：
- ❌ MySQL Protocol：生态也不错，但 PostgreSQL 更适合分析场景
- ❌ gRPC：性能好但需要专用客户端，不适合"标准兼容"目标
- ❌ REST：作为辅助接口（管理/监控），不适合核心查询

---

### 决策 4: MVCC 采用 PostgreSQL 风格

**选择**：PostgreSQL 风格的元组版本链 + ReadView

**理由**：
- 成熟稳定：PostgreSQL 多年生产验证
- 易于理解：xmin/xmax/ctid 经典设计
- 性能良好：读不阻塞写，写不阻塞读
- 学习价值：深入理解 MVCC 原理的最佳实践

**核心数据结构**：
```c
// 元组头扩展
typedef struct {
    uint32_t xmin;      // 插入事务 ID
    uint32_t xmax;      // 删除/更新事务 ID
    uint32_t ctid;      // 指向新版本的指针
    uint8_t  t_xmin_status;  // 事务状态
    uint8_t  t_xmax_status;
    uint8_t  t_xmin_commit_seq;  // 提交序列
} TupleHeader_MVCC;

// ReadView
typedef struct {
    uint32_t xmin;           // 最小活跃事务 ID
    uint32_t xmax;           // 最大已分配事务 ID
    uint32_t *active_txs;    // 活跃事务数组
    uint32_t n_active;
} ReadView;
```

**参考实现**：
- PostgreSQL src/include/access/tupmacs.h
- PostgreSQL src/backend/access/heap/heapam_visibility.c
- PostgreSQL src/backend/access/transam/snapshot.c

**替代方案考虑**：
- ❌ 乐观锁（CAS）：适合冲突少的场景，不适合高并发
- ❌ 写时复制（COW）：内存开销大，不适合写密集型
- ❌ 内存时间戳：实现简单但功能受限

---

### 决策 5: 隔离级别支持 RC/SI/SSI 可配置

**选择**：三种隔离级别可选

**理由**：
- RC（Read Committed）：性能最好，适合大多数场景
- SI（Snapshot Isolation）：避免脏读和不可重复读，适合分析
- SSI（SERIALIZABLE）：最强一致性，适合关键业务

**实现策略**：
- GUC 参数 `transaction_isolation` 控制
- 运行时可切换（会话级别）
- SI 通过 ReadView 实现
- SSI 在 SI 基础上增加谓词锁

**替代方案考虑**：
- ❌ 只支持 SI：忽略 RC 的性能优势
- ❌ 只支持 RC：功能不足，无法满足关键业务

---

### 决策 6: 锁管理器采用分层锁设计

**选择**：表级锁 + 行级锁 + 谓词锁三层设计

**理由**：
- 兼容性：与 PostgreSQL 锁模型一致
- 性能：行级锁减少锁冲突
- 功能：谓词锁支持 SSI 隔离级别

**锁类型层次**：
```
表级锁（AccessShareLock, RowShareLock, ...）
    ↓
行级锁（FOR UPDATE, FOR SHARE）
    ↓
谓词锁（KeyRange 锁，用于 SSI）
```

**死锁检测**：
- 构建 Wait-For Graph
- 周期性检测环
- 超时回滚策略

**参考实现**：
- PostgreSQL src/backend/storage/lmgr/lock.c
- PostgreSQL src/backend/storage/lmgr/deadlock.c

---

### 决策 7: 向量索引采用 HNSW + DiskANN 分层

**选择**：内存用 HNSW，磁盘用 DiskANN

**理由**：
- HNSW：内存友好，查询快，构建相对简单
- DiskANN：磁盘友好，支持超大规模
- 分层策略：热数据用 HNSW，温数据用 IVF-PQ，冷数据用 IVF

**索引选择策略**：
```c
// 根据数据规模自动选择
if (num_vectors < 1M) {
    // 内存充足，使用 HNSW
    return HNSW;
} else if (num_vectors < 10M) {
    // 中等规模，IVF-PQ 压缩
    return IVF_PQ;
} else {
    // 超大规模，DiskANN
    return DISKANN;
}
```

**参考实现**：
- Faiss（Facebook）- HNSW, IVF-PQ
- hnswlib - 纯 HNSW 实现
- DiskANN (Microsoft) - 磁盘友好图索引
- Milvus - 分层索引实践

---

### 决策 8: 时序存储采用列式 + 压缩

**选择**：时序数据列式存储 + Gorilla 压缩

**理由**：
- 分析友好：列式存储适合聚合查询
- 压缩高效：Gorilla 对时序数据压缩率高
- 行业标准：FacebookGorilla、TimescaleDB、InfluxDB 均采用类似方案

**压缩算法**：
- Delta encoding：相邻值差分
- RLE：重复值游程编码
- Gorilla：异或 + 熵编码（压缩率最高）
- Bit-packing：整数紧凑存储

**参考实现**：
- Facebook Gorilla 时序压缩论文
- InfluxDB TSM 存储引擎
- ClickHouse MergeTree 列式存储
- TimescaleDB 压缩

---

### 决策 9: 图查询语言支持 openCypher 子集

**选择**：实现 openCypher 的核心子集 + SQL 扩展

**理由**：
- 标准化：openCypher 是事实上的图查询语言标准
- 兼容性：与 Neo4j、Apache AGE 兼容
- 实用性：覆盖大多数常见图查询场景
- 渐进性：逐步扩展支持的语法

**支持的 Cypher 语法**：
```cypher
-- 模式匹配
MATCH (a:Person)-[r:KNOWS]->(b:Person)
WHERE a.name = 'Alice'
RETURN b.name

-- 路径查询
MATCH path = (a)-[:FRIEND*1..3]->(b)
RETURN path

-- 聚合
MATCH (a:Person)-[:FRIEND]->(b:Person)
RETURN a.name, count(b)
```

**SQL 扩展**：
```sql
SELECT GRAPH_TRAVERSE('social', start_id, depth => 3)
FROM users WHERE name = 'Alice';
```

**参考实现**：
- Neo4j Cypher 实现
- Apache AGE（PostgreSQL 扩展）
- Memgraph Cypher

---

### 决策 10: 分布式采用 Raft 共识

**选择**：Raft 算法实现高可用

**理由**：
- 易于理解：比 Paxos 更易于理解和实现
- 成熟稳定：etcd、TiKV、CockroachDB 均采用
- 功能完整：Leader 选举、日志复制、成员变更
- 学习价值：深入理解分布式一致性协议

**实现要点**：
- 日志复制：类似 etcd Raft
- 线性一致性读：ReadIndex / LeaseRead
- 成员变更：Joint Consensus

**参考实现**：
- etcd raft - 最成熟的开源实现
- tikv/raft (Rust) - TiDB 的 Raft 实现
- hashicorp/raft - Go 实现

---

### 决策 11: 分片策略支持 Hash + Range

**选择**：Hash 分片和 Range 分片可选

**理由**：
- Hash 分片：数据分布均匀，适合点查
- Range 分片：适合范围查询，时序数据友好
- 可配置：不同集合可用不同策略

**分片键选择**：
```sql
-- Hash 分片
CREATE COLLECTION users SHARD BY HASH(user_id);

-- Range 分片（时序数据）
CREATE COLLECTION metrics SHARD BY RANGE(timestamp);
```

**路由策略**：
- 协调节点接收请求
- 计算分片路由
- 并行分发到数据节点
- 聚合返回结果

---

### 决策 12: 嵌入式 + 服务双模式架构

**选择**：共享核心引擎，适配层分离

**理由**：
- 代码复用：核心引擎只需实现一次
- 灵活部署：嵌入式适合边缘，服务适合云端
- 接口一致：上层 API 统一
- 便于测试：单元测试用嵌入式更方便

**架构分层**：
```
┌─────────────────────────────────────┐
│  适配层 (Adapter Layer)             │
│  ┌─────────────┐  ┌─────────────┐  │
│  │ Embedded    │  │  Server     │  │
│  │ Adapter     │  │  Adapter    │  │
│  └──────┬──────┘  └──────┬──────┘  │
│         │                │          │
│         └────────┬───────┘          │
│                  ▼                   │
│  ┌─────────────────────────────────┐│
│  │       核心引擎 (Core Engine)    ││
│  │  SQL │ 存储 │ 事务 │ 索引       ││
│  └─────────────────────────────────┘│
└─────────────────────────────────────┘
```

---

## Risks / Trade-offs

### 风险 1: SQL 解析器覆盖度不足

**风险**：手写解析器可能遗漏一些边界情况

**影响**：复杂 SQL 可能解析失败

**缓解**：
- 参考 PostgreSQL 测试用例
- 渐进式支持，先覆盖核心语法
- 保留扩展点，便于后续补充

---

### 风险 2: MVCC 实现复杂度高

**风险**：MVCC 与现有代码深度耦合，改动风险大

**影响**：可能引入性能回退或一致性问题

**缓解**：
- Phase 1 单独实现，与现有事务隔离
- 充分单元测试和集成测试
- 提供关闭 MVCC 的回退选项

---

### 风险 3: pgwire 协议兼容性问题

**风险**：某些 PostgreSQL 客户端的特性可能不完全兼容

**影响**：部分工具可能无法正常工作

**缓解**：
- 先支持简单查询协议（Simple Query）
- 扩展查询协议作为可选功能
- 提供兼容性测试矩阵

---

### 风险 4: 分布式一致性保证

**风险**：Raft 实现可能存在边界情况

**影响**：极端情况下可能丢数据或分裂

**缓解**：
- 参考 etcd raft 实现（经过生产验证）
- 充分的故障注入测试
- 线性一致性测试

---

### 风险 5: 跨模型查询优化难度大

**风险**：异构模型 JOIN 的优化空间有限

**影响**：复杂跨模型查询性能可能不理想

**缓解**：
- 优先支持简单场景（向量+过滤条件）
- 逐步增加优化规则
- 提供 hints 让用户引导优化

---

### 权衡 1: 自研 vs 复用

**权衡**：坚持自研原则 vs 引入成熟库加速开发

**决策**：
- 核心模块坚持自研（Parser、Executor、协议）
- 非核心模块可考虑引入（如 Raft 可参考 etcd）
- 学习为主，不追求绝对自研

---

### 权衡 2: 完整性 vs 渐进性

**权衡**：一次性实现完整功能 vs 逐步迭代

**决策**：
- 每 Phase 有交付物，不做太长的半成品
- 优先实现核心功能，扩展功能后续迭代
- 保持架构开放，便于扩展

---

## Migration Plan

### 部署策略

**Phase 1-2（单机能力）**：
- 新增功能默认关闭，通过配置启用
- 现有 API 保持兼容
- 逐步将现有引擎迁移到新架构

**Phase 3-8（模型增强）**：
- 新增 API 与现有 API 并存
- 提供迁移工具
- 文档说明迁移步骤

**Phase 9（分布式）**：
- 支持单机模式平滑升级到集群模式
- 数据自动重平衡
- 提供回滚能力

### 回滚策略

**Phase 1-2**：
- 配置回退：关闭新增功能
- 代码回退：通过 Git 回滚

**Phase 3-8**：
- 数据格式兼容旧版本
- 新旧 API 同时支持

**Phase 9**：
- 集群降级为单机
- 数据保留，可重新升级

---

## Open Questions

### Q1: SQL 语法兼容性边界

**问题**：需要支持多少 PostgreSQL 语法？

**待定**：
- 先支持核心 DDL/DML
- 窗口函数、分析函数逐步支持
- 特定扩展语法（JSON、数组）按需支持

---

### Q2: 事务隔离级别默认值

**问题**：默认使用哪种隔离级别？

**待定**：
- 推荐 SI（Snapshot Isolation）
- 可配置切换
- 与 PostgreSQL 行为一致（默认 RC）

---

### Q3: 分布式部署形态

**问题**：分布式模式是嵌入还是独立进程？

**待定**：
- 协调节点可嵌入或独立
- 数据节点独立进程
- 参考 TiDB/TiKV 架构

---

### Q4: 冷数据存储

**问题**：分层存储的冷数据层如何实现？

**待定**：
- S3 兼容对象存储
- 本地文件归档
- 未来可扩展到云存储

---

### Q5: 多租户隔离级别

**问题**：多租户是否需要资源隔离？

**待定**：
- Phase 1-8 不考虑多租户
- Phase 9 考虑命名空间隔离
- 资源配额后续实现
