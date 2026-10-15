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

## 二十六、参考资料

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