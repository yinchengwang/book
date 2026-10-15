# Firebird Wiki（关系型数据库 / 嵌入式 + C/S）

> 版本：本次参考 FirebirdSQL/firebird master（commit `f2ba590d2`），对应 Firebird 5.x 系列。

## 一、项目定位

Firebird 是源自 **Borland InterBase**（2000 年开源）的**成熟关系型数据库系统**，强调**ANSI SQL 标准兼容性、强大的存储过程语言（PSQL）、多模式架构（嵌入式 / C/S）**，在欧洲、俄罗斯、印度等地有深厚应用基础。

- **痛点**：InterBase 商业版授权昂贵；开发者需要一个成熟稳定、SQL 标准合规、嵌入式友好的 RDBMS。
- **典型用户**：欧洲企业 ERP、CRM 系统（俄罗斯 1C 集成最广）、医疗信息系统、零售 POS、政府项目；中国部分制造、医疗系统。

## 二、发展历史与社区

- **起源**：1981 年 Groton Datatech 数据库 → 1986 年 InterBase（Ashton-Tate）→ 1991 年 Borland → 2000 年 Borland 关闭 InterBase 部门，开源源代码 → 2000 年 7 月 IBPhoenix 接管；2004 年 Firebird 1.5；2006 年 Firebird 2.0。
- **里程碑**：
  - 2004 年 1.5：完善 SQL 标准、PSQL 改进。
  - 2006 年 2.0：派生表、COALESCE、SAVEPOINT。
  - 2009 年 2.1：CTE、窗口函数（早期）。
  - 2014 年 2.5：VARCHAR(8192) 长度扩展、PSQL 异常改进。
  - 2016 年 3.0：自增 ID via `IDENTITY`、原生 boolean、UUID 函数、Bigint identity、批量优化。
  - 2020 年 4.0：核心引擎全面重构（OAT 引擎替换旧 ODS）、多进程架构、加密连接。
  - 2022-2024 年 5.0：性能提升、时序表、DECFLOAT、UPSERT（`MERGE`）、窗口函数增强。
- **社区**：Firebird 基金会 + IBPhoenix + 全球 SIG；俄语社区最活跃（1C 企业版默认集成 Firebird）。
- **维护节奏**：每 1-2 年一个主版本。

## 三、许可与商业限制

- **许可证**：**Initial Developer's Public License (IDPL) v1.0**（类 Mozilla MPL）。
  - 来源：`reference/relational/firebird/doc/license/IDPL.txt`
  - 关键条款：允许商用、修改、闭源衍生（与 MPL 类似），文件级 copyleft。
- **商业支持**：IBSurgeon、MER Systems（俄）等提供商业支持与衍生品（如 RedSoft RedDatabase）。
- **无 GPL 传染**：可链接到闭源应用而不强制开源整个项目。

## 四、业界主要使用方与案例

- **俄罗斯 / 东欧**：1C:Enterprise 企业级 ERP 平台默认使用 Firebird（俄罗斯 80%+ 中小企业 ERP）。
- **欧洲**：Hospices Civils de Lyon（医院系统）、法国部分政府项目、意大利市政部门。
- **医疗**：Atmosoft（德国医疗）、大量诊所系统。
- **零售**：DigiPoS POS、英国 Boots 部分门店系统。
- **中国**：部分国产化 ERP、CRM、计量检测系统。
- **云**：Linux 上可独立部署，暂无大型云厂商托管版本。

## 五、一句话对比定位

> Firebird 是**单文件数据库**与**C/S 数据库**的混合体，介于 SQLite 与 PostgreSQL 之间；以**PSQL 强大的存储过程**、**多模式部署**、**无 GPL 传染**为特色。

## 六、整体架构

Firebird 是**单进程多线程**架构（**SuperServer** 模式），每个数据库连接由一个线程处理（不像 PG 的多进程）；同时支持**嵌入式**模式（直接 link 进应用进程）。

```
┌──────────────────────────────────────────────────────────────────────┐
│                  客户端进程                                            │
│           (isql / Jaybird JDBC / .NET Provider / Python FDB)         │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ Firebird Wire Protocol (XDR + 端口 3050)
┌─────────────────────────▼────────────────────────────────────────────┐
│   fbserver (SuperServer) / fb_inet_server (Classic)                   │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ Y-Valve (src/yvalve/) - 入口层                                    │    │
│  │   解析 attach/create/transaction/statement/execute 等请求        │    │
│  └──────────────────────────┬───────────────────────────────────┘    │
│                             │                                          │
│  ┌──────────────────────────▼───────────────────────────────────┐    │
│  │ DSQL (src/dsql/) - SQL 解析与执行引擎                            │    │
│  │   Parser → Algebra tree → Optimizer → Statement                │    │
│  └──────────────────────────┬───────────────────────────────────┘    │
│                             │                                          │
│  ┌──────────────────────────▼───────────────────────────────────┐    │
│  │ JRD (src/jrd/) - Database Engine                              │    │
│  │   - Relation Manager (rel.cpp/Attachment.cpp)                 │    │
│  │   - B+Tree Index (idx.cpp)                                     │    │
│  │   - Record (req.cpp/rec.cpp)                                  │    │
│  │   - Transaction (src/jrd/tra.cpp:1)                            │    │
│  │   - Lock (src/lock/lock.cpp:1)                                 │    │
│  │   - Cache Manager (src/jrd/CacheManager.cpp)                   │    │
│  └──────────────────────────┬───────────────────────────────────┘    │
│                             │                                          │
│  ┌──────────────────────────▼───────────────────────────────────┐    │
│  │ Page Cache (src/jrd/pag.cpp) - 自管理 Buffer Pool               │    │
│  └──────────────────────────┬───────────────────────────────────┘    │
│                             │                                          │
│  ┌──────────────────────────▼───────────────────────────────────┐    │
│  │ Disk I/O (src/jrd/dba.cpp, pag.cpp)                            │    │
│  └─────────────────────────────────────────────────────────────┘    │
└─────────────────────────┬────────────────────────────────────────────┘
                          │
┌─────────────────────────▼────────────────────────────────────────────┐
│               单文件 .fdb 数据库 + 备份文件 .fbk                        │
└──────────────────────────────────────────────────────────────────────┘
```

### 6.1 进程模型（关键差异）

- **SuperServer**：单进程多线程（v1.0+）。
- **Classic**：多进程（v1.0+），每连接 fork；适合 SMP。
- **Embedded**：直接 link `libfbembed.so`，无独立服务进程（SQLite 风格）。
- **SuperClassic**：v2.5+ 引入，单进程多线程 + 每连接独立 page cache，避免共享缓存争用。

### 6.2 Wire Protocol

- 自有协议（XDR-encoded），端口 3050；非 PostgreSQL/MySQL 兼容。
- 客户端：Jaybird（JDBC）、IBPP（C++）、FDB（Python）。

源码入口：
- `src/jrd/jrd.cpp:1`（核心引擎）。
- `src/dsql/dsql.cpp:1`（DSQL 引擎）。
- `src/jrd/tra.cpp:1`（事务）。
- `src/lock/lock.cpp:1`（锁）。

## 七、存储引擎

### 7.1 ODS（On-Disk Structure）

- 单一 `.fdb` 文件存储所有数据。
- ODS 版本与 Firebird Server 版本强相关；v4.0 引入新 ODS13 替代旧 ODS11/12。

### 7.2 页（Page）结构

默认 4 KB，可配置 1K / 2K / 4K / 8K / 16K：

```
┌──────────────────────────────────────────────────────────────┐
│ Page Header (variable length based on page type)               │
│   - pag_type (data / pointer / blob / index)                   │
│   - pag_seqno, pag_offset, pag_flags                          │
├──────────────────────────────────────────────────────────────┤
│ Page-specific content:                                        │
│   - Data page: record header + records (variable length)      │
│   - Pointer page: pointer to other pages (B+Tree internal)     │
│   - Index page: B+Tree node                                    │
│   - Blob page: large object data                              │
└──────────────────────────────────────────────────────────────┘
```

### 7.3 数据页（Data Page）

- 单页可存多条变长记录（FRBR：Formatless Record Block）。
- 记录格式：FRBR header（`rhd_operation` + `rhd_transaction`） + 字段数据。
- 版本：每条记录保留多个版本（基于 TIP 事务状态判断）。

### 7.4 事务实现

源码：`src/jrd/tra.cpp`。

- **快照事务（Snapshot）**：v2.0+ 引入，事务开始时建立快照；默认（`isc_tpb_concurrency`）。
- **读一致（Read Consistent）**：读请求时建立快照。
- **脏读（Read Dirty）**：看最新版本，未提交数据。
- **Transaction Inventory Page（TIP）**：每页含 4 bit 事务状态（committed / rolled back / active），事务提交时更新。

### 7.5 引擎版本（OAT）

- **旧 OAT（v3.x 及之前）**：老引擎，与 ODS11/12。
- **新 OAT（v4.0+）**：完全重写，支持 64 位 page_id、128 位事务 ID、多线程优化、TIP 并发扫描。

## 八、索引机制

### 8.1 B+Tree 索引

- 源码：`src/jrd/idx.cpp`。
- 单文件索引，存储在 `.fdb` 内（同数据）。
- 复合索引、表达式索引、部分索引（v4.0+）、降序索引。

### 8.2 唯一约束

- 自动创建唯一索引（PK、UNIQUE 约束）。

### 8.3 计算列索引

- v3.0+：`CREATE INDEX ON t (computed_col)`。

### 8.4 倒排索引（ODS13 / v4.0+）

- v4.0+ 实验性的全文索引。

## 九、查询与执行

### 9.1 解析

- **DSQL Parser**：`src/dsql/Parser.cpp` / `parse.y`。
- 输出：抽象语法树 → Algebra 树。

### 9.2 优化器

- **RBO**：规则化（谓词下推、子查询展开）。
- **CBO**：v3.0+ 引入，基于统计直方图（`FIB$STATISTICS`）。
- v5.0+ 增强：考虑索引合取、自动创建临时索引。

### 9.3 执行

- 迭代器模型 + BLR（load request block，二进制 IR）。
- **Stored Procedure / Trigger**：编译成 BLR，存储在数据库内。
- **PSQL**（Procedural SQL）：完整的过程语言（IF/WHILE/FOR SELECT/EXCEPTION/CURSOR），运行时编译成 BLR。

### 9.4 EXPLAIN

- `EXPLAIN PLAN`：输出执行计划（TEXT 格式）。
- `MON$EXPLAINED_PLANS` 视图（v4.0+）：最近执行计划。

## 十、事务与一致性

### 10.1 隔离级别

通过 TPB（Transaction Parameter Block）控制：

| 隔离级别 | TPB 选项 | 说明 |
|----------|----------|------|
| **Snapshot** | `isc_tpb_concurrency` | 事务快照，跨语句一致 |
| **Snapshot Table Stability** | `isc_tpb_consistency` | 表级稳定性（最严格） |
| **Read Committed（v3.0+）** | `isc_tpb_read_committed` + `isc_tpb_rec_version` / `isc_tpb_no_rec_version` | 看最新已提交或当前事务提交前快照 |
| **Read Dirty** | `isc_tpb_read_committed` + dirty | 看未提交数据 |

### 10.2 MVCC

- **基于页面版本链**：记录保留旧版本，新版本指向旧版本。
- **TIP（Transaction Inventory Pages）**：每数据库维护，标识事务状态。
- **垃圾回收**：Sweep（后台线程，每 20000 事务触发）或后台 GC 进程。

### 10.3 锁机制

源码：`src/lock/lock.cpp`。

- **行级锁**：记录头 `rhd_transaction` 标记持有事务。
- **表级锁**：RESERVING 子句（`RESERVING table FOR PROTECTED/READ/SHARED WRITE`）。
- **死锁检测**：自动检测 + 报告。

## 十一、复制与高可用

Firebird 历史上**缺乏原生主从复制**，需要第三方方案：

### 11.1 内置特性

- **nBackup**：`nbackup -L` 在线全量 + 增量物理备份（增量记录 `delta.fbk`）。

### 11.2 第三方复制

- **Caron Replication**：Payam Payesteh 出品（C 库）的开源逻辑复制，支持双向、发布订阅。
- **Replicator / FBReplicator**：基于 nBackup 的物理复制。
- **IBSurgeon HQbird Enterprise Edition**：商业版同步复制。

### 11.3 高可用方案

- **Firebird Replication Plugin**（v3.0+ 引入但社区版仅基础）：异步复制。
- **Pacemaker + Corosync + DRBD**：Linux HA 标准组合。
- **HBX Cluster / FBX Cluster**：商业版集群方案。
- **Async Replication**：基于 nBackup 增量备份到从机再恢复。

### 11.4 备份恢复

- `gbak`（`src/utilities/gbak.cpp`）：逻辑备份/恢复。
- `nbackup`：物理热备。

## 十二、性能与基准

- **TPC-C**：单机 4 节点 100k+ tpmC（4.0+）。
- **Sysbench**：单机 10-30k TPS。
- **典型调优**：
  - `DefaultDbCachePages = 50K pages`（200 MB，默认偏小）。
  - `TempCachePages = 16K`。
  - `FileSystemCacheThreshold = 65536`。
  - `MaxUserTraceLogSize = 10000`。
  - 数据库页大小：`Page Size = 16384`（大数据集）。

## 十三、生态与集成

### 13.1 客户端 SDK

- **C/C++**：IBPP（社区 C++）、Firebird C API（`fbclient`）。
- **Java**：Jaybird（JDBC，Firebird 官方）。
- **Python**：`fdb`（Pure-Python）、`pyfirebirdsql`、SQLAlchemy 适配器。
- **.NET**：Firebird .NET Provider（官方）。
- **Node.js**：`node-firebird`（libfbclient 绑定）。
- **Go**：`firebirdsql`（Pure-Go）、`go-firebird`。
- **Delphi / Lazarus**：FireDAC、IBX、ZeosLib（俄罗斯广泛使用）。

### 13.2 工具链

- **isql**（`src/isql/`）：交互式 CLI。
- **gbak**（`src/utilities/gbak.cpp`）：逻辑备份/恢复。
- **nbackup**：物理热备。
- **gfix**：数据库修复与参数调整。
- **gstat**（`src/utilities/gstat.cpp`）：统计与诊断。
- **fbtrace**：跟踪器与 SQL 跟踪。
- **gsec**：安全管理（添加/删除用户）。
- **fbguard**：守护进程监控与自动重启。

### 13.3 1C 集成（俄语生态）

- 1C:Enterprise 默认数据库为 Firebird；嵌入式版本作为 Server DLL 链接。

## 十四、源码学习路径

Firebird 是**学习 C++ 经典企业级数据库设计**的好材料，代码组织清晰（早期 InterBase 风格 + 现代 C++ 增强）。

1. **入口**：`src/yvalve/`（Y-Valve 路由层）。
2. **DSQL 引擎**：`src/dsql/dsql.cpp:1` + `src/dsql/Statement.cpp`。
3. **核心引擎**：`src/jrd/jrd.cpp:1`（约 9969 行）。
4. **事务**：`src/jrd/tra.cpp:1`（约 4455 行）。
5. **锁**：`src/lock/lock.cpp:1`。
6. **页面缓存**：`src/jrd/pag.cpp`、`src/jrd/CacheManager.cpp`。
7. **索引**：`src/jrd/idx.cpp`。
8. **远程协议**：`src/remote/inet.cpp`、`parser.cpp`。
9. **工具链**：`src/utilities/gbak.cpp`（备份）、`src/utilities/gstat.cpp`（诊断）。

源码规模：约 250 万行 C/C++ 代码（`src/`）。

## 十五、风险与陷阱

### 15.1 兼容性陷阱

- **ODS 版本强绑**：低版本 server 不能打开高版本 ODS 数据库（必须导出 + 导入）。
- **4.0+ 引擎重写**：老应用升级 4.0 必须用 `gbak` 重新备份/恢复一次。
- **页面大小选择**：建库时定，不可改；OLTP 默认 4K，OLAP 推荐 16K。

### 15.2 运维雷区

- **垃圾回收（Sweep）**：默认每 20000 事务触发，可能导致响应延迟抖动；监控 `MON$DATABASE.GSTAT`。
- **未启用 Forced Writes**：默认开启，禁用后断电可能丢失数据。
- **缓存不足**：默认 50K 页（200MB）太小，需按数据集调整 `DefaultDbCachePages`。
- **数据库文件碎片**：未关闭 fbserver 直接拷贝 .fdb 会损坏。

### 15.3 复制与高可用

- **没有原生同步复制**：原生只支持 nBackup 物理备份到从机；高可用需商业方案（HQbird / Caron）。
- **Caron Replication 配置复杂**：需手工配置 pub/sub 关系。

### 15.4 性能陷阱

- **SELECT * 大量 BLOB**：拖慢网络；用 `SELECT col1, col2`。
- **未加索引的字段排序**：全表扫描。
- **深度嵌套触发器**：B 级联触发器性能差；避免 5+ 层嵌套。
- **多次 COMMIT**：commit 频繁导致 TIP 页刷新抖动。

### 15.5 安全

- `SYSDBA` 默认密码 `masterkey`：必须立即修改。
- 启用 SRP 或 Legacy_Auth 控制访问。

## 十六、学习路径

### 16.1 入门（1 周）

1. 安装 Firebird（Windows 安装器、Linux apt）。
2. 用 `isql` 创建数据库、执行 DDL/DML。
3. 学习 PSQL 基础（`CREATE PROCEDURE`、`CREATE TRIGGER`）。

### 16.2 进阶（1-2 月）

1. 阅读官方文档：<https://firebirdsql.org/en/documentation/>
2. 学习 Firebird 架构（OAT 引擎、ODS）。
3. 学习 PSQL 高级特性（异常处理、游标、动态 SQL）。
4. 写简单的 PSQL 程序。

### 16.3 高级（3-6 月）

1. 深入源码：`src/jrd/tra.cpp`、`src/jrd/idx.cpp`、`src/lock/lock.cpp`。
2. 配置 Caron Replication。
3. 性能调优：`MON$STATEMENTS`、`gstat -h`。

### 16.4 推荐资源

- 官方文档：<https://www.firebirdsql.org/en/documentation/>
- Firebird FAQ：<https://www.firebirdfaq.org/>
- 《The Firebird Book》（Helen Borrie，Firebird 2.5 经典）。
- 《Firebird 2.5 Administration Guide》。
- 论坛：FirebirdSQL 邮件列表。
- 俄语社区：<https://firebirdsql.su/>（最活跃）。

## 十七、可移植性与部署

- **平台**：Linux（Debian/Ubuntu/CentOS）、Windows（32/64）、macOS、Android、iOS（libfbclient）。
- **国产化**：支持龙芯、麒麟、统信（部分版本）。
- **容器化**：Docker 镜像（`jacobalberty/firebird` 镜像流行）。
- **Kubernetes**：无官方 Operator；社区 Helm Chart。
- **嵌入式模式**：可直接 link `libfbembed.so`，SQLite 风格。

## 十八、变体与衍生项目

- **RedDatabase**（Red Soft 俄罗斯）：Firebird 商业分支。
- **Red Expert**（IDE，Red Soft）：Firebird GUI 工具。
- **HQbird**（IBSurgeon）：Firebird 商业增强版。
- **Firebird Embedded**：纯嵌入式版本。
- **NET Firebird Client**（社区）：.NET 客户端。

## 十九、与本项目的关联

- Firebird 的**嵌入式模式**与 SQLite 类似，可作为 `engineering/src/db/` 未来支持嵌入式模式的参考。
- Firebird 的**PSQL（过程语言）**设计精巧；本项目可借鉴到 PL/pgSQL-like 扩展。
- **`learning/playground/psql_test/`** 可作为 Firebird PSQL 的测试驱动模板。

## 二十、参考资料

1. Firebird 官方文档：<https://www.firebirdsql.org/en/documentation/>
2. Firebird FAQ：<https://www.firebirdfaq.org/>
3. Firebird 源码 `src/jrd/jrd.cpp`、`src/dsql/dsql.cpp`、`src/jrd/tra.cpp`、`src/lock/lock.cpp`
4. IDPL License：<https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/>
5. 《The Firebird Book》：Helen Borrie
6. IBSurgeon HQbird 博客：<https://ibsurgeon.com/en/>
8. Firebird Performance Guide：<https://firebirdsql.org/en/firebird-performance-guide/>
9. Caron Replication：<https://github.com/mariadb-developers/caron>（部分相关）
10. DeepWiki Firebird：<https://deepwiki.com/FirebirdSQL/firebird>