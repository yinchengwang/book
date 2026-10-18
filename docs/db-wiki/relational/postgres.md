# PostgreSQL Wiki（关系型数据库 / 全维度覆盖）

> 版本：本次参考 PostgreSQL master 分支 `20devel`（commit `ea9c9432c`），主要 API 与数据布局在 v15/v16/v17 之后保持稳定。

## 一、项目定位

PostgreSQL（通常简称 Postgres）是世界上最先进的开源**对象-关系型数据库管理系统（ORDBMS）**。它以**严格遵循 SQL 标准**、**可扩展的数据类型与索引框架**、**成熟的多版本并发控制（MVCC）**、**流复制与逻辑复制**著称，被广泛用作 OLTP、HTAP 与数据仓库场景的核心存储。

- **痛点**：商业关系型数据库（Oracle、DB2、SQL Server）授权昂贵且锁定；MySQL 在复杂查询、事务隔离、扩展能力上偏弱；开发者需要一款既开源又能支撑企业级关键负载的数据库。
- **典型用户**：互联网公司（Instagram、Uber、Notion、Reddit、Discord 等）、金融行业（券商核心系统、计费）、电信运营商、政府/事业单位 GIS 系统、地理空间与时序应用（PostGIS、TimescaleDB 扩展）。

## 二、发展历史与社区

- **起源**：1986 年加州大学伯克利分校 Michael Stonebraker 教授的 POSTGRES 项目；1995 年 Postgres95 重写并添加 SQL；1996 年正式更名为 PostgreSQL 6.0。
- **里程碑**：2005 年 8.0（首个 Windows 原生版本）、2010 年 9.0（流复制与 Hot Standby）、2014 年 9.4（JSONB、逻辑解码）、2017 年 10（逻辑复制、声明式表分区）、2020 年 13（逻辑复制 slot 同步、增量排序）、2023 年 16（逻辑复制订阅者并行应用）、**2024 年 17（MERGE、SQL/JSON 构造、COPY 增强）、2025 年 18（主版本升级 in-place、scratchpad 缓存）**。
- **社区**：PostgreSQL Global Development Group（PGDG）+ 遍布全球的 Regional Groups（PgUS、PgEU、PGChina 等）+ 邮件列表 `pgsql-hackers`、`pgsql-general`。每年有 PGConf 系列会议（纽约、欧洲、亚洲、中国）。
- **维护节奏**：每年一个大版本（约 Q3/Q4），约每 3 个月一个次版本，长期支持分支每 5 年一次。

## 三、许可与商业限制

- **许可证**：PostgreSQL License（BSD 风格），非常宽松。
  - 来源：`reference/relational/postgres/COPYRIGHT`
  - 关键条款：允许商业使用、修改、分发、闭源衍生，无需付费。
- **商业版**：基于 PostgreSQL 的衍生商业产品须留意：
  - **AWS Aurora / RDS**：基于 PG 二次开发，云厂商闭源。
  - **EDB Postgres Advanced Server**：EnterpriseDB 提供闭源扩展（兼容 Oracle 语法）。
  - **Citus（Microsoft）**：列式与分片扩展，被微软收购后仍开源（PostgreSQL License + AGPL 部分）。
- **生态中立**：PGC（PG 商业委员会）保障开放治理，禁止单一厂商独占路线图。

## 四、业界主要使用方与案例

- **互联网**：Instagram（早期最大 PG 部署，分片到上千节点）、Uber（核心 trip 表）、Reddit、Discord、Notion、Twitch、Spotify。
- **金融**：摩根大通、BNY Mellon、上海证券交易所部分子系统、招商银行核心账务部分模块；Snowflake 早期也基于 PG 改造。
- **电信运营商**：NTT、Cisco 内部运营系统、中国移动部分省 BOSS。
- **云**：AWS RDS / Aurora、Google Cloud SQL、阿里云 RDS、腾讯云 TDSQL-PG、华为云 GaussDB（基于 openGauss 即 PG 衍生）。
- **GIS**：法国 IGN（国家测绘）、OpenStreetMap 主库、HERE 地图部分子系统。

## 五、一句话对比定位

> PostgreSQL 是**学术派 ORDBMS**的代表，强调标准合规、可扩展、复杂查询与事务严谨；与 **MySQL（Web 应用、性能优先）**、**SQLite（嵌入式、单文件）**、**Oracle（商业闭源、极致性能与生态）** 形成鲜明对照。

## 六、整体架构

PostgreSQL 采用经典的**客户端-服务器 + 多进程**架构，每个客户端连接由一个独立的 backend 进程处理（不是线程），通过共享内存交换数据。

### 6.1 总体分层

```
┌────────────────────────────────────────────────────────────────────┐
│                          客户端进程                                 │
│           (psql / libpq 客户端 / ODBC / JDBC)                       │
└─────────────────────────┬──────────────────────────────────────────┘
                          │ TCP/Unix Socket + Postgres Wire Protocol
┌─────────────────────────▼──────────────────────────────────────────┐
│                postmaster (src/backend/postmaster/postmaster.c)     │
│   - 监听端口 (默认 5432)                                             │
│   - fork() 出新 backend 处理每个连接                                 │
│   - 管理 shared memory / semaphore / WAL 启动                       │
│   - 进程池：startup / bgwriter / checkpointer / walwriter /         │
│     autovacuum launcher / walsender / walreceiver                  │
└─────────────────────────┬──────────────────────────────────────────┘
                          │ 共享内存 + 信号
┌─────────────────────────▼──────────────────────────────────────────┐
│                      单个 backend 进程                               │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │  解析器 Parser (parser/) → 分析器 Analyzer (parser/parse_*) │    │
│  │     → 改写器 Rewriter (rewrite/) → 规划器 Planner (optimizer/)│    │
│  │     → 执行器 Executor (executor/)                            │    │
│  └────────────────────────────────────────────────────────────┘    │
│  ┌────────────────────────────────────────────────────────────┐    │
│  │  访问方法 AM: heap (access/heap) / btree (access/nbtree)    │    │
│  │            / hash (access/hash) / gist / gin / spgist        │    │
│  ├────────────────────────────────────────────────────────────┤    │
│  │  Buffer Pool (storage/buffer/) — Clock-Sweep 替换             │    │
│  ├────────────────────────────────────────────────────────────┤    │
│  │  存储管理 storage/ — smgr, file, freespace, lmgr 锁          │    │
│  └────────────────────────────────────────────────────────────┘    │
└─────────────────────────┬──────────────────────────────────────────┘
                          │ 系统调用 read/write/fsync
┌─────────────────────────▼──────────────────────────────────────────┐
│                       磁盘文件层                                     │
│   base/<db_oid>/<relfilenode>   ← 堆表 / 索引 / toast                │
│   pg_wal/                       ← WAL 日志段                        │
│   pg_xact/                      ← CLOG 事务状态                      │
│   pg_multixact/                 ← 多事务状态                         │
│   pg_subtrans/                  ← 子事务状态                         │
│   pg_replslot/                  ← 复制槽元数据                       │
└────────────────────────────────────────────────────────────────────┘
```

### 6.2 进程模型

- **postmaster 单一主进程**：监听端口、维护共享内存池、为每个新连接 `fork()` 出独立 backend 进程。
- **辅助后台进程**：
  - `bgwriter`：周期性把脏页刷盘，平滑 checkpoint 抖动。
  - `checkpointer`：每个 `checkpoint_timeout`（默认 5 分钟）或 `max_wal_size` 触发全量 checkpoint。
  - `walwriter`：把 WAL buffer 异步刷盘。
  - `autovacuum launcher` + `autovacuum worker`：自动 VACUUM/ANALYZE。
  - `walsender` / `walreceiver`：流复制主从链路。
  - `startup`：备机回放 WAL 进程。
- **共享内存**：包含 buffer pool、锁表、clog、subtrans、proc array、prepared xacts、replication slot 等。

源码入口：`src/backend/postmaster/postmaster.c:1`（postmaster 主循环）。

## 七、存储引擎

### 7.1 页（Page）结构

8 KB 默认页大小（编译时可配置为 `4K、8K、16K、32K`），典型布局：

```
┌────────────────────────────────────────────────────────────┐
│ PageHeaderData (24 字节)                                   │
│   - pd_lsn         最后修改的 WAL LSN                       │
│   - pd_checksum    可选 CRC 校验                            │
│   - pd_flags       是否有空闲 / HAS_FREE_LINES              │
│   - pd_lower       空闲空间起始偏移                          │
│   - pd_upper       元组结束偏移（line pointer 数组从下往上） │
│   - pd_special     特殊区起始（索引专用）                    │
├────────────────────────────────────────────────────────────┤
│ ItemIdData[] (line pointer 数组，从 pd_lower 向下增长)      │
│   - lp_off / lp_flags / lp_len                             │
├────────────────────────────────────────────────────────────┤
│  空闲空间 (free space)                                       │
├────────────────────────────────────────────────────────────┤
│  HeapTuple 元组 (从 pd_upper 向下增长)                       │
│   t_xmin / t_xmax / t_infomask2 / t_infomask /             │
│   t_hoff / t_data (用户列数据 + null bitmap)                 │
├────────────────────────────────────────────────────────────┤
│ Special 区 (索引如 B+Tree 内部页才有)                        │
└────────────────────────────────────────────────────────────┘
```

### 7.2 表空间 / OID / relfilenode

- 每个数据库有独立 OID；每个表/索引有 `relfilenode`（默认与 OID 同值，TRUNCATE/REINDEX/VACUUM FULL 后会变更）。
- 物理文件：`base/<db_oid>/<relfilenode>`，每个表对应多个 fork：main（数据）、fsm（free space map）、vm（visibility map）、init（empty page）。

### 7.3 Heap AM（堆表）

- **核心函数**：`src/backend/access/heap/heapam.c`
  - `heap_insert`（行号 2005）：插入元组并记录 WAL。
  - `heap_delete` / `heap_update`：标记 `t_xmax` + WAL。
  - `heap_lock_tuple`：行级锁与可见性判断。
- **HOT（Heap Only Tuple）**：`src/backend/access/heap/README.HOT` 详细描述：当 UPDATE 不修改索引列时，新元组与旧元组共享同一索引条目，且支持**单页碎片整理（prune）**，避免触发表级 VACUUM。源码：`src/backend/access/heap/pruneheap.c`。
- **MVCC**：每行元组有 `t_xmin`（插入事务）+ `t_xmax`（删除/更新事务），`HeapTupleHeaderData`（`src/include/access/htup_details.h`）。

### 7.4 TOAST

大字段（>2KB）自动切分到独立的 TOAST 表（`pg_toast.<oid>`），以 chunk 方式存储，支持 `EXTERNAL`（不压缩）、`EXTENDED`（默认压缩+外部）、`MAIN`（尽量在主表）、`PLAIN`（禁用）。源码：`src/backend/access/heap/heaptoast.c`。

### 7.5 VACUUM 与 Autovacuum

- `src/backend/access/heap/vacuumlazy.c`：lazy vacuum（标记死亡元组为空闲空间，回收事务 ID）。
- `src/backend/commands/vacuum.c`：VACUUM/ANALYZE 主入口。
- **事务回卷（XID wraparound）防护**：每 `autovacuum_freeze_max_age`（默认 2 亿）触发 aggressive vacuum。

## 八、索引机制

PostgreSQL 提供**可插拔索引访问方法**框架（`Index Access Method API`），核心内置实现：

| 索引 | 适用场景 | 源码 |
|------|----------|------|
| **B+Tree（b-tree）** | 通用等值/范围查询，默认索引 | `src/backend/access/nbtree/` |
| **Hash** | 仅等值查询（10+ 版本之后更好） | `src/backend/access/hash/` |
| **GiST** | 几何、范围、全文、IP、网络 | `src/backend/access/gist/` |
| **GIN** | 全文、数组、JSONB、trigram | `src/backend/access/gin/` |
| **SP-GiST** | 空间分区树（trie / quadtree） | `src/backend/access/spgist/` |
| **BRIN** | 块级摘要（顺序写入时序数据） | `src/backend/access/brin/` |

### 8.1 B+Tree 关键特性

- **页面结构**：meta 页 → internal 页（含 high key + child pointer） → leaf 页（含 ItemPointer）。叶子页之间通过双向链表连接，支持顺序扫描。
- **去重（Deduplication）**：v13+ 支持将相邻重复 key 合并存储，节省索引空间（`nbtuplesort.c`、`nbtdedup.c`）。
- **并发控制**：B+Tree 自 v14 起从 `L&Y` 算法迁移到首页链接风格（`src/backend/access/nbtree/README`），减少页面分裂时的写放大。

### 8.2 表达式索引与部分索引

- **表达式索引**：`CREATE INDEX ON t (lower(name))`，支持函数索引。
- **部分索引**：`CREATE INDEX ON t (status) WHERE status = 'active'`，仅索引部分行。

### 8.3 索引代价估算

- 代价模型：`src/backend/optimizer/path/costsize.c`，参数：`random_page_cost`、`seq_page_cost`、`cpu_tuple_cost`、`effective_cache_size`。

## 九、查询与执行

### 9.1 解析阶段

```
SQL 文本
  → lex (src/backend/parser/scan.l + src/backend/parser/gram.y → src/backend/parser/parser.c)
  → parse tree (raw_parser)
  → analyze (parse_analyze + transformExpr)
  → parse tree with Var/Const/Expr
  → rewrite (Rule system / 视图展开)
  → Query tree
  → plan (Planner)
  → Plan tree (Volcano 风格)
  → Executor
```

### 9.2 优化器

- `src/backend/optimizer/plan/planner.c`（行号 1 起，约 9276 行）：`planner()` 主入口，包含子查询 pull-up、聚合下推、连接顺序选择（动态规划）。
- **统计信息**：`src/backend/optimizer/path/allpaths.c` 读取 `pg_statistic` / `pg_statistic_ext`。
- **遗传查询优化（GEQO）**：超过 `geqo_threshold`（默认 12 表）时启用遗传算法搜索连接顺序，`src/backend/optimizer/geqo/`。
- **JIT 编译**：v11+ 支持 LLVM JIT（`src/backend/jit/`），对表达式计算、聚合、tuple deforming 进行 native 编译加速。

### 9.3 执行器（Volcano 模型）

- 入口：`src/backend/executor/execMain.c`
  - `ExecutorStart`（行号 124）：初始化 plan state、打开 relation、初始化 EState。
  - `ExecutorRun`（行号 308）：火山式拉取 tuple；`direction` 支持前向/后向，`count` 控制拉取数量。
- 计划节点：`src/backend/executor/nodeXxx.c`，常见：
  - `nodeSeqscan.c` / `nodeIndexscan.c` / `nodeIndexonlyscan.c`：扫描。
  - `nodeNestloop.c` / `nodeHashjoin.c` / `nodeMergejoin.c`：连接。
  - `nodeAgg.c` / `nodeGroup.c` / `nodeSort.c` / `nodeLimit.c`：聚合/排序/分页。
  - `nodeModifyTable.c`：DML 执行。
  - `nodeAppend.c` / `nodeMergeAppend.c`：UNION/分区表合并。
- **并行执行**：v9.6+ 支持并行 seq scan / hash join / agg / nodeGather，`src/backend/executor/execParallel.c`。

### 9.4 EXPLAIN 输出

- `EXPLAIN (ANALYZE, BUFFERS, VERBOSE, FORMAT JSON)` 返回 `Plan` 树，节点含 `Actual Rows / Actual Loops / Actual Total Time / Shared Read/Write / Local Read/Write`。

## 十、事务与一致性

### 10.1 隔离级别

SQL 标准四级全实现：

| 级别 | PG 实现 | 是否真正支持 |
|------|---------|--------------|
| READ UNCOMMITTED | 实际等价 READ COMMITTED | 否（会升档） |
| READ COMMITTED | 每个语句获取新快照 | 是 |
| REPEATABLE READ | 事务开始快照 + SSI 检查 | 是 |
| SERIALIZABLE | Snapshot Isolation + SSI 冲突检测 | 是 |

### 10.2 MVCC

- **快照**：`src/backend/storage/ipc/procarray.c` 维护 `xmin`/`xmax` 边界。
- **可见性**：`src/backend/access/heap/heapam_visibility.c` 中 `HeapTupleSatisfiesMVCC` 系列函数。
- **快照隔离（SI）**：每个事务起始保存快照 `SnapshotData`（`src/include/utils/snapshot.h`）。
- **SSI（可串行化快照隔离）**：v9.1 引入，检测 rw 冲突并 abort 其中之一，`src/backend/storage/lmgr/predicate.c`。

### 10.3 锁机制

- **四级锁**（`src/backend/storage/lmgr/README`）：
  1. **Spinlock**：硬件原子指令实现，仅用于 LWLatch 内部。
  2. **LWLock（轻量锁）**：保护共享内存数据结构，可共享/排他。
  3. **Lock（普通锁）**：表级、页级、行级；由 lock manager 管理（`lmgr.c`）。
  4. **Predicate lock**：SSI 序列化冲突检测。
- **行级锁**：通过 `t_infomask` 中的 `HEAP_XMAX_*` 位实现，不占独立锁表，事务结束自动释放。
- **咨询锁（advisory lock）**：应用层命名锁，`pg_advisory_lock`/`pg_try_advisory_xact_lock`。

### 10.4 两阶段提交（2PC）

- `PREPARE TRANSACTION` 后写入 `pg_twophase/` 目录；`COMMIT/ABORT PREPARED` 完成提交。
- 协调器实现：`src/backend/access/transam/twophase.c`。

## 十一、复制与高可用

### 11.1 流复制（Physical Streaming Replication）

- **WAL 源**：`src/backend/replication/walsender.c`：主库 WAL 推送到从库。
- **WAL 接收**：`src/backend/replication/walreceiver.c`：备库通过 libpq 协议接收。
- **同步复制**：`src/backend/replication/syncrep.c`：基于 `synchronous_standby_names` 保证 RPO=0。

### 11.2 逻辑复制（Logical Replication）

- v10+ 引入；`src/backend/replication/logical/`（含 `decode.c`、`reorderbuffer.c`、`worker.c`、`apply.c`）。
- 发布订阅模型：`PUBLICATION`、`SUBSCRIPTION`；v15+ 支持两向/多向复制（双向冲突由应用层处理）。
- **输出插件**：`pgoutput`（内置）、`test_decoding`、`wal2json`（扩展）。

### 11.3 高可用方案

- **Patroni + etcd/ZooKeeper**：业界主流 HA 框架，自动 failover。
- **pg_auto_failover**：微软开源，简化部署。
- **PgBouncer + HAProxy**：连接池 + 负载均衡。
- **PgBackRest / Barman / pg_basebackup**：物理备份。
- **PgPool-II**：读写分离、连接池、负载均衡。

## 十二、性能与基准

- **TPC-C**：PostgreSQL 16 单机 ~30-50k tpmC（NVMe SSD、64 核调优后），Aurora 更高。
- **TPC-H**（分析）：300 GB SF300 单机 100-300 秒（依赖优化）。
- **Sysbench OLTP_READ_WRITE**：单机 PG 16 在高端硬件 20-50k TPS。
- **参数调优**（典型）：`shared_buffers = 25% RAM`、`effective_cache_size = 70% RAM`、`work_mem = 64MB`、`max_connections = 200`（建议搭配连接池降到 100-300）、`wal_compression = zstd`、`random_page_cost = 1.1`（SSD）。
- **监控视图**：`pg_stat_activity`、`pg_stat_statements`、`pg_stat_user_tables`、`pg_statio_user_indexes`、`pg_locks`。

## 十三、生态与集成

### 13.1 客户端 SDK

- **C / libpq**（`src/interfaces/libpq/`）：官方 C 客户端。
- **psql ODBC**：`src/interfaces/odbc/`。
- **绑定**：`src/interfaces/ecpg/`（嵌入式 SQL C）。
- **生态**：
  - **Python**：psycopg2 / psycopg3 / asyncpg / SQLAlchemy / Django ORM。
  - **Go**：pgx / lib/pq / GORM / Bun / Ent。
  - **Java**：PostgreSQL JDBC Driver。
  - **Node.js**：pg / pg-promise / Prisma / TypeORM / Drizzle / Kysely。
  - **Rust**：tokio-postgres / sqlx / diesel。

### 13.2 扩展生态

- **PostGIS**：地理空间（`postgis/`）。
- **pgvector**：向量相似度搜索（`reference/extension/pgvector/`）。
- **TimescaleDB**：时序。
- **pg_stat_statements**：SQL 统计。
- **pg_trgm / fuzzystrmatch**：模糊匹配。
- **pg_partman**：分区管理。
- **PL/Perl / PL/Python / PL/Tcl**：内部过程语言。

### 13.3 工具链

- **psql**：交互式 CLI（`src/bin/psql/`）。
- **pg_dump / pg_restore**：逻辑备份。
- **pg_basebackup**：物理备份。
- **pg_upgrade**：主版本升级（v17+ 支持 in-place 升级）。
- **pg_ctl**：启停服务（`src/bin/pg_ctl/`）。
- **initdb**：初始化数据目录（`src/bin/initdb/`）。
- **pg_isready / pg_rewind**：集群同步工具。

## 十四、源码学习路径

推荐从底层到上层阅读：

1. **入口与进程模型**：`src/backend/postmaster/postmaster.c:1`
2. **Buffer Pool**：`src/backend/storage/buffer/bufmgr.c:879`（`ReadBuffer`）、`:1137`（`ZeroAndLockBuffer`）
3. **Heap 访问方法**：`src/backend/access/heap/heapam.c:2005`（`heap_insert`）
4. **HOT 与 prune**：`src/backend/access/heap/pruneheap.c` + `README.HOT`
5. **执行器主循环**：`src/backend/executor/execMain.c:124`（`ExecutorStart`）、`:308`（`ExecutorRun`）
6. **执行器 README**：`src/backend/executor/README`
7. **规划器**：`src/backend/optimizer/plan/planner.c:1`（约 9276 行）
8. **代价模型**：`src/backend/optimizer/path/costsize.c`
9. **锁管理**：`src/backend/storage/lmgr/README`
10. **复制**：`src/backend/replication/walsender.c` + `walreceiver.c`
11. **WAL**：`src/backend/access/transam/xlog.c`

源码规模：当前 master 约 160 万行 C 代码（src/ 内），加上测试、contrib、pl 约 200 万+。

## 十五、风险与陷阱

### 15.1 运维雷区

- **长事务**：导致表膨胀（bloat）、CLOG 累积、`pg_xact` 增长、replication slot 阻塞。
- **未启用 autovacuum**：默认开，但表频繁 UPDATE/DELETE 的列必须单独调高 `autovacuum_vacuum_scale_factor`。
- **`max_connections` 设置过大**：每连接消耗 ~10 MB 内存，数千连接 OOM；务必搭配连接池。
- **未开启 `wal_compression`**：大 WAL 增加网络带宽和备机回放压力。
- **WAL 归档未配**：replication slot 失效会导致 WAL 堆积撑爆磁盘。

### 15.2 迁移痛点

- **MySQL → PG**：自增 ID / 字符集 / `GROUP BY` 严格性 / 日期函数差异。
- **Oracle → PG**：序列（SEQUENCE vs Oracle SEQUENCE）/ 同义词 / 物化视图语法差异 / 包（package）需重构为 schema + 函数。
- **主版本升级**：v17 之前必须 `pg_upgrade --link` 或 dump/restore，锁停机；v17+ 支持 in-place rolling upgrade。

### 15.3 性能陷阱

- **无索引的全表扫描**：OLTP 场景下导致 buffer pool 抖动；用 `EXPLAIN ANALYZE` 检查。
- **函数体内副作用函数**：不可 IMMUTABLE，导致索引失效。
- **大 JOIN 未走 Hash Join**：缺少 `work_mem` 时退化到 nested loop。
- **JSONB 字段无 GIN 索引**：导致全字段扫描。

### 15.4 安全与权限

- `pg_hba.conf` 默认 trust 风险：必须改为 `md5`/`scram-sha-256`。
- `superuser` 滥用：应用连接不应使用 postgres 超级用户。

## 十六、学习路径

### 16.1 入门（1-2 周）

1. 安装 PG（官方包或源码 `configure && make && make install`），用 `psql` 执行基本 CRUD。
2. 阅读官方教程 `src/tutorial/`（含 basic、advanced、funcs、syscat）。
3. 学习 SQL 基础：`SELECT/INSERT/UPDATE/DELETE`、`JOIN`、聚合、子查询。
4. 学习 psql 元命令：`\\d`、`\\dt`、`\\dv`、`\\dn`、`\\df`、`\\timing`。

### 16.2 进阶（1-2 月）

1. 阅读 `docs/src/sgml/` 官方手册，特别是：
   - "Server Programming"（PL/pgSQL）
   - "Performance Tips"
   - "Concurrency Control"
   - "Replication"
2. 源码阅读 `src/backend/executor/README`、`src/backend/storage/buffer/README`、`src/backend/access/heap/README.HOT`。
3. 实战：写 PL/pgSQL 函数/触发器、设计 schema（含分区表）、调优 query。

### 16.3 高级（3-6 月）

1. 深入源码：`bufmgr.c` 的 clock sweep、repl_gram.y 的复制协议语法、`xlog.c` 的 WAL 写入。
2. 写自定义扩展（`CREATE EXTENSION` 框架）。
3. 部署 Patroni/Etcd 集群、配置流复制 + 逻辑复制。
4. 性能调优：`pg_stat_statements`、`auto_explain`、采样 `pg_stat_io`。

### 16.4 推荐资源

- **官方文档**：<https://www.postgresql.org/docs/current/>
- **官方 Wiki**：<https://wiki.postgresql.org/wiki/Main_Page>
- **PG 14 Internals**（社区免费电子书）：<https://postgrespro.com/community/books/internals>
- **《The Art of PostgreSQL》**（Dimitri Fontaine）
- **《PostgreSQL 14 Administration Cookbook》**
- **邮件列表**：`pgsql-hackers@postgresql.org`（开发）、`pgsql-general@postgresql.org`（用户）
- **会议**：PGConf.EU、PGConf.US、PGConf.Asia、PGConf.China

## 十七、可移植性与部署

- **平台**：Linux（所有发行版）、macOS、Windows（官方安装器）、BSD。
- **容器化**：官方 `postgres` Docker 镜像（基于 Debian/Alpine），可调整 `PGDATA` 环境变量。
- **Kubernetes**：CloudNativePG、Zalando Postgres Operator、Patroni Operator。
- **云原生**：Aurora PG、Citus Cloud、Aliyun RDS-PG、Tencent Cloud TDSQL-PG、华为云 GaussDB。
- **嵌入式**？否，PG 是 C/S 架构，进程外运行；嵌入式场景请用 SQLite。

## 十八、变体与衍生项目

- **EDB Postgres Advanced Server**：EnterpriseDB 维护，闭源扩展兼容 Oracle 语法。
- **Greenplum**：基于 PG 8.x 改造的 MPP 分析型数据库（开源）。
- **Citus**：基于 PG 的分布式扩展，支持分片与 HTAP，被微软收购后开源。
- **openGauss**：华为基于 PG 9.2 衍生（详见 `docs/db-wiki/relational/openGauss.md`）。
- **PolarDB**：阿里基于 PG 改造的云原生数据库。
- **FerretDB**：用 PG 作为后端实现 MongoDB 协议。
- **Apache AGE**：图数据库扩展（基于 PG）。

## 十九、与本项目的关联

本仓库 `engineering/src/db/` 实现了一个**简化版的 PG 风格存储引擎**：

- `db/catalog.h/.c`：系统表与 OID 管理，对应 PG `pg_class/pg_attribute/pg_type`。
- `db/buf.h/.c`（`bufmgr.c`）：Clock-Sweep 置换算法，对应 PG `src/backend/storage/buffer/bufmgr.c` 的简化版。
- `db/rel.h/.c`（`rel.c`）：Relation 抽象 + TupleDesc，对应 PG `access/common/heaptuple.c`。
- `db/heapam.h/.c`：堆表 CRUD，对应 PG `src/backend/access/heap/heapam.c`（无 MVCC）。
- `db/btreeam.h/.c`：B+Tree 索引，对应 PG `src/backend/access/nbtree/`。
- `db/wal.h/.c`：WAL 简化版，对应 PG `src/backend/access/transam/xlog.c`。

可以直接参考本 wiki 学习 PG 实现，再对比 `engineering/src/db/` 中的简化实现，加深理解。

## 二十、参考资料

1. PostgreSQL 官方文档：<https://www.postgresql.org/docs/current/>
2. PostgreSQL 内部架构 wiki：<https://wiki.postgresql.org/wiki/Internals>
3. PostgreSQL 源码 `src/backend/executor/README`、`src/backend/access/heap/README.HOT`、`src/backend/storage/buffer/README`、`src/backend/storage/lmgr/README`
4. 《PostgreSQL 14 Internals》：<https://postgrespro.com/community/books/internals>
5. Robert Haas 博客（PG 核心开发者）：<https://rhaas.blogspot.com/>
6. 《The Art of PostgreSQL》：<https://theartofpostgresql.com/>
7. PgCon 大会演讲：<https://www.pgcon.org/archives.html>
8. PG 14+ 版本发布说明：<https://www.postgresql.org/docs/release/>
9. 知乎「PostgreSQL 学徒」专栏：<https://zhuanlan.zhihu.com/postgresql>
10. 本仓库 `engineering/src/db/` 中的简化 PG 风格存储引擎源码

## 二十一、B+Tree 节点分裂算法

本节深挖 `src/backend/access/nbtree/` 下的页面分裂策略。第八节 8.1 仅概览了首页链接风格，本节给出具体算法伪代码、复杂度分析与本项目对应实现。

### 21.1 分裂触发

当插入新元组后页内空间不足（PD 头计算 free space < `sizeof(IndexTuple)` + `sizeof(ItemIdData)` + page header 余量），PG v14+ 采用"首页链接（right-link）"风格而非 L&Y 算法，以减少页面分裂时的写放大（避免左兄弟同步更新）。整体流程：

```
_bt_insertonpage(itup_key, buf, itemsz, newitemoff, rel):
  if page has enough free space:
      place itup on buf
      return buf
  else:
      // nbtinsert.c:1232 触发 _bt_split
      newbuf = _bt_split(rel, heaprel, itup_key, buf, cbuf,
                         newitemoff, itemsz, newitem, ...)
      return newbuf
```

源码：`reference/relational/postgres/src/backend/access/nbtree/nbtinsert.c:1489`（`_bt_split` 函数定义）、`:1232`（分裂调用点）。

### 21.2 分割点定位：`_bt_findsplitloc` 与 `_bt_split_penalty`

分裂质量取决于"何处切"。PG 枚举所有候选 split offset，对每个候选计算 penalty，目标是**最小化写放大**（让某一侧更满、降低后续插入的再次分裂概率）：

```
_bt_split_penalty(state, split):
  // state 含左右区间分别的 free space（差）与原始空间
  diff = split.left_free  - split.right_free
  if |diff| <= page_size / 8:
      // 单边差异不大时：剩余空闲越多越好（写放大最小）
      penalty = min(split.left_free, split.right_free)
  else:
      // 严重倾斜时大偏移惩罚
      penalty = max(split.left_free, split.right_free) + HUGE_OFFSET_BIAS
  return penalty

_bt_findsplitloc(rel, origpage, newitemoff, newitemsz):
  candidates = []
  for each valid split offset in page:
      candidates.append(compute_split(offset, ...))
  qsort(candidates by _bt_splitcmp)
  return candidates with minimal penalty
```

源码：`reference/relational/postgres/src/backend/access/nbtree/nbtsplitloc.c:1132`（`_bt_split_penalty`）、`:588`（`qsort` 排序）、`nbtinsert.c:1567`（`_bt_findsplitloc` 调用）。

### 21.3 后缀截断（Suffix Truncation）

当 newitem 与页面 high key 共享后缀时，PG 会在新右页的 high key 上**截断冗余后缀**，节省内部页与叶页之间重复 key 的存储：

```
_bt_truncate(rel, lastleft, firstright, itup_key):
  // nbtinsert.c:1682 处的 _bt_truncate
  // 截断 lastleft/firstright 共享后缀，使 high key 更短
  return truncated_highkey
```

源码：`reference/relational/postgres/src/backend/access/nbtree/nbtinsert.c:1682`（`_bt_truncate`）。该优化建立在 v13 的 dedup 之上，是 PG 在写放大控制上的二次升级。

### 21.4 复杂度分析

| 步骤 | 时间复杂度 | 备注 |
|------|-----------|------|
| 候选 split 枚举 | O(N) | N = 页内 tuple 数（≤ ~256） |
| Penalty 计算 | O(1) / candidate | |
| Split 排序 | O(N log N) | `qsort` |
| 页面写入 | O(N/2) | 左右各写一半 |
| 总分裂成本 | O(N log N) + O(log M) 寻路 | M = 总页数 |

业界实测平均空间利用率约 **70-90%**，远优于朴素 halving（50%）。`_bt_split` 内部还会触发 `_bt_newlevel` 处理根节点分裂（2 → 3 页 → 根级数 +1），属于 O(log M) 摊销。

### 21.5 本项目对应

`engineering/src/db/btreeam.c` 实现的简化版仅做朴素 halving（左右各 50%），**未实现** penalty-based split 与 suffix truncation。对比阅读可看出 PG 在写放大与空间利用率上的工程深度。

## 二十二、MVCC 可见性判断与快照语义

第十节 10.2 仅列出 `HeapTupleSatisfiesMVCC` 与 `SnapshotData` 名称。本节给出可见性判断的完整公式、hint bits 缓存策略与复杂度分析。

### 22.1 快照数据结构

PG 快照 `SnapshotData` 定义见 `reference/relational/postgres/src/include/utils/snapshot.h:153`，核心三字段：

```
typedef struct SnapshotData {
    TransactionId xmin;   // 所有 < xmin 的 XID 一定可见
    TransactionId xmax;   // 所有 >= xmax 的 XID 一定不可见
    TransactionId *xip;   // (xmin, xmax) 间已活动但未提交的事务列表
    uint32      xcnt;     // xip 数组长度
    TransactionId *subxip;// 子事务活动列表（>= xmin，未过滤 xmax）
    int32       subxcnt;
    bool        takenDuringRecovery;
    ...
} SnapshotData;
```

`xmin ≤ xip[i] < xmax` 三元组共同定义快照的"可见性世界"。源码：`reference/relational/postgres/src/include/utils/snapshot.h:153-180`。

### 22.2 可见性判断公式：`HeapTupleSatisfiesMVCC`

源码入口：`reference/relational/postgres/src/backend/access/heap/heapam_visibility.c:939`。逻辑可表达为：

```
HeapTupleSatisfiesMVCC(tuple, snapshot):
  xmin = tuple.t_xmin
  xmax = tuple.t_xmax

  // ===== 阶段 1：xmin（插入事务）可见性 =====
  if xmin == snapshot.self_xact:
      visible_xmin = true                       // 自己插入
  elif XidInMVCCSnapshot(xmin, snapshot):       // xmin ∈ snapshot.xip
      return INVISIBLE                          // 插入事务未提交
  elif TransactionIdDidCommit(xmin):
      visible_xmin = true                       // 插入事务已提交
  else:
      return INVISIBLE                          // 已回滚/crash

  // ===== 阶段 2：xmax（删除/更新事务）检查 =====
  if xmax == InvalidTransactionId:
      return VISIBLE                             // 未删除
  if xmax == snapshot.self_xact:
      return INVISIBLE                            // 自己刚刚删除
  if XidInMVCCSnapshot(xmax, snapshot):
      return VISIBLE                              // 删除事务未提交 → 仍可见
  elif TransactionIdDidCommit(xmax):
      return INVISIBLE                            // 删除已提交 → 不可见
  else:
      return VISIBLE                              // 删除已回滚 → 仍可见
```

源码同文件 `:1005`（`XidInMVCCSnapshot(xmin, snapshot)` 调用 + `SetHintBitsExt`）、`:1053`（xmax 检查）。

### 22.3 Hint Bits 缓存

第一次判断后会设置 `t_infomask` 中的 hint 位（`HEAP_XMIN_COMMITTED` / `HEAP_XMIN_INVALID` / `HEAP_XMAX_COMMITTED` / `HEAP_XMAX_INVALID`），后续判断退化为**常数时间位测试**，避免反复访问 CLOG。

```
// heapam_visibility.c:1010 附近
if TransactionIdDidCommit(HeapTupleHeaderGetRawXmin(tuple)):
    SetHintBitsExt(tuple, buffer, HEAP_XMIN_COMMITTED,
                   HeapTupleHeaderGetRawXmin(tuple), state)
else:
    SetHintBitsExt(tuple, buffer, HEAP_XMIN_INVALID, ...)
```

源码：`reference/relational/postgres/src/backend/access/heap/heapam_visibility.c:1010`（hint bits 设置）。

### 22.4 快照获取与推进

- `GetTransactionSnapshot()` 在事务/语句开始时构建 `xmin/xmax/xip`。
- `ProcArray`（`src/backend/storage/ipc/procarray.c`）维护所有活动事务的 XID 列表。
- READ COMMITTED 每条语句新快照；REPEATABLE READ / SERIALIZABLE 整个事务共用一个。

### 22.5 复杂度与代价

| 操作 | 复杂度 | 备注 |
|------|--------|------|
| 首次判断（无 hint） | O(log N) | CLOG SLRU 缓存二分查找 |
| 命中 hint bits | O(1) | 位测试 + 短路 |
| 快照构建 | O(K) | K = 全局活跃事务数 |
| `XidInMVCCSnapshot` | O(xcnt) | 二分查找 xip 数组 |

### 22.6 本项目对应

`engineering/src/db/heapam.c` 当前实现无 MVCC（单版本覆盖写），**未实现** xmin/xmax 字段、hint bits 与 CLOG。对比阅读可清晰看到 PG 在多版本并发控制上的工程深度。

## 二十三、查询优化器与 Join 算法

第九节 9.2/9.3 提到 GEQO / Volcano，但未给出动态规划 join order 伪代码、Hash/Merge Join 实现细节。本节深入优化器与 Join 算法。

### 23.1 优化器总体流程

PG 优化器位于 `reference/relational/postgres/src/backend/optimizer/plan/planner.c`，主入口 `planner()`。整体流程：

```
planner(root):
  1. subquery_pull_up + 表达式预处理
  2. 统计信息读取（pg_statistic / pg_statistic_ext）
  3. 初始化 base rels（size estimate、单表 access paths）
  4. 启发式优化（强制单表 access path 选择）
  5. join search:
       if join_search_hook:
           return hook(root, levels_needed, initial_rels)
       elif enable_geqo && levels_needed >= geqo_threshold:
           return geqo(root, levels_needed, initial_rels)            // 遗传算法
       else:
           return standard_join_search(root, levels_needed, initial_rels)  // DP
  6. top-level: distinct, aggregation, sort, limit 处理
```

源码：`reference/relational/postgres/src/backend/optimizer/path/allpaths.c:3915`（GEQO vs DP 路由判断）。

### 23.2 Join 顺序搜索：动态规划（`standard_join_search`）

源码：`reference/relational/postgres/src/backend/optimizer/path/joinrels.c:78`（`join_search_one_level`）。按层级构造 joinrel，每层处理规模递增。

```
standard_join_search(root, levels_needed):
  // levels_needed = N - 1（N 个 base rel 需要 N-1 次连接）
  for lev = 2 to levels_needed:
    // 上一轮 join 出的中间 joinrel（每个含 lev-1 张 base rel）
    for each old_rel in joinrels[lev - 1]:
      for each initial_rel not yet in old_rel:
        new_joinrel = make_join_rel(old_rel, initial_rel, ...)
        paths = generate_paths(new_joinrel)   // 嵌套循环 / 哈希 / 归并
        keep cheapest_path(new_joinrel)
    joinrels[lev] = all new joinrels at this level
  
  return cheapest_path(joinrels[levels_needed])
```

源码同文件 `:78`（`join_search_one_level` 函数定义）。

**复杂度**：O(3^N)（每个 N 元集合有 3^N − 1 个非空子集）；空间 O(2^N)（存每层 joinrel）。N ≤ `geqo_threshold`（默认 12）可行，超出后切换 GEQO。

### 23.3 GEQO 遗传算法

当 N > 12 时 DP 空间爆炸，PG 切换到遗传算法：

```
geqo(root, levels_needed, initial_rels):
  // 染色体编码：表的排列 + join 方法标签
  init_population(pool_size = pool_size)        // 默认 50
  for gen = 0 to generations:                   // 默认 200
    fitness = geqo_eval(pool)                   // 每条染色体 → 计划 cost
    selected = selection(pool)                  // 锦标赛 / 赌轮
    children = crossover_and_mutate(selected)   // ERX / OX1 / OX2
    pool = children
  return best_fitness_chromosome
```

源码目录：`reference/relational/postgres/src/backend/optimizer/geqo/`（含 `geqo_main.c`、`geqo_eval.c`、`geqo_pool.c`、`geqo_cx.c`、`geqo_mutation.c` 等）。

### 23.4 Volcano 拉取模型

执行器采用经典 Volcano pull-based，每个 Plan Node 实现三个回调 `init_plan / next_tuple / end_plan`。主循环在 `ExecProcNode`：

```
ExecProcNode(node):
  if node->chgParam != NULL:
      re_scan(node)
  return node->ExecNodeProc(node)         // 节点类型多态派发
```

源码：`reference/relational/postgres/src/backend/executor/execMain.c:308`（`ExecutorRun` 顶层循环）。

### 23.5 Hash Join 算法

源码：`reference/relational/postgres/src/backend/executor/nodeHashjoin.c:225`（`ExecHashJoinImpl`）。

```
ExecHashJoin(outer, inner, hash_keys):
  // 阶段 1：build（内表构造 hash 表）
  for tuple in inner:
    bucket = hash(tuple.hash_key) % nbuckets
    append tuple to bucket
  
  // 阶段 2：probe（外表探测，可分批 spill）
  for tuple in outer:
    h = hash(tuple.hash_key)
    for match in bucket[h]:
      if match.join_key == tuple.join_key:
        emit(match, tuple)                       // 输出连接结果
```

时间复杂度：O(M + N) 平均（M = inner 行数，N = outer 行数）；最坏 O(M * N)（严重 hash 冲突）。源码同文件 `:225`。

### 23.6 Merge Join 算法

源码：`reference/relational/postgres/src/backend/executor/nodeMergejoin.c`。

```
ExecMergeJoin(outer_sorted, inner_sorted, join_keys):
  outer_it = outer_sorted.begin()
  inner_it = inner_sorted.begin()
  while outer_it != end and inner_it != end:
    cmp = compare(outer_it.key, inner_it.key)
    if cmp < 0: outer_it++
    elif cmp > 0: inner_it++
    else:                                         // 相等 key
      emit_all_equal_pairs(outer_it, inner_it)    // 内连接输出所有配对
      mark_outer_end / mark_inner_end             // 处理重复 key
```

要求两侧已排序（pre-sort 或 index scan），时间复杂度：O(M + N)；对重复 key 最坏退化为 O(M * N)。

### 23.7 统计信息

PG 统计信息由 ANALYZE 命令维护，存储于系统表：

| 系统表 | 关键字段 | 用途 |
|--------|---------|------|
| `pg_class` | `reltuples`, `relpages` | 基础行数与页数 |
| `pg_statistic` | `stainherit`, MCVs, `histogram_bounds`, `null_frac`, `distinct` | 单列分布 |
| `pg_statistic_ext` | n-distinct 多列、functional dependencies、MCV list | 多列扩展统计 |

代价模型 `src/backend/optimizer/path/costsize.c` 用这些统计量估算 rows：

```
rows = base_rows * selectivity_from_clauses(stats)
     * join_selectivity(stats, join_type)
```

`EXPLAIN (ANALYZE)` 把 estimated rows 与 actual rows 对比，偏差过大说明统计过期（需 ANALYZE）。

### 23.8 复杂度对比

| 算法 | 适用场景 | 时间复杂度 | 空间复杂度 |
|------|----------|-----------|-----------|
| Nested Loop | 外表小 + 内表带索引 | O(M * log N) | O(1) |
| Hash Join | 等值连接，无索引 | O(M + N) | O(M) |
| Merge Join | 已排序 / 范围连接 | O(M + N) | O(1) |
| DP Join Order | N ≤ 12 | O(3^N) | O(2^N) |
| GEQO | N > 12 | O(P * G) | O(P) |

P = 种群大小（默认 50），G = 代数（默认 200）。

### 23.9 本项目对应

`engineering/src/db/` 当前仅实现 catalog / heap / btree，**未实现** join 算法与查询优化器。可对比 PG 学习如何把动态规划、Volcano 拉取、统计信息三者结合，构造可扩展的关系型优化器。教学实现可参考 `learning/code-solutions/` 下的 LeetCode 排序/搜索类题目。