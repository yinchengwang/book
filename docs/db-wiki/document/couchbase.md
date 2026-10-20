# Couchbase Lite / LiteCore Wiki（嵌入式文档数据库 C++ 引擎）

> 参考版本：Couchbase LiteCore 当前 commit `e048a059`（CBL-8715 系列；Couchbase Lite 3.x），C++20 实现的嵌入式引擎，Apache 2.0 / Business Source License 双重许可。

## 一、项目定位

Couchbase LiteCore 是 **Couchbase Lite 移动端 / 边缘端嵌入式文档数据库的核心引擎**——一个纯 C++（无外部服务依赖）的本地文档数据库。它的设计目标是"**让移动设备、桌面应用、嵌入式系统都能拥有 CouchDB 风格的文档同步能力**"——既支持本地 CRUD，又通过 BLIP/WebSocket 协议与 Couchbase Sync Gateway / CouchDB 双向同步。

- **痛点**：
  - 移动设备经常离线/弱网，需要本地数据库；
  - 端云同步需要稳定的冲突解决协议；
  - 多设备一致性（手机 ↔ 平板 ↔ 桌面 ↔ 云）；
  - 设备资源受限（CPU、内存、电量）；
  - 安全要求（加密存储）；
- **典型用户**：
  - **Couchbase Lite 用户**：移动应用（iOS/Android）、桌面应用（MAUI/Xamarin/.NET/Flutter）、边缘设备；
  - **Sync Gateway 用户**：Couchbase Server 的前端代理；
  - **典型客户**：American Airlines、United Airlines、Marriott、Equifax、Synchrony Financial（金融）、Verizon、T-Mobile（电信）；
- **核心口号**："**Embed Couchbase everywhere**"——把企业级文档能力下沉到设备端。

## 二、发展历史与社区

- **起源**：2014 年由 Couchbase 公司发布（前身是 TouchDB，基于 CouchDB 1.x 协议）；
- **里程碑**：
  - 2014：Couchbase Lite 1.0（TouchDB 改名 + Apache 2.0）；
  - 2017：LiteCore 重写（C++ 实现，跨平台）；
  - 2019：Couchbase Lite 3.0（C4 API 稳定，多集合、查询改进）；
  - 2020：增加 Predictive Query、Vector Search（实验性）；
  - 2021：WebAssembly 支持；
  - 2023：Vector Index 实验性落地（`vendor/vector_search/`）；
  - 2024：Couchbase Lite 3.2（同步改进）；
- **维护状态**：活跃（Couchbase 公司核心产品线之一）；
- **社区**：Couchbase 论坛（forums.couchbase.com）、Discord、GitHub Issues（couchbase-lite-core / couchbase-lite-android / couchbase-lite-ios）。

## 三、许可与商业限制

- **许可证**：**Apache License 2.0**（核心 LiteCore 引擎）；
- **企业版**：部分高级功能（属性加密 `PropertyEncryption`、Vector Search GA）属于 **Enterprise Edition**（商业许可证）；
- **生态关系**：
  - 协议层与 CouchDB 兼容（`_changes` + `_bulk_docs`）；
  - 协议层与 Couchbase Server Sync Gateway（BLIP）兼容；
  - 是 Couchbase Mobile 套件的底层引擎。

## 四、业界主要使用方与案例

- **American Airlines / United Airlines**：飞行员/乘务员机舱应用（离线优先）；
- **Verizon / T-Mobile**：现场工程师工具；
- **Equifax / Synchrony Financial**：金融代理（离线审单）；
- **Marriott**：酒店员工移动端；
- **Toyota / BMW**：车载信息娱乐系统；
- **Hertz**：租车现场应用；
- **社区案例**：零售 POS、医疗 EHR、物流配送、农业 IoT、军用移动端。

## 五、一句话对比定位

> Couchbase LiteCore 是 **"嵌入式 C++ 文档数据库 + CouchDB 兼容同步 + 加密 + LiveQuery"** 的边缘端引擎；与 **PouchDB（JavaScript 嵌入式）**、**Realm（移动端对象数据库）**、**SQLite（嵌入式 SQL）**、**WatermelonDB（移动端同步）** 一起构成 **嵌入式数据层阵营**。LiteCore 的差异化是 **"原生 C++ + 真正的 CouchDB/SG 同步 + 加密 + LiveQuery"**。

## 六、整体架构

```
┌────────────────────────────────────────────────────────────────────────┐
│                      语言绑定（独立仓库）                                  │
│   - couchbase-lite-ios (Swift/Objective-C)                             │
│   - couchbase-lite-android (Java/Kotlin)                               │
│   - couchbase-lite-net (C#/MAUI/Xamarin)                              │
│   - couchbase-lite-core (C API + C++)                                  │
│   - cblite (CLI / Node.js binding)                                     │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │ C4 API（C/c4*.h + C/c4*.cc）
┌─────────────────────▼──────────────────────────────────────────────────┐
│              C4 公开 API 层（C/ + C/Cpp_include/）                       │
│   - C4Database / C4Collection / C4Document / C4Query / C4Replicator    │
│   - C4Observer / C4Listener / C4BlobStore / C4Certificate              │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│            LiteCore/                                                    │
│   - Database/   (DatabaseImpl + LiveQuerier + SequenceTracker)         │
│   - Query/      (N1QL Parser + Translator + 索引)                      │
│   - RevTrees/   (Revision Tree + Version Vector)                       │
│   - Storage/    (DataFile + KeyStore + SQLite 后端)                    │
│   - Support/    (Actor + Timer + EncryptedStream + ObserverList)        │
│   - Logging/    (日志)                                                  │
│   - BlobStore/  (附件流式读写)                                          │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│           Replicator/ + Networking/                                    │
│   - Replicator Actor（Pull/Push 编排）                                  │
│   - Pusher / Puller / RevFinder / Inserter                            │
│   - BLIP (Networking/BLIP/) — 同步协议帧层                              │
│   - WebSockets (Networking/WebSockets/) — 传输层                       │
└─────────────────────┬──────────────────────────────────────────────────┘
                      │
┌─────────────────────▼──────────────────────────────────────────────────┐
│            SQLite + Fleece（vendor 子模块）                              │
│   - SQLiteCpp：C++ SQLite 包装（存储）                                   │
│   - fleece：二进制 JSON 编码（替代 JSON）                                │
│   - mbedTLS：TLS / 加密 / 哈希                                          │
│   - vector_search：向量索引（CBL-3000+ 实验性）                          │
└───────────────────────────────────────────────────────────────────────┘
```

### 6.1 关键设计：Fleece

LiteCore 不直接使用 BSON 或纯 JSON，而是使用 **Fleece**——种紧凑的二进制 JSON 编码：

- **优势**：
  - 字段名经过 `SharedKeys` 字典共享压缩（去重常见字段名）；
  - 嵌套结构零拷贝解码；
  - 与 SQLite 集成（`SQLiteFleeceFunctions.cc` 注册自定义 SQL 函数）；
- **关系**：与 BSON 类似但更紧凑（典型 30%~50% 体积缩减）；
- **同步**：BLIP 帧体使用 Fleece（而非 BSON 或 JSON）。

### 6.2 关键设计：Actor

LiteCore 自带 **`actor` 框架**（`LiteCore/Support/Actor.hh`），核心模块（Replicator、Query、Database）都是 Actor：

- **单线程串行处理**：避免数据竞争；
- **阻塞 IO**：通过 actor 调度，UI 主线程不会卡；
- **消息传递**：actor::Actor 的 mailbox 模式。

## 七、文档模型

### 7.1 BSON-like 但不等同

- **存储格式**：Fleece（二进制 JSON），单文档默认 256KB~1MB；
- **类型**：null/bool/int/double/string/array/dict/Date/Blob；
- **字段名**：UTF-8 字符串；
- **Schema-less**：不强制校验，类型校验依赖应用；
- **`_id` 与 `_rev`**：
  - `_id`：字符串，唯一标识；
  - `_rev`：形如 `"<integer>-<digest>"`（与 CouchDB 兼容），MVCC 核心；
- **C4Document 与 C4Collection**：
  - C4Database 内含多个 C4Collection（Couchbase Lite 3.0+）；
  - 每个 Collection 有自己的索引和 query；

### 7.2 文档版本与 Revision Tree

- `LiteCore/RevTrees/RevTree.cc` — Revision Tree（每个文档的完整版本树）；
- `LiteCore/RevTrees/RevID.cc` — CouchDB 兼容 RevID；
- **冲突解决**：保留所有分支（active + merged），由应用或 replicator 决定 winner。

### 7.3 附件（Attachment / Blob）

- `LiteCore/BlobStore/` — 附件流式读写；
- 附件以独立文件形式存储（不内嵌于文档）；
- 同步时附件单独走 `BLOB` 协议；
- C4 API：`C4BlobStore` / `C/include/c4BlobStore.h`。

## 八、聚合管道（N1QL Lite）

Couchbase Lite 使用 **N1QL Lite** 查询语言——Couchbase Server N1QL 的子集。

### 8.1 工作流

```
N1QL 字符串 (JSON-like)
        ↓  N1QL_Parser (LiteCore/Query/N1QL_Parser/n1ql.leg)
N1QL JSON AST
        ↓  Translator (LiteCore/Query/Translator/)
SQLite SQL（含 fleece_* 函数）
        ↓  SQLite 执行引擎
结果集
```

### 8.2 支持的子集

| 算子 | 描述 |
| --- | --- |
| `SELECT ... FROM ...` | 基本投影 |
| `WHERE` | 过滤 |
| `JOIN` | 跨集合 |
| `GROUP BY` | 分组 |
| `ORDER BY` | 排序 |
| `LIMIT` / `OFFSET` | 分页 |
| 集合操作符：`[]`/`->`/`->>`/`<->`（Fleece 路径） | JSONB-like 操作 |
| 数组函数：`ARRAY_LENGTH`、`ARRAY_CONTAINS` | 数组操作 |
| 全文函数：`MATCH()` | 全文索引查询 |
| 向量函数：`VECTOR_DISTANCE()` | 向量近邻（实验性） |
| Predictive Query：`PREDICT()` | 模型预测结果查询 |

### 8.3 不支持的子集

- 复杂聚合管道（如 Couchbase Server 的 `$group/$facet/$bucket`）；
- 跨库 JOIN；
- 复杂子查询。

### 8.4 源码路径

- `C:/code/book/reference/document/couchbase/LiteCore/Query/Query.cc` — 查询抽象；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/IndexSpec.cc` — 索引规约；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/N1QL_Parser/n1ql.leg` — PEG 语法；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/Translator/QueryTranslator.cc` — AST → SQL；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/SQLiteN1QLFunctions.cc` — 注册 N1QL 函数；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/SQLiteKeyStore+Indexes.cc` — 普通 value 索引；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/SQLiteKeyStore+ArrayIndexes.cc` — 数组索引；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/SQLiteKeyStore+FTSIndexes.cc` — 全文索引；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/SQLiteKeyStore+VectorIndex.cc` — 向量索引；
- `C:/code/book/reference/document/couchbase/C/c4Query.cc` — C API 桥接；
- `C:/code/book/reference/document/couchbase/C/c4Index.cc` — 索引管理 API；
- `C:/code/book/reference/document/couchbase/docs/overview/QueryTranslator.md` — 设计文档。

## 九、查询语言

### 9.1 C4 Query API（C API）

```c
C4Query *q = c4query_new(db, kC4JSONQuery,
    "SELECT _id, name FROM _ WHERE type = 'user' AND age >= $minAge",
    NULL, &err);

C4QueryEnumerator *e = c4query_run(q, &bindParams, &err);
while (c4queryenum_next(e, NULL, NULL)) {
    FLValueSlice doc = c4queryenum_keyAtIndex(e, 0);
    // ...
}
c4queryenum_free(e);
c4query_release(q);
```

### 9.2 LiveQuery（变更响应查询）

`LiveQuerier` 是 LiteCore 的灵魂特性：

- `LiveQuerier.cc` — 实现核心；
- 触发机制：订阅 `BackgroundDB::TransactionObserver` + `SequenceTracker`；
- **节流**：常量 `kRapidChanges = 250ms`、`kShortDelay = 0ms`、`kLongDelay = 500ms`；
- 应用语义：当底层数据变更后，查询结果**自动重新执行**，订阅者收到新的 `QueryEnumerator`；
- 跨平台：iOS/Android/MAUI 都暴露 `Query.addChangeListener`。

### 9.3 源码路径

- `C:/code/book/reference/document/couchbase/LiteCore/Database/LiveQuerier.cc` — LiveQuery 核心；
- `C:/code/book/reference/document/couchbase/LiteCore/Database/SequenceTracker.cc` — 序列号通知；
- `C:/code/book/reference/document/couchbase/LiteCore/Database/BackgroundDB.cc` — 后台事务视图；
- `C:/code/book/reference/document/couchbase/LiteCore/Support/ObserverList.cc` — 多观察者注册；
- `C:/code/book/reference/document/couchbase/C/c4Query.cc: c4query_live2` / `c4query_liveEnumeratorUpdate` — C API 桥接。

## 十、索引机制

### 10.1 支持的索引类型

| 索引 | 用途 | 源码位置 |
| --- | --- | --- |
| **Value Index** | 字段值索引 | `SQLiteKeyStore+Indexes.cc` |
| **Array Index** | 数组元素索引 | `SQLiteKeyStore+ArrayIndexes.cc` |
| **FTS Index** | 全文索引 | `SQLiteKeyStore+FTSIndexes.cc` |
| **Predictive Index** | 模型预测索引 | `SQLiteKeyStore+PredictiveIndexes.cc` |
| **Vector Index** | 向量近邻索引 | `SQLiteKeyStore+VectorIndex.cc` + `vendor/vector_search/` |
| **Special**：`_all_docs` / `_design_docs` | 系统索引 | `mango_idx_special.erl`（参考 CouchDB） |

### 10.2 索引配置

```c
C4IndexOptions opts = {
    .indexType = kC4ValueIndex,
    .expr = "name",  // 表达式（JSON 路径或函数）
    .language = "",  // FTS 使用
    .isUpsert = true,
};
c4db_createIndex2(db, "byName", "name", kC4ValueIndex, "", &err);
```

### 10.3 索引限制

- 单集合默认 64 个索引；
- 表达式索引：`UNNEST`、`ARRAY`、`DISTINCT`；
- 部分索引：`WHERE` 子句；
- 限制与成本：详见 `docs/overview/QueryTranslator.md`。

### 10.4 源码路径

- `C:/code/book/reference/document/couchbase/C/c4Index.cc` — 索引 C API；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/LazyIndex.cc` — 惰性索引；
- `C:/code/book/reference/document/couchbase/LiteCore/Query/IndexSpec.cc` — 索引规约模型。

## 十一、变更流（LiveQuery + Replicator）

### 11.1 本地变更通知（C4Observer）

```c
C4DatabaseObserver *obs = c4dbobs_new(db, func(_),
                                       callback, NULL, &err);
```

- 内部实现：`ObserverList.cc` + `SequenceTracker.cc`；
- 通知粒度：文档级别（document token）；
- 应用层轮询 `c4dbobs_getChanges(obs, ...)` 拿到变更集。

### 11.2 LiveQuery（推荐）

详见 9.2 节。LiveQuery = C4Observer + Query 重新执行。

### 11.3 端云同步（C4Replicator）

- `C4Replicator` 是 LiteCore 的端云同步抽象；
- 支持模式：
  - **Push**：本端 → 远端；
  - **Pull**：远端 → 本端；
  - **Push + Pull**：双向；
- **通道（Channels）**：基于过滤函数 + doc_id 集合；
- **附件同步**：附件走独立 BLOB 协议；
- **断线重连**：自动重试 + checkpoint（`LocalSequence`/`RemoteSequence`）；
- **源码路径**：
  - `C:/code/book/reference/document/couchbase/Replicator/Replicator.cc` — 顶层 Actor；
  - `C:/code/book/reference/document/couchbase/Replicator/Pusher.cc` — Push 协议状态机；
  - `C:/code/book/reference/document/couchbase/Replicator/Puller.cc` — Pull 主循环；
  - `C:/code/book/reference/document/couchbase/Replicator/RevFinder.cc` — 拉取变更；
  - `C:/code/book/reference/document/couchbase/Replicator/Inserter.cc` — 写入本地；
  - `C:/code/book/reference/document/couchbase/Replicator/IncomingRev.cc` — 接收 rev；
  - `C:/code/book/reference/document/couchbase/Replicator/Checkpoint.cc` — 复制断点；
  - `C:/code/book/reference/document/couchbase/Replicator/c4Replicator.cc` — C API 桥接；
  - `C:/code/book/reference/document/couchbase/Networking/BLIP/BLIPConnection.cc` — BLIP 同步协议；
  - `C:/code/book/reference/document/couchbase/Networking/WebSockets/WebSocketImpl.cc` — WebSocket 传输层。

### 11.4 监听器（C4Listener）

`C4Listener` 是 LiteCore 的嵌入式 HTTP 端点：

- 让其他 Couchbase Lite 实例或 CouchDB 通过 BLIP 同步到本端；
- 嵌入式使用（如 Android Wear、平板到手机）；
- **源码路径**：
  - `C:/code/book/reference/document/couchbase/C/c4Listener.cc` — C API；
  - `C:/code/book/reference/document/couchbase/REST/` — HTTP 服务端。

## 十二、事务与一致性（裁剪）

### 12.1 单文档 ACID

LiteCore 单文档写入是原子的（基于 SQLite + MVCC + `_rev` 乐观锁）。

### 12.2 多文档事务

LiteCore **不提供多文档 ACID 事务**——与多数文档数据库一致：

- 应用层通过 `_bulk_docs` + 冲突解决实现 saga；
- 同步基于"最后写入获胜 + 冲突保留"；
- 实际语义：MVCC + CouchDB-style conflict resolution。

### 12.3 隔离级别

- 通过 SQLite 的 MVCC 实现快照隔离；
- 默认 `Serializable`（SQLite 默认）；
- 在 `c4db_beginTransaction` / `c4db_endTransaction` 之间串行化。

### 12.4 冲突解决

- 同步期间，每个文档可能有多个分支（active + merged）；
- 应用可通过 `_revs_info` / `_conflicts` 查询；
- 默认策略：保留所有版本 + 选 `_revisions` 最深的为 winner。

## 十三、复制与高可用

### 13.1 多设备复制拓扑

```
       Cloud (Couchbase Server / Sync Gateway)
                    ↕ BLIP / WebSocket
       ┌────────────┴────────────┐
   Phone A                   Tablet B
   (Couchbase Lite)          (Couchbase Lite)
                    ↕ P2P (C4Listener + 短链)
              Laptop C
              (Couchbase Lite)
```

### 13.2 冲突解决策略

- **服务端权威**：通过 Sync Gateway / Couchbase Server 配置自定义合并函数；
- **客户端权威**：应用层读 `_conflicts`，手动解决；
- **CRDT 模式**：3.x 引入的部分实验性支持。

### 13.3 增量同步优化

- **Checkpoint**：每个 channel 维护 `LocalSequence` / `RemoteSequence`；
- **Attachment 优化**：附件独立 `BLOB` 通道（不在普通同步通道阻塞）；
- **压缩**：BLIP 帧体用 Fleece 编码（比 JSON 紧凑 30~50%）；
- **WebSocket 多路复用**：单 TCP 连接多 channel。

### 13.4 监控

- `c4Replicator_getStatus()` 获取当前状态；
- `progress`（已完成变更数 / 总数）；
- `error`（最近错误）；
- 订阅 `C4ReplicatorListener.statusChanged`。

## 十四、扩展性

### 14.1 嵌入式限制

- **单进程**：LiteCore 假设单进程访问（SQLite 默认）；
- **多进程**：通过 `C4Listener` 暴露 BLIP 端点（其他进程通过 BLIP 同步）；
- **线程安全**：跨线程访问 C4 API 必须用 `c4db_lock()` / `c4db_unlock()`。

### 14.2 资源约束

- 单数据库建议 < 5GB（移动端）；
- 索引全部加载到内存（典型 < 100MB）；
- 内存压力：LiveQuery 缓存所有结果集。

### 14.3 跨平台

- iOS / Android / macOS / Linux / Windows / tvOS / watchOS；
- WebAssembly（实验性）；
- 嵌入式 Linux / Yocto（设备端）；
- ARM / x86_64 / MIPS（路由器）。

## 十五、性能与基准

- **典型性能**（iPhone 13）：
  - `INSERT` 单文档：~5ms；
  - `SELECT BY _id`：< 1ms（索引命中）；
  - `SELECT BY age WHERE age > 20`：~10ms（带索引）；
  - 全文 MATCH 查询：~50ms（典型）；
  - 向量 ANN 查询（10k 向量，128 维）：~20ms；
  - 同步吞吐（局域网）：~1000 docs/sec；
- **官方基准**：在 `LiteCore/tests/` 下的 `C4Test.cc` + `PerformanceTest.cc`；
- **第三方比较**：
  - vs PouchDB：LiteCore 快 5~10x（原生 vs JavaScript）；
  - vs SQLite：相当（LiteCore 底层就是 SQLite）；
  - vs Realm：相当（Realm 是对象数据库，LiteCore 是文档数据库）；
- **调优点**：
  - `c4query_setParameters` 复用查询计划；
  - LiveQuery 节流（`kRapidChanges = 250ms`）；
  - 批量插入用 `_bulk_docs`。

## 十六、运维与可观测性

- **配置**：通过 `C4DatabaseConfig2`（`C4EncryptionKey`、`C4DatabaseFlags`）；
- **日志**：slog 风格，5 级（Debug/Verbose/Info/Warning/Error），`LiteCore/Logging/`；
- **指标**：
  - 通过 `c4Replicator_getStatus2` 获取同步状态；
  - 应用层包装（Prometheus / OpenTelemetry）；
- **错误码**：`C4Error`（domain + code）；
- **加密**：
  - 存储加密：`C4EncryptionKey` + SQLCipher（社区版 / Enterprise Edition 加密）；
  - 通道加密：TLS via mbedTLS；
- **备份**：
  - 文件级：`cp -R db.cblite2/`；
  - 在线：通过复制到中心节点；
- **升级**：数据库 schema 迁移由 LiteCore 自动处理（`Schema.cc` + `DataFile::upgrade`）。

## 十七、生态与集成

- **官方 SDK**（独立仓库）：
  - **iOS/macOS**：couchbase-lite-ios（Swift/Objective-C）；
  - **Android**：couchbase-lite-android（Java/Kotlin）；
  - **.NET**：couchbase-lite-net（C#/MAUI/Xamarin/UWP）；
  - **C**：couchbase-lite-core（C API）；
  - **C++**：直接使用 LiteCore（`#include "c4.hh"`）；
  - **JavaScript**：cblite-js（Node.js binding）；
  - **Python**：cblite-python（社区）；
- **同步生态**：
  - **Couchbase Sync Gateway**：服务端代理（处理安全、过滤、路由）；
  - **Couchbase Server**：服务端存储；
  - **CouchDB**：兼容协议（CouchDB 1.x/2.x/3.x）；
- **BI / 同步到 SQL**：
  - **Kafka Connect**；
  - **Couchbase Analytics**；
- **容器化**：通过 REST API（`/cblite/`）调用 CLI；
- **Helm Chart**：Couchbase Operator 提供 Mobile 部署；
- **云服务**：
  - **Couchbase Capella**（托管 Mobile + Sync）；
  - **AWS / Azure**：Marketplace 镜像。

## 十八、核心论文与算法原理

- **MVCC**：SQLite WAL + 版本链；
- **Fleece 编码**：类似 FlatBuffers / Cap'n Proto 的零拷贝模式；
- **Version Vector**（LiteCore 3.x+）：替代 `_rev`，支持 CRDT；
- **BLIP 协议**：[github.com/couchbaselabs/BLIP-Cpp](https://github.com/couchbaselabs/BLIP-Cpp)；
- **Replication 协议**：基于 CouchDB 的 `_changes` + `_bulk_docs`；
- **Predictive Query**：参考 [Couchbase 博客](https://www.couchbase.com/blog/)；
- **Vector Search**：HNSW + 自定义量化（详见 `vendor/vector_search/`）。

## 十九、源码学习路径

### 19.1 入门（1-2 天）

1. `C:/code/book/reference/document/couchbase/C/include/c4Database.h` — C4 公开 API；
2. `C:/code/book/reference/document/couchbase/C/c4Database.cc` — Database 实现；
3. `C:/code/book/reference/document/couchbase/C/c4Document.cc` — 文档读写；
4. `C:/code/book/reference/document/couchbase/C/c4Query.cc` — Query API；
5. `C:/code/book/reference/document/couchbase/C/c4Replicator.cc` — Replicator API；
6. `C:/code/book/reference/document/couchbase/LiteCore/Database/DatabaseImpl.cc` — Database 实现核心。

### 19.2 进阶（3-5 天）

1. `LiteCore/Database/LiveQuerier.cc` — LiveQuery 核心；
2. `LiteCore/RevTrees/RevTree.cc` + `RevID.cc` — 版本树；
3. `LiteCore/Storage/SQLiteDataFile.cc` + `SQLiteKeyStore.cc` — 存储后端；
4. `LiteCore/Query/Translator/QueryTranslator.cc` — N1QL → SQL；
5. `LiteCore/Query/SQLiteFleeceFunctions.cc` — Fleece SQL 函数；
6. `Replicator/Replicator.cc` + `Pusher.cc` + `Puller.cc` — 同步主流程；
7. `Networking/BLIP/BLIPConnection.cc` + `Networking/WebSockets/WebSocketImpl.cc` — 协议栈。

### 19.3 高级（1 周+）

1. `LiteCore/Support/Actor.hh` — Actor 框架；
2. `LiteCore/Support/EncryptedStream.cc` — 加密流；
3. `Replicator/c4Replicator+Pool.hh` + `IncomingRev.cc` — 完整复制路径；
4. `Crypto/SecureSymmetricCrypto.cc` — 加密原语；
5. `vendor/vector_search/` — 向量索引；
6. 自己 build LiteCore + 运行 `cblite` CLI，调试一次同步过程。

### 19.4 源码规模

- **C++ 文件**：~219 `.cc` + ~198 `.hh`（约 417 个 C++ 文件）；
- **C 文件**：~8 个（SQLite chooser、c4.c、少量 MSVC 胶水）；
- **公共 C 头**：~73 个（`C/include/` + `c4Private.h` 等）；
- **C++ 占比**：约 **84%**（现代 C++20 为主）；
- **构建系统**：CMake + Makefile + Taskfile；
- **测试**：每个模块独立 `tests/` 目录（Catch2 + GoogleTest）。

## 二十、学习资源

- **官方文档**：[docs.couchbase.com](https://docs.couchbase.com/couchbase-lite/current/index.html)；
- **API 参考**：[docs.couchbase.com/mobile](https://docs.couchbase.com/couchbase-lite/current/introduction.html)；
- **官方博客**：[couchbase.com/blog](https://www.couchbase.com/blog/)；
- **GitHub Discussions**：[github.com/couchbase/couchbase-lite-core/discussions](https://github.com/couchbase/couchbase-lite-core/discussions)；
- **Sample Apps**：[github.com/couchbase/codefest-maui](https://github.com/couchbaselabs/quickstart-couchbase-mobile)；
- **书籍**：无系统书籍，依赖官方文档 + 博客；
- **视频**：[Couchbase Connect 大会](https://www.couchbase.com/connect/)；
- **Reddit**：[r/couchbase](https://www.reddit.com/r/couchbase/)。

## 二十一、可移植性与部署

- **支持的平台**：
  - iOS 13+、tvOS 13+、watchOS 6+、macOS 10.15+；
  - Android 7+（API 24+）；
  - .NET Standard 2.0+（iOS/Android/macOS/Windows）；
  - Linux x86_64 / ARM64 / MIPS / RISC-V；
  - Windows 10+；
  - WebAssembly（实验性）；
- **嵌入式部署**：典型用于移动 App / IoT / 边缘设备；
- **C4Listener 嵌入式 HTTP**：可在其他进程暴露 BLIP 端点（用于 P2P）；
- **打包**：
  - iOS：XCFramework（couchbase-lite-ios）；
  - Android：AAR / Maven Central；
  - .NET：NuGet；
  - C++：静态库 + 头文件；
- **最小硬件**：
  - 移动端：50MB 内存（典型）；
  - 嵌入式 Linux：100MB 内存（建议 1GB）；
  - 数据库大小：典型 1MB~1GB。

## 二十二、变体与衍生项目

- **Couchbase Lite for iOS**（couchbase-lite-ios）；
- **Couchbase Lite for Android**（couchbase-lite-android）；
- **Couchbase Lite for .NET**（couchbase-lite-net）；
- **Couchbase Lite Java**（社区开源，PDF 重构）；
- **cblite CLI**（Node.js + LiteCore 绑定，用于调试）；
- **Couchbase Sync Gateway**（服务端 BLIP 代理，独立项目）；
- **PouchDB**（JavaScript 等价物，协议兼容）；
- **TouchDB**（历史版本，1.x）；
- **Predictive Query**：基于 CoreML（iOS）/ TensorFlow Lite（Android）的客户端 ML 集成。

## 二十三、风险与陷阱

### 23.1 协议兼容性陷阱

- **N1QL Lite 子集**：与 Couchbase Server N1QL **不完全兼容**，部分 Server-only 算子在 LiteCore 中**不可用**（如 `$facet`、`$bucket`）；
- **同步协议版本**：CouchDB 1.x/2.x/3.x 兼容性，Couchbase Server 必须用 Sync Gateway；
- **附件格式**：Couchbase Lite 3.x 之前是 inline attachments（与 CouchDB 兼容），之后改独立 BLOB；
- **版本冲突**：`_conflicts` 处理不当会导致数据丢失；
- **WebSocket 兼容性**：企业代理（防火墙）可能中断 WebSocket；
- **Vector Search GA**：3.x 仍实验性，API 不稳定；
- **Predictive Model 版本**：模型签名变更会导致预测索引重建。

### 23.2 性能陷阱

- **LiveQuery 频繁触发**：底层变更频繁时（>250ms 节流窗口），订阅者会被打爆；
- **大文档**：单文档 > 100KB 影响同步性能；
- **过多索引**：每个索引都要更新，索引更新是同步阻塞；
- **全文索引**：中文分词默认不支持；
- **向量索引**：内存占用与召回率平衡需调优；
- **同步 checkpoint 失败**：网络抖动导致 checkpoint 重传，全量重同步；
- **附件大文件**：附件走独立 BLOB 通道，不阻塞普通同步，但大附件吃带宽；
- **SQLite 锁竞争**：跨线程访问必须加锁，否则 `SQLITE_BUSY`。

### 23.3 运维陷阱

- **数据库文件丢失**：SQLite WAL 文件丢失会导致数据损坏；
- **加密密钥丢失**：`C4EncryptionKey` 丢失则数据库永久不可读（**必须备份**）；
- **多设备冲突**：用户预期"以最后点击为准"，但 CouchDB 是多版本，应用必须显式处理；
- **Schema 迁移**：LiteCore 自动迁移（`Schema.cc`），但跨大版本（如 1.x → 3.x）需手动迁移；
- **同步连接池**：默认实现单连接，多 client 并发需自己维护连接池；
- **日志级别**：默认 Info，生产环境调试时改为 Debug 可能影响性能；
- **升级 LiteCore 版本**：可能引入 schema 不兼容，需测试；
- **WebAssembly 限制**：单线程 + 内存受限 + 部分加密 API 不可用；
- **跨平台编译**：macOS / Linux / Windows 的 build flags 不同，需注意。

## 二十四、学习路径

### 24.1 入门（1 周）

1. 跑官方 [Quickstart](https://docs.couchbase.com/couchbase-lite/current/c/quickstart.html)；
2. 用 cblite CLI 在本地创建数据库，执行 CRUD；
3. 启动本地 Sync Gateway + Couchbase Server（Docker），建立端云链路；
4. 阅读 `C/include/c4Database.h` + `C/c4Database.cc`；
5. 编译 LiteCore：`cmake -B build && cmake --build build`；
6. 写一个 C 测试程序，调用 C4 API。

### 24.2 中级（2-3 周）

1. 部署 Android / iOS 应用，集成 Couchbase Lite SDK；
2. 配置 Sync Gateway 复制过滤器 + 通道；
3. 实现自定义冲突解决（`_revs_info` + 应用层 merge）；
4. 阅读 `LiteCore/Database/LiveQuerier.cc`；
5. 阅读 `LiteCore/Query/Translator/QueryTranslator.cc`（理解 N1QL → SQL 翻译）；
6. 跟踪一次 Push 同步：本地写入 → Replicator → BLIP → Sync Gateway → Couchbase Server。

### 24.3 进阶（1 个月+）

1. 编译 LiteCore + 集成自定义扩展（如自定义 SQL 函数）；
2. 实现自定义通道（`_channels` 文档字段 + sync 函数）；
3. 部署 Couchbase Lite + Sync Gateway + Couchbase Server 三层架构；
4. 性能基准 vs PouchDB / Realm / SQLite；
5. 阅读 `vendor/vector_search/`（向量索引实现）；
6. 在嵌入式 Linux（Yocto）部署 LiteCore；
7. 贡献 LiteCore 新功能（如新索引类型、新查询算子）。

## 二十五、与本项目的关联

`engineering/` 中**没有移动端/嵌入式文档数据库**实现，但有以下可借鉴模式：

- `engineering/src/db/buf.h` 的 buffer pool 设计可参考 `LiteCore/Storage/SQLiteDataFile.cc` 的"块缓存 + WAL"思路；
- `engineering/include/db/btreeam.h` 的 BTree AM 可参考 `LiteCore/Query/SQLiteKeyStore+Indexes.cc` 的索引实现；
- `engineering/src/db/sql_engine.c` 的 SQL 执行器可参考 `LiteCore/Query/Translator/QueryTranslator.cc` 的"N1QL → SQL"翻译模式（DSL → SQL）；
- `engineering/src/db/wal.c` 的 WAL 可参考 SQLite WAL 实现（LiteCore 直接使用 SQLite）；
- **Actor 模式**（`LiteCore/Support/Actor.hh`）值得借鉴到 `engineering/src/db/` 中需要并发安全的模块；
- **Fleece 零拷贝 JSON** 思路可应用到 `engineering/src/algo/` 的字符串处理。

## 二十六、RevTree 版本树与赢家选举算法

§7.2 只列出了 `RevTree.cc` / `RevID.cc` 两个路径，本节补上 CouchDB 2.x 传统 revision-tree 版本体系的**实际算法**：revid 的双形态编码、赢家排序比较函数、共同祖先查找与历史插入、以及深度剪枝。

### 26.1 revid 的双形态二进制编码

`revid` 是一个 `slice` 包装（非拥有内存），有**两种互斥形态**，靠首字节区分（`LiteCore/RevTrees/RevID.hh:20-45`）：

| 形态 | ASCII 样例 | 二进制布局 | 用途 |
| --- | --- | --- | --- |
| **digest**（revision-tree） | `123-cafebabedeadbeefdeadfade` | `varint(generation)` ‖ 原始 digest 字节 | CouchDB 2.x 及以下 |
| **version**（version-vector） | `1772c7cb27da0000@ZegpoldZegpoldZegpoldA` | `0x00` ‖ `varint(压缩时间戳)` ‖ `SourceID` | Couchbase Lite 3.x |

- 判别式：`revid::isVersion()` 即 `size > 0 && (*this)[0] == 0`（`RevID.hh:65`）；
- digest 形态构造把 ASCII 的 hex 对**两两折叠成字节**再前缀 varint 代数（`RevID.cc:152-175`），`_buffer` 定长 42 字节、溢出抛 `BadRevisionID`（`RevID.hh:130`）；
- 二进制序即字典序的排序基础：

```cpp
// LiteCore/RevTrees/RevID.cc:62-70（已验证）
bool revid::operator<(const revid& other) const {
    if (isVersion()) {
        return Version::byAscendingTimes(asVersion(), other.asVersion());  // 混合逻辑时间
    } else {
        auto [myGen, myDigest]       = generationAndDigest();
        auto [otherGen, otherDigest] = other.generationAndDigest();
        return (myGen != otherGen) ? myGen < otherGen : myDigest < otherDigest;
    }
}
```

`generationAndDigest()` 会校验 `0 < gen ≤ UINT_MAX`，否则抛 `CorruptRevisionData`（`RevID.cc:32-39`）——**代数即版本树深度**，代数乱序意味着树被破坏。

### 26.2 赢家选举：compareRevs 五级优先级字典序

CouchDB 的"当前版本"（current rev）不是简单的最大代数，而是按一组标志位做**逐级降序比较**（`LiteCore/RevTrees/RevTree.cc:532-549`）：

```cpp
// 返回 true ⇔ rev1 优先级高于 rev2（配合 std::ranges::sort 即降序）
__hot static bool compareRevs(const Rev* rev1, const Rev* rev2) {
    int delta = rev2->isLeaf() - rev1->isLeaf();          // ① 叶优先
    if (delta) return delta < 0;
    delta = rev1->isConflict() - rev2->isConflict();      // ② 冲突分支永不当 current
    if (delta) return delta < 0;
    delta = rev1->isDeleted() - rev2->isDeleted();        // ③ 活跃优先于墓碑
    if (delta) return delta < 0;
    delta = rev1->isClosed() - rev2->isClosed();        // ④ 已关闭分支垫底
    if (delta) return delta < 0;
    return rev2->revID < rev1->revID;                     // ⑤ 否则按 revid 降序
}
```

伪代码形式（便于对照）：

```text
compareRevs(r1, r2):
    if isLeaf(r1)     ≠ isLeaf(r2):      return isLeaf(r1)
    if isConflict(r1) ≠ isConflict(r2):  return ¬isConflict(r1)
    if isDeleted(r1)  ≠ isDeleted(r2):   return ¬isDeleted(r1)
    if isClosed(r1)   ≠ isClosed(r2):    return ¬isClosed(r1)
    return revid(r2) < revid(r1)
```

配套的"活跃"判定（`RevTree.cc:165-175`）：

```cpp
bool Rev::isActive() const {
    // 传统（CouchDB 以来）活跃 = 叶 且 非墓碑；墓碑用于封顶冲突分支，本身不制造冲突。
    // 但 CBL 2 的 no-conflicts 语义下允许「活文档 vs 已删文档」构成冲突，
    // 因此当墓碑正是服务端 current rev 时也算活跃。
    return isLeaf() && (!isDeleted() || isLatestRemoteRevision());
}
```

排序后 `_revs[0]` 即 current，`hasConflict()` 退化为 O(1) 的 `_revs[1]->isActive()`（`RevTree.cc:148-163`，未排序时才 O(n) 全扫）。**复杂度**：排序 O(n log n)，比较函数摊还 O(1)（`revid::operator<` 对 version 形态是常数时间的 64 位比较，对 digest 形态是 O(digest 长度)）。

### 26.3 insertHistory：共同祖先查找与历史批量插入

同步推送时对端送来的 `revisions` 数组形如 `[新 rev, 父 rev, 祖 rev, ...]`，`RevTree::insertHistory` 需要先在本地树里定位**最长公共前缀**，再把缺失的一段补进去（`RevTree.cc:366-374`）：

```text
findCommonAncestor(history, allowConflict):
    lastGen, parent ← 0, nil
    for i ← 0 … len(history)-1:
        gen ← generation(history[i])
        if lastGen > 0 and gen ≠ lastGen - 1:
            # 代数不连续
            if gen < lastGen and i ≥ _pruneDepth - 1:
                pass                    # 例外：即将被剪掉的深度，允许挖洞以压缩长历史
            else:
                return (nil, -400)      # HTTP 400，history 非法
        lastGen ← gen
        parent ← get(history[i])
        if parent ≠ nil: break
    if ¬allowConflict:
        if (parent ≠ nil and ¬isLeaf(parent)) or (parent = nil and _revs ≠ ∅):
            return (nil, -409)          # HTTP 409，插入将产生冲突
    return (parent, i)
```

```text
insertHistory(history, body, flags, allowConflict, markConflict):
    (parent, idx) ← findCommonAncestor(history, allowConflict)
    if idx > 0 and body ≠ nil:
        for i ← idx-1 down to 1:                 # 中间缺失段，无 body，逆序补链
            parent ← _insert(history[i], ∅, parent, kNoFlags, markConflict)
        _insert(history[0], body, parent, flags, markConflict)
```

- **单调不变量**：`newGen == parentGen + 1`，否则 HTTP 400（`RevTree.cc:342-346`）——这是防止伪造 revid 造出"跳跃代数"的关键防线；
- **幂等**：revid 已存在直接返回 200 不做事（`RevTree.cc:321-324`）；
- **复杂度**：单次 `findCommonAncestor` O(H)（H = history 长度，逐项 hash/线性查表）；`get(revid)` 在已排序树上可二分，未排序时退化为 O(n)（`RevTree.cc:132-138`）。

### 26.4 prune 与 compact：深度剪枝

版本树是无界的（每次写入 +1 代），LiteCore 用 `_pruneDepth` 限制树深（`RevTree.hh:225`，默认 `UINT_MAX`）（`RevTree.cc:436-478`）：

```text
prune(maxDepth):
    for each rev in _revs:                       # _revs 已按优先级降序，叶在前
        if isLeaf(rev):
            depth ← 0
            for anc ← rev; anc ≠ nil; anc ← anc.parent:
                if ++depth > maxDepth and ¬keepBody(anc):
                    anc.kPurge ← true; numPruned++        # 打标记
        else if _sorted: break                   # 已排序则后续全是非叶，提前收工
    for (rid, r) in _remoteRevs:                 # 例外：远端 current rev 永不剪
        if r.kPurge: clearFlag(r, kPurge); numPruned--
    for rev in _revs:                            # 修补父指针，跳过被剪节点
        while rev.parent and rev.parent.kPurge: rev.parent ← rev.parent.parent
    compact()
```

`compact()` 用一次**原地滑动压缩**回收被标 `kPurge` 的 `Rev` 槽位（`RevTree.cc:504-528`），并同步清理 `_remoteRevs` / `_rejectedRevs` 两个反向索引——注意它要**拷贝 `_remoteRevs`** 再遍历，因为擦除会失效迭代器。

**复杂度**：`prune` 是 O(L·D)（L = 叶数，D = maxDepth），只在触发剪枝时执行；`compact` 是 O(n)。

**工程含义**：`_pruneDepth` 直接决定了"同步断点续传能回溯多远"——深度被剪掉的历史无法再用于增量追平，只能全量 dump，这也是 §23.2 中"checkpoint 失败导致全量重同步"的根因之一。

## 二十七、Version Vector 向量时钟与冲突合并算法（CouchDB 3.x 路径）

§18 只用一行提过 Version Vector。本节展开 Couchbase Lite 3.x 的向量时钟体系：**混合逻辑时钟（Hybrid Logical Clock）**生成单调逻辑时间戳，**版本向量（VersionVector）**记录各 peer 的最新时间以判定 happens-before 与冲突，`merge()` 把两个冲突向量合成为一个带 merge 标记的向量。

### 27.1 Version 结构与 HLC 逻辑时间

```cpp
// LiteCore/RevTrees/Version.hh:131-132
SourceID    _author;   // 创建该版本的 peer（本地用 kMeSourceID = 0 缩写）
logicalTime _time;     // 逻辑时间戳
```

逻辑时间戳是 64 位，**高 48 位物理时间（ns）+ 低 16 位同刻计数器**（`LiteCore/Support/HybridClock.cc:40-53`）。`now()` / `seen()` 严格实现 HLC 论文 Figure 5 的两段规则（`HybridClock.cc:77-124`）：

```text
now():                                   # 取新逻辑时间
    c ← source.now()                     # 真实墙钟
    if c.wall ≤ last.wall: last.counter++         # 物理时间未前进 → 计数器自增
    else:                  now ← (c.wall, 0)      # 物理时间前进 → 计数器归零
    return CAS(last ← now)              # 失败则重试

seen(seenT):                             # 吸收远端时间戳，防时钟回退
    local ← source.now()
    if seenT.wall > local.wall + kMaxClockSkew:  reject   # 远未来 → 丢弃
    now ← max(seenT.wall, last.wall, local)
    # 三分支决定计数器：
    if now.wall = last.wall:
        now.counter ← (now.wall = seenT.wall) ? max(last.counter, seenT.counter) + skipPast
                                               : last.counter + skipPast
    elif now.wall = seenT.wall:
        now.counter ← seenT.counter + skipPast
    return now
```

- `kMaxClockSkew = 2 * 60 * kNsPerSec`（**120 秒**，`LiteCore/Support/HybridClock.hh:79`）：超出本地墙钟 2 分钟的远端时间戳直接拒收，防止恶意 peer 用巨大时间戳毒化向量（`HybridClock.cc:107-112`）；
- `checkTime()` 另一侧兜底：过旧（低于 `_minValid`）或 `> kMaxValidTime` 都拒绝（`HybridClock.cc:126-136`）；
- `update()` 用 `compare_exchange_strong` 做无锁 CAS 循环（`HybridClock.cc:66-75`），保证同一 HLC 上并发 `now()` 不产生重复时间戳。

### 27.2 逻辑时间的压缩编码

大多数 HLC 的低 16 位计数器恒为 0，直接 varint 编码要 10 字节。LiteCore 做了位级压缩（`LiteCore/RevTrees/Version.cc:107-119`）：

```cpp
static uint64_t compress(logicalTime g) {
    auto i = uint64_t(g);
    if (i & 0xFFFF) return (i << 1) | 1;   // 计数器非零：左移 1 位，LSB 置 1 作 tag
    else             return i >> 15;       // 计数器为零：右移 15 位（省下 15 个 0）
}
static logicalTime decompress(uint64_t i) {
    if (i & 1) i >>= 1;                    // tag=1 → 去 tag，右移还原
    else        i <<= 15;                  // tag=0 → 左移补回 15 个 0
    return logicalTime(i);
}
```

**空间收益**：计数器为零时有效位从 64 降到 49，varint 宽度由 `⌈64/7⌉ = 10` 字节降到 `⌈49/7⌉ = 7` 字节，**每个 Version 省 3 字节**。代价是计数器非零时反而要 11 字节——用"绝大多数正常路径"换掉极端路径。写入路径见 `Version::writeBinary`（`Version.cc:160-163`）：`writeUVarInt(compress(_time))` 后接 `SourceID::writeBinary`。

### 27.3 向量比较：四点定序与 happens-before 判定

`versionOrder` 是四值域 `{kOlder, kSame, kNewer, kConflicting}`。原语是"本向量 vs 单个 Version"（`LiteCore/RevTrees/VersionVector.cc:219-226`）：

```text
VersionVector::compareTo(v):          # 我方向量 vs 单个版本 v = ⟨t, author(v)⟩
    mine ← find(v.author())           # 线性查该 author 的条目
    if mine 缺失 or mine.time < v.time:   return kOlder
    if mine.time = v.time and mine 是首项: return kSame     # 首项 = current
    return kNewer                       # mine.time > v.time
```

向量 vs 向量（`VersionVector.cc:237-254`）：

```text
VersionVector::compareTo(other):
    if count() = 0: return (other.count() = 0) ? kSame : kOlder
    if other.count() = 0: return kNewer
    myCmp ← compareTo(other.current())          # 我的 current vs 对方同 author 条目
    if myCmp = kSame: return kSame              # 双方 current 同源同刻 → 同一版本
    theirCmp ← other.compareTo(this.current())
    if myCmp = theirCmp: return kConflicting    # 互为"更新" → 典型并发写冲突
    return myCmp                                # 否则一边 happens-before 另一边
```

**关键点**：`kConflicting` 不由单个字段比较得出，而是"我的 current 比对方新 **且** 对方 current 比我新"——两个方向都为 `kNewer` 时才判定并发。这正是分布式向量时钟的标准判定式，**不需要传递完整因果历史**，只需每个 peer 的最新时间戳，通信量 O(peer 数) 而非 O(操作数)。

### 27.4 merge：把两个冲突向量合成为一个

冲突被检测到后，`VersionVector::merge` 生成一个新的、**带 merge 标记**的向量：前 3 项固定是「我自己刚生成的新版本 + 两个冲突方的 current」，标记为 `_nCurrent = 3`（`VersionVector.cc:420-446`）：

```text
merge(v1, v2, clock):
    v1.current().updateClock(clock, anyone=true)     # 吸收双方时间戳，防回退
    v2.current().updateClock(clock, anyone=true)
    result ← VersionVector({Version(clock.now(), kMeSourceID), v1.current(), v2.current()}, nCurrent=3)
    sort(result[1..], byDescendingTimes)              # 冲突方按时间降序
    c1, c2 ← result[1].author, result[2].author
    if c1 = c2: throw BadRevisionID                    # 同 author 的"冲突"非法
    # 并行双指针遍历两个向量，按 author 对齐：
    compareBySource(v1, v2, |author, t1, t2|:
        if author ∉ {kMeSourceID, c1, c2}:             # 非冲突方 → 取双方较新时间戳
            result.push_back(Version(max(t1, t2), author))
        return true)
    sort(result[3..], byDescendingTimes)               # 历史条目同样按时间降序
    return result
```

`compareBySource`（`VersionVector.cc:392-418`）是**归并式双指针**：两个向量各自按 `SourceID` 升序排列，缺失的一侧时间戳取 `logicalTime::none`（最小值），从而在不建哈希表的前提下完成按 author 的对齐：

```text
compareBySource(v1, v2, cb):
    i ← v1.versionsBySource(); j ← v2.versionsBySource()      # 各 O(n log n) 排序
    do:
        if i.author = j.author:  t1←i.t, t2←j.t; i++; j++
        elif i.author < j.author: t1←i.t, t2←none;    i++     # 只有 v1 有该 author
        else:                      author←j.author; t1←none; j++
        if not cb(author, t1, t2): break
    while (i,j 未耗尽)
```

- **不变式**：`merge` 结果的 `_nCurrent = 3 > 1`，下游据此知道这是"合并产物"而非"单点更新"，`_revs[3..]` 全是历史 peer 条目；
- **兄弟版本检测**：`mergesSameVersions()`（`VersionVector.cc:505`）判断两个向量是否由同一次冲突合并产生——这让**重复同步同一合并结果**成为幂等操作，不会再次触发合并；
- **复杂度**：设两向量规模 n、m，`merge` 为 **O(n log n + m log m)**（两次排序 + 一次线性归并）；`compareTo` 为 **O(n + m)**（两次线性 author 查找）；空间 O(n + m)。

### 27.5 向量记录的持久化

单个文档的当前版本向量与冲突影子分支由 `VectorRecord`（`LiteCore/RevTrees/VectorRecord.hh:92`，实现 `VectorRecord.cc` 790 行）承载：它把 current vector、merge versions、逐 peer 的影子记录一起编码成一条定长可比较的二进制记录，随文档存进 KV，从而让"打开一个文档"这一动作本身就能判定 `kConflict` / `kOlder` / `kNewer` 而无需读取正文（`VectorRecord.hh:201` 注释："conflict 是以 `kConflict` 返回值而非异常表达"）。这与 §26 的 RevTree 是**两套并存的版本体系**，通过 `revid::isVersion()` 单字节 tag 互不干扰地共存于同一份 `_rev` 字段。

## 二十八、增量同步引擎：ChangesFeed、SequenceSet 区间集与 Checkpoint 断点续传

§11.3 与 §13.3 只说"每个 channel 维护 LocalSequence / RemoteSequence"。Couchbase Lite 里真正驱动增量同步的是 **ChangesFeed（历史扫描 + 观察者双通道）+ SequenceSet（稀疏区间集）+ Checkpoint（稀疏断点）** 三件套——这是 LiteCore 版的增量同步引擎。

### 28.1 双通道架构：先挂观察者，再补历史

`ChangesFeed::getMoreChanges` 的关键在于**注册观察者的时机**（`Replicator/ChangesFeed.cc:65-87`）：

```cpp
ChangesFeed::Changes ChangesFeed::getMoreChanges(unsigned limit) {
    if (_continuous && !_changeObserver) {
        // 先启动 observer，再查询历史变更，避免历史与通知之间出现空隙。
        // 但此时还不置 _notifyOnChanges。
        _changeObserver = C4DatabaseObserver::create(coll, [this](C4DatabaseObserver*) { this->_dbChanged(); });
    }
    Changes changes       = {};
    changes.firstSequence = _maxSequence + 1;
    if (_caughtUp && _continuous) getObservedChanges(changes, limit);
    else                         getHistoricalChanges(changes, limit);
    changes.lastSequence = _maxSequence;
    if (_options->isActive() && changes.lastSequence >= changes.firstSequence)
        _checkpointer->addPendingSequences(changes.revs, changes.firstSequence, changes.lastSequence);
    return changes;
}
```

```
          ┌────────────── 连续模式 ──────────────┐
          │  先注册 C4DatabaseObserver           │
          │        ↓                             │
          │  getHistoricalChanges(_maxSequence)  │  按序扫描补历史
          │        ↓ (_caughtUp = true)          │
          └──→ getObservedChanges()  ←── _dbChanged() 回调唤醒
                     ↑ 去重：sequence ≤ startingMaxSequence 的直接丢弃
```

**正确性论证**：若反过来（先扫历史再挂 observer），历史扫描窗口内发生的写入既不在历史结果里、也不会触发 observer 通知，**变更永久丢失**。先挂后扫 + 按 `_maxSequence` 过滤掉重复区间（`ChangesFeed.cc:143`）是标准的"订阅先于消费 + 单调水位线"模式。

### 28.2 历史扫描：by-sequence 枚举器

`getHistoricalChanges` 用 `C4DocEnumerator(collection, _maxSequence, options)` 做顺序扫描（`ChangesFeed.cc:89-117`）：

```cpp
C4DocEnumerator e(collection, _maxSequence, options);   // 从 _maxSequence 起
changes.revs.reserve(limit);
while (e.next() && limit > 0) {
    auto rev = makeRevToSend(e.documentInfo(), &e);
    if (rev) { changes.revs.push_back(rev); --limit; }
}
if (limit > 0 && !_caughtUp) _caughtUp = true;   // 拿不满 = 已追平历史
changes.askAgain = !_caughtUp || _continuous;
```

- 若已配置 `pushFilter`，则**去掉 `kC4IncludeBodies`**（`ChangesFeed.cc:95`）——被过滤掉的文档连正文都不用从磁盘读出，这是"过滤器下推"的省 IO 手段；
- **复杂度**：O(batch)，无索引开销，按序扫描；filter 命中后可在更早阶段短路。

### 28.3 观察者通道：乱序去重与批量上限

`getObservedChanges` 从 `C4DatabaseObserver` 拉变更，单批上限 `kMaxChanges = 100`（`ChangesFeed.cc:119-184`）：

```cpp
static constexpr uint32_t kMaxChanges = 100;
C4DatabaseObserver::Change c4changes[kMaxChanges];
...
while (limit > 0) {
    nextObservation = _changeObserver->getChanges(c4changes, min(limit, kMaxChanges));
    uint32_t nChanges = nextObservation.numChanges;
    if (nChanges == 0) break;
    for (uint32_t i = 0; i < nChanges; ++i, ++c4change) {
        if (c4change->sequence <= startingMaxSequence) continue;   // ① 越过水位线：丢弃
        // ② purge 变更的 sequence 为 0，被 ① 顺带过滤
        if (!allowObservedChange(info)) { _maxSequence = info.sequence; continue; }  // ③
        if (auto rev = makeRevToSend(info, nullptr); rev) {
            // ④ 同一 docID 在两次 getChanges 之间可能重复提交 → 线性扫描去重
            for (size_t j = 0; j < oldChangesCount; ++j)
                if (changes.revs[j]->docID == c4change->docID) { changes.revs.erase(...); ++limit; break; }
            changes.revs.push_back(rev); --limit;
        }
    }
}
```

- ① 的水位线过滤是 §28.1 的对偶：订阅先于扫描带来的重复，在这里被抹掉；
- ④ 的去重是 **O(b²) 线性扫描**（b ≤ min(limit, 100)），小批量下比哈希表更划算；`kMaxChanges = 100` 这个常量同时也是栈上固定数组 `c4changes[100]` 的尺寸依据；
- **墓碑策略**（`ChangesFeed.cc:156-158`）：即便 `skipDeletions` 为真，实时变更里的墓碑**照发**——`skipDeletions` 只作用于初次 dump，不作用于 live 变更，否则对端的删除永远不会传播；
- `_dbChanged()` 由观察者回调，用 `std::atomic<bool>` 做 **test-and-clear**（`ChangesFeed.cc:188-192`，字段声明见 `ChangesFeed.hh:119`）：`if (_notifyOnChanges.exchange(false)) { ... }`，保证"有新变更"事件不会丢失也不会重复触发。

### 28.4 SequenceSet：稀疏区间集

推送侧要表达"哪些 sequence 已完成、哪些还欠着"。若用位图，10⁹ 条变更就是 125 MB；若用集合，同样稀疏。LiteCore 用**区间映射**：`std::map<左端, 右端(开区间)>`（`LiteCore/Support/SequenceSet.hh:28-132`）。

单点插入 `_add` 只有 5 种情形（`LiteCore/Support/SequenceSet.cc:30-70`）：

```text
_add(s) → 覆盖 s 的区间迭代器:
    upper ← 第一条 start > s 的区间
    if upper 存在 and s = upper.start - 1:
        if upper 有前驱 lower and lower.end = s:
            lower.end ← upper.end; erase(upper); return lower     # ③ 夹缝合并
        return 前接 s 到 upper                                    # ②
    if upper 有前驱 lower:
        if s < lower.end:  return lower                           # ① 已含（幂等，O(1)）
        if s = lower.end:  ++lower.end; return lower              # ④ 后接
    return 插入单点区间 [s, s+1)                                  # ⑤ 新建
```

`add(s0, s1)` 在此之上再 `_add(s1-1)` 并把中间区间整段擦掉（`SequenceSet.cc:72-85`）。**复杂度**：单点 O(log r)；区间 add 为 O(log r + k)（k = 被吞掉的中间区间数）；`size()` / `pendingSequenceCount()` 是 O(r)（`Checkpoint.cc:154-164`，直接累加区间之间的"空洞"长度）。

集合运算（`SequenceSet.cc:143-180`）：

```text
intersection(s1, s2):            # 归并双指针 O(r1 + r2)
    while i1,i2 未耗尽:
        start ← max(i1.first, i2.first);  end ← min(i1.second, i2.second)
        if start < end: ret.add(start, end)
        advanceFirst  ← (i1.second ≤ i2.second)
        advanceSecond ← (i1.second ≥ i2.second)   # 相等时同时推进
        按上面两个布尔量推进迭代器

difference(s1, s2) = s1 依次 remove(s2 每条区间)      # O(r1·log r2 + …)
unionOf(s1, s2)     = s1 依次 add(s2 每条区间)
```

持久化用的是 **Fleece 稀疏数组**（`SequenceSet.cc:182-203`）：每条区间写成 `(start, length)` 一对，省掉"最后一个区间的无穷端点"：

```cpp
void SequenceSet::encode_fleece(FLEncoder flEnc) const {
    Encoder enc(flEnc); enc.beginArray();
    for (auto& range : _sequences) {
        enc.writeUInt(uint64_t(range.first));
        enc.writeUInt(uint64_t(range.second - range.first));   // 长度而非右端
    }
    enc.endArray(); enc.detach();
}
```

**空间**：O(r)，r = 未完成区间数。稳态下 r 通常为个位数，断点文件大小因此与"文档总数"**无关**，只与"乱序在途量"有关——这是 §23.2「checkpoint 失败 → 全量重同步」在存储层面的根因：`intersection` 回滚后空洞可能被放大。

### 28.5 Checkpoint：序列化与对账回滚

`Checkpoint::toJSON` 只在"有多于一条区间"时才写稀疏字段（`Replicator/Checkpoint.cc:36-66`）：

```cpp
#define SPARSE_CHECKPOINTS  // 若定义，整份稀疏集写入 JSON

auto minSeq = localMinSequence();
if (minSeq > 0_seq) enc.writeKey("local"_sl), enc.writeUInt(uint64_t(minSeq));   // 兼容旧格式
#ifdef SPARSE_CHECKPOINTS
    if (_completed.rangesCount() > 1) {
        enc.writeKey("localCompleted"_sl);
        _completed.encode_fleece(enc);        // 新格式：[ (start,len), ... ]
    }
#endif
    if (_remote) enc.writeKey("remote"_sl), expert(enc).writeRaw(_remote.toJSON());
```

读回时 `readDict` 优先尝试新字段、失败或缺失才回落到旧的单值 `local`（`Checkpoint.cc:77-96`）——**向后兼容的字段级版本协商**。

真正的分歧处理在 `validateWith`（`Checkpoint.cc:98-142`），三条分支：

```text
validateWith(remoteCP) → match:
    if _completed ≠ remoteCP._completed:                    # 本地与服务端认为完成的集合不一致
        log "Rolling back to a failsafe"
        _completed ← intersection(_completed, remoteCP._completed)   # 退回"双方都确认"的安全集
        match ← false                                        # 会重复提议部分变更

    if _remote ≠ remoteCP._remote:
        if 两侧都能 ParsedSequenceID 解析:
            if remoteParsed.before(localParsed):
                _remote ← remoteCP._remote; match ← false    # 服务端更旧 → 回滚接受
            else:
                忽略服务端 sequence（本地不更旧）
        else:
            Warn "Unparseable remote sequence ... Resetting replication"
            _remote ← ∅; match ← false                       # 兜底：整体重置
```

**设计要点**：任何不确定情形都选择**多发而不是漏发**（宁可重复提议、幂等去重，也不丢变更），只有"不可解析"才走最重的整体重置。这是典型的 at-least-once + 幂等消费组合。

### 28.6 复合序列号定序：ParsedSequenceID::before

LiteCore 需要与 Sync Gateway 的 `SequenceID.Before()` **判定完全一致**，否则双方会各持"较新"断点形成活锁。SG 在 `_changes` 上发三种序列号（`Replicator/ParsedSequenceID.hh:21-45`）：

| 形态 | 样例 | 语义 | 比较值 CV |
| --- | --- | --- | --- |
| Simple | `"42"` | 普通数据库序号 | `seq` |
| Backfill | `"100:35"` | 因授权变更（seq 100）触发的回补，seq 35 是回补内容 | `triggeredBy` |
| LowSeq | `"20:100:35"` / `"20::35"` | 回补 + 客户端已完整处理的最低连续序号 | `lowSeq` |

`before()` 实现 3×3 的跨形态定序表（`ParsedSequenceID.hh:141-172`）：

```text
before(b):
    # 键规则：LowSeq 看 lowSeq；Backfill 看 triggeredBy；Simple 看 seq
    if a.isLowSeq:      if b.isLowSeq:    return a.lowSeq < b.lowSeq or (相等时对内层 (seq,trig) 递归)
                        if b.isTriggered: return a.lowSeq <  b.triggeredBy
                        else:             return a.lowSeq <  b.seq
    if a.isTriggered:   if b.isLowSeq:    return a.triggeredBy ≤ b.lowSeq     # ≤ 而非 <
                        if b.isTriggered: return a.triggeredBy < b.triggeredBy or (相等时 a.seq < b.seq)
                        else:             return a.triggeredBy ≤ b.seq
    # a 是 Simple
                        if b.isLowSeq:    return a.seq ≤ b.lowSeq
                        if b.isTriggered: return a.seq <  b.triggeredBy
                        else:             return a.seq <  b.seq
```

注意 Backfill 与 Simple 交叉时用的是 **`<=`**（Backfill 可被视为"覆盖到 triggeredBy 时刻"的等价进度），LowSeq ↔ Backfill 亦然。`Checkpoint::validateWith` 正是靠它来判断该接受还是忽略服务端的 remote sequence（`Checkpoint.cc:118-130`）。**复杂度**：O(1)，纯整数比较；`parse` 为 O(L)（L = 字符串长度，最多 3 段数字，`ParsedSequenceID.hh:85-118`）。

### 28.7 拉取侧的乱序窗口：RemoteSequenceSet

推送侧按序消费，拉取侧却可能乱序到达（网络重排）。`RemoteSequenceSet` 维护一个**乱序容忍窗口**（`Replicator/RemoteSequenceSet.hh:22-101`）：

```cpp
void add(RemoteSequence s, uint64_t bodySize) {
    bool wasEmpty = empty();
    auto p = _sequences.insert({s, {_nextOrder++, _lastAdded, bodySize}});  // 按 sequence 排序的 map
    _lastAdded = std::move(s);
    if (wasEmpty) _first = p.first;                    // 空集时 _first = map 的首元素
}

void remove(const RemoteSequence& s, bool& wasEarliest, uint64_t& outBodySize) {
    auto i = _sequences.find(s);
    if (i == _sequences.end()) { outBodySize = 0; wasEarliest = false; return; }
    outBodySize  = i->second.bodySize;
    wasEarliest  = (i == _first);
    if (wasEarliest) { size_t minOrder = i->second.order; _sequences.erase(i); findFirst(minOrder + 1); }
    else              { _sequences.erase(i); }
}
```

- 双索引结构：`_sequences` 是 `map<RemoteSequence, value>`（按**序列值**有序），`value.order` 是**入队顺序**；`_first` 指向"最早入队且仍在窗口内"的元素；
- `findFirst` 是全量线性扫描找最小 order（`RemoteSequenceSet.hh:75-87`，源码里带 `// OPT: Slow linear scan. Keep a secondary collection sorted by order?` 的显式 TODO 标注）——**复杂度 O(r)**，但只在"被移除的恰是最早元素"这一低频路径上触发；
- **单调水位线**：`since()` 返回 `_first->second.prevSequence`（`RemoteSequenceSet.hh:39`）——只要最早元素还没到，拉取断点就不能推进。这正是"只有连续前缀才算已消费"的流控语义。

**复杂度汇总**：推送侧 `SequenceSet` O(log r)/操作、O(r) 空间；拉取侧 `RemoteSequenceSet` O(log r) 插入删除、O(r) 的最早元素维护；两者稳态 r 都由**网络乱序窗口**而非数据量决定，因此移动端 5GB 数据库的复制元数据仍是常数级。

## 二十九、参考资料

1. **Couchbase LiteCore 仓库**：[github.com/couchbase/couchbase-lite-core](https://github.com/couchbase/couchbase-lite-core)；
2. **Couchbase Lite 文档**：[docs.couchbase.com/couchbase-lite](https://docs.couchbase.com/couchbase-lite/current/index.html)；
3. **Sync Gateway 文档**：[docs.couchbase.com/sync-gateway](https://docs.couchbase.com/sync-gateway/current/index.html)；
4. **Couchbase Mobile 博客**：[couchbase.com/blog/category/mobile](https://www.couchbase.com/blog/category/mobile/)；
5. **BLIP 协议规范**：[github.com/couchbaselabs/BLIP-Cpp](https://github.com/couchbaselabs/BLIP-Cpp)；
6. **SQLite WAL 文档**：[sqlite.org/wal.html](https://www.sqlite.org/wal.html)；
7. **Fleece 设计文档**（LiteCore 内部）：`docs/overview/QueryTranslator.md`；
8. **Vector Search 设计**：[blog.couchbase.com/vector-search-mobile](https://www.couchbase.com/blog/)；
9. **Couchbase Connect 大会视频**：[couchbase.com/connect](https://www.couchbase.com/connect/)；
10. **Sample Apps**：[github.com/couchbaselabs](https://github.com/couchbaselabs)；
11. **Couchbase Academy**：[learn.couchbase.com](https://learn.couchbase.com/)；
12. **Couchbase Lite C 教程**：[docs.couchbase.com/couchbase-lite/c](https://docs.couchbase.com/couchbase-lite/current/c/quickstart.html)。