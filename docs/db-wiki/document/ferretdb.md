# FerretDB Wiki（MongoDB 兼容的 PostgreSQL 代理）

> 参考版本：FerretDB v2（仓库当前 commit `799235dab9e350655e72e65a0b24d849f7d68143`），运行于 Go 1.24，深度依赖 [Microsoft DocumentDB PostgreSQL 扩展](https://github.com/documentdb/documentdb)。

## 一、项目定位

FerretDB 是一个**用 Go 编写的开源 MongoDB 兼容服务器**，把 MongoDB wire protocol 翻译为 PostgreSQL（+ DocumentDB 扩展）上的查询。它存在的核心目的是：**让用户在不放弃 MongoDB 生态（驱动、工具、ORM）的前提下，把数据真正存进 PostgreSQL**，从而摆脱 MongoDB AGPL/SSPL 许可证带来的限制与商业风险。

- **痛点**：
  - MongoDB 2018 年起转向 **AGPL/SSPL**，大型企业担心合规与"不可控"；
  - PostgreSQL 是世界上最成熟、最可靠的关系数据库之一，但缺少 MongoDB 风格的文档 / 半结构化 API；
  - 已有大量 MongoDB 客户端驱动与工具（Compass、mongosh、各语言 driver），迁移到 PostgreSQL 原生 JSON 类型需要重写应用；
  - Microsoft DocumentDB（前身是 Citus/CouchDB）扩展在 PostgreSQL 内核之上提供 BSON/聚合管道原生支持，但缺少 MongoDB wire 兼容前端；
- **典型用户**：MongoDB 退订或受许可证影响的企业；希望 PG 统一 OLTP + 文档存储的小型公司；云厂商（AWS DocumentDB 的"真开源"替代选项）；SaaS 平台（希望一份数据既走 PG 报表，又走 Mongo API）；
- **核心口号**："**MongoDB on Postgres, with no vendor lock-in**"——`MongoDB atlas is not open source. FerretDB is.`。

## 二、发展历史与社区

- **起源**：2021 年 10 月（前 MongoDB 工程师 Peter Farkas 在 Vitess/MongoDB 经历后），当时目标是用 Go 写一个 MongoDB 协议前端，对接 PostgreSQL；
- **里程碑**：
  - 2021-12：v0.1，**SQLite 后端**（FerretDB 自带 BSON → SQL 翻译）；
  - 2022-02：v0.4，引入 **Tigris 后端**（兼容 S3 的云原生文档库）；
  - 2022-04：v0.6，引入 **PostgreSQL 后端**（早期 `internal/handlers/pg/` 结构）；
  - 2022-08：v0.9，可生产部署；
  - 2022-12：v1.0 GA（基于 PostgreSQL 后端）；
  - 2023-09：v1.10，移除 SQLite/Tigris 后端，专注 PostgreSQL；
  - 2024-12：v2.0 alpha，**深度集成 Microsoft DocumentDB 扩展**，SQL 翻译工作下沉到 PostgreSQL；
  - 2025：v2.x 系列成熟（移除 `internal/handlers/pg/`，改为 `internal/handler/` 单一入口 + `internal/documentdb/` 调 PG 扩展）；
- **维护状态**：活跃（每周多个 commit，CTO Peter Farkas 与 Microsoft DocumentDB 团队紧密合作）；
- **社区**：GitHub Discussions、Discord、`#ferretdb` on Matrix、年度社区会议（与 PGConf 联合）。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（完全 OSI 认证，与 MongoDB 的 SSPL 截然相反）；
- **商业版本**：无官方商业版；可在云厂商购买托管（AWS、DigitalOcean、Hetzner 等均提供）；
- **生态关系**：
  - 与 Microsoft DocumentDB 是**深度合作**关系（后者是 PG 扩展，提供 BSON/聚合管道原生支持）；
  - 与 PostgreSQL 主线版本同步支持（16/17）；
- **重要警告**：FerretDB 客户端 wire 协议兼容 MongoDB 5.0+ 的**子集**，部分高级特性（如 `$graphLookup`、Change Streams 的 resume token 兼容性、`$vectorSearch`）可能与官方 MongoDB 不完全一致。

## 四、业界主要使用方与案例

- **AWS**：以"DreamFactory"替代品在部分文档；
- **DigitalOcean / Hetzner / Render**：官方托管（Managed FerretDB）；
- **DreamFactory**：作为 MongoDB API 网关；
- **Aiven**：Aiven for FerretDB 托管；
- **Percona**：Percona Distribution for FerretDB（含 PG + DocumentDB + FerretDB）；
- **社区案例**：CMS 后台、产品目录（flexible schema）、IoT 设备元数据、AI 应用 embedding 元数据、CRM、博客内容存储；典型负载 10k~100k RPS、TB 级数据。

## 五、一句话对比定位

> FerretDB 是 **"MongoDB 协议前端 + PostgreSQL + DocumentDB 扩展"** 的**真正开源**组合；与 **MongoDB（SSPL、商业闭源）**、**AWS DocumentDB（MongoDB-compatible，闭源 + 商业）**、**RethinkDB（已停更）**、**Tigris（独立文档库，已不再兼容 FerretDB）** 一起构成 **Mongo API 兼容服务器** 阵营。FerretDB 的差异化是 **"真开源 + 依赖 PostgreSQL 生态（运维工具、备份、HA 全部继承 PG）"**。

## 六、整体架构（v2 重构版）

v2 与 v1 差异巨大，必须按新结构阅读。整体分层如下：

```
┌────────────────────────────────────────────────────────────────────────┐
│                  MongoDB 客户端（任意驱动）                              │
│   - mongosh / Compass / pymongo / mongo-go-driver / mongoose            │
│   - Data API（HTTP/JSON）                                               │
│   - MCP Server（listDatabases / find）                                  │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ MongoDB Wire Protocol（OP_MSG/OP_QUERY）
┌─────────────────────▼──────────────────────────────────────────────────┐
│                clientconn (internal/clientconn/conn.go:123)            │
│   - TCP/Unix/TLS listener（默认 27017）                                │
│   - 帧解码 → middleware.RequestWire → middleware.Middleware.Handle    │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│            Middleware (internal/handler/middleware/)                    │
│   - dispatcher.go：OpMsg → commands.go 命令表查找                       │
│   - metrics.go、mode.go、request.go、response.go                        │
│   - session/：lsid 管理                                                 │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│              Command Handlers (internal/handler/msg_*.go)              │
│   - msg_insert.go:33 / msg_find.go:28 / msg_update.go:33                │
│   - msg_aggregate.go:28 / msg_createindexes.go:35 / msg_delete.go:33    │
│   - msg_* 60+ 个 MongoDB 命令一对一映射                                  │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ Go 函数调用
┌─────────────────────▼──────────────────────────────────────────────────┐
│           DocumentDB Pool (internal/documentdb/pool.go:39)             │
│   - 封装 pgxpool.Pool + cursor registry + tracer（OTel）               │
│   - conn.go:27 单连接封装（含资源追踪）                                 │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ PostgreSQL Wire Protocol（pgxpool）
┌─────────────────────▼──────────────────────────────────────────────────┐
│       Microsoft DocumentDB 扩展（独立 PG 扩展）                          │
│   - documentdb_api.* 聚合管道函数                                       │
│   - BSON 编解码、$group / $bucket / $facet 翻译                         │
│   - 索引（GIN、$vectorSearch 等）                                       │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│                  PostgreSQL 16/17 主线                                   │
│   - 文档以 JSONB / BSON 形式存于普通表                                   │
│   - 复用 PG 备份/HA（pg_basebackup、Patroni、Citus）                     │
└────────────────────────────────────────────────────────────────────────┘
```

### 6.1 v1 → v2 重大变化

| v1（已废弃） | v2（当前） |
| --- | --- |
| `internal/handlers/pg/` + `internal/handlers/pg/pgdb/` | `internal/handler/` + `internal/documentdb/` |
| `internal/handlers/sqlite/` + `internal/handlers/tigris/` | **已移除**（专注 PG） |
| `internal/handlers/dummy/` | **已移除** |
| `internal/handlers/handlers.go` (Backend interface) | **不再有 Backend 接口**——`Handler` 是单类型，依赖 `documentdb.Pool` |
| `internal/bson/`（手写 BSON 编码） | **已移除**——外移到 `github.com/FerretDB/wire/wirebson` |
| 聚合管道在 FerretDB 中翻译为 SQL | **下沉到 PG DocumentDB 扩展**——FerretDB 仅做参数透传 |

## 七、文档模型

FerretDB 的文档模型完全继承 MongoDB：

- **存储**：BSON 文档（最大 16MB），字段名 UTF-8，值类型（int32/int64/double/string/bool/date/array/object/null/ObjectId/Binary/Decimal128/Regex）；
- **Schema-less vs Schema-on-read**：
  - **写入**：不校验 schema（任意字段、嵌套结构、数组）；
  - **读取**：可选 `validator`（`msg_validate.go` → DocumentDB `Validate` 函数），支持 `$jsonSchema`、`$expr`；
- **唯一约束**：`_id` 默认唯一（PG 表上 PK），可创建 compound unique index；
- **类型精度损失**：
  - JavaScript `NumberLong`（int64）安全；
  - `NumberDecimal`（Decimal128）依赖 PG `numeric` 类型；
  - `Date` 存为 PG `timestamp`；
  - 警告：`ObjectId` → 12 字节 hex string；`BSON Binary` → `bytea`；
- **大小限制**：16MB（与 MongoDB 同）；
- **校验实现**：参见 `internal/handler/msg_validate.go:33` 调用 `internal/documentdb/documentdb_api/documentdb_api.go:861` `Validate`。

## 八、聚合管道（Aggregation Pipeline）

聚合管道是文档数据库的核心。FerretDB 通过 DocumentDB 扩展实现：

### 8.1 支持的阶段（v2）

| 阶段 | 描述 | 备注 |
| --- | --- | --- |
| `$match` | 过滤 | 直接翻译为 SQL `WHERE` |
| `$project` | 字段投影 | 翻译为 JSONB 函数 |
| `$group` | 分组聚合 | 支持 `$sum`/`$avg`/`$min`/`$max`/`$first`/`$last`/`$push`/`$addToSet` |
| `$sort` | 排序 | SQL `ORDER BY` |
| `$limit` / `$skip` | 分页 | `LIMIT/OFFSET` |
| `$lookup` | 跨集合连接 | 翻译为 `LEFT JOIN LATERAL` |
| `$facet` | 多管道并行 | 多个子 pipeline 合并 |
| `$bucket` / `$bucketAuto` | 分桶聚合 | 依赖 `bsonAggregate` 系列函数 |
| `$unwind` | 数组展平 | PG `jsonb_array_elements` |
| `$replaceRoot` / `$replaceWith` | 替换根文档 | |
| `$addFields` / `$set` | 新增字段 | |
| `$count` | 计数 | |

### 8.2 不支持/部分支持的阶段

- `$graphLookup`：v2 部分支持（递归层数有限制）；
- `$unionWith`：v2 alpha；
- `$out` / `$merge`：v2 部分支持；
- `$function` / `$accumulator`：不支持（无 JS 执行环境）；
- `$where`：不支持；
- `$vectorSearch`：依赖 DocumentDB 扩展的最新版本（仍在演进）。

### 8.3 源码路径

- `internal/handler/msg_aggregate.go:28` — `msgAggregate` 入口；
- `internal/documentdb/documentdb_api/documentdb_api.go:21` — `AggregateCursorFirstPage`（PG 函数 `documentdb_api.insert`/`aggregate_cursor_first_page` 包装）；
- `internal/documentdb/documentdb_api_internal/documentdb_api_internal.go` — `BsonAddToSet`/`BsonSum`/`BsonAvg` 等所有 BSON 聚合算子。

## 九、查询语言（MQL → SQL）

FerretDB 接受 **MongoDB Query Language (MQL)** 风格的 `find({...})`、`aggregate([...])`、`update({...}, {...})`，由 DocumentDB 扩展负责翻译。

### 9.1 操作符映射（部分）

| MQL 操作符 | 含义 | PG 翻译 |
| --- | --- | --- |
| `$eq`, `$ne`, `$gt`, `$gte`, `$lt`, `$lte` | 比较 | `=`, `<>`, `>`, `>=`, `<`, `<=` |
| `$in`, `$nin` | 包含 | `= ANY(ARRAY[...])` / `NOT IN` |
| `$and`, `$or`, `$not`, `$nor` | 逻辑 | `AND`/`OR`/`NOT`/`NOT (... OR ...)` |
| `$exists` | 字段存在 | `?`/`jsonb_exists` |
| `$type` | 类型匹配 | `jsonb_typeof` |
| `$regex` | 正则 | `~` |
| `$mod` | 取模 | `%` |
| `$elemMatch` | 数组元素匹配 | `jsonb_array_elements` |
| `$size` | 数组长度 | `jsonb_array_length` |
| `$all` | 全部包含 | `@>` |
| `$expr` | 表达式 | DocumentDB 翻译 |
| `$text` | 全文 | 依赖 PG `tsvector` 索引 |

### 9.2 重要 Caveat

- **复杂聚合管道**翻译效率可能不如手写 SQL（每阶段都走 PG 函数调用）；
- **地理空间查询** `$geoWithin`/`$near` 部分支持（依赖 PostGIS 扩展，可选安装）；
- **数组下标路径**：`docs.0.tags.1` —— FerretDB 解析后传给 DocumentDB 的 `bson_get_value_path`。

### 9.3 源码路径

- `internal/handler/msg_find.go:28` — `msgFind` 入口；
- `internal/documentdb/documentdb_api/documentdb_api.go:525` — `FindCursorFirstPage`（PG 函数 `documentdb_api.find_cursor_first_page` 包装）；
- `internal/documentdb/documentdb_api/documentdb_api.go:546` — `Insert`；
- `internal/documentdb/documentdb_api/documentdb_api.go:378` — `Delete`；
- `internal/documentdb/documentdb_api/documentdb_api.go:777` — `Update`。

## 十、索引机制

### 10.1 支持的索引类型

| 索引 | MongoDB 等价 | 实现 | 性能 |
| --- | --- | --- | --- |
| 单字段索引 | ✓ | B-tree on BSON path expression | 高 |
| 复合索引 | ✓ | B-tree on 多列 | 高 |
| 多键索引（数组字段） | ✓ | PG GIN on JSONB array | 中等 |
| 唯一索引 | ✓ | `CREATE UNIQUE INDEX` | 高 |
| 部分索引 | ✓ | `WHERE` 子句 | 中等 |
| TTL 索引 | ✓（部分） | 依赖 PG 调度器扩展 | 中等 |
| 地理空间 2dsphere | ✓（部分） | PostGIS（可选） | 取决于 PostGIS |
| 文本索引 | ✓（部分） | PG `tsvector`（需手工扩展） | 中等 |
| Hash 索引 | ✓ | `CREATE INDEX USING HASH` | 高 |

### 10.2 索引实现位置

- `internal/handler/msg_createindexes.go:35` — `msgCreateIndexes` + line 71 `createIndexes` 调用 DocumentDB API；
- `internal/documentdb/documentdb_api/documentdb_api.go:252` — `CreateIndexesBackground`（PG 函数 `documentdb_api.create_indexes_background`）；
- `internal/documentdb/documentdb_api_internal/documentdb_api_internal.go` — `CreateIndexesNonConcurrently`（msg_createindexes.go:74 调用）。

### 10.3 索引最佳实践

- **前缀规则**：复合索引遵循最左前缀；
- **覆盖查询**：FerretDB 通过 `explain()` 可看到索引使用情况；
- **限制**：单集合默认 64 个索引；多键索引可对数组字段建 GIN；
- **运维**：通过 `db.collection.getIndexes()` 查看，`db.collection.dropIndex()` 删除（msg_dropindexes.go）。

## 十一、变更流（Change Streams）

FerretDB v2 的 Change Streams 通过 **PostgreSQL logical replication + LISTEN/NOTIFY** 实现：

- **触发机制**：每次 Insert/Update/Delete 写完后，DocumentDB 扩展通过 PG `NOTIFY` 发送事件；
- **客户端 API**：`db.collection.watch(pipeline, options)` → 与 MongoDB 完全相同；
- **Resume Token**：基于 PG LSN（Log Sequence Number），格式与 MongoDB 不完全兼容；
- **过滤**：支持 `$match` / `$project`；
- **限制**：
  - **不支持** `startAfter`/`startAtOperationTime` 的 MongoDB 增强语义；
  - **不支持**跨分片事务的连续视图；
  - **不支持**跨集合/跨库的 Change Streams；
- **源码路径**：
  - `internal/handler/msg_change_stream*.go`（若存在）；
  - 实际事件链路在 DocumentDB 扩展内部（PG 源码，不在本仓库）。

## 十二、事务与一致性（裁剪）

FerretDB v2 支持 **单文档 ACID 事务**（继承 PG 单行原子性）。多文档事务：

- **MongoDB 4.0+ 多文档事务**：FerretDB **不支持完整 ACID 多文档事务**，但通过 PG 的 `BEGIN`/`COMMIT` 提供基础事务；
- **Session ID（lsid）**：在 `internal/handler/session/session.go` 中维护，与 MongoDB 一致；
- **快照隔离**：依赖 PG `REPEATABLE READ` / `SERIALIZABLE`；
- **因果一致性**：lsid + causal consistency token 部分实现（v2 alpha）；
- **应用语义**：建议在应用层实现 saga 模式或事件溯源，避免依赖多文档 ACID。

## 十三、复制与高可用

FerretDB **自身不实现复制**——所有 HA/复制逻辑继承自 PostgreSQL：

- **流复制**：PG 原生 streaming replication（async / sync）；
- **逻辑复制**：用于 Change Streams + 部分跨库复制；
- **Patroni / pg_auto_failover**：推荐 HA 方案（社区生态）；
- **Citus**：多节点分片（FerretDB 兼容性已验证）；
- **备份**：`pg_basebackup` + WAL archiving + `pgbackrest`；
- **PITR（Point-in-time Recovery）**：依赖 PG 自身的 PITR；
- **源码路径**：
  - **FerretDB 无 HA 代码**——所有 HA 都在 PG + 生态；
  - `internal/clientconn/listener_metrics.go` — Prometheus 指标（FerretDB 层）。

## 十四、扩展性

- **垂直扩展**：受限于单 PG 实例能力（通常 < 100GB hot data 性能最佳）；
- **水平扩展**：
  - **Citus**：多节点 PG 分片；
  - **FerretDB 不实现应用层 sharding**——不识别 MongoDB `mongos` 的 shard key 协议；
- **多租户**：通过 PG `schema` 隔离；
- **冷热分层**：依赖 PG 表分区（declarative partitioning）或 TimescaleDB 扩展；
- **运维约束**：单实例 FerretDB 的连接池大小 = PG `max_connections`，需配合 PG 连接池（pgBouncer）。

## 十五、性能与基准

- **官方基准**（FerretDB 团队发布）：
  - 单 PG 实例、4 vCPU、16GB RAM：
    - `find({_id:...})` ≈ 80k QPS；
    - `find({a: 1})` 带索引 ≈ 60k QPS；
    - `aggregate({$match}, {$group})` ≈ 5k QPS（受限 PG 函数调用）；
    - `insert` ≈ 40k QPS；
    - `update` ≈ 30k QPS；
  - 数据规模：1000 万文档、1KB/文档 ≈ 10GB；
- **第三方比较**：
  - vs MongoDB：FerretDB 慢 1.5~3x（取决于操作类型）；
  - vs PostgreSQL 原生 JSONB 查询：FerretDB 慢 2~10x（额外协议转换）；
- **性能调优点**：
  - 加大 PG `shared_buffers`、`work_mem`；
  - 使用 `EXPLAIN ANALYZE` 检查 DocumentDB 函数执行计划；
  - 减少 `$lookup` 嵌套层数；
  - 避免在 `$project` 中使用 `$function`；
- **基准备注**：FerretDB 官方提供 `bench/` 目录下的基准脚本（ferretdb/bench）。

## 十六、运维与可观测性

- **配置**：通过 `cmd/ferretdb/main.go:38` 的 Kong CLI 参数启动；
  ```bash
  ferretdb \
    --postgresql-url=postgres://user:pass@localhost:5432/ferretdb \
    --listen-addr=:27017 \
    --debug-addr=:8080 \
    --mode=normal  # 或 proxy / diff-normal / diff-proxy
  ```
- **Prometheus**：默认 `:8080/metrics`（debug 端口）；
- **OpenTelemetry**：`internal/documentdb/tracer.go` 启用 OTel 追踪 PG 查询；
- **日志**：slog 结构化日志，`internal/util/logging/` 封装；
- **健康检查**：`cmd/ferretdb/readyz.go` 提供 `/readyz` HTTP 端点；
- **慢查询**：通过 PG 自身的 `pg_stat_statements`；
- **MCP 端点**：`/mcp` 路径（v2 新功能，AI 助手友好）；
- **Data API**：`internal/dataapi/server/server.go` HTTP REST 端点（OpenAPI 规范）。

## 十七、生态与集成

- **客户端 SDK**：完全兼容 MongoDB 客户端：
  - **Go**：`go.mongodb.org/mongo-driver`
  - **Python**：`pymongo` / `motor`
  - **Node.js**：`mongodb` driver / `mongoose`
  - **Java**：`mongodb-driver-sync`
  - **C#**：`MongoDB.Driver`
  - **Rust**：`mongodb` crate
- **ORM/ODM**：
  - **Mongoose**（Node.js）
  - **Mongoengine**（Python）
  - **Spring Data MongoDB**
- **BI 工具**：
  - MongoDB Compass（部分兼容）
  - Metabase（通过 JDBC，需 PG 适配器）
- **Data API**：内置 HTTP REST（v2 新功能）；
- **MCP**：内置 MCP 服务器（v2 新功能）；
- **容器化**：官方 Docker image + docker-compose.yml；
- **K8s Operator**：Percona Operator for PostgreSQL（管理 PG）+ FerretDB deployment；
- **云服务**：AWS Marketplace / DigitalOcean / Hetzner / Aiven / Render / ScaleGrid。

## 十八、核心论文与算法原理

- **PostgreSQL MVCC**：`_postsgres_/src/backend/storage/ipc/procarray.c` + `xact.c`；
- **PG logical replication**：`_postsgres_/src/backend/replication/logicallauncher.c`；
- **DocumentDB 扩展**：基于 PostgreSQL 16+ 的 extension API；
- **BSON 规范**：[bsonspec.org](http://bsonspec.org/)；
- **MongoDB Wire Protocol**：[github.com/mongodb/specifications](https://github.com/mongodb/specifications)；

## 十九、源码学习路径

### 19.1 入门（1-2 天）

1. `cmd/ferretdb/main.go:38` — 入口，理解 CLI 启动；
2. `internal/clientconn/conn.go:123` — `processRequest`，看 wire 协议入口；
3. `internal/handler/handler.go:63` — `Handler` 结构，理解单实例设计；
4. `internal/handler/commands.go:47` — `initCommands`，看命令注册表；
5. `internal/handler/msg_find.go:28` — 单个命令的最简实现；
6. `internal/documentdb/pool.go:39` — `Pool`，理解 PG 连接管理。

### 19.2 进阶（3-5 天）

1. 选 3 条 `msg_*.go`（`insert`/`update`/`aggregate`）对比看；
2. 看 `internal/documentdb/documentdb_api/documentdb_api.go`（877 行自动生成代码），理解 SQL 包装器；
3. 看 `internal/documentdb/documentdb_api_internal/documentdb_api_internal.go`（更大，自动生成），了解所有 BSON 聚合算子；
4. 跟踪一次 `db.collection.find()` 请求：clientconn → middleware → msg_find → DocumentDB API → PG 函数；
5. 看 `internal/handler/middleware/`：理解 middleware 链。

### 19.3 高级（1 周+）

1. **跨进程调试**：自己 build DocumentDB PG 扩展 + FerretDB，配 `pg_stat_statements`；
2. **生成代码**：`internal/documentdb/genwrap/genwrap.go` — 从 PG 系统目录反射生成 Go 包装代码；
3. **错误码**：`internal/mongoerrors/codes.go` + `error.go` 的映射；
4. **Scram 认证**：`internal/util/scram/` + `internal/documentdb/user.go`；
5. **多副本部署**：FerretDB + Patroni + etcd，观察 LSN 在 Change Streams 中的语义。

### 19.4 源码规模

- **Go 文件数**：约 **200 个**（含测试）；
- **内部目录数**：40+；
- **总仓库目录数**：190（含 website、build、tools、integration、.github）；
- **cmd/ferretdb/**：3 个文件（main.go + main_test.go + readyz.go）；
- **internal/handler/msg_*.go**：60+（每 MongoDB 命令一个文件）；
- **internal/documentdb/documentdb_api/documentdb_api.go**：877 行（自动生成）；
- **构建系统**：Go modules + Taskfile.yml；
- **测试**：integration/ 9 类（admin、aggregation、auth、cursor、diag、query、session、setup、shareddata）。

## 二十、学习资源

- **官方文档**：[docs.ferretdb.io](https://docs.ferretdb.io/)；
- **官方博客**：[ferretdb.io/blog](https://www.ferretdb.io/blog/)（含版本演进、性能基准）；
- **GitHub Discussions**：[github.com/FerretDB/FerretDB/discussions](https://github.com/FerretDB/FerretDB/discussions)；
- **Quick Start**：`README.md` + `docker-compose.yml`；
- **Operator 文档**：[Percona Operator for PostgreSQL](https://docs.percona.com/percona-operator-for-postgresql/)；
- **演讲**：PGConf 各届大会（FerretDB 团队多次出席）；
- **Reddit**：[r/PostgreSQL](https://www.reddit.com/r/PostgreSQL/) 中 FerretDB 标签；
- **Hacker News**：多次讨论（搜 "FerretDB"）；
- **视频**：YouTube 上 FerretDB 团队的 PGConf 演讲、PG Day 演讲。

## 二十一、可移植性与部署

- **支持的平台**：
  - **Linux**：amd64 / arm64；
  - **macOS**：amd64（Apple Silicon）；
  - **Windows**：amd64（v2 实验性）；
  - **FreeBSD**：amd64（社区支持）；
- **容器化**：官方 `ghcr.io/ferretdb/ferretdb` image；
- **Helm Chart**：[github.com/FerretDB/charts](https://github.com/FerretDB/charts)；
- **K8s Operator**：Percona Operator；
- **嵌入式部署**：可作为 Go 库嵌入到其他应用（`ferretdb.New(...)` in `ferretdb/ferretdb.go`）；
- **配置文件**：纯命令行参数，无 YAML 配置；
- **最小硬件**：1 vCPU / 1GB RAM（开发）；生产建议 4 vCPU / 16GB RAM / 100GB SSD。

## 二十二、变体与衍生项目

- **Microsoft DocumentDB**（上游）：FerretDB 的"后端"PG 扩展；
- **Percona Distribution for FerretDB**：Percona 打包的发行版（含 PG + DocumentDB + FerretDB）；
- **AWS DocumentDB**：与 FerretDB 目标相似但**闭源 + MongoDB 兼容**；
- **DocDB（MongoDB 协议 → ClickHouse）**：探索中的项目，思路类似但 OLAP 后端；
- **FerretDB + pgvector**：组合方案，构建"向量 + 文档"二合一存储；
- **WiredTiger 兼容层**：社区探索用 WiredTiger 作为 FerretDB 存储引擎（实验性）。

## 二十三、风险与陷阱

### 23.1 协议兼容性陷阱

- **聚合管道子集**：部分 MongoDB 6.0+ 算子（`$rank`/`$densify`/`$fill`/`$sample`）在 FerretDB 中**部分或完全不支持**；
- **Change Streams resume token 格式**：与 MongoDB 不完全兼容，跨版本升级可能丢事件；
- **Capped collections**：v2 仍部分实现（基于 PG 表 + 触发器）；
- **GridFS**：v2 通过 `internal/dataapi/` 提供，但与 MongoDB GridFS 协议有差异；
- **MongoDB 事务**：FerretDB **不支持多文档 ACID 事务**，跨集合操作需应用层补偿；
- **视图（Views）**：v2 部分支持；
- **时序集合**：v2 不支持；
- **客户端驱动**：少数驱动（特别是老版本）可能使用 v2 尚未完全实现的命令。

### 23.2 性能陷阱

- **复杂 `$lookup`**：每次调用都走 PG 函数，性能退化严重；
- **大文档（接近 16MB）**：PG 单行性能下降（JSONB 编码开销）；
- **频繁 `$project`**：每阶段都走 PG `jsonb` 函数，避免过度投影；
- **缺少索引的 `$match`**：全表扫描 + JSONB 解析；
- **写放大**：PG `WAL` + MVCC 版本对纯文档负载的开销。

### 23.3 运维陷阱

- **连接池**：FerretDB 端不需要连接池，但 PG 端必须用 pgBouncer（FerretDB 默认会占用大量 PG 连接）；
- **备份一致性**：FerretDB 不感知 PG 备份时点，应用需协调；
- **升级 FerretDB**：可能伴随 PG + DocumentDB 扩展版本要求；
- **监控盲点**：FerretDB 的命令调用延迟 vs PG 端函数延迟需分别监控；
- **跨节点部署**：FerretDB 自身无集群，HA 完全依赖 PG 生态；
- **大查询执行计划**：`EXPLAIN` 输出需理解 DocumentDB 扩展生成的 SQL；
- **缓存**：FerretDB 不实现查询缓存，全部依赖 PG `shared_buffers`；
- **配额**：FerretDB 不感知 MongoDB 的 `maxBsonObjectSize` 校验，部分客户端可能发送超大 BSON。

## 二十四、学习路径

### 24.1 入门（1 周）

1. 阅读官方 [Quick Start](https://docs.ferretdb.io/quickstart/)；
2. 跑 `docker-compose up`（README 中提供的官方 compose）；
3. 用 mongosh 连接，执行 `db.collection.find({})`、`insert`、`aggregate`；
4. 编译 FerretDB：`task build`（需要 Go 1.24）；
5. 阅读 `cmd/ferretdb/main.go:38` 与 `internal/clientconn/conn.go:123`。

### 24.2 中级（2-3 周）

1. 部署 Percona Distribution for FerretDB；
2. 配置 Patroni HA；
3. 测试故障切换时 Change Streams 是否连续；
4. 在应用层（Go/Python）使用 mongo-driver 连接 FerretDB；
5. 阅读 `internal/handler/msg_aggregate.go` 与 DocumentDB API 文档；
6. 用 `pg_stat_statements` 优化慢查询。

### 24.3 进阶（1 个月+）

1. 编写自定义 DocumentDB PG 函数（扩展 SQL 操作符）；
2. 贡献 FerretDB 新命令实现（如 `msg_change_stream.go`）；
3. 部署 Citus 多节点 FerretDB；
4. 集成 pgvector，构建向量 + 文档二合一存储；
5. 阅读 DocumentDB 扩展源码（GitHub 上的 documentdb/documentdb）；
6. 在 MongoDB 与 FerretDB 间进行迁移演练，验证兼容性子集。

## 二十五、与本项目的关联

`engineering/` 中**没有 MongoDB 协议前端**实现，但有以下可借鉴模式：

- `engineering/include/db/mm_storage.h` 中的多模态存储设计与 FerretDB 的"一个协议 + 多种后端"思路相似（v1 时期 FerretDB 也曾是多后端）；
- `engineering/src/db/sql_engine.c` 中的 SQL 执行器可参考 DocumentDB 的"SQL 函数包装"模式——把高层 API 映射到底层 SQL；
- `engineering/src/db/core/catalog.c` 的 catalog 设计与 `internal/documentdb/documentdb_api_catalog/` 类似；
- 学习 FerretDB 的 `genwrap` 模式（`internal/documentdb/genwrap/genwrap.go`）：从底层 schema 反射生成高层 API，可应用到 `engineering/src/db/` 的代码生成；
- 测试模式：`integration/` 9 类分目录与 `engineering/test/` 的 gtest 风格可对比学习。

## 二十六、参考资料

1. **FerretDB 官方仓库**：[github.com/FerretDB/FerretDB](https://github.com/FerretDB/FerretDB)；
2. **Microsoft DocumentDB 扩展**：[github.com/documentdb/documentdb](https://github.com/documentdb/documentdb)；
3. **FerretDB 官方文档**：[docs.ferretdb.io](https://docs.ferretdb.io/)；
4. **FerretDB 博客**：[ferretdb.io/blog](https://www.ferretdb.io/blog/)；
5. **BSON 规范**：[bsonspec.org](http://bsonspec.org/)；
6. **MongoDB Wire Protocol 规范**：[github.com/mongodb/specifications](https://github.com/mongodb/specifications)；
7. **MongoDB 官方文档**（兼容目标）：[mongodb.com/docs](https://www.mongodb.com/docs/)；
8. **PostgreSQL JSONB 文档**：[postgresql.org/docs/current/datatype-json.html](https://www.postgresql.org/docs/current/datatype-json.html)；
9. **PostgreSQL MVCC 论文**：[_postsgres_ 内部文档 `src/backend/storage/ipc/procarray.c`](https://github.com/postgres/postgres)；
10. **Percona Operator for PostgreSQL**：[docs.percona.com/percona-operator-for-postgresql](https://docs.percona.com/percona-operator-for-postgresql/)；
11. **FerretDB 性能基准博客**（[ferretdb.io/blog/category/benchmark](https://www.ferretdb.io/blog/category/benchmark/)）；
12. **FerretDB Helm Chart**：[github.com/FerretDB/charts](https://github.com/FerretDB/charts)。