# openGauss Wiki（关系型数据库 / 华为衍生）

> 版本：本次参考 openGauss-server master（commit `33f3e1485`）。openGauss 由华为于 2020 年 6 月开源，基于 PG 9.2.4（部分较新特性 cherry-pick）。

## 一、项目定位

openGauss 是华为开源的**企业级关系型数据库**，源自 PostgreSQL 9.2.4，并在内核、事务、并发控制、SQL 兼容性、安全性、运维工具方面做了大量增强。它瞄准**金融、电信、政企、能源**等高端场景，强调**高可用（多节点 Paxos）、高性能（USTore 存储引擎、向量化执行）、高安全**。

- **痛点**：PG 社区版在企业级特性（如多节点强一致、AI 增强、行列混存）上需要大量定制；商业 Oracle 替代成本高且锁定。
- **典型用户**：华为内部（电信 BOSS、计费核心）、中国工商银行（**发起）、招商银行、上海银行、国家电网、中航信、中国移动；openGauss 社区中各大型行业用户。

## 二、发展历史与社区

- **起源**：2019 年华为宣布基于 PG 9.2.4 开发企业级关系型数据库 GaussDB 300；2020 年 6 月 30 日正式开源 openGauss 1.0.0。
- **里程碑**：
  - 2020 年 1.0.0：USTore 存储引擎、多核 NUMA 优化。
  - 2021 年 2.0.0：新增分布式特性、MOT 内存引擎、AI 调优 DBMind。
  - 2022 年 3.0.0：增量 checkpoint、TurboHeap、向量化执行。
  - 2023 年 5.0.0：行存 + 列存双模、并行查询、HyperLogLog。
  - 2024-2026 年 6.x：持续增强 HTAP、兼容性、AI 运维。
- **社区**：openGauss 社区由华为牵头，联合海量数据、云和恩墨、东方国信、中兴、PostgreSQL 中国社区等；社区技术委员会 + SIG；每年 openGauss Summit、OGTC、openGauss 训练营。
- **维护节奏**：每年约 2-3 个主版本，3 个月一次小版本，长期支持约 3 年。

## 三、许可与商业限制

- **许可证**：**木兰宽松许可证第 2 版（Mulan PSL v2）**，类 BSD/MIT。
  - 来源：`reference/relational/openGauss/License`
  - 关键条款：允许商用、修改、闭源衍生，无开源传染。
- **商业版**：华为云 GaussDB（for openGauss）提供云托管版、企业版（增加更多特性与高级支持）。
- **兼容协议**：与 PG 高度兼容，支持 `pg_dump/pg_restore/psql/libpq` 等工具。

## 四、业界主要使用方与案例

- **金融**：工商银行（核心系统替换试点）、中国邮政储蓄银行、招商银行、上海银行、人保财险、太平洋保险。
- **电信运营商**：中国移动（核心账务）、中国电信、中国联通。
- **政企**：国家电网、南方电网、中国航信、公安部、最高法、海关总署。
- **互联网**：美团（部分订单系统）、小米、知乎部分场景。
- **华为云**：GaussDB for openGauss 已在 50+ 行业落地，10+ 万实例。

## 五、一句话对比定位

> openGauss 是**PG 9.2.4 的华为超集**，在保持 PG 兼容性的同时，提供企业级高可用（Paxos 强同步）、高性能（USTore + 向量化执行）、AI 调优（DBMind）；比 PG 商业版（EDB）更适合国内政企信创需求。

## 六、整体架构

openGauss 沿用 PG 的**多进程架构**（fork 出每个 backend），但内核经过大量 C++ 化改造与增强。

```
┌──────────────────────────────────────────────────────────────────────┐
│                       客户端 (psql / JDBC / ODBC / libpq)             │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ TCP/Unix Socket + PostgreSQL Wire Protocol
┌─────────────────────────▼────────────────────────────────────────────┐
│                 Postmaster (src/gausskernel/process/postmaster.cpp)   │
│   - 监听端口 (默认 5432, 也支持多端口 DN/Coordinator)                 │
│   - fork() 出 backend / worker                                       │
│   - DN (Data Node) 主备、Coordinator 中间件                          │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ 共享内存 + 信号
┌─────────────────────────▼────────────────────────────────────────────┐
│                     单个 backend 进程                                  │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ 解析器 (parser/) + 分析器 + 重写器 + 优化器 (optimizer/)     │    │
│  │  + 执行器 (runtime/executor/ + vecexecutor/ 向量化执行器)     │    │
│  └─────────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ 访问方法 (storage/access/):                                   │    │
│  │   heap/ (堆表)  nbtree/ (B+Tree)  hash/  gist/ gin/  spgist/ │    │
│  │   cstore/ (列存)  ubtree/ (UBTree)  ustore/ (USTore 堆表)    │    │
│  │   htap/ (混合 HTAP)  datavec/ (向量)  mot/ (内存)            │    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │ Buffer Pool (storage/buffer/ — Clock-Sweep, 优化自 PG)        │    │
│  ├─────────────────────────────────────────────────────────────┤    │
│  │ 存储管理 (storage/): smgr/file/freespace/lmgr/redo/          │    │
│  │ 多版本并发 (storage/dss/ 分布式 segment storage)              │    │
│  │ 双写保护 (dorado_operation/)                                   │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ 文件 I/O
┌─────────────────────────▼────────────────────────────────────────────┐
│                磁盘 (base/pg_xact/pg_wal/undo/redo/...)               │
│   UStore 在每个页头存 undo record pointer（USTore 专属）              │
│   cstore/ 列存 (cu/cudesc/zm) 目录                                   │
└──────────────────────────────────────────────────────────────────────┘
```

### 6.1 多节点架构（分布式）

```
┌────────────────┐      ┌────────────────┐      ┌────────────────┐
│ Coordinator 1  │      │ Coordinator 2  │ ...  │ Coordinator N  │
│  (CN)          │      │                │      │                │
│  - SQL 路由     │      │                │      │                │
│  - 计划下发     │      │                │      │                │
└────┬───────────┘      └─────┬──────────┘      └────┬───────────┘
     │ GTM (全局事务管理器)                            │
     │ - 提供全局 xid/snapshot                         │
     └────────┬──────────────────────────────────────┘
              │           ┌────────────────┐
              ├─────────► │  Data Node 1   │ (主)
              │           │   ↕ 备机        │
              │           └────────────────┘
              │           ┌────────────────┐
              └─────────► │  Data Node 2   │
                          │   ↕ 备机        │
                          └────────────────┘
```

- **CN**：无状态，接收 SQL 并路由到 DN。
- **DN**：实际存储数据，可主备（同机房 1 主 2 备 / 跨机房 1 主 1 备 1 仲裁）。
- **GTM**（Global Transaction Manager）：提供全局事务 ID 与快照；GTM-Lite 替代品降低性能瓶颈。

源码入口：`src/gausskernel/storage/access/heap/heapam.cpp:2980`（`heap_insert`）、`src/gausskernel/storage/access/heap/heapam.cpp:4029`（`heap_multi_insert`）、`src/gausskernel/storage/buffer/bufmgr.cpp:1649`（`ReadBuffer`）。

## 七、存储引擎

openGauss 提供**多种可选存储引擎**，可在 CREATE TABLE 时指定。

### 7.1 引擎对比

| 引擎 | 数据组织 | 适用 | 源码 |
|------|----------|------|------|
| **Heap** | PG 原生堆表 | OLTP 兼容 PG 行为 | `storage/access/heap/` |
| **USTore** | Heap + Undo（In-page Undo） | 高并发 OLTP、减少膨胀 | `storage/access/ustore/` |
| **AStore** | Heap + Append-Only（追加） | 时序、日志 | `storage/access/ubtree/` |
| **UHeap** | 早期 UStore 实现 | 旧版本 | `storage/access/ubtree/` |
| **cStore** | 列存 + 压缩 | OLAP、分析 | `storage/access/cstore/` |
| **MOT** | 内存表（Memory-Optimized Table） | 极致读写延迟 | `storage/mot/` |
| **HBStore** | 行存 + 列存混合 | HTAP 场景 | `storage/access/hbstore/` |

### 7.2 UStore（核心增强）

USTore 是 openGauss **最核心**的存储引擎改进：

- **传统 PG 问题**：行被 UPDATE 时产生旧版本指针 + 死元组，需要 VACUUM 回收，导致**表膨胀（bloat）**与并发问题。
- **USTore 改进**：把 **Undo Log 存到页内（In-page Undo）**，而非系统表空间。
- **优势**：每页独立 undo，无 undo 表空间 I/O 竞争，无版本链跨页问题。
- **代价**：页空间利用率略降（每页预留 undo slot）。
- **代码**：`storage/access/ustore/ustore.cpp`、`ustore_page.cpp`、`ustore_undo.cpp`。

### 7.3 cStore 列存

- 数据按列存储，每个列存为 CU（Compression Unit）。
- 编码：`RLE / DELTA / ZSTD / LZ4 / DICTIONARY`。
- 适合：OLAP 查询、聚合、范围扫描。
- 配套列存索引：BTree、Hash、MinMax、表达式索引。

### 7.4 页（Page）结构

- 默认 8 KB，与 PG 兼容；可配置 4K / 8K / 16K / 32K / 64K。
- Heap 页布局与 PG 类似：`pd_lsn / pd_checksum / pd_lower / pd_upper / pd_special / line pointer / tuple data`。
- UStore 页增加：**`pd_undo_slot[]` 数组**（指向页内 undo 记录）。

### 7.5 行格式

- PG 兼容：xmin / xmax / t_infomask / t_data。
- UStore：额外 `xact_id_in_page`（标识页内事务链）。

## 八、索引机制

| 索引 | 适用 | 源码 |
|------|------|------|
| **B+Tree（nbtree）** | 通用 | `storage/access/nbtree/` |
| **UBTree** | UStore 专属（事务可见性增强） | `storage/access/ubtree/` |
| **Hash** | 等值查询 | `storage/access/hash/` |
| **GiST** | 几何、范围 | `storage/access/gist/` |
| **GIN** | 全文、数组、JSONB | `storage/access/gin/` |
| **SP-GiST** | 空间 | `storage/access/spgist/` |
| **UBTReePCR** | UStore + PCR | `storage/access/ubtreepcr/` |
| **cbtree** | 列存专用 B+Tree | `storage/access/cbtree/` |

USTore 的 UBTree 索引是**事务可见性与索引分离**的优化：插入事务和读取事务的 MVCC 验证都通过 undo 链。

## 九、查询与执行

### 9.1 优化器增强

- **RBO + CBO**（PG 9.2.4 基础上增强）。
- **增量基数估计**：根据 ANALYZE 数据动态修正。
- **参数化路径**：缓存 prepared statement 的执行计划。

### 9.2 执行器

- **标量执行器**（`runtime/executor/`）：PG 兼容。
- **向量化执行器**（`runtime/vecexecutor/`）：openGauss 独有，batch 模式执行（一次处理 N 行），加速 OLAP。
- **Codegen**（`runtime/codegen/`）：LLVM 即时编译表达式，性能与 PG 15+ JIT 相当。
- **OpFusion**（`runtime/opfusion/`）：将 simple query 整体编译，跳过拉取模型，单次函数调用完成。

### 9.3 并行执行

- 多线程并行：并行 seq scan / index scan / hash join / agg。
- 多节点并行（DN 间）：`nodeGroup` 配合 `PARALLEL HINT`。

### 9.4 SQL 兼容

- 兼容 PostgreSQL 9.2.4 SQL 语法 + PG 9.4+ 引入的部分特性（如 UPSERT via `MERGE INTO`）。
- 兼容 Oracle 部分语法（`ROWNUM`、`DECODE`、`DUAL`）。
- 兼容 MySQL 部分语法（`LIMIT/OFFSET` 兼容）。
- 兼容 Teradata 部分语法（`QUALIFY`）。

## 十、事务与一致性

### 10.1 隔离级别

继承 PG：
- READ COMMITTED（默认）
- REPEATABLE READ
- SERIALIZABLE（SSI）

USTore 还引入 **SSI 增强**：基于 undo 链跟踪读写冲突。

### 10.2 UStore MVCC 增强

- **传统 PG**：tuple header 携带 `t_xmin/t_xmax + xid`。
- **USTore**：tuple 仅携带 `xid`，Undo 记录在页内 slot，事务提交时丢弃 undo。
- **优势**：避免表膨胀，加快 vacuum。

### 10.3 锁机制

与 PG 类似：
- Spinlock / LWLock / 普通锁 / 谓词锁。
- 新增 **全局死锁检测**（DN 间跨节点锁冲突）。

### 10.4 分布式事务

- **GTM 模式**：2PC + GTM 全局事务 ID。
- **GTM-Lite**：每个 DN 自维护 xid，仅在跨节点查询时协调。
- **强同步（synchronous_commit=remote_write/remote_apply）**：保证 RPO=0。

## 十一、复制与高可用

### 11.1 主备复制

- 与 PG 类似：基于 WAL 流复制（`storage/replication/`）。
- 增加：**Quorum 复制协议**（1 主 2 备多数派写入成功）。

### 11.2 强同步复制

- 同步模式：`async / sync / quorum`。
- **强同步**（`most_available_sync / synchronous_standby_names`）：RPO=0。
- 跨机房仲裁（`arbitrator`）：同城 1 主 2 备 / 跨城 1 主 1 备 + 1 仲裁。

### 11.3 自动故障转移（CM）

- **CM（Cluster Manager）**：`Tools/cm`。
- 自动选主（基于 priority + LSN），自动 VIP 漂移，自动通知应用。
- RTO 通常 < 10 秒。

### 11.4 逻辑复制

- 基于 PG 9.4+ 的 logical decoding + 自定义发布订阅。

## 十二、性能与基准

- **TPC-C**：openGauss 1PC 4 节点 350 万 tpmC（华为官方公布）。
- **Sysbench**：单机 OLTP 读写混合 50k+ TPS。
- **典型调优**：
  - `shared_buffers = 系统内存 25%`
  - `max_process_memory = 系统内存 80%`
  - `enable_ustore = on`（USTore）
  - `enable_vector_engine = on`（向量化）
  - `num_cn / num_dn` 视并发调整

## 十三、生态与集成

### 13.1 客户端 SDK

- 兼容 PG libpq / psql / JDBC / ODBC。
- 兼容 PG 生态：psycopg2/3、pgx、PostgreSQL JDBC、SQLAlchemy、Django、Prisma、Drizzle、Hibernate。

### 13.2 工具链

- **gs_om**：openGauss 一键运维工具（替代 pg_ctl）。
- **gs_dump / gs_restore**：替代 pg_dump。
- **gs_upgradectl**：版本升级。
- **gsql**：openGauss 增强版 psql。
- **CM（Cluster Manager）**：`Tools/cm/`。
- **DataKit**：可视化运维平台。

### 13.3 监控

- **DBMind**：AI 调优引擎（异常检测、慢 SQL 根因分析、索引推荐、参数调优）。
- WDR（Workload Diagnosis Report）报告。
- 兼容 PG `pg_stat_*` 视图。

## 十四、源码学习路径

1. **入口**：`src/gausskernel/process/postmaster.cpp`（主进程）、`src/gausskernel/storage/buffer/bufmgr.cpp:1649`（`ReadBuffer`）。
2. **Heap AM**：`src/gausskernel/storage/access/heap/heapam.cpp:2980`（`heap_insert`）、`src/gausskernel/storage/access/heap/heapam.cpp:4029`（`heap_multi_insert`）。
3. **USTore**：`src/gausskernel/storage/access/ustore/ustore.cpp`（事务管理）、`ustore_page.cpp`、`ustore_undo.cpp`。
4. **优化器**：`src/gausskernel/optimizer/`（与 PG 类似但增强）。
5. **向量化执行**：`src/gausskernel/runtime/vecexecutor/`。
6. **Codegen**：`src/gausskernel/runtime/codegen/`。
7. **分布式**：`src/gausskernel/ddes/` + `src/gausskernel/storage/dss/`。
8. **复制**：`src/gausskernel/storage/replication/` + `Tools/cm/`。

源码规模：约 200 万行 C/C++ 代码 + 大量文档（`doc/`）和 contrib。

## 十五、风险与陷阱

### 15.1 PG 兼容性差异

- **PG 9.2.4 起步**：许多 PG 9.4+ 特性（如 `UPSERT`）早期不在；新版本逐步补齐但要查文档。
- **系统表布局**：部分 pg_catalog 表新增字段，但 PG 工具可能误判。
- **数据字典**：表名、列名以 `gs_` 或 `pg_` 前缀区分。

### 15.2 运维雷区

- **CM 强同步**：少数备机故障会导致主库 `synchronous_commit` 阻塞 → 应用超时；务必配仲裁。
- **GTM 性能瓶颈**：传统 GTM 在高并发下是单点；建议 GTM-Lite 或分布式 GTM。
- **多版本升级**：openGauss 主版本升级需重启，跨主版本需 dump/restore。

### 15.3 性能陷阱

- **Heap 引擎未迁移 UStore**：老 PG 兼容表膨胀严重；新表建议直接 UStore。
- **未启用向量化执行**：`enable_vector_engine = off` 退化到标量执行，OLAP 性能下降 5-10x。
- **MOT 内存表超限**：内存 OOM；监控 `mot_memory_detail`。

### 15.4 安全

- **默认不启用 SSL**：`ssl = off`；生产必须开启。
- **审计**：开启 `enable_audit` 否则无审计日志。

## 十六、学习路径

### 16.1 入门（1 周）

1. 安装 openGauss（Docker、rpm、源码编译）。
2. 用 `gsql` 执行 DDL/DML；兼容 PG 教程即可。
3. 学习 `gs_om` 启停集群、`gsql` 连接。

### 16.2 进阶（2-4 周）

1. 阅读官方文档：<https://opengauss.org/zh/docs/>
2. 学习 UStore 原理（与传统 Heap 区别）。
3. 实战主备 + CM + 强同步。

### 16.3 高级（3-6 月）

1. 深入源码：`storage/access/ustore/`、`runtime/vecexecutor/`。
2. 配置分布式（多 CN + 多 DN）。
3. AI 调优（DBMind）。
4. 性能调优：`WDR`、`gs_collector`。

### 16.4 推荐资源

- 官方文档：<https://opengauss.org/zh/docs/>
- openGauss 源码（GitHub 镜像）：<https://github.com/opengauss-mirror/openGauss-server>
- openGauss 社区：<https://opengauss.org/zh/community/>
- 培训认证：HCIE-GaussDB、HCIP-GaussDB
- 会议：openGauss Summit、openGauss 开发者大会

## 十七、可移植性与部署

- **平台**：Linux（CentOS、OpenEuler、麒麟、统信 UOS）、x86_64 / ARM / 鲲鹏。
- **国产化**：支持鲲鹏、海光、麒麟、统信、深度操作系统、龙芯。
- **容器化**：官方 Docker / 镜像；Kubernetes Operator（部分厂商提供）。
- **云端**：华为云 GaussDB、阿里云 PolarDB-PG（部分特性）、移动云 BC-Linux。
- **嵌入式**？否，openGauss 是 C/S 架构。

## 十八、变体与衍生项目

- **GaussDB（for openGauss）**：华为云商业版。
- **MogDB**：云和恩墨（OpenPie）推出的 openGauss 商业封装版。
- **openGauss-MOB**：海量数据（基于 openGauss 的企业版）。
- **CBase（海量）**：海量数据的列存衍生。
- **AgileDB**：东方国信的 openGauss 商业版。

## 十九、与本项目的关联

- 本仓库 `engineering/src/db/` 实现的是 PG 9.x 风格存储引擎；openGauss 在其上做了 UStore 改造。
- 学习 openGauss 的 UStore 设计可作为对本项目 `engineering/src/db/heapam` 后续扩展的参考方向。
- 华为云 GaussDB 是本项目可能的部署目标之一。

## 二十、参考资料

1. openGauss 官方文档：<https://opengauss.org/zh/docs/>
2. openGauss 源码 `src/gausskernel/storage/access/ustore/ustore.cpp`、`ustore_page.cpp`
3. openGauss 源码 `src/gausskernel/storage/access/heap/heapam.cpp:2980`、`src/gausskernel/storage/access/heap/heapam.cpp:4029`、`src/gausskernel/storage/buffer/bufmgr.cpp:1649`、`src/gausskernel/storage/access/ustore/ustore.cpp`、`src/gausskernel/runtime/vecexecutor/`
4. 木兰宽松许可证 v2：<https://license.coscl.org.cn/MulanPSL2>
5. openGauss SIG-Meetup 演讲录像（社区）：<https://opengauss.org/zh/community/events/>
6. HCIE-GaussDB 培训资料（华为官方）
7. PG 官方文档（openGauss 兼容参考）：<https://www.postgresql.org/docs/>
8. 《openGauss 数据库核心技术》：华为公司编著
9. DBMind AI 调优白皮书：<https://opengauss.org/zh/blog/2024/01/db-mind-ai-tuning.html>
10. openGauss Summit 历届演讲材料

## 二十一、UBTree 分裂定位算法与 UStore 页内 Undo 可见性

openGauss 用 **UBTree**（Ustore B-Tree）取代 PG 原生 B+Tree 索引，搭配 **UStore 堆表**在每页头存放 undo slot 指针。本节给出分裂点选择算法与基于 undo 链的可见性回溯流程，两者是 UBTree 与 UStore 的核心实现差异。

### 21.1 UBTree 分裂点选择算法

UBTree 与原生 nbtree 的关键差异在于：UBTree 在插入索引项时直接走页内 undo 协议，因此分裂点函数（`UBTreeFindsplitloc`）需要同时考虑数据项大小 + 高键 (high key) 复制成本。算法采用「`goodenough` 提早终止」策略，目标是在 `pagesize/16` 的不平衡容差内就返回 split point，而不是全表扫描全部 offset。

```c
// 真实实现：reference/relational/openGauss/src/gausskernel/storage/access/ubtree/ubtsplitloc.cpp:85
OffsetNumber UBTreeFindsplitloc(Relation rel, Buffer buf,
                                OffsetNumber newitemoff, Size newitemsz,
                                bool* newitemonleft)
{
    // 1. 计算左右页可用空间（包含 high key 复制成本）
    leftspace = rightspace =
        PageGetPageSize(page) - SizeOfPageHeaderData - MAXALIGN(sizeof(UBTPageOpaqueData));
    if (!P_RIGHTMOST(opaque)) {
        itemid = PageGetItemId(page, P_HIKEY);  // 右页继承 high key
        rightspace -= (int)(MAXALIGN(ItemIdGetLength(itemid)) + sizeof(ItemIdData));
    }
    olddataitemstotal = rightspace - (int)PageGetExactFreeSpace(page);

    // 2. fillfactor：右页 rightmost 使用默认 fillfactor；其它页填满 50%
    state.fillfactor = state.is_leaf ? RelationGetFillFactor(rel, BTREE_DEFAULT_FILLFACTOR)
                                     : BTREE_NONLEAF_FILLFACTOR;

    // 3. goodenough 阈值：pagesize/16 的不平衡容差
    goodenough = leftspace / 16;

    // 4. 扫描每个 offset，调用 UBTreeChecksplitloc 评估
    for (offnum = P_FIRSTDATAKEY(opaque); offnum <= maxoff; offnum = OffsetNumberNext(offnum)) {
        itemsz = MAXALIGN(ItemIdGetLength(itemid)) + sizeof(ItemIdData);
        if (offnum > newitemoff)
            UBTreeChecksplitloc(&state, offnum, true,  olddataitemstoleft, itemsz);
        else if (offnum < newitemoff)
            UBTreeChecksplitloc(&state, offnum, false, olddataitemstoleft, itemsz);
        else {  // 新项恰好在这个 offset：两个分支都试
            UBTreeChecksplitloc(&state, offnum, true,  olddataitemstoleft, itemsz);
            UBTreeChecksplitloc(&state, offnum, false, olddataitemstoleft, itemsz);
        }
        if (state.have_split && state.best_delta <= goodenough) {
            goodenoughfound = true;
            break;   // 提早终止：当前方案已够好
        }
        olddataitemstoleft += itemsz;
    }
}
```

`UBTreeChecksplitloc` 计算不平衡 delta 的核心公式（右页继承 high key）：

```c
// 真实实现：reference/relational/openGauss/src/gausskernel/storage/access/ubtree/ubtsplitloc.cpp:215
static void UBTreeChecksplitloc(FindSplitData* state, OffsetNumber firstoldonright,
                                bool newitemonleft, int olddataitemstoleft,
                                Size firstoldonrightsz)
{
    // 左/右页可用 = 总空间 - 已占用数据项空间
    leftfree  = state->leftspace  - olddataitemstoleft;
    rightfree = state->rightspace - (state->olddataitemstotal - olddataitemstoleft);
    leftfree -= firstrightitemsz;   // firstright 也作为左页 high key 计数

    if (newitemonleft) leftfree  -= (int)state->newitemsz;
    else               rightfree -= (int)state->newitemsz;

    // 非叶页可丢 firstright 的 key 数据（保留 TID）
    if (!state->is_leaf) {
        int indexTupleDataSize = (MAXALIGN(sizeof(IndexTupleData)) - TXNINFOSIZE);
        rightfree += (int)firstrightitemsz - (int)(indexTupleDataSize + sizeof(ItemIdData));
    }

    // delta 计算：右页 rightmost 时按 fillfactor 加权，否则按左右均分
    if (leftfree >= 0 && rightfree >= 0) {
        if (state->is_rightmost)
            delta = (state->fillfactor * leftfree) - ((100.0 - state->fillfactor) * rightfree);
        else
            delta = leftfree - rightfree;   // 目标 delta = 0
        if (delta < 0) delta = -delta;
        if (!state->have_split || delta < state->best_delta) {
            state->have_split = true;
            state->best_delta = delta;
            state->firstright = firstoldonright;
        }
    }
}
```

**算法目的**：在「插入新项后左右两页剩余空间尽量相等」的约束下，定位最优 split offset；当扫描到 `best_delta ≤ pagesize/16` 时立即停止，保证平均扫描次数 ≈ `maxoff/2`，避免逐 offset 评估。最坏情况全表扫描 `maxoff` 次（当 fillfactor 设到极端值导致一直找不到平衡点时）。

### 21.2 UStore 页内 Undo 链与可见性回溯

UStore 不再把旧版本指针塞进 heap tuple header，而是在每个 page header 维护 `pd_undo_slot[]` 数组；tuple 仅携带 `xid`。事务提交时**丢弃** undo record（而非像 Heap 那样的 VACUUM 回收旧 heap tuple）。可见性判断通过回溯 undo 链完成。

简化的可见性回溯伪代码（基于 `knl_uvisibility.cpp` 真实函数族）：

```c
// 参考：reference/relational/openGauss/src/gausskernel/storage/access/ustore/knl_uvisibility.cpp
// 关键函数族：UHeapTupleSatisfiesMVCC / UHeapTupleVisible / UHeapFetchTupleVisibility

bool UHeapTupleVisible(Relation rel, UHeapTuple tup, Snapshot snap) {
    xid  = tup->xid_in_page;               // tuple 当前 xid
    undo = read_undo_slot(tup->pd_undo_ptr); // 从页内 undo slot 拿 chain 头

    // 1. 在 active xip 中：tuple 未提交，不可见
    if (TransactionIdInProgress(xid))
        return false;
    // 2. xid < snap.xmax 且 xid 不在 snap.xip：对当前快照可见
    if (xid < snap->xmax && !TransactionIdInArray(xid, snap->xip))
        return true;
    // 3. 否则沿 undo 链回溯，找一个满足 (1)/(2) 的版本
    while (undo != NULL) {
        UUndoRecord *rec = (UUndoRecord *)PageGetItem(page, undo);
        if (rec->xid == xid && rec->u_type == UNDO_INSERT)  // 该 undo 是 INSERT，原始 tuple 不存在
            return false;
        if (TransactionIdDidCommit(xid))                     // 提交过
            return true;
        undo = rec->next_urec;   // 沿 next_urec 链回溯
    }
    return false;
}
```

**关键不变量**：

- 每页的 undo slot 是**有限环形**：当 slot 耗尽，undo launcher 后台线程 `knl_undolauncher.cpp` 触发「驱逐 + 回收」，把已提交事务的 undo record 丢弃、仍活跃的 undo 记录 move 到 undo 表空间。
- 撤销操作（UPDATE/DELETE）只往 `pd_undo_slot[]` 写 undo record 并修改 `tuple.xid`，**不复制原 heap tuple**；VACUUM Lazy（`knl_uvacuumlazy.cpp`）据此可以快速回收。

**复杂度对比**：

| 操作 | Heap（PG 原生） | UStore |
|------|----------------|--------|
| 单 tuple UPDATE | 写新 tuple + 旧 tuple header `xmax` + 设置 `t_infomask.heaptuple` 死亡标记 | 写新 tuple + 在 `pd_undo_slot` 分配 undo record + 修改 tuple `xid`，**不复制原 tuple** |
| 可见性判定 | `HeapTupleSatisfiesMVCC` 直接读 tuple header | `UHeapTupleVisible` 沿 undo 链回溯（最坏 O(undo_chain_len)） |
| 空间回收 | 全表 VACUUM（高 I/O） | 原地 undo slot 重用（无 I/O） |
| 表膨胀 | 是（VACUUM 前） | 否（提交即丢弃 undo） |

### 21.3 本项目对应

- 本项目 `engineering/src/db/btreeam.c` 实现的是 PG 风格朴素 halving（无 `goodenough` 早停、无 `is_rightmost` fillfactor 区分）；可参考本节 21.1 引入 `goodenough = leftspace / 16` 优化分裂点选取。
- 本项目 `engineering/src/db/heapam.c` 无 MVCC（更新采用「删除 + 插入」两阶段），未实现页内 undo 协议；若后续要做 MVCC 改造，可参照本节 21.2 的 in-page undo 设计。
- 真实源码路径：
  - `reference/relational/openGauss/src/gausskernel/storage/access/ubtree/ubtsplitloc.cpp:85`（`UBTreeFindsplitloc`）
  - `reference/relational/openGauss/src/gausskernel/storage/access/ubtree/ubtsplitloc.cpp:215`（`UBTreeChecksplitloc`）
  - `reference/relational/openGauss/src/gausskernel/storage/access/ustore/knl_uvisibility.cpp`（可见性判断主文件，2620 行）
  - `reference/relational/openGauss/src/gausskernel/storage/access/ustore/knl_upage.cpp:1040`（页内 undo slot 管理）
  - `reference/relational/openGauss/src/gausskernel/storage/access/ustore/knl_uvacuumlazy.cpp`（惰性 vacuum）

## 二十二、AI 优化器：基于 Chow-Liu 树的贝叶斯网络选择性估计（ABO）

DBMind 子组件 **ABO（AI-Based Optimizer）** 是 openGauss 的 AI 增强优化器，对 multi-column 选择性估计采用 **Bayesian Network + Chow-Liu 结构学习**。在 PG 9.2.4 原生 optimizer 之上引入 `optimizer/aioptimizer/aianalyze.cpp` 与 `aiselectivity.cpp`，并行于传统 `pg_statistic` 的多列直方图。

### 22.1 Chow-Liu 树结构学习算法

Chow-Liu 算法的核心思想：通过两两互信息 (Mutual Information) 构造最大权生成树 (Maximum Weight Spanning Tree, MWST)，用 `likelihood[i][j] = P(xi,xj) * log(P(xi,xj) / (P(xi)*P(xj)))` 量化属性对之间的相关性，再用类似 Prim 算法逐节点扩展。

```c
// 真实实现：reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aianalyze.cpp:118
//
// [AI Stats] Chow-Liu 算法：最大权生成树 (Maximum Weight Spanning Tree)
// 通过两两 mutual information 估计相关性，用 MWST 构造最贴近联合分布 P(X1,...,Xn)
// 的树形 Bayesian Network
//
// 关键步骤：
//   1. 单列频率统计 (hash 单列频次)
//   2. 两列共现频率统计 (hash 两列组合)
//   3. 计算 likelihood[i][j] = Σ prob(xi,xj) * log(prob(xi,xj) / (prob(xi)*prob(xj)))
//   4. 类似 Prim 算法的 MWST 构造

static void find_maximal_edge(unsigned int nattrs, uint128 visited, uint128 currentindicate,
                              double *likelihood, double *maximal,
                              int *maximal_srcid, int *maximal_destid)
{
    // visited: 已加入树的属性集合（用 uint128 bitmap 标记）
    for (unsigned int k = 0; k < nattrs; k++) {
        for (unsigned int j = 0; j < nattrs; j++) {
            double likeli = likelihood[k * nattrs + j];
            if (((visited & (currentindicate << k)) > 0)   // k 已访问
                && ((visited & (currentindicate << j)) == 0) // j 未访问
                && (likeli > *maximal)) {
                *maximal = likeli;
                *maximal_srcid = k;
                *maximal_destid = j;
            }
        }
    }
}

// 上层 structure_learning 主循环 (基于 aianalyze.cpp:162)
// 1) 扫描表，建立 single_col_freqs[i] 哈希表统计每列频次
// 2) 对每对 (left, right) 建立 multi_col_freqs 哈希表统计共现频次
//    prob_x1x2 = entry->cnt / sample_size
//    likelihood[left][right] += (prob_x1x2 * log(prob_x1x2 / (prob_x1 * prob_x2)))
// 3) 反复调用 find_maximal_edge，类似 Prim 把 nattrs-1 条边加入 MWST
//    StringInfoData str -> 序列化 "src0,dst0,src1,dst1,..."
```

**复杂度分析**：

- 两两互信息阶段：`(n_attrs * (n_attrs - 1) / 2)` 对属性，每对扫描全表一遍 → **O(n_attrs² × n_rows × log sample)** (哈希表更新常数)。
- MWST 构造阶段：`n_attrs - 1` 次迭代，每次 `find_maximal_edge` 扫描 `n_attrs²` 矩阵 → **O(n_attrs³)**。
- 总训练成本：**O(n_attrs² × n_rows + n_attrs³)**，其中第一项主导；适合小表/小样本的 off-line 模型训练。

### 22.2 选择性预测与模型接口

`aiselectivity.cpp` 提供运行时预测：

```c
// 真实实现：reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aiselectivity.cpp:57
double StatsModel::predict_model(Datum *values, Oid *typids, bool *isnulls,
                                 int ncolumns, bool *iseliminate)
{
    if (bayesNetModel == nullptr) {
        ereport(INFO, (errmsg("[AI Stats] must do model train before predict\n")));
        return -1;
    }
    // 把 datum 喂给训练好的 BayesNet，输出联合概率（选择性）
    Datum dt = model_predict(bayesianNetModel, values, isnulls, typids, ncolumns);
    return DatumGetFloat8(dt);
}
```

训练入口与模型生命周期由 GUC `enable_ai_stats` (默认 `on`) 控制；模型名 `gs_model` 元数据存于系统表。运行时通过 `search_model_from_cache` 命中 `g_instance.abo_cxt.models` 哈希表：

```c
// 真实实现：reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aiselectivity.cpp:86
StatsModel *search_model_from_cache(const char *model_name)
{
    double current_time = GetCurrentTimestamp();
    bool found = false;
    AboModelCacheEntry *cache = (AboModelCacheEntry *)
        hash_search(g_instance.abo_cxt.models, model_name, HASH_FIND, &found);
    if (found) {
        cache->recent_used_time = current_time;   // LRU 维护
        return cache->model;
    }
    return NULL;
}
```

**与 PG 多列直方图对比**：

| 维度 | PG 9.2.4 `pg_statistic_ext`（MCV list） | openGauss ABO（Bayesian Network） |
|------|----------------------------------------|-----------------------------------|
| 训练样本 | 抽样（`ANALYZE`） | 抽样（`ANALYZE`/`gs_collect`） |
| 多列相关性 | 仅枚举低基数组合 | 任意 n 列联合分布近似 |
| 时间复杂度 | O(n_rows) 单次扫描 | O(n_attrs² × n_rows + n_attrs³) |
| 空间复杂度 | O(n_mcv) 枚举表 | O(n_edges) = O(n_attrs) 树结构 |
| 适合场景 | 低基数枚举列 | 中高基数任意列 |
| 失效模式 | MCV 集合外 fallback 差 | 树结构假设下联合 P |
| 缓存 | 进程 local | 全局 `g_instance.abo_cxt.models` 哈希表 + LRU |

### 22.3 本项目对应

- 本项目 `engineering/src/db/sql/sql_planner.c` 仅实现 RBO + 简单 CBO（无多列相关性建模）；ABO 提供一种「off-line 训练 + on-line 预测」框架参考。
- 真实源码路径：
  - `reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aianalyze.cpp:118`（`find_maximal_edge`，Chow-Liu MWST 主循环）
  - `reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aianalyze.cpp:162`（`structure_learning`，157 行主流程）
  - `reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aiselectivity.cpp:57`（`predict_model`，运行时预测）
  - `reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aiselectivity.cpp:86`（`search_model_from_cache`，LRU 缓存命中）
  - `reference/relational/openGauss/src/gausskernel/optimizer/aioptimizer/aianalyze.cpp:38`（`StatsModel::create_model`，模型训练入口）

## 二十三、NUMA-aware 线程池与内存分配

openGauss 提供 NUMA-aware 调度：每个 thread group 绑定到特定 NUMA 节点，使**线程访问本地 memory 的概率最大化**。此能力在 Oracle 商业版、PG 9.2.4 中均不内置，是 openGauss 高并发 OLTP 的关键优化之一。

### 23.1 CPU/NUMA 拓扑发现与绑定策略

`threadpool_controler.cpp` 在启动时通过 `GetCpuAndNumaNum` 系统调用获取 `totalCpuNum` 和 `totalNumaNum`，并支持 4 种绑定模式（`bindType` 枚举）：

```c
// 参考：reference/relational/openGauss/src/gausskernel/process/threadpool/threadpool_controler.cpp
//
// 4 种 bindType (来自 m_cpuInfo.bindType 赋值逻辑):
//   NO_CPU_BIND   = 332 行：不绑定，由 OS 调度
//   ALL_CPU_BIND  = 356 行：所有 worker 共享 CPU 池（无 NUMA 亲和）
//   CPU_BIND      = 359 行：按 GUC cpu_bind 列表绑定 CPU（无 NUMA 分组）
//   NODE_BIND     = 380 行：按 node_bind NUMA 节点绑定（粗粒度 NUMA）
//   NUMA_BIND     = 384 行：按 numabind 精确绑定 CPU 到 NUMA 节点（细粒度 NUMA）

if (pg_strcasecmp(psave, "nodestrict") == 0)      // NODE_BIND
    m_cpuInfo.bindType = NODE_BIND;
else if (pg_strcasecmp(psave, "numastrict") == 0)  // NUMA_BIND
    m_cpuInfo.bindType = NUMA_BIND;
```

NUMA-bind 模式下 worker 线程 CPU 平均分配公式（线程数 → 每个 NUMA 节点负载）：

```c
// 参考：reference/relational/openGauss/src/gausskernel/process/threadpool/threadpool_controler.cpp:129-150
//
// 每个 NUMA 节点分配的 thread 数（比例 = 节点 CPU 数 / 总活动 CPU 数）：
//
//   threads_for_numa[numaId]    = m_threadNum    * (cpuArrSize[numaId] / activeCpuNum)
//   maxPoolSize_for_numa[numaId] = m_maxPoolSize * (cpuArrSize[numaId] / activeCpuNum)
//
// 其中 cpuArrSize[numaId] 是 numaId 节点的逻辑 CPU 数

for (int numaId = 0; numaId < m_cpuInfo.totalNumaNum; numaId++) {
    Assert(numaId < m_cpuInfo.totalNumaNum);
    int cpuNum = m_cpuInfo.cpuArrSize[numaId];
    int* cpuArr = m_cpuInfo.cpuArr[numaId];

    // 启动该 NUMA 节点的 thread group（每个 NUMA 节点一个 group）
    // 每个 group 跑 threads_for_numa[numaId] 个活跃线程，
    // 最大线程池 maxPoolSize_for_numa[numaId]
}
```

`activeCpuNum` 是排除被剔除的 CPU（`isMcsCpuArr`）后的活跃 CPU 总数，体现**资源调度感知**。

### 23.2 NUMA-aware 内存分配

`knl_instance.cpp` 在 NUMA-aware 模式下为每个线程组预分配 `numa_cxt->maxLength * sizeof(NumaMemAllocInfo)` 的内存上下文数组（即 `MEMORY_CONTEXT_NUMA`）：

```c
// 参考：reference/relational/openGauss/src/gausskernel/process/threadpool/knl_instance.cpp:821
//
// InstanceNuma 内存上下文初始化：
// 每个 numa_cxt 槽位追踪一个 (numaNodeId, size) 组合，记录 NUMA 节点上的分配请求
numa_cxt->maxLength = INIT_NUMA_ALLOC_COUNT;
INSTANCE_GET_MEM_CXT_GROUP(MEMORY_CONTEXT_NUMA,
                           numa_cxt->maxLength * sizeof(NumaMemAllocInfo));
```

**核心理念**：worker thread 在本 NUMA 节点上的内存池分配 → 数据结构（hash table、buffer pool slot、plan cache）**最终落到本节点局部内存** → 后续跨核访问该结构时跨 NUMA 链路代价（`qpi/upi`）被最小化。

### 23.3 复杂度与作用

| 维度 | 公式/估计 |
|------|-----------|
| 拓扑发现 | O(N) = O(totalCpuNum + totalNumaNum)，一次性 |
| 线程分配 | O(NUMA_nodes) group，每个 group 内 threads 数按 CPU 比例分配 |
| 内存分配 | O(maxLength * NUMA_nodes) 预分配（`maxLength = INIT_NUMA_ALLOC_COUNT`） |
| NUMA 命中率 | worker 完全本地 → 命中率 100%；混合 worker（CPU_BIND）→ 命中率 ≈ activeCpuNum/totalCpuNum |
| 加速比 | 单 NUMA 节点（鲲鹏 920 / EPYC）相对不绑定的 OLTP 测试通常提升 **15-30%**（同负载下 pinned 优于 OS 调度） |

### 23.4 本项目对应

- 本项目 `engineering/src/db/` 多 backend 进程**未实现** NUMA 亲和：进程由 OS 调度器调度到任意 NUMA 节点；高并发场景下跨 NUMA 内存访问成为瓶颈。
- 真实源码路径：
  - `reference/relational/openGauss/src/gausskernel/process/threadpool/threadpool_controler.cpp:188`（`GetCpuAndNumaNum`，CPU/NUMA 拓扑发现）
  - `reference/relational/openGauss/src/gausskernel/process/threadpool/threadpool_controler.cpp:332-386`（`bindType` 4 种绑定模式分发）
  - `reference/relational/openGauss/src/gausskernel/process/threadpool/threadpool_controler.cpp:545-547`（每 NUMA 节点的 `cpuArr[i]` 数组预分配）
  - `reference/relational/openGauss/src/gausskernel/process/threadpool/knl_instance.cpp:821`（`INSTANCE_GET_MEM_CXT_GROUP(MEMORY_CONTEXT_NUMA, ...)`，NUMA-aware 内存上下文）
  - `reference/relational/openGauss/src/gausskernel/cbb/communication/comm_proxy/comm_core.cpp:27`（`__USE_NUMA` 宏门控的通信绑定 NUMA）

---

> **学习闭环**：本节三个补充章节与 WD-1/2/3（postgres、mysql、sqlite3 深化）保持 **二十一/二十二/二十三** 的统一节号体系；每个新章节对应 openGauss 区别于 PG 9.2.4 基础版的核心算法创新 (UBTree 分裂 + UStore undo / AI 优化器 / NUMA-aware)。