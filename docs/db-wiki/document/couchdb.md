# CouchDB Wiki（Apache 顶级项目、面向文档的 Erlang 分布式数据库）

> 参考版本：CouchDB **3.5.2**（`reference/document/couchdb/version.mk`），32 个 Erlang/OTP 应用，源码 444 个非测试 `.erl` 文件 + 约 40 个 C/C++ NIF 文件。

## 一、项目定位

Apache CouchDB 是面向 **文档（JSON）** 的 **多主复制** 关系数据库，由 Erlang/OTP 编写。它的设计哲学是 **"the database that completely embraces the web"**——文档即 HTTP 资源，复制即 HTTP 同步。它是 **CAP 定理中 AP 优先** 的代表数据库（牺牲部分强一致性以换取高可用与分区容忍）。

- **痛点**：
  - 关系数据库在 Web 时代的"模式变更"代价高；
  - 关系数据库在多数据中心间异步复制能力有限；
  - Web 开发者需要"JSON 即数据库行"的语义；
  - 离线优先 / 移动设备同步需求；
- **典型用户**：W3C 标准化组织、Amazfit / Pirelli 智能轮胎、3D 打印社区（Thingiverse 早期）、医疗电子病历、零售 POS、IoT 设备；
- **核心口号**："**Relax**"——Replication And Lots of JSON + HTTP + Linux。

## 二、发展历史与社区

- **起源**：2005 年由 **Damien Katz** 在 IBM 离职后创立；最初为 C++ 实现；
- **里程碑**：
  - 2008-04：CouchDB 0.7，迁向 Erlang；
  - 2008-11：成为 Apache Incubator 项目；
  - 2009-11：毕业为 Apache 顶级项目（Apache CouchDB）；
  - 2010-03：1.0 GA，纯 Erlang 实现；
  - 2012-05：1.2.0；
  - 2016-06：2.0 GA（**集群 + 大规模改进**，引入 `couch_bt_engine` 新存储引擎）；
  - 2019-09：2.3.1（首次启用 Nouveau 全文索引）；
  - 2020-01：3.0 GA（**移除集群许可证限制**，完全开源）；
  - 2022：3.2、3.3；
  - 2024：3.4、3.5（持续维护，Smoosh 压缩合并器稳定）；
  - 当前：**3.5.2**（2025 年）；
- **维护状态**：活跃但节奏较缓（每月 1~2 个 commit），由 Apache CouchDB PMC 维护；
- **社区**：Apache CouchDB mailing lists、Slack、IRC、年度 CouchDB 中文社区活动。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（完全 OSI 认证）；
- **商业版本**：3.0 之前有 **IBM Cloudant**（基于 CouchDB 2.x 的商业发行版），3.0 后 Cloudant 转向 Couchbase 主要功能，社区版无任何限制；
- **生态关系**：
  - 与 **PouchDB**（JavaScript 嵌入式）构成端云同步闭环；
  - 与 **Cloudant**（IBM）的协议兼容；
  - 与 **Couchbase Lite / LiteCore** 共享相同的同步协议（`_changes` + `POST _bulk_docs`）。

## 四、业界主要使用方与案例

- **W3C**：标准化提案存储；
- **Amazfit（华米科技）**：智能手环健康数据云端同步；
- **Thingiverse**：3D 模型元数据（早期版本）；
- **Odoo**（部分模块）：CRM/电商的某些灵活字段；
- **国防、医疗、零售**：政府/医疗电子病历（替代关系数据库处理半结构化字段）；
- **社区案例**：IoT 设备元数据、离线优先应用、内容管理、P2P 同步、备份归档。

## 五、一句话对比定位

> CouchDB 是 **"JSON over HTTP + 多主复制 + AP 优先 + Erlang 健壮性"** 的开源文档数据库；与 **MongoDB（CP + BSON）**、**Couchbase（CP + 内存缓存 + N1QL）**、**RavenDB（.NET + ACID）**、**FerretDB（PG 代理）** 一起构成 **文档数据库阵营**。CouchDB 的差异化是 **"Web-native（HTTP/JSON 即一等公民）" + "多主复制" + "MVCC with `_rev` 乐观锁"**。

## 六、整体架构

CouchDB 3.x 是 32 个 OTP 应用的松耦合联邦，最核心的几个应用：

```
┌───────────────────────────────────────────────────────────────────────┐
│                  HTTP 客户端（curl / SDK / PouchDB）                   │
│   - GET /db/doc → document                                            │
│   - PUT /db/doc → write                                               │
│   - _all_docs / _find / _view / _changes / _bulk_docs                  │
└─────────────────────┬─────────────────────────────────────────────────┘
                      │ HTTP/1.1 + JSON
┌─────────────────────▼─────────────────────────────────────────────────┐
│              chttpd (src/chttpd/)                                      │
│   - chttpd.erl 监听（Mochiweb/Hackney）                                │
│   - chttpd_db.erl 数据库相关路由                                       │
│   - handle_changes_req/2（_changes 入口，第 26 行）                    │
└─────────────────────┬─────────────────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┐
        ▼             ▼             ▼
┌──────────────┐ ┌─────────────┐ ┌─────────────────┐
│  couch/      │ │  mango/     │ │  couch_mrview/  │
│  (MVCC +     │ │  (_find /   │ │  (_view         │
│   B-tree)    │ │   _explain) │ │   Map/Reduce)   │
└──────┬───────┘ └──────┬──────┘ └────────┬────────┘
       │                │                  │
       ▼                ▼                  ▼
┌──────────────┐ ┌──────────────────────────────┐
│ couch_db     │ │  couch_index/                │
│ .erl (2525)  │ │  (统一索引抽象：              │
│ + db_updater │ │   couch_index_server.erl)    │
│ + db_engine  │ └──────────────────────────────┘
└──────┬───────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│   couch_btree.erl (1324 行)                  │
│   + couch_bt_engine.erl (1266 行) 新一代      │
│   + Smoosh (src/smoosh/) 压缩合并             │
└─────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│   couch_file.erl (1068 行) + NIF             │
│   (priv/couch_cfile/couch_cfile.c)           │
└─────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────┐
│   本地文件系统（默认 4096 字节节点）              │
└─────────────────────────────────────────────┘

         集群：fabric/ (跨节点路由) + mem3/ (分片) + global_changes/
         复制：couch_replicator/（38 个 .erl）
         全文：nouveau/（21 个 .erl，倒排索引）
```

### 6.1 核心模块

| 应用 | 职责 | 关键文件 |
| --- | --- | --- |
| `couch` | 数据库引擎、MVCC、B-tree、文件存储 | `src/couch/src/couch_db.erl` (2525 行)、`couch_db_updater.erl` (1015)、`couch_db_engine.erl` (1044) |
| `chttpd` | HTTP API 层 | `src/chttpd/src/chttpd_db.erl` (2835)、`chttpd.erl` |
| `mango` | `_find` / `_explain`（Mango 查询） | `src/mango/src/mango_httpd.erl`、`mango_selector.erl`、`mango_idx_view.erl` (719) |
| `couch_mrview` | Map/Reduce 视图 | `src/couch_mrview/src/couch_mrview.erl` (807)、`couch_mrview_updater.erl` |
| `couch_replicator` | 复制协议 | `src/couch_replicator/src/couch_replicator.erl` (418) + 37 个 .erl |
| `fabric` + `mem3` + `global_changes` | 集群路由 / 分片管理 / 全局变更 | `src/fabric/`、`src/mem3/` |
| `nouveau` | 内置倒排全文索引 | `src/nouveau/`（21 个 .erl，3.x 新增） |
| `ddoc_cache` | `_design` 文档内存缓存 | `src/ddoc_cache/` |

## 七、文档模型

### 7.1 文档即 HTTP 资源

- **存储**：JSON 文档（任意结构，UTF-8，最大 64MB）；
- **必有字段**：
  - `_id`：字符串，文档唯一标识；
  - `_rev`：形如 `"<integer>-<hash>"`，MVCC 版本号，**每次写入更新**；
- **可选字段**：
  - `_attachments`：附件（base64 + content_type + digest），以"独立 BLOB 文件"形式存储；
  - `_deleted=true`：软删除（tombstone）；
  - `_revisions`：历史版本列表；
  - `_revs_info` / `_conflicts`：冲突元信息；
- **Schema-less vs Schema-on-read**：
  - **写入**：不校验 schema；
  - **设计文档（`_design/xxx`）**：通过 `validate_doc_update` 函数实现写入校验（JavaScript / QuickJS 执行）；
  - **Mango `validator`**：`mango_idx.erl` 中实现 JSON Schema 验证（部分支持）；
- **MVCC**：
  - 写入冲突解决依赖 `_rev`（乐观锁）；
  - 冲突保留**所有版本**（不会自动覆盖），由应用或 `_revs_info` 自行解决；
- **限制**：
  - 单文档 64MB；
  - 单库最大文档数 = 2^64；
  - 单节点建议 < 50GB 数据（之后 compaction 成本高）。

### 7.2 设计文档（`_design`）

- `_design/<name>` 文档内嵌 view、update、filter、validate、show、list、rewrite 函数；
- 这些函数在 **JavaScript 运行时**中执行：CouchDB 3.x 默认 QuickJS（`src/couch_quickjs/` + `quickjs/`），社区可换 Spidermonkey/QuickJS/V8；
- 设计文档也通过 `_rev` 参与 MVCC。

## 八、聚合管道（聚合查询）

CouchDB 不支持 MongoDB 风格的 `$match/$group` 聚合管道。其"聚合"通过 **Map/Reduce 视图** 或 **Mango `_find`** 实现：

### 8.1 Map/Reduce 视图（传统方式）

```javascript
// _design/myapp/_view/by_user
function(doc) {
  if (doc.type === 'order') {
    emit([doc.user_id, doc.created_at], doc.amount);
  }
}
```

```javascript
// reduce 函数（可选）
function(keys, values, rereduce) {
  return sum(values);
}
```

- **查询**：`GET /db/_design/myapp/_view/by_user?startkey=["u1"]&endkey=["u1",{}]&reduce=true&group=true`
- **增量构建**：基于变更订阅，新写入触发增量更新；
- **限制**：JS 函数必须有良好分布性，避免热键。

### 8.2 Mango `_find`（类 MongoDB 查询）

CouchDB 2.0+ 引入 Mango，提供 JSON selector 语法：

```json
POST /db/_find
{
  "selector": {
    "type": "order",
    "amount": {"$gt": 100},
    "tags": {"$elemMatch": {"$eq": "premium"}},
    "created_at": {"$gt": "2025-01-01"}
  },
  "sort": [{"created_at": "desc"}],
  "limit": 50,
  "use_index": "by_created"
}
```

支持的操作符：$eq/$gt/$gte/$lt/$lte/$ne/$in/$nin/$and/$or/$not/$nor/$exists/$type/$regex/$size/$elemMatch/$all/$mod。

### 8.3 聚合管道 vs Mango

- CouchDB **没有 `$group`/`$facet`/`$bucket`** 等 MongoDB 聚合算子；
- 复杂聚合通过：
  - **Reduce 函数**：`_sum`/`_count`/`_stats` 内置 + 自定义 JS reduce；
  - **List 函数**：Map 后逐条处理；
  - **Mango 索引 + 客户端聚合**；
- 性能考量：Map/Reduce 视图是**预计算**（增量更新），查询 O(1)；Mango 是**运行时**（依赖索引，复杂度 O(log n)）。

### 8.4 源码路径

- `src/mango/src/mango_selector.erl` — JSON selector 解析；
- `src/mango/src/mango_idx_view.erl` (719 行) — Mango → mrview 翻译；
- `src/couch_mrview/src/couch_mrview.erl` (807) — Map/Reduce 视图主入口；
- `src/couch_mrview/src/couch_mrview_updater.erl` — 视图增量更新（订阅 db updates）。

## 九、查询语言

### 9.1 HTTP 资源映射

| 路径 | 行为 |
| --- | --- |
| `GET /db/<id>` | 读取文档 |
| `PUT /db/<id>` | 写入文档（带 `_rev`） |
| `DELETE /db/<id>?rev=...` | 删除文档 |
| `GET /db/_all_docs` | 列出所有文档 |
| `POST /db/_bulk_docs` | 批量写入 |
| `POST /db/_find` | Mango 查询 |
| `GET /db/_design/<design>/_view/<view>` | 视图查询 |
| `GET /db/_changes` | 变更 feed |
| `POST /db/_replicator` | 触发复制 |

### 9.2 Mango 完整语法

- **Selector 树**：JSON 对象嵌套表达 `$and` / `$or` / `$not`；
- **Sort**：单字段或 `[field1, field2]` 多字段；
- **Limit / Skip**：分页；
- **Use Index**：强制指定 Mango 索引（`use_index: ["by_user", "by_date"]`）；
- **Bookmark**：Mango cursor 分页（base64 编码的排序位置）；
- **Fields**：投影（`{fields: ["user_id", "created_at"]}`）；
- **Update**：是否读取最新数据（`update: true`，破坏游标稳定性）；
- **r**：读一致性级别（`r=1` 默认主写读，`r>1` 多节点读）。

### 9.3 复制查询

- `_replicator` 端点：HTTP POST 启动复制任务；
- 查询参数：`source` / `target` / `filter` / `doc_ids` / `since`；
- **Checkpoint**：复制进度（`seq`），持久化到 `_local/<replicator-id>` 文档。

### 9.4 源码路径

- `src/chttpd/src/chttpd_db.erl` — HTTP 路由分发；
- `src/couch/src/couch_changes.erl` (821) — `_changes` 实现；
- `src/couch/src/couch_multidb_changes.erl` (1222) — 跨库 `_db_changes`；
- `src/couch_replicator/src/couch_replicator_changes_reader.erl` — 复制端 read changes feed。

## 十、索引机制

CouchDB 3.x 支持三类索引：

### 10.1 Mango 索引（`_find` 加速）

- **创建**：`POST /db/_index` with `{"index": {"fields": ["user_id", "amount"]}, "name": "by_user_amount"}`；
- **底层**：实际上是 `couch_mrview` 视图的封装（`mango_idx_view.erl`）；
- **类型**：单字段 / 复合 / 部分 / 多键（数组）；
- **特殊索引**：`_all_docs` / `_design_docs`（`mango_idx_special.erl`）；
- **唯一性**：不支持 unique 索引（应用层保证）。

### 10.2 Nouveau 全文索引（3.x 新增）

- **倒排索引**：内置实现（`src/nouveau/` 21 个 .erl）；
- **替代 dreyfus**（旧 Clouseau/Lucene 后端，3.5 deprecated）；
- **创建**：`_design/<design>/_nouveau/<index>` 或 Mango `{"type": "text"}`；
- **查询**：`_find` with `$text` operator；
- **支持**：词项/短语/模糊匹配、字段权重；
- **语言**：内置英语（French/German/Italian/Spanish 等通过 ICU）。

### 10.3 设计文档视图

- 详见第八节；
- 索引本身是 B-tree on `<key, docid>`；
- 增量更新 + 后台 compaction。

### 10.4 源码路径

- `src/couch_index/src/couch_index.erl` + `couch_index_plugin_couch_db.erl` + `couch_index_server.erl` — Mango/Nouveau 共用索引抽象；
- `src/couch_mrview/src/couch_mrview_index.erl` (388) — 视图索引生命周期；
- `src/nouveau/src/` — Nouveau 全文索引实现。

## 十一、变更流（Changes Feed）

`_changes` 是 CouchDB 的灵魂特性——通过 HTTP **SSE** 或 **long-poll** 推送变更。

### 11.1 工作模式

| 模式 | 描述 | 适用 |
| --- | --- | --- |
| `normal` | 一次性返回当前 `since` 之后的全部变更 | 离线同步 |
| `longpoll` | 长轮询，等待新变更 | 实时性较差 |
| `continuous` | HTTP chunked 持续推送 | 实时同步 |

### 11.2 参数

- `since=<seq>`：起始序列号（数字或字符串 `now`）；
- `feed=normal/longpoll/continuous`；
- `heartbeat=<ms>`：长连接心跳；
- `timeout=<ms>`：超时；
- `filter=<type>/<name>`：`_design/<d>/_filter/<f>` 或内置 `_doc_ids` / `_view`；
- `include_docs=true`：附带完整文档；
- `style=all_docs/main_only`：是否包含冲突分支。

### 11.3 源码路径

- `src/couch/src/couch_changes.erl` (821) — 单库 `_changes`；
- `src/couch/src/couch_multidb_changes.erl` (1222) — 跨库 `_db_changes`；
- `src/chttpd/src/chttpd_db.erl:26` — HTTP `handle_changes_req/2` 入口；
- `src/fabric/src/fabric_view_changes.erl` (1070) — 集群 fan-in；
- `src/global_changes/src/global_changes_server.erl` — `_global_changes`（3.x 集群级）；
- `src/couch/src/couch_db_updater.erl` — 提交后 `notify_db_update/2` 触发变更广播。

### 11.4 `_changes` 与 PouchDB

PouchDB（JavaScript 嵌入式）使用 `_changes` 实现端云同步：

```
PouchDB (浏览器/移动端)
   ↑                        ↓
   │ sync                   │ sync
   ↓                        ↑
CouchDB (云端)
```

协议：`GET /_changes?feed=continuous&since=...&heartbeat=...0` + `POST /_bulk_docs`。

## 十二、事务与一致性（裁剪）

### 12.1 单文档 ACID

CouchDB 单文档写入是原子的（依赖 `_rev` 乐观锁 + MVCC）。但**多文档写入无原子性**：

- **客户端多文档原子性**：依赖 `_bulk_docs` + 应用层 conflict 处理；
- **应用层 saga**：补偿事务；
- **`_bulk_docs` 的 `all_or_nothing: false` 参数**：禁用（避免误用）。

### 12.2 MVCC

- **核心机制**：
  - 每个文档有 `_rev`（整数 + hash），每次成功写入递增；
  - 写冲突（`_rev` 不匹配）返回 409 Conflict；
  - 旧版本保留在 `_revisions` 字段中（可选深度限制）；
- **持久化**：所有版本的文档元数据存于 `couch_db.erl` 管理的本地 B-tree；
- **冲突解决**：客户端读取 `_conflicts`，自行决定保留哪个版本；
- **关键源码**：
  - `src/couch/src/couch_db.erl` (2525) — 数据库主进程；
  - `src/couch/src/couch_db_updater.erl` (1015) — gen_server，串行化更新；
  - `src/couch/src/couch_db_engine.erl` (1044) — 引擎抽象层；
  - `src/couch/src/couch_db_int.hrl` — `#db_header / #doc_info / #update / #indexed_doc / #purge` 记录定义。

### 12.3 隔离级别

- 实际提供 **Read Committed**（写完即可读，写之前读不到旧值）；
- 副本读一致性：通过 `_rev` + sequence number + 应用层协调。

## 十三、复制与高可用

CouchDB 是 **AP 优先** 的多主复制数据库：

### 13.1 复制模式

| 模式 | 描述 |
| --- | --- |
| `one-shot` | 一次性同步 |
| `continuous` | 持续同步（监听 `_changes`） |
| `replicate`（默认） | 一次性 + 持续增量 |

### 13.2 冲突解决策略

- **默认**：保留所有冲突版本（`_conflicts` 字段），由应用决定 winner；
- **自定义**：`_design/<d>/_reconcile/<n>` 函数（QuickJS/Spidermonkey）；
- **自动选择**：默认选择 `_revisions` 最深的版本（即"最新"）。

### 13.3 集群（3.x）

- **节点角色**：所有节点等同（无主从）；
- **分片**：基于 `hash(_id)` 在 ring 上分布（`fabric/` + `mem3/`）；
- **Reshard**：在线重分片（`mem3_reshard.erl` + `mem3_reshard_job.erl`）；
- **全局变更**：`_global_changes`（`global_changes_server.erl`）；
- **监控**：`couch_prometheus` 应用导出 Prometheus 指标。

### 13.4 复制源码

- `src/couch_replicator/src/couch_replicator.erl` (418) — 顶层 API；
- `src/couch_replicator/src/couch_replicator_scheduler.erl` + `couch_replicator_scheduler_job.erl` — 调度循环；
- `src/couch_replicator/src/couch_replicator_worker.erl` — 单次复制作业；
- `src/couch_replicator/src/couch_replicator_changes_reader.erl` / `_writer.erl` — `_changes` 拉 / `POST _bulk_docs` 推；
- `src/couch_replicator/src/couch_replicator_doc_processor.erl` — 文档去重、冲突解决；
- `src/couch_replicator/src/couch_replicator_auth.erl` — Cookie / Session / Noop 认证。

## 十四、扩展性

- **垂直扩展**：单机可处理 GB 级数据，但写吞吐受限（gen_server 串行化）；
- **水平扩展**：通过 **分片集群**（3.x 默认）：
  - `fabric/`：跨分片查询；
  - `mem3/`：分片位置管理；
  - `rexi/`：节点间 RPC；
- **多数据中心**：多主复制 + `_changes` + `_bulk_docs`；
- **冷热分层**：3.x 提供 `couchdb` 与 `couch_peruser` 数据库隔离；
- **运维约束**：单节点大库建议 < 100GB；频繁 compaction 影响读写。

## 十五、性能与基准

- **典型性能**（3.x 单节点，4 vCPU / 16GB RAM / SSD）：
  - `PUT /db/<id>`：~10k QPS；
  - `GET /db/<id>`：~30k QPS；
  - `POST /db/_find`（带索引）：~5k QPS；
  - `GET /db/_changes?feed=continuous`：~3k QPS（每个客户端）；
  - 视图查询：~2k QPS（命中索引）；
- **基准测试**：
  - 官方：`test/load/` 目录；
  - 社区：[github.com/cloudant-labs/couchdb-benchmark](https://github.com/cloudant-labs/couchdb-benchmark)；
  - **YCSB-CouchDB** 绑定；
- **调优点**：
  - 调整 `[compactions]`（`os_process_limit`、`attachment_chunk_size`）；
  - `[cluster]` 配置 `q=8, n=3`（quorum / replication factor）；
  - Nouveau 索引分片大小；
  - 设计文档预编译；
- **vs MongoDB**：写吞吐相当（10k QPS 量级），读较慢（30k vs MongoDB 100k）；同步延迟低（_changes + heartbeat）。

## 十六、运维与可观测性

- **配置文件**：`etc/local.ini`（主）+ `etc/default.ini`（默认）；
- **关键配置**：
  - `[couchdb]`：`single_node=true/false`、`max_dbs_open`、`max_attachment_size`；
  - `[cluster]`：`q`（quorum）、`n`（副本数）、`w`（写成功数）；
  - `[chttpd]`：`port`、`bind_address`、`enable_cors`；
  - `[compactions]`：`view_compaction` / `database_compaction`；
- **监控**：
  - `couch_prometheus` 应用导出 Prometheus 指标；
  - **stats URL**：`GET /_node/_local/_stats`；
  - **membership**：`GET /_membership`；
  - **uptime**：`_node/_local/_uptime`；
- **日志**：默认 stderr，可配置到 syslog/file；
- **备份**：
  - 文件级：`cp -R data/`（需停机）；
  - 在线：`_design/_replicator` + 复制到专用备份库；
  - `couchback`（社区工具）；
- **升级**：3.x 系列之间可直接升级；2.x → 3.x 需运行 `couchup` 工具。

## 十七、生态与集成

- **HTTP/REST**：任何语言均可直接调用；
- **官方 SDK**：
  - Python：`cloudant-python`（IBM）/ `python-couchdb`（社区）；
  - Node.js：`nano`（官方推荐）、`pouchdb`（端云同步）；
  - Java：**Ektorp**（已停更）/ **LightCouch**（社区）；
  - Go：`kivik`；
  - Rust：`couch_rs`；
- **同步生态**：
  - **PouchDB**：JavaScript 嵌入式（Cordova、Electron、React Native）；
  - **Couchbase Lite / LiteCore**（参考本仓 `reference/document/couchbase/`）：原生移动端，与 CouchDB 协议兼容；
- **ORM**：无主流 ORM；通过 SDK + ODM（如 `cloudant` Node.js）；
- **BI**：通过 `_find` + 复制到 SQL 库（如 `couchwarehouse` → Snowflake）；
- **容器化**：官方 Docker image + docker-compose；
- **K8s Operator**：[cloudnative-couchdb](https://github.com/IBM/cloud-native-couchdb)（IBM）；
- **云服务**：
  - **IBM Cloudant**（基于 CouchDB 2.x 分叉）；
  - **Cloudant on AWS**；
  - **DigitalOcean / Hetzner**：自托管。

## 十八、核心论文与算法原理

- **最终一致性 + MVCC**："Optimistic Replication" 论文（Yan & Junqueira 等，ACM Computing Surveys 2013）；
- **Erlang OTP**：[Learn You Some Erlang](https://learnyousomeerlang.com/)（社区经典）；
- **B-tree**：[Bayer & McCreight 1972 论文](https://infolab.umn.edu/research/research_database/group_meetings/eads_2016_fall/CS_7900_4_B-tree_Bayer_McCreight_1972.pdf)；
- **Smoosh 压缩合并**：[CouchDB 3.x 设计文档](https://github.com/apache/couchdb-pkg/blob/main/components/smoosh.md)；
- **Map/Reduce**：[Dean & Ghemawat, "MapReduce: Simplified Data Processing on Large Clusters", OSDI 2004](https://www.usenix.org/legacy/events/osdi04/tech/dean.html)（概念参考）。

## 十九、源码学习路径

### 19.1 入门（2-3 天）

1. `src/chttpd/src/chttpd.erl` — HTTP 监听；
2. `src/chttpd/src/chttpd_db.erl` — 数据库路由；
3. `src/couch/src/couch_db.erl` (2525) — 数据库主进程（核心中的核心）；
4. `src/couch/src/couch_db_updater.erl` (1015) — 单文档更新；
5. `src/couch/src/couch_changes.erl` (821) — `_changes` 实现；
6. `src/couch/src/couch_btree.erl` (1324) — B-tree。

### 19.2 进阶（1 周）

1. `src/couch/src/couch_db_engine.erl` (1044) — 引擎抽象；
2. `src/couch/src/couch_bt_engine.erl` (1266) — 新一代 B-tree 引擎；
3. `src/couch/src/couch_file.erl` (1068) — 文件 I/O；
4. `src/couch_replicator/src/couch_replicator.erl` (418) — 复制顶层 API；
5. `src/mango/src/mango_selector.erl` — Mango selector 解析；
6. `src/mango/src/mango_idx_view.erl` (719) — Mango → mrview 翻译；
7. `src/fabric/src/fabric.erl` + `src/mem3/src/mem3.erl` — 集群路由。

### 19.3 高级（2 周+）

1. `src/smoosh/src/` — 压缩合并器；
2. `src/nouveau/src/` — 倒排索引；
3. `src/couch_quickjs/src/` + `quickjs/` — JS 运行时；
4. 跟踪一次 `_changes` 请求：HTTP → chttpd → couch_changes → couch_db_updater → notify；
5. 跟踪一次复制：`_replicator` POST → couch_replicator → changes_reader → 远端 _changes → bulk_docs POST。

### 19.4 源码规模

- **OTP 应用数**：32 个；
- **Erlang .erl 文件数**：444 个非测试；
- **Erlang 代码行数**：~35k；
- **C/C++ 文件**：40 个（NIF，主要是 QuickJS、cfile、ejson_compare）；
- **构建系统**：rebar3 + Makefile（顶层）；
- **测试**：每个应用都有独立 `test/` 目录（`eunit` + `common_test`）。

## 二十、学习资源

- **官方文档**：[docs.couchdb.org](https://docs.couchdb.org/en/stable/)（**官方文档非常优秀**）；
- **书籍**：
  - **CouchDB: The Definitive Guide**（O'Reilly）；
  - **CouchDB in Action**（Manning，2012，老但仍可用）；
- **Quick Start**：[docs.couchdb.org/tutorials](https://docs.couchdb.org/en/stable/tutorials/index.html)；
- **官方博客**：[couchdb.apache.org](https://couchdb.apache.org/)；
- **Mailing Lists**：user@couchdb.apache.org、dev@couchdb.apache.org；
- **Slack**：[community-couchdb](https://join.slack.com/t/community-couchdb/shared_invite/...)；
- **视频**：[Apache CouchDB YouTube](https://www.youtube.com/@apachecouchdb)；
- **Reddit**：[r/couchdb](https://www.reddit.com/r/couchdb/)。

## 二十一、可移植性与部署

- **支持的平台**：Linux（amd64/arm64/ppc64le/s390x）、macOS、Windows（官方 Docker）；
- **容器化**：`apache/couchdb` 官方 Docker image；
- **Helm Chart**：[cloudnative-couchdb](https://github.com/IBM/cloud-native-couchdb) Helm Chart；
- **K8s Operator**：[IBM cloud-native-couchdb](https://github.com/IBM/cloud-native-couchdb)；
- **嵌入式部署**：CouchDB 设计是服务器模式，但可通过 **couch_peruser** 实现多租户；
- **最小硬件**：1 vCPU / 1GB RAM（开发）；生产建议 4 vCPU / 16GB RAM / SSD；
- **依赖**：Erlang/OTP 25+。

## 二十二、变体与衍生项目

- **IBM Cloudant**：基于 CouchDB 2.x 的商业发行版（已转向 Couchbase，2.x 历史保留）；
- **PouchDB**：JavaScript 嵌入式实现，与 CouchDB 协议兼容；
- **Couchbase Lite / LiteCore**：原生移动端（参考本仓 `reference/document/couchbase/`）；
- **BigCouch**：Cloudant 早期的 CouchDB 分叉（已合并）；
- **FoundationDB + CouchDB**：早期原型，未实用；
- **Cloudant on AWS**：托管服务；
- **Dreyfus → Nouveau**：全文索引的演进。

## 二十三、风险与陷阱

### 23.1 一致性陷阱

- **最终一致性 + 多主冲突**：AP 数据库默认，应用必须处理 `_conflicts`；
- **跨文档写入**：`_bulk_docs` 不保证原子性，需应用层协调；
- **`_rev` 缺失**：客户端忘记带 `_rev` 会导致覆盖（**经验教训**：永远要带 `_rev`）；
- **副本数量**：默认 `n=3` + `q=2` 写 quorum，应用需权衡延迟/一致性；
- **删除 tombstone**：`_deleted` 文档在 `_changes` 中仍出现，可能影响"删除校验"逻辑；
- **设计文档版本**：`_design` 也参与 MVCC，更新时必须带 `_rev`；
- **JS 引擎切换**：QuickJS / Spidermonkey 兼容性差异（reduce 函数语义）。

### 23.2 性能陷阱

- **Map 函数无 `emit`**：视图不可用；
- **Reduce 函数有副作用**：禁止（应在 Map 端处理）；
- **视图全量重建**：修改 Map 函数导致历史视图作废，**性能灾难**；
- **Mango 索引**：单次 `_find` 仅用一个索引（不支持 multi-index merge）；
- **Nouveau 字段权重**：错配权重导致命中错；
- **附件（`_attachments`）**：base64 编码开销大，单文档超过 1MB 应切分；
- **`_changes` continuous**：每个客户端独占连接，**不支持数千客户端**；
- **Smoosh compaction**：频繁触发导致 I/O 抖动。

### 23.3 运维陷阱

- **本地文件复制备份**：必须停机或 _replicator 复制；
- **升级路径**：2.x → 3.x 需 `couchup`；3.x → 3.x 通常就地升级；
- **集群拆分**：3.x reshard 期间性能下降；
- **监控指标**：默认指标有限，必须配合 `couch_prometheus`；
- **Erlang `/` JavaScript 桥接**：JS 函数异常可能使视图卡住；
- **多数据中心**：复制延迟高（典型 1~10s），应用需 idempotent 写入；
- **配额**：CouchDB 没有 per-db 配额，需 OS 层 lvm/quotas；
- **TLS**：内置支持，但需要 Erlang 26+；
- **认证**：CouchDB 3.x 内置 Cookie + JWT + Proxy 认证，无 RBAC（依赖 `_security` 文档）。

## 二十四、学习路径

### 24.1 入门（1 周）

1. 跑官方 Docker：`docker run -p 5984:5984 apache/couchdb:3`；
2. 用 `curl` 执行 `PUT /db` / `PUT /db/doc` / `GET /db/doc`；
3. 阅读 [CouchDB: The Definitive Guide](https://docs.couchdb.org/en/stable/intro/index.html)；
4. 阅读 `src/couch/src/couch_db.erl` 第 100~200 行；
5. 在本地装 PouchDB，连接 CouchDB，调试 `_changes` 流。

### 24.2 中级（2-3 周）

1. 部署 3 节点 CouchDB 集群（官方 quickstart cluster）；
2. 测试 reshard 操作；
3. 写 Mango `_find` 查询；
4. 阅读 `src/couch_replicator/src/couch_replicator.erl` + `couch_replicator_changes_reader.erl`；
5. 跟踪一次复制：复制状态变更 → `_changes` 拉取 → `_bulk_docs` 推送；
6. 部署 Nouveau 全文索引，写 `_find` with `$text`；
7. 在应用层（Python/Node.js）实现冲突解决逻辑。

### 24.3 进阶（1 个月+）

1. 部署 CouchDB + Couchbase Lite + PouchDB 三层架构（移动 → 浏览器 → 服务端）；
2. 深入阅读 Smoosh 压缩合并器源码（`src/smoosh/`）；
3. 贡献 Mango 新操作符或 Nouveau 分词器；
4. 配置 Prometheus 监控；
5. 跨数据中心多主复制演练；
6. 性能基准 vs MongoDB / PostgreSQL JSONB；
7. 写 `_design` 文档：自定义 `_view` + `_filter` + `_update`。

## 二十五、与本项目的关联

`engineering/` 中**没有 CouchDB 风格文档数据库**实现，但有以下可借鉴模式：

- `engineering/src/db/core/catalog.c` 的元数据管理可参考 `src/couch/src/couch_db.erl` 中的 `#doc_info` 设计；
- `engineering/src/db/bufmgr.c` 的 buffer pool 可参考 `src/couch/src/couch_bt_engine.erl` 的"块缓存 + 写前/写后日志"思路；
- `engineering/src/db/wal.c` 的 WAL 与 `src/couch/src/couch_file.erl` 的 commit 协议类似——先 commit 头，再写数据；
- `engineering/src/db/storage/btree.c`（如有）可参考 `src/couch/src/couch_btree.erl` 的节点分裂/合并算法；
- **AP 优先的多主复制** 设计模式可应用到 `learning/code-solutions/` 中的分布式存储练习题。

## 二十六、MVCC 修订树与 B+Tree 引擎

CouchDB 的核心数据结构有两层：内存中的 **文档修订树**（`_rev` 版本链）+ 磁盘上的 **append-only B+Tree** 文件。理解这两层是理解 CouchDB MVCC 的关键——它不是关系数据库那种行级 MVCC，而是**文档级 MVCC + 文件级原子提交**。

### 26.1 修订树（Revision Tree）

每个文档对应一棵以 root 为叶、`{Pos, [RevPath]}` 为分支的有序树，结构定义在 `src/couch/src/couch_key_tree.erl:68`：

```erlang
-type treenode() :: {Key :: term(), Value :: term(), [Node :: treenode()]}.
-type tree() :: {Depth :: pos_integer(), [treenode()]}.
```

- **节点**：`{Key, Value, [SubNodes]}`——`SubTree == []` 时为 **leaf**（终末版本），否则为 **branch**（分裂点）；
- **Key**：形如 `{Pos, Hash}` 的修订号；
- **Value**：指向 `couch_bt_engine.erl` 中本地节点文件的 `{Pointer, Size}`；
- **整棵树**：`{Depth, [RootNode]}` 列表，多个 root 表示并发分支（冲突）。

修订树的**合并**在 `couch_key_tree.erl:87-208` 的 `merge/2` → `merge_tree/3` → `merge_at/3` → `merge_extend/2` 中实现。调用方是 `couch_db_updater.erl:484` 的 `merge_rev_tree/4`：

```erlang
% src/couch/src/couch_db_updater.erl:538-600（节选）
merge_rev_tree(OldInfo, NewDoc, Client, false) when OldInfo#full_doc_info.deleted ->
    ...
    {NewTree1, new_leaf} ->
        NewInfo#full_doc_info{rev_tree = NewTree1, deleted = NewDeleted};
merge_rev_tree(OldInfo, NewDoc, Client, false) ->
    NewTree = couch_key_tree:merge(OldInfo#full_doc_info.rev_tree,
                                   couch_doc:to_path(NewDoc#doc.revs)),
    case NewTree of
        {NewTree, new_leaf} when not NewDeleted ->
            ...; % 创建新分支
        {NewTree, new_leaf} when NewDeleted ->
            ...; % 软删除
    end.
```

合并结果有四种语义：`new_leaf`（新版本）/ `new_branch`（创建分支）/ `internal_node`（插入中间节点）/ 失败（`_rev` 冲突，返回 409）。

### 26.2 Stem 修剪（revs_limit）

修订树深度有上限（默认 1000，`revs_limit`），超长历史会被修剪。算法在 `couch_key_tree.erl:477-525`：

```erlang
% src/couch/src/couch_key_tree.erl:477-491
stem(Trees, Limit) ->
    try
        {_, Branches} = lists:foldl(
            fun(Tree, {Seen, TreeAcc}) ->
                {NewSeen, NewBranches} = stem_tree(Tree, Limit, Seen),
                {NewSeen, NewBranches ++ TreeAcc}
            end,
            {#{}, []},
            Trees
        ),
        lists:sort(Branches)
    catch
        throw:dupe_keys ->
            repair_tree(Trees, Limit)  % 容错：去重后重试
    end.
```

- **剪枝方向**：自顶向下 DFS，每走过一个 leaf，`Limit - 1`；
- **Seen map**：避免重复节点被剪两次（`repair_tree` 兜底）；
- **调用链**：`couch_db_updater.erl:497` 的 `maybe_stem_full_doc_info/2` 每次写入后触发；
- **配置**：`config:get_boolean("couchdb", "stem_interactive_updates", true)` 控制是否对交互式写入修剪（复制路径不修剪，保留全量历史）。

### 26.3 B+Tree 节点类型与分裂

CouchDB 的 B+Tree 是两类型节点设计，见 `src/couch/src/couch_btree.erl:365-368`：

```erlang
% src/couch/src/couch_btree.erl:365-370
case NodeType of
    kp_node ->                 % 内部节点：Key → {Pointer, Reduction}
        lookup_kpnode(Bt, list_to_tuple(NodeList), 1, Keys, [], Depth);
    kv_node ->                 % 叶子节点：Key → Value
        lookup_kvnode(Bt, list_to_tuple(NodeList), 1, Keys, [], Depth)
end
```

- **kp_node**（key-pointer）：内部节点；`[{K, {Pointer, Reduction}}]`；
- **kv_node**（key-value）：叶子节点；`[{K, Value}]`；
- **Reduction 缓存**：每个内部节点存孩子子树的 reduce 结果，实现 O(log n) range aggregate（`reduce_node/3` 见 `couch_btree.erl:523-528`，分别对 kp_node 调用 `R(rereduce, ...)`、对 kv_node 调用 `R(reduce, ...)`）。

节点**切分阈值** `?DEFAULT_CHUNK_SIZE = 1279`（字节数，`couch_btree.erl:24`），由 `chunkify/1` 控制：

```erlang
% src/couch/src/couch_btree.erl:449-477（核心循环）
chunkify([InElement | RestInList], ChunkThreshold, OutList, OutListSize, OutputChunks) ->
    case ?term_size(InElement) of
        Size when (Size + OutListSize) > ChunkThreshold andalso OutList /= [] ->
            chunkify(RestInList, ChunkThreshold, [], 0,
                     [lists:reverse([InElement | OutList]) | OutputChunks]);
        Size ->
            chunkify(RestInList, ChunkThreshold,
                     [InElement | OutList], OutListSize + Size, OutputChunks)
    end.
```

每次写节点都按 1279B 自动切块（chunk），这就是 brief 提到的 **chunk/stem/node** 的 chunk 概念——并非四级树结构，而是**节点按字节切片 + stem 修剪 + 内部/叶子两型**。

### 26.4 文件级原子提交（commit_data）

CouchDB 的"事务"边界是**整个数据库文件**（不是页级），实现见 `couch_bt_engine.erl:557-576`：

```erlang
% src/couch/src/couch_bt_engine.erl:557-576
commit_data(St) ->
    #st{fd = Fd, header = OldHeader, needs_commit = NeedsCommit} = St,
    NewHeader = update_header(St, OldHeader),
    case NewHeader /= OldHeader orelse NeedsCommit of
        true ->
            ok = couch_file:write_header(Fd, NewHeader, [sync]),  % 头 fsync
            couch_stats:increment_counter([couchdb, commits]),
            {ok, St#st{header = NewHeader, needs_commit = false}};
        false ->
            {ok, St}
    end.
```

- **append-only 写**：新 kv_node / kp_node 追加到文件末尾，由 `couch_file:append_terms/3` 写入；
- **头覆盖**：每次提交**覆盖文件头**（`update_seq` 等元信息），头是热路径所以 `couch_file.erl` 会单独缓冲；
- **fsync 协议**：先 `write_header(Fd, NewHeader, [sync])` 同步刷头，再由 `couch_file.erl` 异步刷数据——崩溃恢复时根据头是否包含某 update_seq 决定重做 / 跳过；
- **Commit 触发点**：所有写入路径（`update_docs_int/4` 见 `couch_db_updater.erl:737`、`set_security/2`、`set_revs_limit/2` 等）最后都调 `commit_data/1`。

### 26.5 复杂度

| 操作 | 复杂度 | 备注 |
| --- | --- | --- |
| `couch_key_tree:find_missing/2` | O(n × m)，n = 修订树分支数、m = 待查 keys（已排序） | 见 `couch_key_tree.erl:211-231`（`splitwith` + 递归） |
| `couch_key_tree:merge/2` | O(d²)，d = 树深度 | 见 `couch_db_updater.erl:87-208` 的 `merge_at` |
| `couch_btree:lookup/2` | O(log n) | 二分查找节点 → chunk 内二分 |
| `couch_btree:fold_reduce/4` | O(log n + k)，k = 命中行数 | 内部节点 reduction 缓存跳过子树 |
| `commit_data/1` | O(1)（仅头写） + fsync 延迟 | 实际吞吐被 fsync 主导 |

### 26.6 源码路径

- `src/couch/src/couch_key_tree.erl` (588 行) — 修订树所有操作（`find_missing`、`merge`、`stem`、`remove_leafs`、`count_leafs`、`get_key_leafs`、`get_all_leafs`）；
- `src/couch/src/couch_btree.erl` (1324) — B+Tree 引擎（`query_modify`、`add_remove`、`fold_reduce`、`final_reduce`、`chunkify`）；
- `src/couch/src/couch_bt_engine.erl` (1266) — 引擎抽象层，`commit_data/1` 在 557 行，`update_header/2` 在 568 行附近；
- `src/couch/src/couch_db_updater.erl:469-536` — `merge_rev_trees/3` 批量合并循环，:538-636 — `merge_rev_tree/4` 单文档合并；
- `src/couch/src/couch_bt_engine.hrl` (28 行) — `#st{}` 状态记录（含 `id_tree` / `seq_tree` / `local_tree` 等多个 B+Tree）；
- `src/couch/src/couch_file.erl` (1068) — append-only 文件 + 头缓存（`write_header/3`、`append_terms/3`、`pread_term/2`）。

## 二十七、MapReduce 视图与并发管道

CouchDB 的视图系统由 `couch_mrview` 应用实现，核心思想：**预计算**（增量更新）+ **B+Tree 索引**（按 `<key, docid>` 排序）+ **并发管道**（Map 与 Write 解耦）。

### 27.1 三角色并发管道

视图更新的主流程在 `src/couch_mrview/src/couch_mrview_updater.erl:22-70` 的 `start_update/4`：

```erlang
% src/couch_mrview/src/couch_mrview_updater.erl:38-68（节选）
MapFun = fun() ->
    erlang:put(io_priority, {view_update, State#mrst.db_name, State#mrst.idx_name}),
    Progress = case NumChanges of 0 -> 0; _ -> (NumChangesDone * 100) div NumChanges end,
    couch_task_status:add_task([...]),
    couch_task_status:set_update_frequency(500),
    map_docs(Self, InitState)               % 角色 1：Map 进程
end,
WriteFun = fun() ->
    erlang:put(io_priority, {view_update, State#mrst.db_name, State#mrst.idx_name}),
    write_results(Self, InitState)           % 角色 2：Write 进程
end,
spawn_link(MapFun),
spawn_link(WriteFun),
{ok, InitState}.
```

- **Map 进程**（`map_docs/2` 见 `couch_mrview_updater.erl:152-184`）：从 `doc_queue` 取批，对每个非 deleted 文档调 `couch_query_servers:map_doc_raw/2`（QuickJS / Spidermonkey 执行用户 Map 函数），结果扔进 `write_queue`；
- **Write 进程**（`write_results/2` 见 `couch_mrview_updater.erl:186-199`）：从 `write_queue` 拉批，调 `couch_btree:add_remove/3` 写入 view 的 B+Tree；
- **协调者**（caller）：持有 `State#mrst`，通过 `receive {new_state, NewState}` 拿到最终状态。

**吞吐优化**：两个 work queue 都按 `max_size + max_items` 双阈值限流（默认 10MB / 500 项，见 `couch_mrview_updater.erl:22-25`）。

### 27.2 Map → Write 数据流

```erlang
% src/couch_mrview/src/couch_mrview_updater.erl:237-258
merge_results({DocId, []}, ViewKVs, DocIdKeys) ->
    {ViewKVs, [{DocId, []} | DocIdKeys]};
merge_results({DocId, RawResults}, ViewKVs, DocIdKeys) ->
    JsonResults = couch_query_servers:raw_to_ejson(RawResults),
    Results = [[list_to_tuple(Res) || Res <- FunRs] || FunRs <- JsonResults],
    case lists:flatten(Results) of
        [] -> {ViewKVs, [{DocId, []} | DocIdKeys]};
        _  ->
            {ViewKVs1, ViewIdKeys} = insert_results(DocId, Results, ViewKVs, [], []),
            {ViewKVs1, [ViewIdKeys | DocIdKeys]}
    end.
```

`insert_results/5`（`couch_mrview_updater.erl:259-278`）做同一文档同一视图多 key 的去重：

```erlang
% src/couch_mrview/src/couch_mrview_updater.erl:262-269
CombineDupesFun = fun
    ({Key, Val}, {[{Key, {dups, Vals}} | Rest], IdKeys}) ->
        {[{Key, {dups, [Val | Vals]}} | Rest], IdKeys};
    ({Key, Val1}, {[{Key, Val2} | Rest], IdKeys}) ->
        {[{Key, {dups, [Val1, Val2]}} | Rest], IdKeys};
    ({Key, Value}, {Rest, IdKeys}) ->
        {[{Key, Value} | Rest], [{Id, Key} | IdKeys]}
end,
```

Map 函数 `emit(K, V)` 一次 → B+Tree 一行 `(K, DocId) → V`；同一文档同一 Key 多次 emit → 折叠为 `{dups, [V1, V2, ...]}`，reduce 阶段按 `rereduce` 合并。

### 27.3 二阶段 Reduce

Reduce 是 B+Tree 节点级的**聚合**操作，分两阶段执行：

```erlang
% src/couch/src/couch_btree.erl:102-112
final_reduce(#btree{reduce = Reduce}, Val) ->
    final_reduce(Reduce, Val);
final_reduce(Reduce, {[], []}) ->
    Reduce(reduce, []);
final_reduce(_Bt, {[], [Red]}) ->
    Red;
final_reduce(Reduce, {[], Reductions}) ->
    Reduce(rereduce, Reductions);
final_reduce(Reduce, {KVs, Reductions}) ->
    Red = Reduce(reduce, KVs),
    final_reduce(Reduce, {[], [Red | Reductions]}).
```

- **第一阶段（reduce）**：叶子节点内的所有 (K, V) → 单值；
- **第二阶段（rereduce）**：子节点的 reduce 结果列表 → 单值；
- **递归终止**：当只剩一个 reduction 时直接返回，跳过第三次调用。

每层内部节点缓存 reduction（`reduce_node/3`，`couch_btree.erl:523-528`），查询时无需下沉到叶子即可返回 group / range reduce——这就是视图查询 `reduce=true&group_level=N` 的 O(log n + k) 来源。

### 27.4 批处理与流控

`process_doc/3`（`couch_mrview_updater.erl:122-130`）按 100 个文档一攒批发送：

```erlang
% src/couch_mrview/src/couch_mrview_updater.erl:122-130
process_doc(Doc, Seq, #mrst{doc_acc = Acc} = State) when length(Acc) > 100 ->
    couch_work_queue:queue(State#mrst.doc_queue, lists:reverse(Acc)),
    process_doc(Doc, Seq, State#mrst{doc_acc = []});
process_doc(#doc{id = Id, deleted = true}, Seq, #mrst{doc_acc = Acc} = State) ->
    {ok, State#mrst{doc_acc = [{Id, Seq, deleted} | Acc]}};
process_doc(#doc{id = Id} = Doc, Seq, #mrst{doc_acc = Acc} = State) ->
    {ok, State#mrst{doc_acc = [{Id, Seq, Doc} | Acc]}}.
```

`accumulate_more/2`（`couch_mrview_updater.erl:230-235`）控制 Write 端的 flush 节奏：

```erlang
% src/couch_mrview/src/couch_mrview_updater.erl:230-235
accumulate_more(NumDocIds, Acc) ->
    MinItems = config:get_integer("view_updater", "min_writer_items", 100),
    MinSize  = config:get_integer("view_updater", "min_writer_size",  16777216),  % 16MB
    CurrMem  = ?term_size(Acc),
    NumDocIds < MinItems andalso CurrMem < MinSize.
```

Write 端不到 100 项 / 16MB 就继续攒批，避免过小的 B+Tree modify。

### 27.5 复杂度

| 操作 | 复杂度 | 备注 |
| --- | --- | --- |
| 单文档 Map | O(m)，m = emit 行数 | 用户 JS 函数时间 |
| 视图增量更新 | O(N × m)，N = 增量文档数 | 每文档 m 次 emit |
| 视图全量重建 | O(M × m)，M = 总文档数 | `first_build=true` 时 |
| 视图查询（reduce） | O(log n + k)，k = 输出行 | 利用内部节点 reduction 缓存 |
| 视图查询（不 reduce） | O(log n + k) | 标准 B+Tree 范围扫描 |
| `_view` group | O(log n + g)，g = 分组数 | group_level 控制聚合深度 |

### 27.6 源码路径

- `src/couch_mrview/src/couch_mrview_updater.erl` (382) — Map/Write 并发管道；
- `src/couch_mrview/src/couch_mrview_index.erl` (388) — view indexer 生命周期（`init/2`、`start_update/4`、`process_doc/3`、`finish_update/1`、`commit/1`）；
- `src/couch_mrview/src/couch_mrview.erl` (807) — view 查询入口（`query_view/3,4,6`、`get_view_index_pid/4`、`validate/2`）；
- `src/couch_mrview/src/couch_mrview_util.erl` (1403) — view group 状态初始化（`init_state/4`、`ddoc_to_mrst/2`、`make_header/1`）；
- `src/couch_mrview/src/couch_mrview_compactor.erl` (322) — view compaction（`compact/3`、`swap_compacted/2`）；
- `src/couch_mrview/src/couch_mrview_update_notifier.erl` (43) — 数据库更新订阅 → 触发 view 增量刷新；
- `src/couch_mrview/include/couch_mrview.hrl` — `#mrst{}` 视图组状态记录与 `#mrview{}` 单视图记录。

## 二十八、Erlang/OTP 并发模型与差量同步

CouchDB 充分利用 Erlang/OTP 的**进程模型 + 消息传递 + gen_server 模式**实现并发与一致性。本节拆解三个最关键的并发原语：**串行化更新器**、**`_changes` 推送**、**`_revs_diff` 差量同步**。

### 28.1 串行化更新器（`couch_db_updater`）

每个数据库对应一个 **gen_server 进程**——这是 CouchDB 实现 MVCC 单写者语义的物理基础。定义在 `couch_db.erl:168-170`：

```erlang
% src/couch/src/couch_db.erl:168-170
start_link(Engine, DbName, Filepath, Options) ->
    Arg = {Engine, DbName, Filepath, Options},
    proc_lib:start_link(couch_db_updater, init, [Arg]).
```

启动后进入 gen_server enter_loop（`couch_db_updater.erl:34-57`）：

```erlang
% src/couch/src/couch_db_updater.erl:34-53
init({Engine, DbName, FilePath, Options0}) ->
    erlang:put(io_priority, {db_update, DbName}),
    ...
    NewDb = Db#db{main_pid = self()},
    proc_lib:init_ack({ok, NewDb}),
    GenOpts = couch_util:hibernate_after(?MODULE),
    gen_server:enter_loop(?MODULE, GenOpts, NewDb).
```

- **单写者**：所有 `PUT /db/<id>` / `POST /db/_bulk_docs` 都通过 `gen_server:call` 或 `cast` 进入此进程的消息队列；
- **顺序处理**：Erlang 调度器保证同一进程的消息按到达顺序处理；
- **hibernate**：空闲 1s 后自动 `hibernate`，降低内存占用。

### 28.2 Group Commit（写入批合并）

`couch_db_updater` 在处理一条 `update_docs` 消息时，会**非阻塞地**继续吸收后续消息，合并为一次 commit。实现见 `couch_db_updater.erl:275-296`：

```erlang
% src/couch/src/couch_db_updater.erl:275-296
collect_updates(GroupedDocsAcc, ClientsAcc, ReplicatedChanges) ->
    receive
        {update_docs, Client, GroupedDocs, [], ReplicatedChanges} ->
            case ReplicatedChanges of
                true  -> couch_stats:increment_counter([couchdb, coalesced_updates, replicated]);
                false -> couch_stats:increment_counter([couchdb, coalesced_updates, interactive])
            end,
            GroupedDocs2 = sort_and_tag_grouped_docs(Client, GroupedDocs),
            GroupedDocsAcc2 = merge_updates(GroupedDocsAcc, GroupedDocs2),
            collect_updates(GroupedDocsAcc2, [Client | ClientsAcc], ReplicatedChanges)
    after 0 ->
        {GroupedDocsAcc, ClientsAcc}
    end.
```

伪代码：

```
ON receive update_docs(Client, GroupedDocs, [], ReplicatedChanges):
    WHILE mailbox has another {update_docs, C', G', [], ReplicatedChanges}:
        Stats += coalesced_updates
        GroupedDocsAcc  = merge_updates(GroupedDocsAcc, G')   // 按 docid 合并
        ClientsAcc      = [C' | ClientsAcc]
    AFTER 0ms timeout, drain stops    // 不阻塞，立即返回

ON drain return:
    {ok, NewDb} = update_docs_int(Db, GroupedDocsAcc, ..., ReplicatedChanges)
    couch_event:notify(NewDb#db.name, updated)   // 触发 _changes 推送
    foreach Client in ClientsAcc: Client ! {done, self()}
```

效果：**N 条并发写入共享一次 fsync**，吞吐量提升 5~10×（受限于 fsync 延迟）。`merge_updates/2`（`couch_db_updater.erl:270-273`）按文档 ID 做线性合并：

```erlang
% src/couch/src/couch_db_updater.erl:270-273
merge_updates([], RestB) -> RestB;
merge_updates(RestA, []) -> RestA.
merge_updates([#doc{id = A} = DocA | RestA], [#doc{id = B} = DocB | RestB]) when A =< B ->
    if A == B -> [DocA, DocB | merge_updates(RestA, RestB)];
       true   -> [DocA | merge_updates(RestA, [DocB | RestB])]
    end;
merge_updates([#doc{id = A} = DocA | RestA], [#doc{id = B} = _DocB | _] = RestB) when A > B ->
    [DocA | merge_updates(RestA, RestB)].
```

### 28.3 `_changes` seqno 语义

`update_seq` 是 CouchDB 的逻辑时钟，每次 commit 递增（`couch_db_updater.erl:522-533`，`update_seq = Acc#merge_acc.cur_seq + 1`）。`src/couch/src/couch_changes.erl:73-82` 给出起始 seq 的解析：

```erlang
% src/couch/src/couch_changes.erl:73-82
Start = fun() ->
    {ok, Db} = couch_db:reopen(Db0),
    StartSeq =
        case Dir of
            rev -> couch_db:get_update_seq(Db);    % 反向：从当前最新开始
            fwd -> Since                              % 正向：从 since 参数开始
        end,
    {Db, StartSeq}
end,
```

- **`since=<seq>`**（fwd 模式）：从该 seq 之后开始（不包含）；
- **`since=now`**：等价于当前 `update_seq`（订阅模式典型用法）；
- **`since=0`**：从数据库诞生开始；
- **dir=rev**：反向流，从最新 seq 倒序；
- **seq 来源**：`couch_db:get_update_seq/1`（`couch_db.erl:567-568`）从引擎状态读 `St#st.update_seq`。

事件触发由 `couch_db_updater.erl:194-200` 完成：

```erlang
% src/couch/src/couch_db_updater.erl:194-200
case {couch_db:get_update_seq(Db), couch_db:get_update_seq(Db2)} of
    {Seq, Seq} -> ok;
    _ -> couch_event:notify(Db2#db.name, updated)
end,
if LocalDocs2 /= [] ->
    couch_event:notify(Db2#db.name, local_updated);
   true -> ok
end,
```

`couch_event:notify/2` 是**带 name 的 pub/sub**，所有通过 `couch_event:link_listener/3` 注册的 `_changes` 客户端进程都会被唤醒并消费新事件（见 `couch_changes.erl:69-71` 的 `StartListenerFun`）。

### 28.4 `_revs_diff` 差量同步

复制器用 `_revs_diff` 协议做差量推送：源端告知目标端"我有哪些 `_rev`"，目标端返回"我缺哪些 + 共同的祖先"。**目标端**实现在 `src/couch/src/couch_db.erl:387-400`：

```erlang
% src/couch/src/couch_db.erl:387-400
get_missing_revs(Db, IdRevsList) ->
    FDIs = get_full_doc_infos(Db, [Id || {Id, _Revs} <- IdRevsList]),
    Results = lists:zipwith(
        fun
            ({Id, Revs}, #full_doc_info{rev_tree = RevTree} = FDI) ->
                MissingRevs = couch_key_tree:find_missing(RevTree, Revs),
                {Id, MissingRevs, possible_ancestors(FDI, MissingRevs)};
            ({Id, Revs}, not_found) ->
                {Id, Revs, []}
        end,
        IdRevsList,
        FDIs
    ),
    {ok, Results}.
```

`couch_key_tree:find_missing/2`（`couch_key_tree.erl:211-231`）通过**树的剪枝遍历**回答"哪些 key 不在树里"：

```erlang
% src/couch/src/couch_key_tree.erl:211-231
find_missing(Tree, SearchKeys) ->
    find_missing_sorted(Tree, lists:sort(SearchKeys)).

find_missing_sorted(_Tree, []) -> [];
find_missing_sorted([], SearchKeys) -> SearchKeys;
find_missing_sorted([{Start, {Key, Value, SubTree}} | RestTree], SearchKeys) ->
    {Impossible, Possible} = lists:splitwith(fun({K, _}) -> K < Start end, SearchKeys),
    Missing = find_missing_simple(Start, [{Key, Value, SubTree}], Possible),
    find_missing_sorted(RestTree, Impossible ++ Missing).
```

- **SearchKeys 必须已排序**（`lists:sort`）；
- **splitwith**：把"不可能在本子树"（key < Start）和"可能在本子树"分离；
- **find_missing_simple**：在单个子树中递归查找——能命中就跳过，否则加入 Missing；
- **possible_ancestors**：从 revtree 中找出可能为祖先的 revision 列表（用于断网恢复时的冲突修复）。

**复制端**发起请求在 `src/couch_replicator/src/couch_replicator_api_wrap.erl:172-199`：

```erlang
% src/couch_replicator/src/couch_replicator_api_wrap.erl:172-199
get_missing_revs(#httpdb{} = Db, IdRevs) ->
    JsonBody = {[{Id, couch_doc:revs_to_strs(Revs)} || {Id, Revs} <- IdRevs]},
    RawBody = ?JSON_ENCODE(JsonBody),
    ...
    send_req(Db, [{method, post}, {path, "_revs_diff"}, ...],
        fun
            (200, _, {Props}) ->
                ConvertToNativeFun = fun({Id, {Result}}) ->
                    MissingRevs = couch_doc:parse_revs(get_value(<<"missing">>, Result)),
                    PossibleAncestors = couch_doc:parse_revs(get_value(<<"possible_ancestors">>, Result, [])),
                    {Id, MissingRevs, PossibleAncestors}
                end,
                {ok, lists:map(ConvertToNativeFun, Props)};
            (ErrCode, _, ErrMsg) ->
                {error, {revs_diff_failed, ErrCode, ErrMsg}}
        end
    ).
```

完整复制流程见 `src/couch_replicator/src/couch_replicator_worker.erl:381` 的 `attempt_revs_diff/2`——根据 `_revs_diff` 成功率动态调整 bulk_get vs `_revs_diff` 的策略（`update_revs_diff_ratio/4` 见 400 行）。

### 28.5 复杂度

| 操作 | 复杂度 | 备注 |
| --- | --- | --- |
| 单文档写入 | O(d²)，d = 修订树深度 | `merge_rev_tree` + stem |
| Group commit 合并 | O(k × n)，k = 合并条数、n = 文档数 | `after 0` 非阻塞 drain |
| `_changes` 推送 | O(1)/事件 | 事件总线 pub/sub |
| `find_missing` 差量 | O(m × log k)，m = 待查 keys、k = 树分支数 | sort + splitwith |
| 复制 `_revs_diff` | O(1) HTTP + 服务端 O(m × log k) | POST + JSON |

### 28.6 源码路径

- `src/couch/src/couch_db_updater.erl` (1015) — gen_server 主循环：`init/1`(34)、`handle_call/3`(64-130)、`handle_cast/2`(138-167)、`handle_info/2`(174-240)、`collect_updates/3`(275-296)、`update_docs_int/4`(650-737)、`commit_data/1`(908-912)；
- `src/couch/src/couch_db.erl` (2525) — 数据库 API 门面：`get_missing_revs/2`(387)、`get_update_seq/1`(567)、`start_link/4`(168)、`time_seq_since/2`(584)；
- `src/couch/src/couch_changes.erl` (821) — `_changes` 实现：`handle_db_changes/3`(57-)、`build_acc/8` 构造累加器、`send_changes/4`、`keep_sending_changes/3`(596-)、`wait_updated/3`(621)；
- `src/couch/src/couch_key_tree.erl` (588) — `find_missing/2`(211)、`find_missing_sorted/2`(214)、`find_missing_simple/3`(223)、`merge_at/3`(128)、`stem/2`(477)；
- `src/couch_replicator/src/couch_replicator_api_wrap.erl` — `get_missing_revs/2`(172-199)、`bulk_get/3`(201-)；
- `src/couch_replicator/src/couch_replicator_worker.erl` — `attempt_revs_diff/2`(381)、`update_revs_diff_ratio/4`(400)、`handle_info(revs_diff_failed,...)`(235)、revs_diff 决策循环(670-707)；
- `src/couch_replicator/src/couch_replicator_changes_reader.erl` — `_changes` 拉取端（流式 reader）；
- `src/couch_replicator/src/couch_replicator_doc_processor.erl` — 文档处理（去重 / conflict resolve）；
- `src/chttpd/src/chttpd_db.erl:26` — HTTP `handle_changes_req/2` 入口；:446-489 — `_missing_revs` / `_revs_diff` HTTP 路由；
- `src/couch/src/couch_event.erl` + `src/couch_event_sup.erl` — pub/sub 事件总线（`notify/2`、`link_listener/3`）；
- `src/couch/src/couch_work_queue.erl` — 视图 / 复制管道使用的有界 work queue（`max_size` + `max_items` 双阈值）。

## 二十九、参考资料

1. **CouchDB 官方仓库**：[github.com/apache/couchdb](https://github.com/apache/couchdb)；
2. **CouchDB 官方文档**：[docs.couchdb.org](https://docs.couchdb.org/en/stable/)；
3. **Apache CouchDB 主页**：[couchdb.apache.org](https://couchdb.apache.org/)；
4. **CouchDB Definitive Guide**：[O'Reilly 在线版](https://docs.couchdb.org/en/stable/intro/index.html)；
5. **PouchDB 文档**：[pouchdb.com](https://pouchdb.com/)；
6. **Cloudant on AWS**：[cloud.ibm.com/catalog](https://cloud.ibm.com/catalog/services/cloudant)；
7. **CouchDB 中文社区**：[github.com/couchdb-china](https://github.com/couchdb-china)；
8. **CouchDB The Definitive Guide (PDF)**：[github.com/CouchDB-The-Definitive-Guide](https://github.com/oreillymedia/couchdb-the-definitive-guide)；
9. **CouchDB Smoosh 设计文档**：[github.com/apache/couchdb-pkg/blob/main/components/smoosh.md](https://github.com/apache/couchdb-pkg/blob/main/components/smoosh.md)；
10. **CouchDB 源代码结构**（本仓）：`reference/document/couchdb/`；
11. **Optimistic Replication 论文**：Yan & Junqueira, ACM CSUR 2013；
12. **Erlang OTP 设计**：[learnyousomeerlang.com](https://learnyousomeerlang.com/)。