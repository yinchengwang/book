# MongoDB Wiki（最流行的文档数据库）

> 参考版本：MongoDB 当前 commit `d88752897110eef1aa5530f6238a53d682469903`（MongoDB 7.x/8.x 系列），C++ 实现，BSON 文档模型，WiredTiger 默认存储引擎。

## 一、项目定位

MongoDB 是全球最流行的 **文档数据库**，由 **MongoDB Inc.** 维护。它把 **JSON-like 的 BSON 文档**作为一等数据模型，配合 **水平扩展（sharding）** 与 **多文档 ACID 事务**（4.0+），成为现代应用的"全栈数据平台"。MongoDB 是文档数据库的 **事实标准**——提到"NoSQL"或"document database"通常默认指它。

- **痛点**：
  - 关系数据库在 Web 时代 schema 变更代价高；
  - 水平扩展需要手工分区（sharding）且易出错；
  - JSON 数据无处不在但关系数据库 JSON 支持薄弱；
  - 半结构化数据（IoT、AI、社交）需要灵活 schema；
  - 多数据模型统一（文档 + 全文 + 时序 + 向量）；
- **典型用户**：几乎所有行业——金融、电商、游戏、AI、IoT、广告、内容管理、社交媒体；
- **核心口号**："**The most popular document database**" + "**Developer data platform**"（**Atlas Data Federation** / **Atlas Vector Search**）。

## 二、发展历史与社区

- **起源**：2007 年由 **Dwight Merriman**、**Eliot Horowitz**、**Kevin Ryan** 在 DoubleClick / ShopWiki 经验后创立；
- **里程碑**：
  - 2009-02：1.0 GA，MongoDB 1.x（启动名"humongous"）；
  - 2010：1.4 + sharding alpha；
  - 2012：2.2 + aggregation framework；
  - 2013：2.4 + text search；
  - 2014：2.6 + security 改进；
  - 2015：3.0 + WiredTiger 存储 + 复制集；
  - 2016：3.2 + change streams；
  - 2017：3.4 + faceted search；
  - 2018：4.0 + **多文档 ACID 事务** + AGPL → SSPL 许可证；
  - 2019：4.2 + **分布式事务**；
  - 2020：4.4 + hidden index / hedged read；
  - 2021：5.0 + **时序集合** + **客户端字段级加密**；
  - 2022：6.0 + **Change Stream 改进** + **搜索节点**；
  - 2023：7.0 + **Queryable Encryption** + **Atlas Vector Search**；
  - 2024-2025：8.0 + **加密改进** + **Time series 分面**；
- **维护状态**：极活跃（每季度主版本，MongoDB 公司全职团队）；
- **社区**：MongoDB Forums、Stack Overflow（最热门 NoSQL 标签）、GitHub Issues、MongoDB University（认证）、MongoDB World / .Local 大会。

## 三、许可与商业限制

- **许可证**：
  - **历史**：2018 之前为 **AGPL v3**；
  - **2018 起**：**SSPL**（Server Side Public License）——禁止将 MongoDB 作为服务对外提供而开源衍生代码，引发开源社区争议；
  - 驱动 / 客户端仍 **Apache 2.0**；
- **商业版本**：
  - **Community Edition**（免费，可自托管）；
  - **Enterprise Edition**（商业，含 LDAP / Kerberos / SNMP / in-memory storage engine 等）；
  - **Atlas**（MongoDB 官方云托管）；
- **生态关系**：
  - 与 **MongoDB Atlas**（托管云）紧密耦合；
  - 与 **MongoDB Compass**（GUI）、**Mongosh**（CLI）共同构成工具链；
  - 与 **Kafka Connect**、**Spark Connector**、**BI Connector** 集成。

## 四、业界主要使用方与案例

- **超大型用户**（公开）：
  - **Google**：内部工具 + Search Console；
  - **Facebook**（Meta）：部分内部基础设施；
  - **Adobe**：Creative Cloud 用户配置；
  - **Cisco**：网络设备遥测；
  - **eBay**：商品目录（早期）；
  - **Forbes**：CMS 后台；
  - **Bosch**：IoT 平台；
  - **SAP**：SAP HANA Cloud 元数据；
  - **Verizon**：电信计费；
- **行业**：金融（信用卡欺诈检测）、电商（购物车 + 商品）、游戏（玩家档案）、AI / ML（特征存储）、IoT（时序）、社交（用户关系 + 内容）、医疗（EHR）、广告（活动流）；
- **社区案例**：几乎所有 SaaS 应用都有 MongoDB 部署。

## 五、一句话对比定位

> MongoDB 是 **"BSON + ACID + 复制集 + Sharding + Atlas"** 的全栈文档数据库平台；与 **CouchDB（AP + HTTP + Erlang）**、**Couchbase（CP + N1QL + 移动端）**、**RavenDB（.NET + 强 ACID）**、**FerretDB（PG 代理）** 一起构成 **文档数据库阵营**。MongoDB 的差异化是 **"最流行 + 全栈 + 多数据模型 + Atlas 一体化"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│           客户端（任意语言）                                              │
│   - mongosh / Compass / pymongo / mongo-go-driver / mongoose          │
│   - HTTP API（4.x+）                                                    │
│   - Atlas Data API（REST/HTTP）                                        │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ MongoDB Wire Protocol（OP_MSG/OP_QUERY/OP_COMMAND）
┌─────────────────────▼──────────────────────────────────────────────────┐
│        mongod（独立进程）                                                │
│                                                                          │
│   ┌──────────────────────────────────────────────────────────────┐   │
│   │  Service 层：transport_layer / run_command / command dispatch │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Catalog：catalog.cpp / database / collection / index catalog │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Query：find / aggregate / update / delete / explain          │   │
│   │      → Pipeline (pipeline/) + Exec (exec/)                    │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Index：B+Tree / Hashed / Text / Geo / Vector / TTL / Partial │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Storage Engine：WiredTiger（默认）/ In-Memory / MMAPv1（已废弃）│   │
│   │      → Btree / WiredTiger WAL / checkpoints                    │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Replication：Oplog / change_streams                           │   │
│   └──────────────────┬───────────────────────────────────────────┘   │
│                      │                                                   │
│   ┌──────────────────▼───────────────────────────────────────────┐   │
│   │  Transactions：sessions / transactions / causal_consistency    │   │
│   └──────────────────────────────────────────────────────────────┘   │
└───────────────────────────────────────────────────────────────────────┘
                      │
                      │ 集群：mongos（路由器）+ 配置服务器 + 分片
```

### 6.1 mongod 核心模块

| 模块 | 路径 | 职责 |
| --- | --- | --- |
| Transport Layer | `src/mongo/transport/` | 网络监听与连接管理 |
| Catalog | `src/mongo/db/catalog/` | 数据库/集合/索引元数据 |
| Query Engine | `src/mongo/db/query/` + `src/mongo/db/exec/` | 查询解析与优化 |
| Aggregation | `src/mongo/db/pipeline/` | 聚合管道 |
| Indexes | `src/mongo/db/index/` + `src/mongo/db/exec/` | 索引 |
| Change Streams | `src/mongo/db/pipeline/document_source_change_stream*.cpp` | 变更流 |
| Sessions/Transactions | `src/mongo/db/sessions/` + `src/mongo/db/transactions/` | ACID 事务 |
| Storage | `src/mongo/db/storage/` | 存储引擎接口 |
| WiredTiger | `src/mongo/db/storage/wiredtiger/` | WiredTiger 适配 |
| Repl | `src/mongo/db/repl/` | 复制集 |
| Sharding | `src/mongo/db/sharding/` | 分片集群 |
| BSON | `src/mongo/bson/` | BSON 编解码 |
| Cluster | `src/mongo/s/commands/` | 配置服务器（mongod 集群模式） |

### 6.2 mongos（路由器）

- `src/mongo/s/` 下的可执行文件；
- 接收客户端请求，路由到对应分片；
- 合并分片结果（`_pipeline` + `sort` + `limit` 优化器）；
- 不存储数据（除内存缓存）。

### 6.3 配置服务器

- `src/mongo/db/s/config/` —— 部署为 **replica set**（CSRS，从 3.4 起强制）；
- 存储分片元数据 + 集群拓扑。

## 七、文档模型

### 7.1 BSON

- **存储格式**：**BSON**（Binary JSON），最大 16MB；
- **类型**：int32 / int64 / double / string / bool / date / array / object / null / ObjectId / Binary / Decimal128 / Regex / DBRef / Timestamp / Min Key / Max Key / Symbol / JavaScript code / JavaScript code w/ scope；
- **字段名**：UTF-8 字符串；
- **有序性**：BSON 字段是有序的（JSON 字段无序）；
- **Schema-less**：不强制 schema；
- **类型精度**：
  - JavaScript `Number` 默认 double（8 字节）；
  - `Decimal128`（128 位十进制）精确但性能较差；
  - `ObjectId` 12 字节（时间戳 + 机器 + 进程 + 计数器）；
  - `Date` 64 位（自 Unix epoch 起的毫秒数）；

### 7.2 Schema-on-read vs Schema Validation

- **写入**：默认不校验（schema-less）；
- **写入校验**：3.2+ 提供 `$jsonSchema` 验证器：
  ```javascript
  db.createCollection("users", {
    validator: {
      $jsonSchema: {
        bsonType: "object",
        required: ["name", "email"],
        properties: {
          email: { bsonType: "string", pattern: "^.+@.+$" },
          age: { bsonType: "int", minimum: 0, maximum: 150 }
        }
      }
    }
  });
  ```
- **典型场景**：用户配置文件 + 多租户 schema 严格 + 内部分析 schema 灵活；
- **源码路径**：
  - `src/mongo/db/catalog/` 中的 collection 创建逻辑；
  - `src/mongo/db/json_schema.cpp`（如有）；

### 7.3 集合（Collection）

- 等价于关系数据库的"表"；
- 集合是逻辑概念（实际存储以文档 + 索引为单位）；
- 集合名最长 120 字节，不能以 `system.` 开头（保留给系统）；
- **Capped Collection**：固定大小的循环集合（用作日志、缓存）；
- **TTL Collection**：自动过期删除（依赖 TTL 索引）。

### 7.4 文档约束

- 单文档大小限制：**16MB**（MongoDB 4.0+ 后可配置）；
- 嵌套层级：建议 < 100 层（实际无硬限制）；
- 字段数：建议 < 800（实际无硬限制，但索引字段数有限制）；
- 单集合文档数：理论 2^32 - 1；
- 唯一约束：`_id` 默认唯一；可创建 `unique: true` 索引。

### 7.5 文档源码路径

- BSON 核心：`src/mongo/bson/bsonobj.cpp`、`bsonelement.cpp`、`bsontypes.h`；
- BSON 验证：`src/mongo/bson/bson_validate.cpp`；
- Schema 校验：`src/mongo/db/catalog/`、`src/mongo/db/commands/create_collection.cpp`；
- Collection 元数据：`src/mongo/db/catalog/collection_options.cpp`。

## 八、聚合管道（Aggregation Pipeline）

聚合管道是 MongoDB 查询的核心能力。从 2.2 引入到 8.x 演进，**200+ 操作符** + **40+ 阶段**。

### 8.1 阶段类型

#### 8.1.1 基础过滤 / 投影

| 阶段 | 描述 |
| --- | --- |
| `$match` | 过滤文档 |
| `$project` | 字段投影（增删 / 重命名） |
| `$limit` / `$skip` | 分页 |
| `$sort` | 排序 |
| `$count` | 计数 |
| `$addFields` / `$set` | 新增字段 |
| `$unset` | 删除字段 |

#### 8.1.2 关联

| 阶段 | 描述 |
| --- | --- |
| `$lookup` | 左外连接（同库 / 跨库 / 跨分片） |
| `$unionWith` | 跨集合合并 |
| `$graphLookup` | 递归图查询（DFS） |
| `$documents` | 内联字面量数组（5.1+） |

#### 8.1.3 聚合

| 阶段 | 描述 |
| --- | --- |
| `$group` | 分组聚合（`_id` + `$sum`/`$avg`/`$min`/`$max`/`$push`/`$addToSet`/`$first`/`$last`） |
| `$bucket` | 分桶 |
| `$bucketAuto` | 自动分桶 |
| `$sortByCount` | 分组排序 |
| `$facet` | 多管道并行（同一个集合多个分析视角） |
| `$collStats` | 集合统计 |
| `$indexStats` | 索引使用统计 |
| `$out` / `$merge` | 输出到集合 |

#### 8.1.4 数组操作

| 阶段 | 描述 |
| --- | --- |
| `$unwind` | 数组展平 |
| `$arrayToObject` | 数组转对象 |
| `$objectToArray` | 对象转数组 |

#### 8.1.5 窗口函数（5.0+）

| 阶段 | 描述 |
| --- | --- |
| `$setWindowFields` | 滑动窗口（`$sum`/`$avg`/`$rank`/`$denseRank`/`$shift`/`$first`/`$last` 等） |

### 8.2 操作符

- 算术：`$add` / `$subtract` / `$multiply` / `$divide` / `$mod`；
- 字符串：`$concat` / `$substr` / `$toLower` / `$toUpper` / `$regexMatch` / `$regexFind`；
- 日期：`$year` / `$month` / `$dayOfWeek` / `$dateToString` / `$dateFromString`；
- 数组：`$size` / `$slice` / `$concatArrays` / `$filter` / `$map` / `$reduce`；
- 类型转换：`$toString` / `$toInt` / `$toDecimal` / `$type`；
- 条件：`$cond` / `$ifNull` / `$switch`；
- 累加器：`$accumulator`（5.4+ 自定义累加器）；

### 8.3 执行模型

聚合管道的执行通过 **MongoDB Query Planner** 优化：

1. `$match` 推到 `$sort` 之前；
2. `$project` / `$addFields` 推到 `$group` 之后；
3. `$group` 用索引覆盖；
4. `$lookup` 翻译为 indexed nested loop / hash join；
5. `$facet` 各分支独立执行；
6. `$out` / `$merge` 写入持久集合。

### 8.4 源码路径

- **聚合框架核心**：
  - `src/mongo/db/pipeline/` —— 整个聚合管道；
  - `src/mongo/db/pipeline/document_source.h` — `DocumentSource` 基类；
  - `src/mongo/db/pipeline/document_source.cpp` — `DocumentSource` 实现；
- **各阶段文件**（命名统一 `document_source_<stage>.cpp`）：
  - `document_source_match.cpp` — `$match`；
  - `document_source_group.cpp` — `$group`；
  - `document_source_project.cpp` — `$project`；
  - `document_source_lookup.cpp` — `$lookup`；
  - `document_source_unwind.cpp` — `$unwind`；
  - `document_source_facet.cpp` — `$facet`；
  - `document_source_bucket.cpp` — `$bucket`；
  - `document_source_bucket_auto.cpp` — `$bucketAuto`；
  - `document_source_sort.cpp` — `$sort`；
  - `document_source_add_fields.cpp` — `$addFields`；
  - `document_source_union_with.cpp` — `$unionWith`；
  - `document_source_graph_lookup.cpp` — `$graphLookup`；
  - `document_source_set_window_fields.cpp` — `$setWindowFields`；
  - `document_source_change_stream.cpp` — `$changeStream`；
- **累加器**：
  - `src/mongo/db/pipeline/accumulator.h` + `accumulator_add_to_set.cpp` + `accumulator_avg.cpp` + `accumulator_first.cpp` + `accumulator_min_max.cpp` + `accumulator_push.cpp` + `accumulator_sum.cpp` + `accumulator_concat_arrays.cpp` + `accumulator_for_bucket_auto.cpp` + `accumulator_for_window_functions.h`；
- **执行**：
  - `src/mongo/db/pipeline/pipeline.cpp` — Pipeline 主类；
  - `src/mongo/db/pipeline/aggregate_command.cpp` — aggregate 命令；
  - `src/mongo/db/exec/agg/` — Pipeline exec stage；
  - `src/mongo/db/exec/` — 其他 exec stage（`IXSCAN`、`COLLSCAN`、`FETCH`、`SORT`、`LIMIT`、`SKIP`、`OR`、`AND` 等）；
- **优化器**：
  - `src/mongo/db/query/optimizer/` — 查询优化器（5.0+ 引入 SBE-based optimizer）；
- **表达式**：
  - `src/mongo/db/pipeline/expression.h` + `expression.cpp` + `expression_context.cpp`；
  - `src/mongo/db/pipeline/expression_internal_construct_ndv_sketch.cpp`（去重统计）。

## 九、查询语言（MQL）

### 9.1 查询语法

```javascript
db.users.find({
  status: "active",
  age: { $gte: 18, $lte: 65 },
  tags: { $elemMatch: { $eq: "premium" } },
  $or: [
    { location: { $geoWithin: { $centerSphere: [[-74, 40.7], 10] } } },
    { premium: true }
  ],
  created_at: { $gt: ISODate("2025-01-01") }
})
.sort({ created_at: -1 })
.skip(100)
.limit(50)
.projection({ name: 1, email: 1 })
.hint({ age: 1 })  // 强制索引
```

### 9.2 操作符分类

- **比较**：`$eq` / `$ne` / `$gt` / `$gte` / `$lt` / `$lte`；
- **逻辑**：`$and` / `$or` / `$not` / `$nor`；
- **数组**：`$elemMatch` / `$size` / `$all` / `$in` / `$nin`；
- **元素**：`$exists` / `$type`；
- **评估**：`$regex` / `$mod` / `$where` / `$expr` / `$function` / `$accumulator`；
- **地理空间**：`$geoWithin` / `$geoIntersects` / `$near` / `$nearSphere`；
- **位运算**：`$bitsAllSet` / `$bitsAnyClear` 等；
- **JSON Schema**：`$jsonSchema`（验证）；
- **全文**：`$text` / `$search`（Atlas Search）；
- **向量**：`$vectorSearch`（Atlas Vector Search，6.0+ GA）。

### 9.3 更新语法

```javascript
db.users.updateOne(
  { _id: 1 },
  { $set: { status: "inactive" }, $inc: { login_count: 1 }, $push: { tags: "vip" } }
);
```

更新操作符：`$set` / `$unset` / `$inc` / `$mul` / `$rename` / `$push` / `$pull` / `$addToSet` / `$pop` / `$bit` / `$currentDate`。

### 9.4 更新源码路径

- `src/mongo/db/update/` — 更新逻辑（`update_object.cpp` / `update_modifier.cpp` / `update_executor.cpp`）；
- `src/mongo/db/ops/` — 历史 CRUD ops；
- `src/mongo/db/commands/update.cpp` / `find_and_modify.cpp` — 命令入口；
- `src/mongo/db/exec/update_stage.cpp` / `update_stage_common.cpp` — exec stage；
- `src/mongo/db/exec/write_stage_common.cpp` — 写阶段公共逻辑。

## 十、索引机制

### 10.1 索引类型

| 索引类型 | 描述 | 用途 |
| --- | --- | --- |
| **单字段索引** | 单字段 B+Tree | 基础查询 |
| **复合索引** | 多字段 B+Tree | 多字段查询 |
| **多键索引** | 数组元素索引 | 数组字段查询 |
| **唯一索引** | `unique: true` | 唯一约束 |
| **稀疏索引** | `sparse: true` | 仅索引存在的字段 |
| **部分索引** | `partialFilterExpression` | 条件索引 |
| **TTL 索引** | `expireAfterSeconds` | 自动过期 |
| **哈希索引** | `hash` 类型 | 等值查询（不可范围） |
| **地理空间 2dsphere** | GeoJSON（WGS84） | 球面距离 |
| **地理空间 2d** | 平面坐标 | 平面距离 |
| **文本索引** | `text` 类型 | 全文搜索（基础） |
| **通配符索引** | `wildcard` 类型（4.2+） | 动态字段 |
| **向量索引** | `$vectorSearch`（Atlas） | 向量近邻（6.0+ GA） |
| **隐藏索引** | `hidden: true`（4.4+） | 隐藏不影响查询计划 |
| **Clustered Index** | `_id` BTree（5.0+） | 时序集合主键 |

### 10.2 索引最佳实践

- **ESR 原则**（Equality, Sort, Range）：高效复合索引顺序；
- **覆盖查询**：投影字段全部在索引中，无须 fetch；
- **`explain()`**：分析查询计划（`executionStats` / `queryPlanner` / `allPlansExecution`）；
- **索引前缀**：复合索引支持前缀查询；
- **`hint()`**：强制使用指定索引（慎用，破坏自动优化）；
- **索引限制**：单集合 64 个索引（4.2+ 可放宽）；索引字段值总大小限制 1024 字节（早期版本）；

### 10.3 索引源码路径

- B+Tree 实现：`src/mongo/db/index/btree_access_method.cpp` + `btree_access_method.h`；
- 索引键构造：`src/mongo/db/index/btree_key_generator.cpp`；
- 索引 catalog：`src/mongo/db/catalog/index_catalog.cpp` + `index_catalog.h`；
- 全文索引：`src/mongo/db/ftdc/` + `src/mongo/db/index/` 中的文本逻辑；
- 向量索引：`src/mongo/db/exec/` + Atlas 集成（`src/mongo/db/commands/atlas_vector_search.cpp`）；
- TTL：`src/mongo/db/ttl*`；
- 地理空间：`src/mongo/db/geo_*`；

### 10.4 索引与 WiredTiger

- 默认存储引擎 WiredTiger 使用 **B+Tree** 实现索引；
- 索引存储于 `collection-<id>-<index_name>.wt` 文件；
- WiredTiger 缓存索引 + 文档数据；
- WAL（write-ahead log）确保索引与数据一致性。

## 十一、变更流（Change Streams）

Change Streams 是 MongoDB 3.6+ 引入的实时变更通知 API。

### 11.1 工作机制

```
DB1 update
   ↓
oplog.rs（replication recordset）写入
   ↓
Primary 节点聚合变更
   ↓
aggregate pipeline 中的 $changeStream stage
   ↓
客户端游标接收
   ↓
resume token 持久化
```

### 11.2 API

```javascript
const changeStream = db.collection('users').watch([
  { $match: { operationType: { $in: ['insert', 'update'] } } },
  { $match: { 'fullDocument.status': 'premium' } }
]);

changeStream.on('change', (next) => {
  console.log('change:', next.fullDocument);
  console.log('resumeToken:', next._id);
});
```

### 11.3 关键特性

- **Resume Token**：基于 oplog 时间戳 + 唯一 ID，断点可恢复；
- **Filter pipeline**：客户端可使用 `$match` / `$project` 过滤；
- **Pre-image / Post-image**：6.0+ 支持变更前后文档完整内容；
- **Cluster 范围**：跨分片聚合（基于全局 oplog）；
- **Atlas 多区域**：跨数据中心；
- **限制**：
  - 单次 watch 持续期间，**oplog 必须保留**（`oplogSize` 默认 5% 磁盘）；
  - **读取关注** 必须 `majority` 或 `snapshot`；
  - 副本集 / 分片集群必须启用 majority write concern；
- **运维考虑**：
  - oplog window（oplog 大小 × 副本数 / 写入速率）；
  - 网络分区导致 stream 中断；
  - 大量 changestream 订阅者会消耗资源；

### 11.4 源码路径

- 聚合阶段：
  - `src/mongo/db/pipeline/document_source_change_stream.cpp` — `$changeStream` 核心；
  - `src/mongo/db/pipeline/document_source_change_stream.h` + `.idl`；
  - `src/mongo/db/pipeline/document_source_change_stream_add_post_image.cpp` — 添加变更后文档；
  - `src/mongo/db/pipeline/document_source_change_stream_add_pre_image.cpp` — 添加变更前文档；
  - `src/mongo/db/pipeline/document_source_change_stream_check_invalidate.cpp` — 检查流失效；
  - `src/mongo/db/pipeline/document_source_change_stream_check_resumability.cpp` — 检查可恢复性；
  - `src/mongo/db/pipeline/document_source_change_stream_check_topology_change.cpp` — 检查拓扑变更；
  - `src/mongo/db/pipeline/document_source_change_stream_ensure_resume_token_present.cpp` — 确保 resume token；
  - `src/mongo/db/pipeline/document_source_change_stream_handle_topology_change.cpp` + `_v2.cpp` — 处理拓扑变更；
- 集群参数：`src/mongo/db/change_streams_cluster_parameter.cpp` + `.h` + `.idl`；
- 指标：`src/mongo/db/change_stream_cursor_option_metrics_test.cpp` + `change_stream_error_metrics_test.cpp` + `change_stream_metrics_util.h`；
- 复制协调：`src/mongo/db/repl/` 中 oplog + replication 相关；
- 实现位置总结：Change Streams 是 oplog（`src/mongo/db/repl/`）之上的视图，客户端通过 aggregation pipeline 形式消费。

## 十二、事务与一致性（裁剪）

MongoDB 在 4.0 引入**多文档 ACID 事务**——文档数据库中较为罕见的特性。

### 12.1 事务支持矩阵

| 版本 | 事务能力 |
| --- | --- |
| MongoDB 4.0 | 单分片多文档 ACID 事务 |
| MongoDB 4.2+ | 分布式多文档 ACID 事务（跨分片） |
| MongoDB 4.4+ | 改进事务并发 |
| MongoDB 7.0+ | Queryable Encryption + 事务 |

### 12.2 API

```javascript
const session = db.getMongo().startSession();
session.startTransaction();
try {
  const orders = session.getDatabase('shop').getCollection('orders');
  const inventory = session.getDatabase('shop').getCollection('inventory');
  orders.insertOne({ sku: 'abc', qty: 1 });
  inventory.updateOne({ sku: 'abc' }, { $inc: { stock: -1 } });
  session.commitTransaction();
} catch (e) {
  session.abortTransaction();
}
```

### 12.3 隔离级别

- 默认 **Snapshot Isolation**；
- 显式设置：`readConcern: 'snapshot'`、`writeConcern: 'majority'`；
- 跨分片事务：基于 **两阶段提交** + **事务协调器**；
- 因果一致性（causal consistency）：通过 session + 逻辑时钟实现；

### 12.4 限制

- **事务有 60 秒默认超时**（`transactionLifetimeLimitSeconds`）；
- **事务中不支持**：
  - 创建集合（4.4+ 部分支持）；
  - 索引创建；
  - DDL；
  - `eval` / `$where`；
  - `mapReduce`（5.0+ 部分）；
- **大事务锁争用**：单文档冲突会导致整个事务回滚；
- **性能开销**：相比非事务写入，慢 2~5x；

### 12.5 源码路径

- 事务存储引擎适配：`src/mongo/db/storage/`（含 WiredTiger 事务包装）；
- Session：`src/mongo/db/sessions/`（会话管理 + 逻辑时钟）；
- 事务逻辑：`src/mongo/db/transactions/` + `src/mongo/db/repl/`（含 oplog 事务协调）；
- 事务指标：`src/mongo/db/` 中的 metrics；
- 两阶段提交：`src/mongo/db/s/transaction_coordinator.cpp`（在 mongod 集群模式下）；

## 十三、复制与高可用

### 13.1 复制集（Replica Set）

- 至少 3 节点（Primary + Secondary + Secondary）；
- 基于 **oplog** 的增量复制；
- **自动故障转移**：选举新 Primary（基于 priority + 心跳）；
- **读一致性**：可选 `primary` / `primaryPreferred` / `secondary` / `secondaryPreferred` / `nearest`；
- **写一致性**：`w: 0` / `w: 1` / `w: "majority"` / `w: <n>`；

### 13.2 分片集群（Sharded Cluster）

- **架构**：
  - `mongos`（路由，**多个实例**）；
  - `config server`（3 节点复制集，存储元数据）；
  - `mongod`（数据节点，可分片）；
- **分片策略**：
  - **范围分片**（`{shardKey: 1}`）：按值范围；
  - **哈希分片**（`{shardKey: "hashed"}`）：按 hash 取模；
  - **区域分片**（`addZone`）：自定义位置；
- **分片键选择**：
  - 高基数；
  - 低频修改；
  - 均匀分布；
  - 不可变（immutable）；
- **Balancing**：自动后台 chunk 迁移；
- **事务**：4.2+ 跨分片 ACID 事务（基于事务协调器）；
- **Jumbo Chunk**：超过 64MB 强制拆分的 chunk；

### 13.3 备份

- `mongodump` / `mongorestore`：逻辑备份；
- `mongoexport` / `mongoimport`：JSON/CSV 导入导出；
- **Ops Manager / Cloud Manager**：在线备份（基于文件系统快照 + oplog）；
- **Atlas** 内置：连续备份 + PITR（Point-in-time Recovery）；
- **文件系统快照**：需要开启 journal（WiredTiger 默认开启）；

### 13.4 源码路径

- 复制集：`src/mongo/db/repl/`（含 `repl_set_state.cpp`、`replication_coordinator.cpp`、`oplog.cpp`、`oplog_entry.cpp`）；
- oplog：`src/mongo/db/repl/oplog.cpp` + `oplog_entry.cpp` + `oplog_buffer.cpp`；
- 分片：`src/mongo/db/sharding/`（含 `cluster_ddl.cpp`、`migration_*`）；
- mongos：`src/mongo/s/`（含 `router.cpp`、`strategy_shard_op.cpp`）；
- 事务协调器：`src/mongo/db/s/transaction_coordinator.cpp`；
- 配置服务器：`src/mongo/db/s/config/`。

## 十四、扩展性

- **垂直扩展**：单分片可处理 TB 级数据；
- **水平扩展**：
  - **分片集群**：水平分片（sharding）；
  - **Atlas Global Clusters**：跨区域分片（地理就近）；
- **冷热分层**：
  - **Atlas Online Archive**：冷数据自动归档到 S3；
  - **Time Series Collection**（5.0+）：自动 bucket 压缩；
- **多云**：Atlas 多云部署；
- **混合云**：自托管 + Atlas 联邦查询（Atlas Data Federation）；

## 十五、性能与基准

- **典型性能**（4 vCPU / 16GB RAM / SSD）：
  - `insert`（单文档）：~30k QPS；
  - `find by _id`：~80k QPS；
  - `find by index`：~50k QPS；
  - `aggregate`（简单）：~10k QPS；
  - `aggregate`（复杂）：~1k QPS；
  - `update by _id`：~25k QPS；
- **基准测试**：
  - [github.com/mongodb/mongo-perf](https://github.com/mongodb/mongo-perf) — MongoDB 官方 micro-bench；
  - [github.com/mongodb-developer/MongoDB-Performance-Benchmarks](https://github.com/mongodb-developer)；
  - **YCSB-MongoDB** 绑定；
- **第三方比较**：
  - vs PostgreSQL JSONB：MongoDB 文档操作更简洁；
  - vs Couchbase：MongoDB 流量稍慢但生态更广；
- **调优点**：
  - WiredTiger 缓存：`storage.wiredTiger.engineConfig.cacheSizeGB`；
  - 写入：`submission.acknowledgement` / `writeConcern`；
  - 索引：`indexHints` / 复合索引；
  - 投影：`projection` 减少字段传输；
  - 批量写入：100~500 个 / 批（避免超过 16MB 限制）；

## 十六、运维与可观测性

- **配置文件**：`/etc/mongod.conf`（YAML）；
- **关键配置**：
  - `systemLog`：日志；
  - `storage.wiredTiger`：存储引擎；
  - `replication.replSetName`：复制集；
  - `sharding.clusterRole`：分片模式；
  - `net.port` / `net.bindIp`：网络；
  - `security.authorization`：RBAC；
- **监控**：
  - **FTDC**（Full Time Diagnostic Data）：本地诊断数据采集；
  - **MongoDB Ops Manager**：商业监控；
  - **Atlas Monitoring**：云端监控；
  - **Prometheus 集成**：通过 `mongodb_exporter`；
- **FTDC 数据采集**：`src/mongo/db/ftdc/`；
- **慢查询**：`db.system.profile` + `db.setProfilingLevel(2)`；
- **日志**：
  - 默认 stderr；
  - JSON 格式可配置；
  - 结构化日志（含 component + severity + id）；

## 十七、生态与集成

- **官方 SDK（驱动）**：
  - **C / C++**：`mongo-cxx-driver`（C++17）；
  - **C# / .NET**：`MongoDB.Driver`；
  - **Go**：`mongo-go-driver`；
  - **Java**：`mongodb-driver-sync` / `mongodb-driver-reactivator`；
  - **Python**：`pymongo`（同步）/ `motor`（异步）；
  - **Node.js**：`mongodb`（官方）/ `mongoose`（ODM）；
  - **PHP**：`mongodb/mongodb`（官方）；
  - **Ruby**：`mongo`；
  - **Rust**：`mongodb` crate；
- **ODM**：
  - **Mongoose**（Node.js，最流行）；
  - **Mongoengine**（Python）；
  - **Spring Data MongoDB**（Java）；
  - **Doctrine MongoDB ODM**（PHP）；
- **BI**：MongoDB BI Connector（SQL 访问）+ Atlas Charts；
- **Data API**：Atlas App Services Data API（REST/HTTP）；
- **集成**：
  - **Kafka Connect**：[github.com/mongodb/kafka-connector](https://github.com/mongodb/kafka-connector)；
  - **Apache Spark Connector**；
  - **PostgreSQL FDW**：`mongo_fdw`（外部表）；
- **容器化**：官方 Docker image；
- **Helm Chart**：[github.com/mongodb/helm-charts](https://github.com/mongodb/helm-charts)；
- **K8s Operator**：[MongoDB Enterprise Kubernetes Operator](https://github.com/mongodb/mongodb-enterprise-kubernetes)；
- **云服务**：
  - **MongoDB Atlas**（官方全托管）；
  - **MongoDB Atlas Data Federation**（跨数据源联邦查询）；
  - **AWS / Azure / GCP**：Atlas 镜像或 DocumentDB（部分兼容）；

## 十八、核心论文与算法原理

- **WiredTiger 引擎**：参考 LSM-Tree 与 B-Tree 混合；
- **MVCC**：基于 WiredTiger 的多版本；
- **一致性算法**：复制集用 Raft-like（自定义）；
- **聚合管道**：基于流式执行模型；
- **Change Streams**：基于 oplog 订阅；
- **事务**：基于 WiredTiger MVCC + 事务协调器；
- **分片均衡**：基于 chunk 迁移；
- **MongoDB History 论文**：参考 Google Bigtable + Amazon Dynamo；
- **Raft 论文**：复制集选举参考。

## 十九、源码学习路径

### 19.1 入门（1 周）

1. `src/mongo/db/` —— 核心模块入口；
2. `src/mongo/bson/` —— BSON；
3. `src/mongo/db/catalog/` —— catalog；
5. `src/mongo/db/pipeline/document_source_match.cpp` —— 聚合 `$match` 阶段；
4. `src/mongo/db/index/btree_access_method.cpp` —— 索引；
5. `src/mongo/db/storage/wiredtiger/` —— 存储引擎；
6. `src/mongo/db/repl/replication_coordinator.cpp` —— 复制协调；

### 19.2 进阶（2-3 周）

1. `src/mongo/db/exec/` —— 查询执行（IXSCAN / COLLSCAN / FETCH / SORT）；
2. `src/mongo/db/pipeline/` —— 聚合管道全貌；
3. `src/mongo/db/pipeline/document_source_lookup.cpp` —— `$lookup` 实现；
4. `src/mongo/db/pipeline/document_source_change_stream.cpp` —— Change Stream；
5. `src/mongo/db/storage/wiredtiger/wiredtiger_record_store.cpp` —— WiredTiger 记录存储；
6. `src/mongo/db/repl/oplog.cpp` + `oplog_entry.cpp` —— oplog；
7. `src/mongo/db/sessions/` —— 会话管理；
8. `src/mongo/db/transactions/` —— 事务；

### 19.3 高级（1 个月+）

1. `src/mongo/db/sharding/` —— 分片路由；
2. `src/mongo/s/` —— mongos 路由器；
3. `src/mongo/db/s/transaction_coordinator.cpp` —— 跨分片事务；
5. `src/mongo/db/query/optimizer/` —— 查询优化器（SBE）；
6. 自己 build MongoDB（`bazel build`） + 启动分片集群 + mongos；
7. 跟踪一次 `db.users.find()`：客户端 → mongos → config server lookup → mongod → WiredTiger → 索引 → 返回；
8. 跟踪一次 Change Stream：oplog → 聚合管道 → resume token；

### 19.4 源码规模

- **C++ 文件**：~3000+ `.cpp` + `.h`；
- **语言**：C++17 为主，少量 Python（工具）；
- **构建系统**：Bazel + SCons；
- **测试**：
  - `src/mongo/db/jstests/` —— JavaScript 集成测试；
  - `src/mongo/db/exec/*_test.cpp` —— C++ 单元测试；
  - **Resmoke**：集群集成测试框架；
- **依赖**：Boost、WiredTiger、ICU、libcurl、ASIO、Snappy、zlib、zstd。

## 二十、学习资源

- **官方文档**：[mongodb.com/docs](https://www.mongodb.com/docs/)（**官方文档极优秀**）；
- **官方教程**：[learn.mongodb.com](https://learn.mongodb.com/)（免费认证课程）；
- **官方书**：
  - **MongoDB: The Definitive Guide**（O'Reilly）；
  - **MongoDB in Action**（Manning）；
- **MongoDB University**（免费在线课程）：[learn.mongodb.com](https://learn.mongodb.com/)；
- **官方博客**：[mongodb.com/blog](https://www.mongodb.com/blog)；
- **Stack Overflow**：[stackoverflow.com/questions/tagged/mongodb](https://stackoverflow.com/questions/tagged/mongodb)；
- **GitHub Discussions**：[github.com/mongodb/mongo/discussions](https://github.com/mongodb/mongo/discussions)；
- **Reddit**：[r/MongoDB](https://www.reddit.com/r/MongoDB/)；
- **YouTube**：MongoDB .Local 大会视频；
- **MongoDB Atlas 文档**：[mongodb.com/docs/atlas](https://www.mongodb.com/docs/atlas/)。

## 二十一、可移植性与部署

- **支持的平台**：Linux（amd64/arm64）、macOS（amd64/arm64）、Windows Server 2019+；
- **容器化**：官方 Docker image；
- **Helm Chart**：[github.com/mongodb/helm-charts](https://github.com/mongodb/helm-charts)；
- **K8s Operator**：[MongoDB Enterprise Kubernetes Operator](https://github.com/mongodb/mongodb-enterprise-kubernetes)；
- **嵌入式部署**：MongoDB Mobile（社区版 + Realm Sync），基于 IoT/移动端；
- **最小硬件**：
  - 开发：1 vCPU / 1GB RAM；
  - 生产建议：8 vCPU / 32GB RAM / SSD；
- **依赖**：libcurl、libicu、libssl 等；

## 二十二、变体与衍生项目

- **MongoDB Atlas**（官方托管云）；
- **MongoDB Community Edition**（开源）；
- **MongoDB Enterprise Edition**（商业，含 LDAP/Kerberos/SNMP）；
- **MongoDB Atlas App Services**（原 Realm）；
- **MongoDB Atlas Vector Search**（向量搜索，6.0+ GA）；
- **MongoDB Atlas Search**（全文搜索，基于 Lucene）；
- **MongoDB Atlas Charts**（BI）；
- **MongoDB Realm Mobile**（移动端同步）；
- **MongoDB Atlas Stream Processing**（流处理，5.3+）；
- **MongoDB Online Archive**（冷数据归档）；
- **AWS DocumentDB**（MongoDB 兼容，闭源，基于 PostgreSQL）；
- **FerretDB**（参考本仓 `reference/document/ferretdb/`，MongoDB 协议 → PG）；
- **Percona Server for MongoDB**（Percona 分叉）；
- **Rockset**（非关联，文档分析）；
- **ClickHouse MongoDB 协议**（非关联，OLAP）。

## 二十三、风险与陷阱

### 23.1 协议兼容性陷阱

- **驱动版本**：驱动版本落后服务端会导致新功能不可用；
- **BSON 16MB 限制**：大文档报错（嵌入式大数组、二进制 blob）；
- **Oplog 大小**：默认 5% 磁盘可能不够（高写入速率）；
- **Change Stream 集群要求**：必须副本集 / 分片 + majority write；
- **事务限制**：DDL 不支持、mapReduce 不支持、`$where` 不支持；
- **MongoDB Atlas 版本**：某些功能（如 Atlas Vector Search）只在 Atlas；
- **WiredTiger 加密**：Atlas 自动启用，自托管需配置；
- **客户端字段级加密 (CSFLE)**：需要特定驱动版本 + KMS 配置；
- **MongoDB Atlas Charts**：商业功能，自托管无；
- **MongoDB 与 PostgreSQL 迁移**：BSON → JSONB 不完全等价（Decimal128、Date 等）；

### 23.2 性能陷阱

- **未索引查询**：全集合扫描（COLLSCAN）；
- **低选择性索引**：查询选择性差导致大量结果；
- **覆盖查询失效**：投影字段不在索引中导致 FETCH；
- **复合索引顺序错误**：违反 ESR 原则；
- **大 `$lookup`**：性能差（无索引 → COLLSCAN）；
- **大 `$group`**：内存限制（100MB 默认，超过需要 `allowDiskUse: true`）；
- **Change Stream 过滤**：客户端过滤 vs 服务端 `$match`；
- **事务频繁写**：20-50% 性能下降；
- **批量写入过大**：超过 16MB 或 100k 操作；
- **频繁 schema 变更**：`$jsonSchema` 校验有开销；
- **高基数索引**：索引内存占用大；
- **TTL 索引频率**：后台 reaper 每 60s 一次，大量过期文档积压；

### 23.3 运维陷阱

- **分片键不可变**：必须一次性选好；
- **Jumbo Chunk**：分片键基数不够导致；
- **Balancer 抖动**：迁移期间查询慢；
- **Oplog window**：oplog 满后复制集挂起；
- **磁盘满**：WiredTiger 至少 10% 空闲（推荐 25%）；
- **备份一致性**：mongodump 不保证一致快照（需 `--oplog`）；
- **集群升级顺序**：先升级副本集，再升级 mongos，最后客户端驱动；
- **监控盲点**：FTDC 数据采集需开启；
- **索引重建**：reIndex 阻塞集合（应使用 `db.collection.createIndex({...}, {background: true})`）；
- **Network partition**：副本集网络分裂导致脑裂（majority write 防止）；
- **License 风险**：SSPL 阻止云厂商提供 MongoDB as a Service（AWS → DocumentDB 分叉）；
- **MongoDB Mobile (Realm)**：与 MongoDB Atlas 紧密耦合；
- **慢查询监控**：开启 profiling 会影响性能；
- **In-memory 存储引擎**：企业版专属，自带限制；

## 二十四、学习路径

### 24.1 入门（1 周）

1. 跑官方 [Quick Start](https://www.mongodb.com/docs/manual/tutorial/getting-started/)；
2. 安装 MongoDB Community Edition；
3. 用 mongosh 插入 / 查询文档；
4. 创建集合与索引；
5. 学习 aggregation pipeline 基础阶段；
6. 阅读 `src/mongo/bson/bsonobj.cpp` + `src/mongo/db/index/btree_access_method.cpp`；
7. 跑 [MongoDB University M001](https://learn.mongodb.com/) 入门课。

### 24.2 中级（2-3 周）

1. 部署 3 节点副本集；
2. 测试故障切换；
3. 部署分片集群（3 mongos + 3 config + 3 mongod × 2 shards）；
4. 测试 Balancer 行为；
5. 使用 Change Stream 写实时管道；
6. 使用聚合管道实现 `$lookup` / `$facet` / `$bucket`；
7. 阅读 `src/mongo/db/pipeline/`；
8. 学习 MongoDB University [M220](https://learn.mongodb.com/)（开发者）；

### 24.3 进阶（1 个月+）

1. 实现多文档 ACID 事务；
2. 部署跨分片事务；
3. 配置 Atlas Vector Search（向量索引）；
4. 配置 Atlas Search（全文搜索）；
5. 性能基准 vs PostgreSQL / Couchbase / RavenDB；
6. 阅读 `src/mongo/db/storage/wiredtiger/` 全部；
7. 阅读 `src/mongo/db/query/optimizer/`（SBE 优化器）；
8. 自己 build MongoDB（`bazel build`） + 启动 + 调试；
9. 部署 MongoDB Atlas 多区域集群；
10. 贡献 MongoDB（新算子、新存储引擎插件）；

## 二十五、与本项目的关联

`engineering/` 中**没有 MongoDB**实现，但有以下可借鉴模式：

- `engineering/src/db/core/` 中的 `catalog.c` / `rel.c` 与 `src/mongo/db/catalog/` 设计模式相似（系统表 + OID 分配）；
- `engineering/include/db/btreeam.h` 的 BTree AM 可参考 `src/mongo/db/index/btree_access_method.cpp`；
- `engineering/src/db/bufmgr.c` 的 buffer pool 与 WiredTiger 缓存层类似；
- `engineering/src/db/wal.c` 的 WAL 与 WiredTiger WAL 类似；
- `engineering/src/db/sql_engine.c` 的 SQL 执行器可参考 `src/mongo/db/exec/` 的 Volcano 模型；
- `engineering/src/db/core/guc.c` 的 GUC 配置与 MongoDB 的 YAML 配置机制类似；
- **Change Streams 设计模式**可应用到 `engineering/src/db/` 中需要变更通知的模块；
- **聚合管道**（`src/mongo/db/pipeline/`）是文档数据库的"杀手锏"，`engineering/` 目前没有等价实现；
- **测试模式**：MongoDB 的 `jstests`（JavaScript 集成测试）+ `Resmoke` 框架值得借鉴到 `engineering/test/`。

## 二十六、参考资料

1. **MongoDB 官方仓库**：[github.com/mongodb/mongo](https://github.com/mongodb/mongo)；
2. **MongoDB 官方文档**：[mongodb.com/docs](https://www.mongodb.com/docs/)；
3. **MongoDB Atlas**：[mongodb.com/atlas](https://www.mongodb.com/atlas)；
4. **MongoDB University**：[learn.mongodb.com](https://learn.mongodb.com/)；
5. **MongoDB Definitive Guide**：[O'Reilly](https://www.mongodb.com/docs/manual/)；
6. **WiredTiger 文档**：[wiredtiger.com](https://source.wiredtiger.com/)；
7. **MongoDB 博客**：[mongodb.com/blog](https://www.mongodb.com/blog)；
8. **MongoDB .Local 大会视频**：[youtube.com/@MongoDB](https://www.youtube.com/@MongoDB)；
9. **MongoDB GitHub**：所有驱动 + 工具 + 集成；
10. **MongoDB SSPL License 讨论**：[mongodb.com/licensing/server-side-public-license](https://www.mongodb.com/licensing/server-side-public-license)；
11. **MongoDB Performance Best Practices**：[mongodb.com/docs/manual/administration/analyzing-mongodb-performance/](https://www.mongodb.com/docs/manual/administration/analyzing-mongodb-performance/)；
12. **MongoDB 内部原理讲座**（O'Reilly 在线会议）；
13. **Sample Apps**：[github.com/mongodb-developer](https://github.com/mongodb-developer)。