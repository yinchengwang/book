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

## 二十六、参考资料

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