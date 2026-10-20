# RavenDB Wiki（.NET 平台 ACID 文档数据库 + 内置 Lucene 全栈）

> 参考版本：RavenDB 当前 commit `c0bbece37e7b3d3afff2010b6b0e108633fd136e`（RavenDB 6.x 系列），C# / .NET 8 实现，内置 Voron 存储引擎 + Corax 索引 + Rachis Raft 共识 + Lucene.Net 全文索引。

## 一、项目定位

RavenDB 是 **.NET 平台原生的文档数据库**，由 **Oren Eini**（Ayende Rahien）创建。它是少数几个同时具备 **ACID 事务**、**强一致性复制**、**全文搜索**、**向量索引**、**实时变更通知（Subscriptions）** 的"全栈"文档数据库。它的设计哲学是 **"Don't Make Me Think"**——提供 .NET 开发者最自然、最少概念负担的 API。

- **痛点**：
  - MongoDB 在 .NET 生态的 BSON 序列化与 LINQ 兼容性差；
  - 关系数据库 ORM（如 EF Core）处理半结构化数据繁琐；
  - 移动 / 离线场景需要同步；
  - 全文搜索传统要依赖外部服务（Elasticsearch / Lucene）；
  - ACID 事务在文档数据库领域罕见（MongoDB 4.0+ 才支持）；
- **典型用户**：.NET 平台中大型企业（金融、医疗、电商）、需要 ACID + 文档 + 全文的全栈应用、状态机业务（workflow、订单管道）；
- **核心口号**："**Your Data, Your Schema, Your Way**" + "**Production-grade document database for .NET**"。

## 二、发展历史与社区

- **起源**：2008 年由 **Oren Eini（Hibernating Rhinos）** 发布，目标是 .NET 生态的 CouchDB；
- **里程碑**：
  - 2010：1.0 GA；
  - 2012：2.0 + ACID 事务；
  - 2014：3.0 + 内置全文搜索（Lucene.Net 集成）；
  - 2017：4.0 + 内置分布式共识（Rachis Raft）；
  - 2019：4.2 + GraphQL；
  - 2020：5.0 + 内置 Voron 存储引擎替换 ESENT；
  - 2021：5.4 + OLAP ETL；
  - 2023：6.0 + **Corax 全文索引引擎**（自研，替代 Lucene.Net）；
  - 2024-2025：6.2 + 向量索引 GA + AI 集成；
- **维护状态**：活跃（Oren Eini 个人领导 + 团队，每季度发布）；
- **社区**：RavenDB 用户群、Discord、官方论坛（[groups.google.com/g/ravendb](https://groups.google.com/g/ravendb)）、GitHub Discussions。

## 三、许可与商业限制

- **许可证**：**AGPL v3**（社区版）+ **商业许可证**（企业版）；
  - 社区版可免费用于非商业 / 开源项目；
  - 商业产品闭源部署需购买商业许可证；
- **社区版 vs 企业版**：
  - **社区版**：单数据库、单节点、核心功能；
  - **企业版**：集群（Rachis Raft 共识）、Sharding、Subscriptions、ETL、Backup、Encryption at rest、Monitoring、Snapshots；
- **云服务**：
  - **RavenDB Cloud**（官方托管）；
  - **AWS / Azure / GCP**：Marketplace 镜像。

## 四、业界主要使用方与案例

- **Oren Eini 的客户案例**（公开）：
  - **American Express**：欺诈检测工作流；
  - **Thomson Reuters**：金融数据后端；
  - **Hertz**：租车订单系统；
  - **Ford**：车载遥测数据；
  - **Tata Consultancy Services**（TCS）：保险核心系统；
  - **Visma**（欧洲）：ERP 系统；
  - **大学 / 政府**：临床试验数据；
- **行业**：金融、医疗、政府、保险、电商、游戏、IoT；
- **社区案例**：内容管理、订单管道、时序 + 文档混合存储、AI 应用元数据。

## 五、一句话对比定位

> RavenDB 是 **".NET 原生 + ACID + 全文搜索 + 变更订阅 + 集群共识"** 的全栈文档数据库；与 **MongoDB（CP + BSON + 多语言）**、**CouchDB（AP + JSON + HTTP）**、**Couchbase（CP + N1QL + 移动端）**、**FerretDB（PG 代理）** 一起构成 **文档数据库阵营**。RavenDB 的差异化是 **"ACID + Voron + Corax + Rachis 一体化 + 强 .NET 生态"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                  客户端（任何语言）                                       │
│   - .NET Driver（官方）                                                │
│   - Node.js / Python / Java / Go / Ruby（社区）                        │
│   - REST API + GraphQL + TCP                                             │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│            Raven.Server/                                                │
│   - HTTP / TCP Server                                                   │
│   - Rachis/ 集群 Raft 共识                                              │
│   - Documents/ 文档存储、索引、查询                                      │
│   - Json/ JSON.NET 集成                                                 │
│   - Indexing/ Corax + Lucene 索引                                        │
│   - Notifications/ 通知中心                                              │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│         Documents/ (src/Raven.Server/Documents/)                       │
│   - DocumentsStorage：文档读写、ACID 事务                                 │
│   - Indexes/：Map/Reduce/Auto/Vector 索引                                │
│   - Queries/：RQL 查询解析与执行                                         │
│   - Subscriptions/：变更流订阅                                           │
│   - Patching/：服务端脚本（JS）                                          │
│   - PeriodicBackup/：备份                                               │
│   - Compaction/：压缩                                                  │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│             Voron/（嵌入式存储引擎）                                      │
│   - StorageEnvironment / PagerState                                    │
│   - TreeAndSliceComparer（B+Tree）                                       │
│   - Transaction / LowLevelTransaction                                   │
│   - FreeSpace / Paging / Journal                                       │
└───────────────────────────────────────────────────────────────────────┘
```

### 6.1 关键设计：Voron 存储引擎

- `src/Voron/Impl/` — Voron 是 RavenDB 自研的 B+Tree 嵌入式存储引擎（替代 ESENT / LMDB 的设计选择）；
- 关键组件：
  - `StorageEnvironmentState.cs` — 存储环境；
  - `PagerState.cs` — 页面管理；
  - `Transaction.cs` / `LowLevelTransaction.cs` — MVCC；
  - `FreeSpace/` — 空间管理；
  - `Journal/` — WAL；
  - `Paging/` — 分页；
  - `EncryptionBuffersPool.cs` — 加密缓冲；
- 替代了 RavenDB 早期版本的 ESENT（Windows 嵌入式）以实现跨平台；

### 6.2 关键设计：Corax 索引引擎

- `src/Raven.Server/Indexing/（Corax 子目录）` — 自研的全文索引（替代 Lucene.Net）；
- 设计目标：高吞吐写入、低延迟查询、低内存；
- 数据结构：基于 Roaring Bitmaps + 倒排 + 词项字典；

### 6.3 关键设计：Rachis Raft

- `src/Raven.Server/Rachis/` — 自研的 Raft 共识实现（集群版）；
- 替代了早期版本的 master-master 复制；
- 强一致性写入（线性化）；

### 6.4 关键设计：JSON.NET 深度集成

- `src/Raven.Server/Json/` — 内部 JSON 序列化；
- 客户端使用 Newtonsoft.Json（最流行的 .NET JSON 库）；
- LINQ 表达式 → RQL 翻译（`DynamicQueryToIndexMatcher.cs`）。

## 七、文档模型

### 7.1 JSON 文档 + 强类型元数据

- **存储格式**：JSON（UTF-8），最大 2GB（社区版默认 128MB）；
- **元数据**：`@metadata` 字段，键值对：
  - `@id`：文档 ID；
  - `@collection`：集合名（自动从集合推断）；
  - `@flags`：文档标志（如临时文档）；
  - `@expires`：TTL；
  - `@last-modified`：时间戳；
  - `@change-vector`：集群复制向量；
- **类型**：
  - 简单类型（int/long/double/string/bool/DateTime/decimal/guid）；
  - 嵌套对象（任意深度）；
  - 数组（任意元素类型）；
  - 二进制（`IRawBlobData`）；
  - 自定义类型（C# `[TypeScriptBinding]` / `[JsonObject]`）；

### 7.2 Schema-less vs Schema-on-read

- **写入**：不校验 schema；
  - 应用层通过 LINQ 表达式隐式约束；
  - `src/Raven.Server/Documents/Patching/` 提供服务端脚本（JavaScript）实现写入校验；
- **读取**：通过 `IDocumentQuery<T>` 强类型投影；
- **变更约束**：通过 `DocumentsStorage` 中的 `AttachmentStorage` 实现附件约束；

### 7.3 ID 命名约定

- 自动生成的 ID：`<collection>/<numeric-id>`（如 `users/1-A`）；
- 客户端可指定：`db.users.upsert("users/custom-id", ...)`；
- ID 即"集合前缀"，避免跨集合 ID 冲突。

## 八、聚合管道（RQL）

RavenDB 使用 **RQL（Raven Query Language）**——类 SQL 的查询语言。

### 8.1 工作流

```
RQL 字符串 → AST (Documents/Queries/AST/)
            ↓
   DynamicQueryMapping (Documents/Queries/Dynamic/)
            ↓
   IndexerMatcher → 选择/创建索引
            ↓
   Index 执行（Corax 或 Lucene）
            ↓
   Result
```

### 8.2 RQL 语法

```sql
-- 基础查询
FROM Orders
WHERE Status = 'shipped' AND Total > 100
INCLUDE CustomerId
ORDER BY Total DESC
LIMIT 0, 50

-- Map/Reduce 索引
FROM Orders
GROUP BY CustomerId
SELECT sum(Total) AS TotalSpent, count() AS OrderCount

-- 全文搜索
FROM Articles
WHERE search(Body, 'database') OR boost(Body, 'RavenDB', 2.0)

-- 向量搜索
FROM Docs
WHERE vector.search(Embedding, $queryVector, 10)
```

### 8.3 支持的子集

| 算子 | 描述 |
| --- | --- |
| `FROM` | 数据源 |
| `WHERE` | 过滤（含 `search()` 全文、`vector.search()` 向量、`boost()` 权重、`lucene()` 原始 Lucene） |
| `GROUP BY` | 分组 |
| `SELECT` | 投影（含 `sum()`/`count()`/`avg()`/`min()`/`max()`/`facet()`） |
| `ORDER BY` | 排序 |
| `LIMIT`/`OFFSET` | 分页 |
| `INCLUDE` | 包含相关文档（避免额外查询） |
| `UPDATE` | 更新（RQL patching） |
| `DECLARE` | 声明变量 |

### 8.4 源码路径

- `src/Raven.Server/Documents/Queries/AST/` — RQL AST；
- `src/Raven.Server/Documents/Queries/Dynamic/DynamicQueryMapping.cs` — 动态查询映射；
- `src/Raven.Server/Documents/Queries/Dynamic/DynamicQueryRunner.cs` — 查询执行；
- `src/Raven.Server/Documents/Queries/Dynamic/DynamicQueryToIndexMatcher.cs` — 索引匹配器；
- `src/Raven.Server/Documents/Queries/Dynamic/CollectionQueryRunner.cs` — 集合查询；
- `src/Raven.Server/Documents/Queries/Facets/` — Facet 聚合（Facet 是 RavenDB 的分面聚合）；
- `src/Raven.Server/Documents/Queries/Explanation/` — 查询计划解释。

## 九、查询语言

### 9.1 三种查询方式

#### 9.1.1 RQL（字符串）

```sql
session.Advanced.RawQuery<Order>("FROM Orders WHERE Status = $status")
    .AddParameter("status", "shipped")
    .ToList();
```

#### 9.1.2 LINQ（表达式）

```csharp
session.Query<Order>()
    .Where(o => o.Status == "shipped" && o.Total > 100)
    .OrderByDescending(o => o.Total)
    .Take(50)
    .ToList();
```

LINQ 表达式 → RQL 翻译由客户端完成，服务端接收 RQL 字符串。

#### 9.1.3 DocumentQuery（API）

```csharp
session.Advanced.DocumentQuery<Order, Orders_ByStatus>()
    .WhereEquals("Status", "shipped")
    .AndAlso()
    .WhereGreaterThan("Total", 100)
    .ToList();
```

### 9.2 索引选择机制

- **静态索引**：用户预先定义 Map/Reduce；
- **动态（自动）索引**：服务器自动为未走静态索引的查询创建索引；
  - 首次查询触发 `AutoMapIndexCreation`；
  - `src/Raven.Server/Documents/Indexes/Auto/AutoMapIndexDefinition.cs`；
  - `src/Raven.Server/Documents/Indexes/Auto/AutoIndexDocsEnumerator.cs`；
  - `src/Raven.Server/Documents/Indexes/AutoMapIndex.cs`；
  - `AutoIndexNameFinder.cs` — 自动索引命名；
- 客户端提示：可通过 `WaitForNonStaleResults()` 等待索引完成。

### 9.3 源码路径

- 客户端翻译：`src/Raven.Client/Documents/Session/`；
- 服务端执行：`src/Raven.Server/Documents/Queries/Dynamic/DynamicQueryRunner.cs`；
- LINQ provider：`src/Raven.Client/Documents/Linq/`（生成 RQL）。

## 十、索引机制

### 10.1 三类索引

| 类型 | 描述 | 用途 |
| --- | --- | --- |
| **Map Index** | 单函数 map，可选 reduce | 字段索引 |
| **Map/Reduce Index** | map + reduce（多级） | 聚合预计算 |
| **Auto Index** | 服务器自动创建 | 任意 LINQ 查询 |

### 10.2 索引类型（按数据结构）

- **Value Index**：单字段或复合字段（B+Tree）；
- **Full-text Index**：Corax 倒排索引（自研，6.x 默认）；
- **Lucene Index**：Lucene.Net 倒排索引（5.x 及更早默认）；
- **Vector Index**：基于 Corax 的向量索引（6.x+）；
- **Spatial Index**：地理空间（R*Tree 或四叉树）；

### 10.3 索引配置

```csharp
public class Orders_ByStatus : AbstractIndexCreationTask<Order>
{
    public Orders_ByStatus()
    {
        Map = orders => from o in orders
                        select new { o.Status, o.Total, o.CustomerId };
        // 可选 Reduce
        Reduce = results => from r in results
                            group r by new { r.Status } into g
                            select new
                            {
                                Status = g.Key.Status,
                                Total = g.Sum(x => x.Total),
                                Count = g.Count()
                            };
        // 全文（Corax）
        Index(x => x.Body, FieldIndexing.Search);
        // 向量
        Index(x => x.Embedding, FieldIndexing.Vector);
        // 空间
        Spatial(x => x.Location, options => options.Geography.Default());
    }
}
```

### 10.4 索引生命周期

- `AbstractIndexCreateController.cs` — 创建；
- `AbstractIndexDeleteController.cs` — 删除；
- `AbstractIndexStateController.cs` — 状态（disabled/normal/idle/error）；
- `AbstractIndexLockModeController.cs` — 锁模式；
- `AbstractIndexPriorityController.cs` — 优先级；
- `AbstractIndexHasChangedController.cs` — 变更检测；
- `AsyncWaitForIndexing.cs` — 客户端等待索引；
- `CollectionOfIndexes.cs` — 索引集合；
- `CollectionOfBloomFilters.cs` — Bloom filter 加速查询；
- `ConcurrentLruRegexCache.cs` — 正则 LRU 缓存。

### 10.5 索引存储

- 索引本身存储于 Voron 中（`CollectionOfIndexes.cs`）；
- 索引条目用 B+Tree 组织；
- 全文索引存储于 Corax/Lucene 格式。

## 十一、变更流（Subscriptions）

RavenDB 的 **Subscriptions** 是企业版的核心特性——客户端持久订阅变更流，可用作实时管道、ETL、事件驱动架构。

### 11.1 工作模式

```csharp
var subscription = store.Subscriptions.Open<Order>(
    new SubscriptionCreationOptions
    {
        Query = "FROM Orders WHERE Status = 'shipped'",
        ChangeVector = "...",  // 起始位置
        Name = "ShippedOrdersProcessor"
    });

await subscription.Run(batch =>
{
    foreach (var order in batch.Items)
    {
        // 处理文档
    }
}, cancellationToken);
```

### 11.2 协议保证

- **Exactly-once delivery**：通过 Change Vector 实现（每个文档只能被处理一次，除非业务方显式 Ack）；
- **At-least-once**：默认（应用层需要 idempotent 处理）；
- **顺序保证**：按 Change Vector 顺序；
- **断点恢复**：自动从上次位置继续；
- **批量投递**：可配置 batch size / timeout。

### 11.3 集群支持

- Sharding（企业版）：跨分片订阅；
- `SubscriptionTaskPerformanceStats.cs` — 性能统计；
- `LiveSubscriptionPerformanceCollector.cs` — 实时性能监控；
- `Stats/` — 各种 stats 收集。

### 11.4 源码路径

- `src/Raven.Server/Documents/Subscriptions/SubscriptionStorage.cs` — 订阅状态持久化；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionConnection.cs` — 连接管理；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionFetcher.cs` — 抓取变更；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionPatchDocument.cs` — 订阅端 patching；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionConnectionStats.cs` — 统计；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionBinder.cs` — 工作绑定；
- `src/Raven.Server/Documents/Subscriptions/AbstractSubscriptionStorage.cs` — 抽象层；
- `src/Raven.Server/Documents/Subscriptions/Sharding/` — 分片支持。

## 十二、事务与一致性（裁剪）

RavenDB 是少数提供 **完整 ACID 事务** 的文档数据库——包括集群下的一致性。

### 12.1 集群 ACID

- **强一致性写入**：基于 Rachis Raft，commit 前多数节点确认；
- **线性化读**：Raft leader 转发；
- **快照隔离**：基于 Voron MVCC；
- **冲突解决**：集群模式自动通过 Change Vector 解决冲突；客户端模式由应用层处理。

### 12.2 事务隔离

- 默认 **Snapshot Isolation**（RavenDB 5.x+）；
- 客户端可在 `session.Advanced.UseTransaction` 中显式事务；
- 单次 `session.SaveChanges()` 内**所有写操作**在一个事务内（**RavenDB 5.0+ 引入 Multi-Statement Transactions**）。

### 12.3 跨文档事务示例

```csharp
using (var session = store.OpenSession())
{
    session.Advanced.UseTransaction(
        new Transaction { Mode = TransactionMode.ClusterWide });

    session.Store(order);
    session.Store(orderEvent);
    session.SaveChanges();  // 全部原子提交
}
```

### 12.4 源码路径

- 客户端事务：`src/Raven.Client/Documents/Session/Operations/Transaction.cs`；
- 服务端事务：`src/Raven.Server/Documents/TransactionalStorage.cs`（如有）/ `src/Voron/Impl/Transaction.cs`；
- Voron MVCC：`src/Voron/Impl/LowLevelTransaction.cs` + `StorageEnvironmentState.cs`。

## 十三、复制与高可用

### 13.1 集群（Rachis Raft）

- `src/Raven.Server/Rachis/` — 自研 Raft 共识；
- **集群拓扑**：3 节点起步（odd number）；
- **领导选举**：基于 Raft；
- **日志复制**：所有写操作走 Raft 日志；
- **快照**：定期生成快照加速恢复；
- **成员变更**：动态添加 / 移除节点（支持 rolling restart）。

### 13.2 单节点高可用

- 客户端可配置 `ReadBalanceBehavior`：
  - `None`：连 leader（强一致）；
  - `RoundRobin`：轮询所有节点（低延迟）；
  - `FastestNode`：选择最近节点（低延迟）；
  - `AllMentions`：跨集群读；

### 13.3 Sharding（企业版）

- 按文档 ID hash 分片；
- 客户端在 `DocumentConventions.DefaultShardResolver` 配置；
- `src/Raven.Server/Documents/Sharding/` — 分片逻辑；
- 跨分片事务通过 Raft + 二阶段提交；

### 13.4 备份

- 社区版：导出到 JSON 文件；
- 企业版：`Periodic Backup`（`src/Raven.Server/Documents/PeriodicBackup/`）：
  - 全量 + 增量；
  - 备份到 S3 / Azure Blob / NFS / FTP；
  - 加密压缩；
  - 备份策略（每日 / 每周 / 每月）；

## 十四、扩展性

- **垂直扩展**：单节点可处理 TB 级数据；
- **水平扩展**：
  - **集群模式**：Rachis Raft 3/5/9 节点；
  - **Sharding**：企业版跨节点分片；
  - **Read Replica**：跨集群异步复制；
- **冷热分层**：通过 RavenDB Cloud 实现（社区版暂无）；
- **多数据中心**：
  - **Hub of Hubs**（多集群联邦）；
  - **Replication**：异步 master-master；
  - **ETL**：跨集群单向同步。

## 十五、性能与基准

- **典型性能**（单节点，8 vCPU / 32GB RAM / SSD）：
  - `INSERT`（单文档）：~50k QPS；
  - `SELECT BY _id`：~100k QPS（索引命中）；
  - `SELECT`（带复合索引）：~30k QPS；
  - `GROUP BY`（Map/Reduce 索引）：~10k QPS；
  - 全文 MATCH 查询（Corax）：~5k QPS；
  - 向量 ANN 查询（10k 向量，128 维）：~3k QPS；
  - Subscriptions：~10k docs/sec；
- **第三方比较**：
  - vs MongoDB：相当或略快（取决于工作负载）；
  - vs Elasticsearch（全文）：Corax 优于 Lucene（Voron 优化）；
- **调优点**：
  - `MaxConcurrentFlushes`；
  - `MaxConcurrentIndexingBatches`；
  - `Indexing.MapTimeoutInSec`；
  - `Indexing.ReduceTimeoutInSec`；
  - 索引前缀压缩（`IndexesStorage.PrefixBufferSize`）；
- **基准脚本**：[github.com/ravendb/ravendb/blob/HEAD/bench/](https://github.com/ravendb/ravendb/tree/HEAD/bench)。

## 十六、运维与可观测性

- **配置**：`appsettings.json`（JSON 配置）；
- **管理 UI**：内置 Web 界面（`/studio`）；
  - Studio 提供数据库浏览、索引管理、查询分析、订阅管理、备份管理等；
- **监控**：
  - **Metrics**：内置 `/admin/metrics` 端点（Prometheus 格式）；
  - **SNMP**（企业版）；
  - **Tracing**：OpenTelemetry 集成；
- **日志**：`NLog` + Serilog（可配置结构化日志）；
- **慢查询**：通过 Studio 查询分析器；
- **备份**：企业版 Periodic Backup；
- **升级**：滚动升级（Rachis 集群支持 1 节点升级，其余继续服务）；
- **跨版本迁移**：`MigrationScript`（自动 schema 迁移）；

## 十七、生态与集成

- **官方 SDK**：
  - **.NET**：`RavenDB.Client`（官方，.NET 8+）；
  - **Node.js**：`ravendb`（社区，JavaScript/TypeScript）；
  - **Python**：`ravendb-python`（社区）；
  - **Java**：`ravendb4j`（社区）；
  - **Go**：`ravendb-go`（社区）；
  - **Ruby**：`ravendb-ruby`（社区）；
- **GraphQL**：`src/Raven.Server/Web/` 内置；
- **REST API**：`/docs/<id>`、`/queries`、`/studio` 等；
- **ETL**：
  - OLAP ETL → SQL Server / PostgreSQL / Snowflake；
  - Kafka ETL → Kafka；
  - Elasticsearch ETL → Elasticsearch；
- **BI**：通过 ETL 同步到 SQL 仓库；
- **容器化**：官方 Docker image；
- **Helm Chart**：官方 [RavenDB Helm](https://github.com/ravendb/ravendb-helm)；
- **K8s Operator**：官方 Operator（企业版）；
- **云服务**：
  - **RavenDB Cloud**（官方）；
  - **AWS / Azure / GCP**：Marketplace。

## 十八、核心论文与算法原理

- **MVCC**：基于 Voron 的 B+Tree + 写前日志（WAL）；
- **Raft 共识**：Diego Ongaro 论文；
- **Corax 索引**：参考 Roaring Bitmap + Lucene 架构；
- **B+Tree**：经典 B+Tree（Voron 内部）；
- **Change Vector**：类似 Lamport 时钟，用于多主复制排序；
- **订阅快照**：基于 Change Vector + Range Query；
- **向量索引**：HNSW（Hierarchical Navigable Small World）；
- **JSON.NET**：Newtonsoft.Json（社区事实标准）。

## 十九、源码学习路径

### 19.1 入门（1-2 天）

1. `src/Raven.Server/RavenServer.cs` + `RavenServerStartup.cs` — 服务入口；
2. `src/Raven.Client/Documents/Session/DocumentSession.cs` — 客户端 Session；
3. `src/Raven.Client/Documents/Queries/QueryExpressionProvider.cs` — LINQ provider；
4. `src/Raven.Server/Documents/DocumentsStorage.cs` — 服务端文档存储；
5. `src/Raven.Server/Documents/Queries/Dynamic/DynamicQueryRunner.cs` — 动态查询执行；
6. `src/Voron/Impl/StorageEnvironment.cs` — Voron 存储入口。

### 19.2 进阶（3-5 天）

1. `src/Voron/Impl/Transaction.cs` + `LowLevelTransaction.cs` — MVCC；
2. `src/Voron/Impl/PagerState.cs` — 页面管理；
3. `src/Voron/Impl/FreeSpace/` — 空间管理；
4. `src/Raven.Server/Documents/Indexes/CollectionOfIndexes.cs` — 索引集合；
5. `src/Raven.Server/Documents/Indexes/Auto/AutoMapIndex.cs` — 自动索引；
6. `src/Raven.Server/Documents/Subscriptions/SubscriptionStorage.cs` — 订阅存储；
7. `src/Raven.Server/Documents/Patching/` — Patching 引擎。

### 19.3 高级（1 周+）

1. `src/Raven.Server/Rachis/` — Raft 共识完整实现；
2. `src/Raven.Server/Documents/Sharding/` — 分片逻辑；
3. `src/Raven.Server/Documents/Indexes/Map/Reduce/IndexWriter.cs` — Map/Reduce 索引；
4. `src/Raven.Server/Indexing/Corax/` — Corax 索引引擎；
5. 自己 build RavenDB（`.NET 8 SDK`） + 启动 + Studio；
6. 跟踪一次 `LoadDocument("orders/1")`：客户端 → wire protocol → DocumentsStorage → Voron Transaction → StorageEnvironment。

### 19.4 源码规模

- **.cs 文件数**：约 **8,000~10,000 个**；
- **语言**：C# 100%（少量 F#）；
- **构建系统**：MSBuild + .NET SDK；
- **测试**：xUnit + NUnit + 测试套件 `Raven.TestDriver`（集成测试）；

## 二十、学习资源

- **官方文档**：[ravendb.net/docs](https://ravendb.net/docs/article-page/intro)；
- **官方教程**：[ravendb.net/tutorials](https://ravendb.net/learn/bootcamp)（**强烈推荐**的实操课）；
- **官方博客**：[ayende.com/blog](https://ayende.com/blog/)（Oren Eini 个人博客，含大量深度技术文章）；
- **官方书籍**：[RavenDB High Performance](https://www.amazon.com/dp/B07L7KQTCY)（Oren Eini 著作）；
- **社区论坛**：[groups.google.com/g/ravendb](https://groups.google.com/g/ravendb)；
- **GitHub Discussions**：[github.com/ravendb/ravendb/discussions](https://github.com/ravendb/ravendb/discussions)；
- **视频**：RavenDB Workshop、YouTube 频道；
- **Reddit**：[r/ravendb](https://www.reddit.com/r/ravendb/)。

## 二十一、可移植性与部署

- **支持的平台**：Windows Server 2019+、Linux（Ubuntu 20.04+ / RHEL 8+ / Debian 11+ / Alpine）、macOS（开发）、Docker、Kubernetes；
- **容器化**：官方 Docker image；
- **Helm Chart**：[github.com/ravendb/ravendb-helm](https://github.com/ravendb/ravendb-helm)；
- **K8s Operator**：[github.com/ravendb/ravendb-operator](https://github.com/ravendb/ravendb-operator)；
- **嵌入式部署**：通过 `Embedded` 提供 .NET 应用嵌入；
- **最小硬件**：1 vCPU / 2GB RAM（开发）；生产建议 8 vCPU / 32GB RAM / SSD；
- **依赖**：.NET 8（最新 LTS）。

## 二十二、变体与衍生项目

- **RavenDB Cloud**（官方托管）；
- **RavenDB 4.x → 5.x → 6.x**：长期演进（Voron 化、Corax 化）；
- **RavenDB Studio**：内置 Web 管理界面；
- **RavenDB ETL**：内置跨数据库同步；
- **RavenDB TestDriver**：测试驱动框架；
- **RavenDB Embedded**：嵌入式模式；
- **RavenDB C++ Client**：社区 C++ SDK；
- **Open Raven**（非关联项目）：云安全 posture management。

## 二十三、风险与陷阱

### 23.1 协议兼容性陷阱

- **协议版本**：RavenDB 5.x → 6.x 协议不兼容，需升级客户端；
- **JSON 序列化**：Newtonsoft.Json 默认，System.Text.Json 需配置；
- **LINQ provider 限制**：复杂 LINQ 不翻译（如自定义方法）；
- **Corax vs Lucene**：全文索引 5.x（Lucene）→ 6.x（Corax）切换需重新索引；
- **Change Vector 兼容性**：集群间升级时需处理；
- **集群 quorum**：3 节点只支持 1 个故障；5 节点支持 2 个；
- **Sharding**：企业版才有，切换困难；
- **跨集群同步**：Hub of Hubs 配置复杂；

### 23.2 性能陷阱

- **Map/Reduce 索引**：首次建立耗时长；
- **Corax 全文索引**：大文档（>10MB）索引效率下降；
- **Subscriptions 慢消费**：批处理不及时会导致 backlog 增长；
- **大文档**：单文档 > 1MB 性能下降；
- **过多索引**：每个索引都要更新，索引更新阻塞写入；
- **Sharding 跨片查询**：性能不如单库；
- **Lucene → Corax 迁移**：重建期间查询性能退化；
- **写入放大**：Raft 日志 + MVCC 版本链对纯文档负载的开销；
- **Vector 索引**：内存占用与召回率平衡需调优；

### 23.3 运维陷阱

- **备份恢复**：Periodic Backup 恢复需停机（应用级快照恢复）；
- **集群升级**：必须滚动升级，不可全停升级；
- **资源监控**：Raft 日志压缩、MVCC 版本清理；
- **跨节点时钟漂移**：Raft 假设时钟单调递增；
- **License 失效**：企业版 License 过期后集群自动禁用；
- **Studio 权限**：Studio 是 Web 端，需配置 admin token；
- **TLS 证书**：跨节点通信需 mTLS；
- **磁盘满**：Voron 强制要求至少 10% 空闲空间；
- **慢查询监控**：需 Studio + Prometheus；
- **索引文件膨胀**：索引需定期 compaction（`IndexesStorage.PrefixBufferSize`）；

## 二十四、学习路径

### 24.1 入门（1 周）

1. 跑官方 [Bootcamp](https://ravendb.net/learn/bootcamp)；
2. 启动 RavenDB（Docker）+ Studio（`http://localhost:8080/studio`）；
3. 用 .NET SDK 写一个 CRUD；
4. 用 LINQ / RQL 写一个查询；
5. 创建一个简单的 Map Index；
6. 阅读 `src/Raven.Server/RavenServer.cs` + `src/Raven.Client/Documents/Session/DocumentSession.cs`。

### 24.2 中级（2-3 周）

1. 部署 3 节点集群；
2. 测试故障切换；
3. 启用 Subscriptions 写实时管道；
4. 创建 Map/Reduce 索引；
5. 启用全文搜索（Corax）；
6. 阅读 `src/Voron/Impl/StorageEnvironment.cs`；
7. 跟踪一次写入：客户端 → Session → DocumentsStorage → Voron → WAL。

### 24.3 进阶（1 个月+）

1. 部署 Sharding（企业版）；
2. 部署跨集群 ETL；
3. 集成 OpenTelemetry；
4. 启用向量索引；
5. 性能基准 vs MongoDB / Couchbase；
6. 阅读 Rachis Raft 源码；
7. 阅读 Corax 索引源码；
8. 贡献 RavenDB 新功能（如自定义索引、新操作符）。

## 二十五、与本项目的关联

`engineering/` 中**没有 RavenDB**实现，但有以下可借鉴模式：

- `engineering/src/db/bufmgr.c` 的 buffer pool 可参考 `src/Voron/Impl/FreeSpace/` 设计；
- `engineering/src/db/wal.c` 的 WAL 可参考 `src/Voron/Impl/Journal/`（Voron 的预写日志）；
- `engineering/include/db/btreeam.h` 的 B+Tree AM 可参考 Voron 的 B+Tree 实现（`src/Voron/Impl/`）；
- `engineering/src/db/sql_engine.c` 的 SQL 执行器可参考 `DynamicQueryRunner.cs`（LINQ → RQL → 索引执行）；
- `engineering/src/db/transactions/` 的事务管理可参考 `src/Raven.Server/Documents/TransactionalStorage` 设计；
- `engineering/src/db/heapam.c` 的堆表 AM 可参考 `src/Raven.Server/Documents/DocumentsStorage` 的文档存储设计；
- **ACID 多文档事务** 模式可应用到 `engineering/src/db/` 中需要事务支持的模块；
- **MVCC 实现**（Voron 的 `Transaction.cs` + `LowLevelTransaction.cs`）是 `engineering/src/db/wal.c` 升级的参考。

## 二十六、Corax 倒排索引引擎：三态 posting list、PForDelta 与频次量化

Corax（`src/Corax/`）是 RavenDB 6.x 起取代 Lucene.Net 的自研倒排索引引擎。它的性能来源不是"更好的压缩算法"，而是三条互相咬合的工程决策：**用位域把 term→posting 的引用压进一个 `long`**、**posting list 随基数增长在三态表示之间自动跃迁**、**posting list 正文用 PForDelta 帧结构编码并交给 SIMD 解码**。本节逐条拆开。

### 26.1 三态存储模型：Single / SmallPostingList / PostingList

倒排索引的 `entriesToTerms` 是一个有序 B+Tree（Voron `Lookup<T>`），key 是 term，value 是一个 `long`。这个 `long` 不是"文档 ID"，而是一个**打包后的容器引用**——低 2 bit 决定容器的物理形态：

```
// src/Corax/Indexing/TermIdMask.cs:7-16
[Flags]
public enum TermIdMask : long
{
    Single = 0,             // 0b00：term → 单个 entry，entryId 直接内联在 long 里
    EnsureIsSingleMask = 0b11,
    SmallPostingList = 1,   // 0b01：term → 内联 PFor 小 posting list
    PostingList = 2         // 0b10：term → 独立的 PostingList 容器（外链）
}
```

容器 ID 保证 4 字节对齐（`TermIdMask.cs:4-6` 的注释），所以低 2 bit 天然空闲，可安全用作标志位。完整打包格式在 `src/Corax/Utils/EntryIdEncodings.cs:15-16` 直接写死为注释：

```
[CONTAINER_ID | 54b][FREQUENCY | 8b][CONTAINER_TYPE | 2b]
```

对应常量：`EntryIdOffset = FrequencySizeInBits + ContainerTypeOffset = 8 + 2 = 10`，`MaxEntryId = 1L << 54`（`EntryIdEncodings.cs:23-27`）。编解码就是两个位运算（`EntryIdEncodings.cs:29-35` 与 `:42-45`）：

```csharp
// Encode（EntryIdEncodings.cs:29-35）
return FrequencyQuantization(count) << ContainerTypeOffset
     | entryId << EntryIdOffset
     | (long)containerType;

// Decode（EntryIdEncodings.cs:42-45）
return (entryId >> EntryIdOffset,
        FrequencyReconstructionFromQuantization((entryId >> ContainerTypeOffset) & 0xFFL));
```

**跃迁规则**（`src/Corax/Indexing/IndexWriter.cs:1661-1754` 的 `AddEntriesToTerm` 三分支）：

```text
AddEntriesToTerm(idInTree, additions, removals):
    if 低2bit == PostingList(0b10):          // 已是大容器
        return MergeViaLargePostingList(...)  // IndexWriter.cs:1663-1666
    if 低2bit == SmallPostingList(0b01):     // 内联小容器
        Decode(PFor) → merge → 见下方判定
    else:                                     // Single(0b00)
        return UpgradeFromSingle(...)          // IndexWriter.cs:1671

# 小 posting list 分支（IndexWriter.cs:1674-1747）
merged ← decode(旧) ++ additions ++ removals
if 需要排序:  SortEntriesAndRemoveDuplicatesAndRemovals(merged)   # :1710-1713
if merged 为空: 删除 term                                       # :1715-1720 → RemoveTermId
if PFor(merged) 放不进临时缓冲:  AddNewTermToSet() 升级为大容器   # :1723-1728 → UpdateTermId
if 新编码长度 == 旧长度: 原地覆盖容器内容                          # :1730-1738 → NothingToDo
else: 删除旧容器 + 重新分配                                      # :1740-1746 → UpdateTermId
```

关键点在于**跃迁是单调的**：一个 term 一旦进入 `PostingList` 态就永不回退，所以 `LargePostingListsSet`（`IndexWriter.cs:2037-2038`）只需要累加、永远不需要清理——这是一条明确的"只增不减"记账捷径。删除路径 `RecordDeletion`（`IndexWriter.cs:742-801`）按低 2 bit 分派，分别解码 PostingList（`:748-773`）或 PFor 小容器（`:775-801`），然后把 entryId 灌进 `_deletedEntries` 做交集判定——注意 1024 长度的 `stackalloc` 缓冲（`:755`、`:782`），这是纯栈上分块解码，无堆分配。

**空间复杂度对比**（设 posting list 长度 n，文档数 N）：

| 形态 | 存储 | 空间 |
| --- | --- | --- |
| Single | 0 额外字节 | Θ(1) |
| SmallPostingList | PFor 编码后 n' 字节 + 计数前缀 | Θ(n' )，n' ≈ 2n bits（理想 delta） |
| PostingList | 容器头 + PFor 流 | Θ(n'/4KB) 页对齐 |

由于 PFor 的块宽随数据自适应，**低基数字段（枚举、布尔、状态码）几乎永远停留在 Single 态，零额外存储**——这是 Corax 相比 Lucene.Net 每字段一个 segment 的最大结构性优势。

### 26.2 PForDelta：帧分离 + 异常值外提 + SIMD

小容器与大容器正文都用同一套编解码器，位于 `src/Voron/Util/PFor/`（注意：**不在 Corax 目录**，它挂在 Voron 下，因为 Corax 的 posting list 最终写进 Voron 容器）。编码器 `FastPForEncoder.cs:56-151`，解码器 `FastPForDecoder.cs:102-237`。

**帧结构（metadata 流 + data 流分离）**：编码器按 **256 个 delta 一块**处理（`FastPForEncoder.cs:88`、`:383`）。每块的 metadata 是 `[块尾数 bestB][异常数 exceptionCount][若>0 则 maxB][越界位置 j...]`，data 流是 SIMD 位打包的定宽数据。

**块宽选择是带代价函数的优化，不是简单的 max 位宽**（`FastPForEncoder.cs:381-420`）：

```text
FindBestBitWidths(block[256]):
    frequencies[w] ← #{ v ∈ block : bitlen(v) == 32 - lzcnt(v) }   # :386-390 直方图
    maxB ← max{ w : frequencies[w] > 0 }                            # :391-396
    bestB ← maxB; bestCost ← maxB * 256
    for curB = maxB-1 down to 0:                                    # :402-417 逐位下调
        exceptionsCount += frequencies[curB + 1]
        curCost = exceptionsCount * 8                                # exceptionOverhead
                  + exceptionsCount * (maxB - curB)                  # 异常值高位额外比特
                  + curB * 256                                       # 主数据位宽成本
        if curCost < bestCost: bestB ← curB; bestExceptionCount ← exceptionsCount
    return (bestB, maxB, bestExceptionCount)
```

即在"每块省 `ΔB × 256` 比特"和"多存 `ΔB` 比特给每个异常值 + 8 字节元数据开销"之间做**精确的比特级 trade-off**，等价于 PForΔ 原论文的优化目标，但实现为 O(16) 的闭式扫描（不是迭代剪枝），且直方图用 `Lzcnt` 一次扫 256 个值构建。异常值存进 `_exceptions[maxB - bestB]` 桶（`FastPForEncoder.cs:371-372`），并按桶宽 `j` 做 `BitPacking.UnpackSegmented`（`FastPForDecoder.cs:82`）。

**SIMD 位打包/解包**：解包走 `SimdBitPacking<NoTransform>.Unpack256(0, _input, buffer, numOfBits)`（`FastPForDecoder.cs:146`），一次 256 个 uint。x86 走 AVX2、ARM 走 NEON，由 .NET `System.Runtime.Intrinsics` 在运行时选择。

**两类旁路元数据标记**：
- `VarIntBatchMarker = 255`（`FastPForEncoder.cs:14`）：尾部不足 256 个的余数块走 VariableByte 编码（`FastPForEncoder.cs:112-120` 写入，`:127-139` 解码）；
- `BiggerThanMaxMarker = 254`（`FastPForEncoder.cs:15`）：某个 delta 超过 `uint.MaxValue`（即 entryId 位移溢出），把 16 字节高位直接裸写进 metadata（`FastPForEncoder.cs:136-150`），解包时记下偏移延后处理（`FastPForDecoder.cs:119-126`）。

**共享前缀优化**：`RemoveSharedPrefix()`（`FastPForEncoder.cs:423-...`）发现所有 entryId 的低 10 位都相同（`PrefixSizeBits = 10`，`:18`）时，把这 10 bit 整体存进 `PForHeader.SharedPrefix` 一次，全块只存高位。entryId 由 4 字节对齐保证低 2 bit 恒为 0，再往上的 8 bit 常常也相同——对刚分配过的连续 entryId 序列，压缩比额外提升明显。

**复杂度**：编码 `O(n · (b/64 + 1))`（b = 选中块宽），解码 `O(n · b/64)`；空间 `O(n · b/8)` 字节。b 由数据自适应，典型 delta 分布下 b ≈ log₂(平均间隔) + log₂(256)。

### 26.3 频次对数量化：4 bit 指数 + 4 bit 尾数的对数量化器

posting list 里每个 entry 还要带一个词频（用于 BM25）。词频是天然长尾的小整数，Corax 用 **8 bit 对数量化器** 压掉（`EntryIdEncodings.cs:149-229`）：

```text
查表预计算（EntryIdEncodings.cs:151-158）：
    Step[i]           = 2^(4+2i)          i ∈ [0,16)   # 指数级数
    StepSize[i]       = 16        (i=0)
                      = Step[i] - Step[i-1] (i>0)
    LevelSizeInStep[i]= StepSize[i] / 16   # 每级 16 个尾数格，步长
    FrequencyTable[q] = Reconstruct(q)     # q ∈ [0,256)，解码查表，O(1)

量化（EntryIdEncodings.cs:191-201，x86 走 Lzcnt）：
    if f < 16: return f                                   # 小值直通
    lz   = Lzcnt.LeadingZeroCount((uint)f)                 # 32-bit
    level= (28 - lz + (lz & 1)) >> 1                      # 奇偶修正 → 精确 floor(log2)
    mod  = (f - Step[level-1]) / LevelSizeInStep[level]   # 0..15
    return (level << 4) | mod                             # 高 4 bit 指数 + 低 4 bit 尾数

反量化（EntryIdEncodings.cs:221-229）：
    level   = (q >> 4) & 0xF
    if level == 0: return q
    mantissa= q & 0xF
    return Step[level-1] + LevelSizeInStep[level] * mantissa
```

误差界：第 `level` 级覆盖 `[Step[level-1], Step[level])`，量程 `3 · 2^level`，16 格 → **相对误差 ≤ 1/16 ≈ 6.25%**。实际验算：`f=16 → level=1, mod=0 → q=16 → 还原 16`（无损）；`f=20 → level=1, mod=(20-16)/3=1 → q=17 → 还原 19`（-5%）。对 BM25 的 `tf` 因子而言这个精度绰绰有余。

`(28 - lz + (lz & 1)) >> 1` 这个式子是**纯位技巧的 `floor(log2(f))`**：因为 `lz = 31 - floor(log2(f))`，直接 `28 - lz` 会因奇偶抖动差 1，补上 `lz & 1` 后右移即得精确整数对数，无分支、无查表。ARM 版本 `ArmLzcntFrequencyQuantization`（`:177-188`）是同一式的 64-bit 变体，`FrequencyQuantization`（`:167-175`）按 `Lzcnt` / `Arm64` / 标量三级派发。

**批量解码的 SIMD 化**：查询时只需要 entryId、不需要频次，于是走 `DecodeAndDiscardFrequency`（`EntryIdEncodings.cs:129-137`）——AVX2 路径 `Vector256.ShiftRightLogical` 一次处理 4 个 `long`（`:69-92`），NEON 路径 `AdvSimd.ShiftRightLogical` 一次 2 个（`:94-117`），标量回退 `:119-126`。删除路径 `RecordDeletion` 就用它（`IndexWriter.cs:759`、`:788`）。

### 26.4 数值范围查询：有序 B+Tree 上的双向定界扫描

**注意：Corax 不使用 Lucene 式的前缀编码数值块（prefix-coded numeric trie）。** 源码里 `NumericUtils.PrefixCoded*` 只出现在旧 Lucene 路径（`src/Raven.Server/Documents/Indexes/Persistence/Lucene/IndexedTerms.cs:98`、`src/Raven.Server/Documents/Queries/Facets/FacetedQueryParser.cs:331-349`）。Corax 的做法不同：

- **索引侧**：`int` / `long` 字段写入 `Lookup<Int64LookupKey>`，`double` 字段写入 `Lookup<DoubleLookupKey>`（`IndexWriter.cs:1863`、`:1938`），每个**精确数值**一个 term，不做分桶、不做精度截断。double 的槽位表示是 `BitConverter.DoubleToInt64Bits(term)`（`IndexWriter.cs:1955`），但**比较时解码回 double 再比**（`src/Voron/Data/Lookups/DoubleLookupKey.cs:55-59`），因此 `-0.0` 与 `+0.0` 被视为相等（`:71-75`）——数值序在零点附近不是严格全序，这是刻意的取舍。
- **查询侧**：`TermNumericRangeProvider<TLookupIterator, TLow, THigh, TVal>`（`src/Corax/Querying/Matches/TermProviders/TermProvider.NumericRange.cs:14-189`）通过 C# 泛型把 `Exclusive` / `Inclusive` 抹成编译期常量，开/闭区间**没有运行时分支**。

```text
PrepareKeys(TermProvider.NumericRange.cs:51-113)：
    startKey ← IsForward ? low : high        # :53-54 反向迭代时先定上界
    Seek(startKey);  MoveNext(key)           # :56-57
    if 空 → _isEmpty = true                   # :58-61

    # 开区间需要跳掉恰好等于边界的那个 term（:63-83）
    skipFirst ← IsForward
              ? (TLow == Exclusive  && key == low)
              : (THigh== Exclusive  && high ≤ key)
              : (THigh== Inclusive && high < key)
    if skipFirst:  MoveNext(key); 收紧 low/high 到 key

    if _skipRangeCheck: return                # :85-86 无界查询：起点之后全都要，直接线性扫到 EOF
    Seek(finalKey);  MoveNext(key, lastTermId) # :89-90 反向再定位一次，拿到"最后一个 term id"
    _includeLastTerm ← 闭区间比较决定是否含端点  # :96-112
```

`_skipRangeCheck`（`:38-45`）是一个编译期常量折叠的**无界快路径**：当查询形如 `Total > 0`（`high == long.MaxValue`）或 `Total < 100`（`low == long.MinValue`）时，直接跳到末尾/开头交给线性 `Fill` 扫完，**省掉一次 Seek**。`Fill(Span<long> containers)`（`:117-121`）把剩余 term id 批量灌进调用方的 `Span`，配合 `PostingList` 的 SIMD 解码。

复杂度：范围查询 `O(log T + k)`（T = 不同数值个数，k = 命中 term 数），加 posting list 归并与 doc 求交 `O(Σ|posting(t)|)`。这比 Lucene 的数值块做法更省内存（无分桶碎片），代价是**字典必须存全部不同数值**——数值基数极高时字典会膨胀，这是 Corax 数值索引的真实权衡点。

### 26.5 分析链与字段索引模式

字段的分词/归一化策略由 `FieldIndexingMode` 决定，在 `src/Corax/Analyzers/AnalyzerScope.cs:45-54` 做静态分派：

```csharp
_analyzer = mode switch {                                 // :47-54
    FieldIndexingMode.Normal => fieldsMapping.DefaultAnalyzer,
    FieldIndexingMode.Search => fieldsMapping.SearchAnalyzer(fieldName),   // n-gram
    FieldIndexingMode.No      => Analyzer.CreateDefaultAnalyzer(allocator),
    FieldIndexingMode.Exact   => fieldsMapping.ExactAnalyzer(fieldName),  // 不分词
    _ => ThrowWhenAnalyzerModeNotFound(mode)
};
```

分析器本身是 **Tokenizer + 最多 3 级 Transformer 的固定流水线**（`src/Corax/Analyzers/Analyzer.cs:334` 的 `Create<TTokenizer, TTransform1, TTransform2, TTransform3>`，`:475-510` 的 `With<>` 组合子），全程 `Span<byte>` + `Span<Token>` 零分配；缓冲从 `Analyzer.BufferPool` / `Analyzer.TokensPool` 两个 `ArrayPool` 租借（`IndexWriter.cs:91-93` 归还，`:2064-2096` 的 `Dispose`）。默认分析器是 `KeywordTokenizer` + `ExactTransformer`（`Analyzer.cs:14`），小写化是 `KeywordTokenizer` + `LowerCaseTransformer`（`:15`）。

### 26.6 源码路径

- `src/Corax/Indexing/TermIdMask.cs:7-16` — 三态容器标志位；
- `src/Corax/Utils/EntryIdEncodings.cs:15-16`（打包 schema 注释）、`:23-27`（常量）、`:29-45`（编解码）、`:149-229`（频次量化/反量化）、`:69-117`（SIMD 解码）；
- `src/Corax/Indexing/IndexWriter.cs:1661-1754`（三态跃迁）、`:2005-2043`（新 term 建立/升级）、`:742-801`（删除路径）、`:1861-1980`（数值字段写入）；
- `src/Voron/Util/PFor/FastPForEncoder.cs:381-420`（块宽代价函数）、`:56-151`（SIMD 帧编码）、`:423+`（共享前缀）；
- `src/Voron/Util/PFor/FastPForDecoder.cs:45-92`（头解析）、`:102-238`（解码）；
- `src/Corax/Querying/Matches/TermProviders/TermProvider.NumericRange.cs:38-45`（无界快路径）、`:51-113`（双向定界）、`:117-121`（批量 Fill）；
- `src/Corax/Analyzers/AnalyzerScope.cs:45-53`（字段模式 → 分析器）；
- `src/Corax/Querying/Matches/MultiTermMatch.cs:196-252`（读侧三态分派）。

## 二十七、Voron 的 MVCC：三阶段提交、Scratch 文件与 Journal 截断

Voron（`src/Voron/`）是 RavenDB 自研的 B+Tree 嵌入式存储引擎（§6.1 已给出组件清单）。本节补上"多版本如何可见、旧版本何时可回收、WAL 何时可删"这三条真正的算法链。

### 27.1 三阶段提交：内存 → 日志 → 生效

`LowLevelTransaction.Commit()`（`src/Voron/Impl/LowLevelTransaction.cs:1101-1118`）把提交拆成三段，每段之间有独立的故障语义：

```text
CommitStage1_CompleteTransaction()          # LowLevelTransaction.cs:1277-1303
    释放 _pagesToFreeOnCommit 栈中的页       # :1288-1291  (栈见 :94 / :1010-1012)
    _txHeader.LastPageNumber = NextPageNumber - 1     # :1296
    _state.Root.CopyTo(ref _txHeader.Root)            # :1298  根指针快照
    _txHeader.TxMarker |= TransactionMarker.Commit    # :1300  ← 持久化提交标志
    raise LastChanceToReadFromWriteTransactionBeforeCommit  # :1302

CommitStage2_WriteToJournal()               # LowLevelTransaction.cs:1250-1275
    n = _journal.WriteToJournal(this)       # :1257  脏页顺序写入 WAL
    FlushedToJournal = true                 # :1258
    失败 → _txState |= TxState.Errored 并抛出 # :1270-1274

CommitStage3_DisposeTransactionResources()  # LowLevelTransaction.cs:1311-1346
    ValidateAllPages()                      # :1318
    Committed = true                        # :1320
    _updatePageTranslationTableAndUnusedPages?.ExecuteAfterCommit()  # :1322 换页表生效
    _env.TransactionAfterCommit(this)       # :1324  推进 CurrentReadTransactionId
    失败 → SetCatastrophicFailure() 并抛出  # :1340-1345  注释明确：此时内存态已脏，必须重启恢复
```

关键设计是 **Stage 2 与 Stage 3 的分界**：WAL 落盘后事务已经"逻辑提交"，Stage 3 只是内存换页表替换。若 Stage 3 抛异常，代码注释（`:1313-1315`）明说"会污染内存态，需要重启并恢复"——所以 Stage 3 是**不可失败**路径，实现上只做指针交换。

异步提交版本 `BeginAsyncCommitAndStartNewTransaction`（`:1129-1146`）允许把 Stage 2 丢进 `Task.Run`（`:1147`）而调用方立刻开启新事务，用 `_asyncCommitNextTransaction` 链式传递 journal 文件引用计数（`:1326-1338`）——本质是 WAL 写入的**写合并流水线**，与 Group Commit 同构。

### 27.2 页面定位：generation-tagged 直接映射页缓存

MVCC 的读路径必须能在同一页号下切换不同版本的物理地址。Voron 的解法不是哈希表，而是一个 **1024 槽直接映射缓存 + 16 bit generation 号**（`src/Voron/PageLocator.cs`）：

```csharp
// PageLocator.cs:11-30
[StructLayout(LayoutKind.Explicit, Size = 20)]
private struct PageData {
    [FieldOffset(0)]  public long  PageNumber;
    [FieldOffset(8)]  public Page  Page;
    [FieldOffset(16)] public ushort Generation;
    [FieldOffset(18)] public bool   IsWritable;
}
private const uint CacheSize = 1024;      // :29
private const uint CacheMask = CacheSize - 1;   // :30
private ushort _generation = 1;           // :25
```

```text
TryGetReadOnlyPage(pageNumber)             # PageLocator.cs:33-42
    bucket = pageNumber & 0x3FF            # 掩码 → 直接映射，无哈希冲突处理
    node   = _cache[bucket]
    return node.Generation == _generation
        && node.PageNumber == pageNumber
        && node.PageNumber != Invalid

Renew()                                   # PageLocator.cs:55-66  全量失效
    _generation++
    if _generation == 0:                   # 16 bit 溢出（每 65536 次 Renew）
        _generation = 1
        清零整张表                        # :64 一次性抹掉所有陈旧条目
```

**这就是失效算法的全部**：失效不遍历缓存、不写脏标记，只递增一个全局 `ushort`。因为槽内条目各自记录了被写入时的 generation 值，一轮 Renew 后所有旧条目自动失配。查询是一条无分支的"掩码 + 两次比较"，**O(1) 且无锁**（缓存是每线程私有的，`_generation` 只由持有者写）。

代价是容量固定 1024 项（约 1024 × 20 B ≈ 20 KB/线程），且天然冲突——两个相差 1024 的活跃页号会互相顶掉，退化为一次 `AcquirePagePointer`。对于工作集远小于 1024 页的 OLTP 负载足够；大扫描则退到 `Raven.Pal` 的 mmap 路径。

### 27.3 Scratch 文件与旧版本回收

写事务不原地改数据页，而是把新版本追加到 **scratch 文件**（`src/Voron/Impl/Scratch/`）。`ScratchBufferPool.Allocate`（`ScratchBufferPool.cs:211-250`）三级降级：先试已分配缓冲（`TryGettingFromAllocatedBuffer`）、再试文件尾部连续空间（`:224-225`）、最后 `EnsureContinuous` 扩展文件（`ScratchBufferFile.cs:135-143`，直接 `_lastUsedPage += sizeToAllocate` 顺序追加）。

**可回收判定**是整个 MVCC 的 GC 根，逻辑只有三行（`ScratchBufferFile.cs:201-210`）：

```csharp
public bool HasActivelyUsedBytes(long oldestActiveTransaction) {
    if (_allocatedPagesCount > 0) return true;            // :203-204 还有活页
    if (oldestActiveTransaction > _txIdAfterWhichLatestFreePagesBecomeAvailable)
        return false;                                     // :206-207 最老活跃事务已越过该文件
    return true;
}
```

即：**只要 (a) 文件里还有未释放的页，或 (b) 系统中最老的活跃读事务还没越过该文件的"最后释放点"，这个 scratch 文件就既不能合并也不能删除**。上层据此计算可删集合（`ScratchBufferPool.cs:718-719`）：

```csharp
CanBeDeleted = scratchBufferItem != current
            && scratchBufferItem.File.HasActivelyUsedBytes(oldestActiveTransaction) == false;
```

`oldestActiveTransaction` 的推进由 `LowLevelTransaction` 的注册/注销维护，这正是 MVCC 的"最老读者快照"（oldest-reader）语义。复杂度：回收判定 **O(1) 每文件**，全量扫描 O(S)（S = scratch 文件数），无需遍历页内引用计数——**用整文件粒度换取 O(1)**。

### 27.4 Journal 截断：先 sync 数据文件，再删日志

`src/Voron/Impl/Journal/WriteAheadJournal.cs` 的 `JournalApplicator` 维护 `_journalsToDelete`（`:620`，`ConcurrentDictionary<long, JournalFile>`）。真正的删除在 `UpdateDatabaseStateAfterSync()`（`:1206-1243`）：

```text
UpdateDatabaseStateAfterSync()              # WriteAheadJournal.cs:1206-1243
    Interlocked.Add(_totalWrittenButUnsyncedBytes, -_currentTotalWrittenBytes)  # :1213
    UpdateFileHeaderAfterDataFileSync(_lastFlushed.JournalId,                # :1226
                                      _lastFlushed.TransactionId, ...)
    for toDelete in _lastFlushed.JournalsToDelete:                           # :1228
        if toDelete.Number > _lastFlushed.JournalId: continue                 # :1230 预检查
        if Options.IncrementalBackupEnabled == false:
            toDelete.DeleteOnClose = true                                    # :1233-1234
        _journalsToDelete.TryRemove(toDelete.Number)                          # :1236
        toDelete.Release()
```

**顺序不可交换**：`CallPagerSync()`（`:1255-1266`）先把脏数据页 fsync 到数据文件（`_dataPager.Sync(_totalWrittenButUnsyncedBytes)`，`:1259`），fsync 返回后**才**允许把覆盖了这些页的 journal 打上 `DeleteOnClose`。这就是经典的 WAL "checkpoint-and-truncate" 两阶段。

两个易错分支被显式处理：
- **正在删除的 journal 可能就是数据文件头里记录的那个** → 设 `ignoreLastSyncJournalMissing = true`，否则 `UpdateFileHeaderAfterDataFileSync` 会抛"数据文件头指向不存在的 journal"（`:1215-1224`）；
- **增量备份开启时不能删**（`IncrementalBackupEnabled == false` 才 `DeleteOnClose`，`:1233`），因为增量备份依赖历史 journal 做时间点恢复。

可观测字段 `JournalsToDeleteCount` / `JournalsToDelete`（`:689-690`）与 `ShouldFlush`（`:686`，仅当"自上次 flush 有新提交页"或"事务号已推进"才刷）共同构成 journal 生命周期的观测面。

### 27.5 Storage Compaction：全库 copy-and-swap

Voron 的空间回收**不是**在线页迁移，而是**整库拷贝后原子替换**（`src/Voron/Impl/Compaction/StorageCompaction.cs`）：

```text
StorageCompaction.Execute(srcOptions, compactOptions)   # StorageCompaction.cs:43-104
    if srcOptions.IncrementalBackupEnabled:
        throw CannotCompactBecauseOfIncrementalBackup  # :49-50  增量备份期间禁止压缩
    srcOptions.ManualFlushing     = true                # :54 压缩期间禁止源库自动 flush
    compactOptions.ManualFlushing = true                # :55 目标库改为手动 flush

    using existingEnv  = new StorageEnvironment(srcOptions)      # :57
    using compactedEnv = new StorageEnvironment(compactOptions)   # :58
        CopyTrees(existingEnv, compactedEnv, ...)                 # :60  逐棵树复制
        compactedEnv.FlushLogToDataFile()                         # :62
        重试 ≤100 次 SyncDataFile()，每次间隔 100 ms               # :65-90  抗 Windows 文件句柄争用
        compactedEnv.Journal.Applicator.DeleteCurrentAlreadyFlushedJournal()  # :93
        compactedEnv.Cleanup()                                    # :95
        minimalCompactedDataFileSize = NextPageNumber * PageSize # :97

    compactedDataFile.SetLength(minimalCompactedDataFileSize)    # :100-103 物理截断到实际用量
```

`CopyTrees`（`:106`）按树的物理类型分别处理：`CopyFixedSizeTreeFromRoot`（`:201`，如 `FreeSpace`/固定槽树）、`CopyVariableSizeTree`（`:272`）、`CopyTableTree`（`:449`）——即 Voron 的三类布局（fixed / variable-size / table）各自有专门的紧凑化逻辑。32 位下还会在事务过大时主动关事务（`ShouldCloseTxFor32Address`，`:618`）。

**取舍**：这条路径把在线碎片整理降级为"停机式全量重写 + 原子替换"，实现简单、无并发 bug 风险；代价是**需要约 2 倍磁盘空间**且压缩期间源库 IO 争用。所以 §23.3 说的"索引文件膨胀需定期 compaction"在 Voron 层面对应的是 `StorageEnvironment.Compact()` 这个运维任务，而不是后台自动的页迁移器。

### 27.6 源码路径

- `src/Voron/Impl/LowLevelTransaction.cs:1101-1118`（提交编排）、`:1250-1275`（Stage2 写日志）、`:1277-1303`（Stage1）、`:1311-1346`（Stage3）、`:1129-1146`（异步提交）；
- `src/Voron/PageLocator.cs:11-30`（页缓存结构）、`:33-42`（O(1) 查找）、`:55-66`（generation 失效）；
- `src/Voron/Impl/Scratch/ScratchBufferFile.cs:135-143`（顺序分配）、`:201-210`（可回收判定）；
- `src/Voron/Impl/Scratch/ScratchBufferPool.cs:211-250`（三级分配降级）、`:718-719`（可删集合）；
- `src/Voron/Impl/Journal/WriteAheadJournal.cs:620`（待删集合）、`:1206-1243`（sync 后删除）、`:1255-1266`（数据文件 fsync）、`:686-690`（可观测字段）；
- `src/Voron/Impl/Compaction/StorageCompaction.cs:43-104`（copy-and-swap）、`:201/272/449`（三类树复制）；
- `src/Voron/Data/Lookups/Lookup.cs:17`（持久化 B+Tree 本体）、`:1320`（`GetFor` 批量点查）。

## 二十八、Subscriptions 的 ack/重放语义与分片扇出聚合

§11 讲了 Subscriptions 的用法与保证，本节补两条真算法：**ack 之后为什么还会有重放（resend）**，以及**分片查询如何扇出、归并、二次 reduce**。

### 28.1 批次泵与 ack 的往返协议

服务端在一个连接上交替发 `Data` / `EndOfBatch`，客户端跑完用户回调后回 ack。客户端侧的泵在 `src/Raven.Client/Documents/Subscriptions/AbstractSubscriptionWorker.cs:654-709`：

```text
while (endOfBatch == false && 未取消):                  # :655
    msg = ReadNextObjectAsync(...)                      # :657
    switch msg.Type:
        Data             → incomingBatch.Add(msg)      # :665-667  只累积，不触发用户代码
        Includes / CounterIncludes / TimeSeriesIncludes → 分类暂存   # :669-680
        EndOfBatch       → endOfBatch = true           # :681-683  一个批次的数据收齐
        Confirm          → await AfterAcknowledgment(batch)         # :685-692
                           batch.Items.Clear()  ← 此刻才真正丢弃  # :691
        Error            → ThrowSubscriptionError(msg)               # :701-703
```

`Confirm` 分支是整个协议的**确认点**：只有收到它，`batch.Items.Clear()` 才释放文档句柄。`Data` 与 `EndOfBatch` 分离意味着服务端可以流式推大文档（附件/时序）而不必整体物化一个批次。

服务端回 ack 的处理在 `src/Raven.Server/Documents/Subscriptions/Processor/DocumentsDatabaseSubscriptionProcessor.cs:81-108`：

```text
AcknowledgeBatchAsync(batchId, changeVector)            # :81
    ItemsToRemoveFromResend.Clear()                      # :83
    for index = BatchItems.Count-1 down to 0:             # :89  逆序遍历（配合 RemoveAt）
        doc = GetDocumentOrTombstone(docContext, doc.DocumentId)   # :92 读最新版本
        if ShouldAddToResendTable(docContext, doc, doc.ChangeVector) == false:
            BatchItems.RemoveAt(index)                   # :95  确认已被消费，丢弃
    await SubscriptionConnectionsState.AcknowledgeBatchAsync(       # :100-101
             Connection.LastSentChangeVectorInThisConnection ?? "DoNotChange",
             batchId, BatchItems)                        # 剩余的进重放表
    if BatchItems?.Count > 0:                            # :103
        SubscriptionConnectionsState.NotifyHasMoreDocs() # :105  还有欠账，立刻唤醒取下一批
```

### 28.2 重放判定：ack 不是"我处理成功了"，而是"这个 change vector 被合并了"

这是本节最值得注意的一点。`ShouldAddToResendTable`（`:284-304`）**不看客户端的返回值**，而是拿"发送时记下的 change vector"和"数据库中该文档当前的 change vector"做冲突状态比对：

```csharp
private bool ShouldAddToResendTable(context, item, string currentChangeVector) {  // :284
    if (item.Document != null) {                                        // :286  文档还在
        var status = DocumentsStorage.GetConflictStatus(
            context, item.Document.ChangeVector, currentChangeVector,
            ChangeVectorMode.Version);                                  // :288
        switch (status) {
            case ConflictStatus.Update:       return true;              // :291-292 服务端又改了 → 必须重放
            case ConflictStatus.AlreadyMerged:
            case ConflictStatus.Conflict:     return false;             // :294-296 已合并或已冲突 → 丢弃
            default: throw new ArgumentOutOfRangeException(...);        // :298-299
        }
    }
    return false;                                                       # :303  文档已被删（tombstone）→ 丢弃
}
```

算法语义：

```text
ack(batchId) 时，对批内每个 doc：
    cur ← 数据库中 doc 的当前状态
    if cur 不存在:                 drop        # 已删除
    elif cv_current 支配 cv_sent:  drop        # AlreadyMerged
    elif 冲突（版本分叉）:          drop        # Conflict（由冲突解决流程接管）
    else:                          resend ← doc  # Update：ack 之后服务端又有新写
```

于是投递语义实际是 **at-least-once + 幂等收敛**：ack 只推进"已发送且服务端未见后续写"的基线；ack 之后的并发写会被主动塞回重放表（`NotifyHasMoreDocs()`，`:105`，实现在 `AbstractSubscriptionConnectionsState.cs:43`），保证客户端最终一定看到最新版本。这比"客户端说处理成功才推进"更强——**即使客户端处理完就崩溃，文档也不会永久丢失**，因为服务端在 ack 落地时就已完成"基线推进 + 变更补偿"的判定。

发送前的另一道过滤在 `ShouldSend`（`:126-184`）：`AlreadyMerged` 跳过（`:140-145`）、已在活跃批次中的跳过（`:147-152`）、归档策略 `ExcludeArchived` 跳过（`:179-184`）。

### 28.3 ack 本身走 Raft 日志

`SubscriptionConnectionsState.AcknowledgeBatchAsync`（`src/Raven.Server/Documents/Subscriptions/SubscriptionConnectionsState.cs:189-196`）：

```csharp
public async Task AcknowledgeBatchAsync(string changeVector, long batchId,
                                         List<DocumentRecord> addDocumentsToResend, ...)
{
    var command = GetAcknowledgeSubscriptionBatchCommand(changeVector, batchId, addDocumentsToResend);  // :191
    modifyCommand?.Invoke(command);                                                                     // :192
    var (etag, _) = await _server.SendToLeaderAsync(command);                                            // :194  ← 走集群共识
    await WaitForIndexNotificationAsync(etag);                                                          // :195
}
```

`ServerStore.SendToLeaderAsync`（`src/Raven.Server/ServerWide/ServerStore.cs:3213-3226`）→ `SendToLeaderAsyncInternal`（`:3228-...`）：

```text
if (_engine.CurrentState == RachisState.Leader && _engine.CurrentLeader?.Running):
    return await _engine.PutAsync(cmd)        # ServerStore.cs:3241  _engine : RachisConsensus
    catch (ConcurrencyException | NotLeadingException) → continue 重试  # :3243-3247
否则：等待 leader tag 或超时                                     # :3249-3269
```

`RachisConsensus.PutAsync`（`src/Raven.Server/Rachis/RachisConsensus.cs:1080-1087`）把命令交给 leader 追加进复制日志。也就是说**订阅位点在集群内是强一致的 Raft 复制状态**，切换 leader 后新 leader 从日志重放仍能恢复出正确的 ack 位置。`SubscriptionState`（`src/Raven.Client/Documents/Subscriptions/SubscriptionState.cs:17-36`）同时保存 `ChangeVectorForNextBatchStartingPoint`、`RaftCommandIndex`、`MentorNode`、`PinToMentorNode`、`LastBatchAckTime`——`RaftCommandIndex` 的存在本身就说明该状态是日志序而非本地态。

### 28.4 客户端重连：固定退避 + 有界异常窗口

`AbstractSubscriptionWorker.RunSubscriptionAsync`（`:794-870`）是订阅的持久性保证：

```text
while (未取消):                                        # :796
    try:
        CloseTcpClient(); await ProcessSubscriptionAsync()   # :803-809
    catch ex:
        while (_recentExceptions.Count >= 10): _recentExceptions.TryDequeue()  # :813-814
        _recentExceptions.Enqueue(ex)                          # :816  环形窗口，只留最近 10 个
        (shouldTryToReconnect, _redirectNode) =
            CheckIfShouldReconnectWorker(ex, ...)              # :832
        if shouldTryToReconnect:
            await WaitFor(GetTimeToWaitBeforeConnectionRetry()) # :835  固定间隔，非指数退避
            if _redirectNode == null:
                nextNodeIndex = (_forcedTopologyUpdateAttempts++) % curTopology.Count  # :841
                GetRequestedNode(curTopology[nextNodeIndex].ClusterTag, ...)          # :844 轮转选点
            OnSubscriptionConnectionRetry?.Invoke(ex)            # :857-858
        else: throw                                           # :865 不可重连错误直接上抛
```

退避间隔由 `GetTimeToWaitBeforeConnectionRetry()`（`:581`）返回 `_options.TimeToWaitBeforeConnectionRetry`——**固定间隔而非指数退避**，因为订阅重连本身是幂等的（重连后从 change vector 续传），无需为长故障拉长退避；异常环形窗口（10 个）仅用于诊断与"连续失败"判定，不影响重试。复杂度：单次重连 O(T_topology)，与分片数无关。

### 28.5 分片扇出：并行下发 → 归并 → 二次 reduce → 排序 → 分页

企业版 Sharding 下，查询不是转发而是**协调节点（orchestrator）扇出 + 归并**。主流程在 `src/Raven.Server/Documents/Sharding/Queries/ShardedQueryProcessor.cs:41-98`：

```text
ExecuteShardedOperations(scope)                          # ShardedQueryProcessor.cs:41
    commands = GetOperationCommands(scope)               # :50  为每个分片构造一条命令
    shards  = GetShardNumbers(commands)                  # :51  与查询参数里的 shard 过滤求交
    result  = ShardExecutor.ExecuteParallelForShardsAsync(shards, operation, token)  # :55
    HandleMissingDocument/Counter/TimeSeriesIncludes(...)# :69-87 跨分片 include 补采
    ReduceResults(ref result, scope)                     # :89   ← map/reduce 二次归约
    FilterAfterMapReduce(ref result, scope)              # :92   ← reduce 之后再过滤
    ProjectAfterMapReduce(ref result, scope)             # :95   ← 投影放到最后
    ApplyPaging(ref result, scope)                       # :97   ← 全局 offset/limit
```

**扇出**（`src/Raven.Server/Documents/Sharding/Executors/AbstractExecutor.cs:147-189`）是"启动全部 + 统一 await"，而不是 `Task.WhenAll`：

```text
ExecuteAsync(shards, operation, commands, token)        # AbstractExecutor.cs:147
    for position in 0..shards.Length:                    # :156
        cmd      = operation.CreateCommandForShard(n)    # :160
        executor = GetRequestExecutorAt(n)               # :164
        ctx      = executor.ContextPool.Allocate(...)    # :165  每分片独立上下文，避免共享
        t        = executor.ExecuteAsync(cmd, ctx, token)# :167  立即启动，不 await
        commands.Add(n, new ShardExecutionResult{ CommandTask = t, ... })   # :169-175
        if TExecutionMode == OneByOneExecution: await t  # :177-188  顺序模式才逐个等
    for (shardNumber, command) in commands:              # :193
        try { await command.CommandTask } catch { if ThrowOnFirstFailure: throw }  # :197-203
    → BuildResults(operation, commands)                   # :90 / :124-145
```

无 `Task.WhenAll` 是因为**必须逐个 await 才能在抛异常时保证其余分片命令已终结**，否则 `finally` 里的 `ContextReleaser.Dispose()`（`:113`）会归还仍在使用的上下文。`:98-111` 的注释明确写了这个原因。`BuildResults`（`:124-145`）最后调 `operation.CombineCommands(commands)`。

失败模式是**泛型化的**：`ExecuteParallelForShardsAsync`（`:61-62`）= `ParallelExecution` + `ThrowOnFirstFailure`；`ExecuteParallelAndIgnoreErrorsForShardsAsync`（`:73-75`）= `ParallelExecution` + `IgnoreFailure`。查询用前者，补采 include 用后者——include 缺失不应让整个查询失败。

**每分片的 limit 必须放大到 offset+limit**（`src/Raven.Server/Documents/Sharding/Queries/AbstractShardedQueryProcessor.cs:38`、`:316-336`）：

```csharp
const string LimitToken = "__raven_limit";              // :38
clone.Limit = new ValueExpression(LimitToken, ValueTokenType.Parameter);   // :316  替换为参数
var limit = ((Query.Limit ?? 0) + (Query.Offset ?? 0));  // :329
if (limit == 0) limit = Query.Start + Query.PageSize;   // :330-331
if (limit > int.MaxValue) limit = int.MaxValue;         // :333-334
modifiedArgs[LimitToken] = limit;                       // :336
modifications.Remove(nameof(IndexQueryServerSide.Start));// :338-339  分页改由协调节点统一做
```

这是分布式分页的**必要非精确性**：每个分片各取 `offset + limit` 条，协调节点归并排序后再切 `offset`——只要没有排序，语义不精确；有排序时结果正确（因为分片返回的是全局序的前 `offset+limit` 条的子集，必含全局前 `offset+limit` 条）。

**归并用线性扫描 k 路归并**（`src/Raven.Server/Documents/Sharding/Operations/Queries/ShardedQueryOperation.cs:157-186` + `src/Raven.Server/Documents/Replication/Senders/MergedEnumerator.cs:36-60`）：

```text
// ShardedQueryOperation.cs:157-161
mergedEnumerator = new MergedEnumerator<BlittableJsonReaderObject>(_sortingComparer)
for (shardNumber, cmdResult) in results: mergedEnumerator.AddEnumerator(GetEnumerator(...))

// MergedEnumerator.cs:36-60
MoveNext():
    if CurrentEnumerator != null and it.MoveNext() == false:
        WorkEnumerators.Remove(CurrentEnumerator)              # 耗尽即移除
    if WorkEnumerators.Count == 0: return false
    CurrentEnumerator = WorkEnumerators[0]                     # :53
    for index in 1..WorkEnumerators.Count:                      # :54
        if Compare(WorkEnumerators[index].Current,
                   CurrentEnumerator.Current) < 0:              # :55
            CurrentEnumerator = WorkEnumerators[index]          # :56
    CurrentItem = CurrentEnumerator.Current
    return true
```

`AddEnumerator`（`:22-34`）在加入时就 `MoveNext()` 一次做掉空表剪枝。**每输出一个元素扫描 k 个游标**，复杂度 `O(N·k)`（N = 总结果数，k = 分片数）；换成二叉堆是 `O(N log k)`。RavenDB 选了线性扫描——因为 k 通常是 2~9（Sharding 默认拓扑规模），`log₂9 ≈ 3.2` vs `k = 9`，线性扫描的分支预测友好度与无堆分配的常数因子反而更优。

**二次 reduce**（`src/Raven.Server/Documents/Sharding/Queries/ShardedQueryProcessorBase.cs:81-98`）是分布式 map/reduce 的关键：分片各自返回**部分聚合**，协调节点再跑一次 reduce：

```text
ReduceResults(result, scope)                              # ShardedQueryProcessorBase.cs:81
    if 非 map/reduce 索引 且 非 auto map-reduce 查询: return   # :83-84
    merger = CreateMapReduceQueryResultsMerger(result)    # :88
    result.Results = merger.Merge()                       # :89
    if orderByFields?.Length > 0:
        # 注释明确：apply ordering AFTER the re-reduce
        result.Results.Sort(new DocumentsComparer(orderByFields, extractFromData: true,
                                                   Query.Metadata.HasOrderByRandom))  # :94-95
```

`ShardedMapReduceQueryResultsMerger.Merge()`（`src/Raven.Server/Documents/Sharding/Queries/ShardedMapReduceQueryResultsMerger.cs:42-62`）按索引类型分两路：auto map-reduce 走 `ShardedAutoMapReduceIndexResultsAggregator.AggregateOn`（`:94-98`，基于 `AggregationResult` 的声明式聚合树）；静态 map-reduce 走"把分片结果当 map 输出重跑一次 `compiled.Reduce`"（`:71-81`）。排序必须放在 re-reduce 之后——因为分片内的局部序在归约后没有意义。

**注意 reduce 后还要再过滤/投影/分页**（`ShardedQueryProcessor.cs:92/95/97`），因为 WHERE 子句的部分条件（如依赖其他字段的 JS filter）无法下推；`FilterAfterMapReduce`（`:205-...`）逐条 `queryFilter.Apply(doc)`，遇到 `FilterResult.LimitReached` 就 `break` 提前退出。`ApplyPaging`（`ShardedQueryProcessorBase.cs:38-79`）最后统一切 `queryOffset` / `queryLimit`，其中 `queryLimit` 还要与 `Query.FilterLimit` 取小（`:56`）。

### 28.6 复杂度汇总

设 S = 分片数，N = 全局结果数，R = 各分片返回行数之和（R ≥ N），P = 最终页大小。

| 阶段 | 复杂度 | 出处 |
| --- | --- | --- |
| 扇出下发 | O(S) 次并发 HTTP，无 `WhenAll` | `AbstractExecutor.cs:156-189` |
| k 路归并 | O(N·S)（线性扫描游标） | `MergedEnumerator.cs:53-58` |
| 二次 reduce | O(R) + 聚合开销 | `ShardedMapReduceQueryResultsMerger.cs:71-81` |
| 排序 | O(N log N) | `ShardedQueryProcessorBase.cs:95` |
| 分页 | O(N)（切尾） | `ShardedQueryProcessorBase.cs:47-67` |

**通信放大**是这套设计的主要成本：每个分片返回 `offset + limit` 条，归并前的总量 `R ≤ S·(offset + limit)`。深分页（`offset` 很大）时 `R` 会线性膨胀——这与 §23.2 说的"Sharding 跨片查询性能不如单库"是同一个根因。

### 28.7 源码路径

- `src/Raven.Client/Documents/Subscriptions/AbstractSubscriptionWorker.cs:654-709`（批次泵）、`:794-870`（重连循环）、`:581`（重试间隔）；
- `src/Raven.Server/Documents/Subscriptions/Processor/DocumentsDatabaseSubscriptionProcessor.cs:81-108`（ack 处理）、`:284-304`（重放判定）、`:126-184`（发送前过滤）；
- `src/Raven.Server/Documents/Subscriptions/SubscriptionConnectionsState.cs:189-196`（ack 走 Raft）、`:222-234`（活跃批次判定）；
- `src/Raven.Server/ServerWide/ServerStore.cs:3213-3247`（leader 转发与重试）、`src/Raven.Server/Rachis/RachisConsensus.cs:1080-1087`（`PutAsync`）；
- `src/Raven.Server/Documents/Sharding/Executors/AbstractExecutor.cs:61-75`（三种执行模式）、`:124-145`（结果合并）、`:147-203`（扇出与等待）；
- `src/Raven.Server/Documents/Sharding/Operations/Queries/ShardedQueryOperation.cs:56-...`（`CombineResults`）、`:157-186`（归并装配）、`:197-...`（distinct 去重）；
- `src/Raven.Server/Documents/Replication/Senders/MergedEnumerator.cs:8-77`（k 路归并器）；
- `src/Raven.Server/Documents/Sharding/Queries/AbstractShardedQueryProcessor.cs:38`（`LimitToken`）、`:316-339`（limit 放大）；
- `src/Raven.Server/Documents/Sharding/Queries/ShardedQueryProcessor.cs:41-98`（编排主流程）、`:205-238`（reduce 后过滤）；
- `src/Raven.Server/Documents/Sharding/Queries/ShardedQueryProcessorBase.cs:38-79`（全局分页）、`:81-98`（re-reduce + 排序）；
- `src/Raven.Server/Documents/Sharding/Queries/ShardedMapReduceQueryResultsMerger.cs:42-62`（归约分派）、`:64-89`（静态索引重跑 reduce）。

## 二十九、参考资料

1. **RavenDB 官方仓库**：[github.com/ravendb/ravendb](https://github.com/ravendb/ravendb)；
2. **RavenDB 官方文档**：[ravendb.net/docs](https://ravendb.net/docs/article-page/intro)；
3. **RavenDB 教程**：[ravendb.net/learn/bootcamp](https://ravendb.net/learn/bootcamp)；
4. **Oren Eini 博客**：[ayende.com/blog](https://ayende.com/blog/)；
5. **RavenDB Cloud**：[cloud.ravendb.com](https://cloud.ravendb.com/)；
6. **Voron 设计文档**（GitHub Wiki）：[github.com/ravendb/ravendb/wiki/Voron](https://github.com/ravendb/ravendb/wiki/Voron)；
7. **Raft 论文**：Ongaro & Ousterhout, "In Search of an Understandable Consensus Algorithm", USENIX ATC 2014；
8. **Corax 介绍**：[ayende.com/blog/post/2023/03/building-corax](https://ayende.com/blog/)；
9. **RavenDB High Performance**：[O'Reilly 出版物](https://ravendb.net/books/book)；
11. **RavenDB 中文社区**：[github.com/ravendbchina](https://github.com/ravendbchina)；
12. **Sample Apps**：[github.com/ravendb/samples](https://github.com/ravendb/samples)。