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

## 二十一、MGA 多代架构与 TipCache 共享内存缓存

Firebird 与 PostgreSQL/MySQL 的 MVCC 实现路径完全不同：PG/InnoDB 把事务状态藏在页头 tuple 头内做回滚段/undo 链，而 Firebird 把**事务状态集中在独立的 TIP（Transaction Inventory Page）上**，并配合**进程共享内存 TipCache**来避免每次读 TIP 的 I/O。这是 Firebird 被称为 **MGA（Multi-Generational Architecture，多代架构）** 的根因——核心数据结构是「记录多版本 + 全局 TIP + 单调 CommitNumber」三件套。

### 21.1 CommitNumber 单调分配与可见性语义

每次事务进入 commit，TipCache::setState 会原子地分配一个**单调递增的 CommitNumber**：

```c
// src/jrd/tpc.cpp:602-639 (TipCache::setState)
case tra_committed:
{
    if (oldStateCn == CN_DEAD)
        ERR_bugcheck_msg("TPC: Attempt to commit dead transaction");

    // If transaction is already committed - do nothing
    if (oldStateCn >= CN_PREHISTORIC && oldStateCn <= CN_MAX_NUMBER)
        return oldStateCn;

    fb_assert(oldStateCn == CN_ACTIVE || oldStateCn == CN_LIMBO);

    // Generate new commit number
    CommitNumber newCommitNumber = header->latest_commit_number++ + 1;

    statePtr->store(newCommitNumber, std::memory_order_relaxed);
    return newCommitNumber;
}
```

- `CommitNumber` 取值含义（`src/jrd/tpc_proto.h:47-52`）：
  - `CN_PREHISTORIC = 1`：事务号 < OIT（Oldest Interesting Transaction），永远可见。
  - `CN_ACTIVE = 0`：事务进行中。
  - `CN_LIMBO = -1`：两阶段提交悬挂状态。
  - `CN_DEAD = -2`：进程崩溃后被清理的状态。
  - 其余值：具体 CommitNumber，表示该事务已 commit。
- 可见性判定伪代码（综合 `tra.cpp:1572-1670` + `tpc_proto.h:181-196`）：

```
canSee(tra_num, snapshot.commitNumber):
    state = TipCache.cacheState(tra_num)         // 走共享内存
    if state == CN_ACTIVE:   return false
    if state == CN_LIMBO:    return false
    if state == CN_DEAD:     return false
    if state == CN_PREHISTORIC: return true
    // state 是具体 commit number
    return state <= snapshot.commitNumber           // 比快照"老"才能看见
```

- **`tra_precommitted` 特殊路径**（`tra.cpp:1100-1154`）：只读 attach（备份、监控等）启动时事务被标记为 `precommitted`（5），其 CommitNumber 直接取 `latest_commit_number`，从而能看见所有已提交数据但阻止写入。

### 21.2 TipCache 共享内存布局

TIP 页是磁盘结构（与 BIT 页一起用于 GC），但访问频繁所以在内存中镜像为 TipCache（`src/jrd/tpc_proto.h:54-374` + `src/jrd/tpc.cpp`）：

```c
// src/jrd/tpc_proto.h:199-226 (GlobalTpcHeader)
class GlobalTpcHeader : public Firebird::MemoryHeader {
public:
    std::atomic<CommitNumber> latest_commit_number;       // 全局最新 commit 号
    std::atomic<TraNumber>    oldest_transaction;         // OIT
    std::atomic<ULONG>        snapshot_release_count;     // 快照释放次数
    std::atomic<TraNumber>    latest_transaction_id;      // 全局最大事务号
    std::atomic<AttNumber>    latest_attachment_id;
    std::atomic<StmtNumber>   latest_statement_id;
    std::atomic<ULONG>        monitor_generation;
    ULONG                     tpc_block_size;
};

// src/jrd/tpc_proto.h:237-244 (SnapshotList)
class SnapshotList : public Firebird::MemoryHeader {
public:
    std::atomic<ULONG> slots_allocated;
    std::atomic<ULONG> slots_used;
    ULONG min_free_slot;                  // 下次分配起点
    SnapshotData slots[1];                 // attachment_id + snapshot(commitNumber)
};

// src/jrd/tpc_proto.h:246-250 (TransactionStatusBlock)
class TransactionStatusBlock : public Firebird::MemoryHeader {
public:
    std::atomic<CommitNumber> data[1];    // 每元素一个事务的 CommitNumber
};
```

布局含义：
- `latest_commit_number` 是**全局单调源**；snapshot 仅保存一个整数即可代表"快照在哪个时刻"。
- `TransactionStatusBlock` 是一个固定大小块（默认 `tpc_block_size = transactionsPerBlock`，约 4 KB / 8 KB），按 `blockNumber = traNumber / transactionsPerBlock` 索引。
- **跨进程**：通过 OS 共享内存文件 + `existenceLock`（`LCK_tpc_block`）保证多进程 attach 同一数据库时的同步（`tpc.cpp:366-378`）。

### 21.3 Snapshot 分配与生命周期

每个 Snapshot 事务（或 isolation 拍）在 `beginSnapshot` 时从共享 SnapshotList 槽位数组中拿一个 slot，记录 `attachment_id + snapshot(commitNumber)`：

```c
// src/jrd/tpc.cpp:909-963 (TipCache::beginSnapshot)
SnapshotHandle TipCache::beginSnapshot(thread_db* tdbb, AttNumber attachmentId, CommitNumber& commitNumber)
{
    // Remap snapshot list if it has been grown by someone else
    remapSnapshots(false);
    SnapshotList* snapshots = m_snapshots->getHeader();

    if (commitNumber != 0) {            // 0 = 用当前 latest；非 0 = 显式"过去时间"
        for (SnapshotHandle slotNumber = 0; slotNumber < snapshots->slots_used; ++slotNumber)
            if (slots[slotNumber].attachment_id != 0 && slots[slotNumber].snapshot == commitNumber)
                { found = true; break; }
        if (!found) ERR_post(isc_tra_snapshot_does_not_exist);
    }

    SnapshotHandle slotNumber = allocateSnapshotSlot();
    snapshots = m_snapshots->getHeader();

    SnapshotData* slot = snapshots->slots + slotNumber;

    if (commitNumber == 0)
        commitNumber = header->latest_commit_number.load(std::memory_order_acquire);

    slot->snapshot.store(commitNumber, std::memory_order_release);
    slot->attachment_id.store(attachmentId, std::memory_order_release);

    snapshots->min_free_slot = slotNumber + 1;
    return slotNumber;
}
```

调用点在 `src/jrd/tra.cpp:3653-3654`：

```c
if (!(trans->tra_flags & TRA_read_committed))
{
    trans->tra_snapshot_handle = dbb->dbb_tip_cache->beginSnapshot(
        tdbb, attachment->att_attachment_id, trans->tra_snapshot_number);
}
```

关键设计要点：
- **`Read Committed` 不分配 snapshot slot**——它直接从 `dbb` 拿"全局最新 CommitNumber"作为隐式快照，事务结束不释放 slot。
- **Snapshot（默认并发隔离）必须显式分配 slot**——`endSnapshot`（`tpc.cpp:993-1029`）清空 slot。
- **快照抑制 GC**：活跃快照越多，oldest snapshot 越大，OIT 不能向前推进——见 §22 Sweep 部分。

### 21.4 复杂度表

| 操作 | 复杂度 | 备注 |
|------|--------|------|
| `TipCache::cacheState` | O(1) | 共享内存命中，1 次 atomic load |
| `TipCache::setState(commit)` | O(1) 摊销 | 1 次 atomic RMW + 可能 O(1) 创建块 |
| `beginSnapshot` | O(slots_allocated) | 仅扫描已用 slot 查找重复 commit 号 |
| `endSnapshot` | O(1) | 清零 slot，可能回收 slots_used 末尾 |
| `findStates(min, max, mask)` | O((max - min) / block) | 按块扫描，最多约 `(max - min) / transactionsPerBlock` 块 |
| `releaseSharedMemory` | O(block_count) | 仅扫描 `SAFETY_GAP_BLOCKS = 1`（`tpc_proto.h:322`）个前置块 |

### 21.5 本项目对应

| Firebird 机制 | 本项目对应 | 差距 |
|--------------|-----------|------|
| MGA 多版本 + TIP 页 + CommitNumber | `engineering/src/db/storage/access/heap/heapam.c` 朴素单版本堆 | 未实现多版本，无 TIP 页结构 |
| TipCache 共享内存缓存 | 无对应 | 未引入跨进程共享内存 |
| SnapshotHandle 槽位复用 | 无对应 | 快照仅存在于进程内 `SnapshotData`，未做共享内存索引 |

> **设计取舍**：本项目为单机单进程实验性 RDBMS，未引入多进程 attach 场景，因此可以省去 TipCache 共享内存层——但 MGA 的「CommitNumber 单调分配」思想对后续多版本化有直接借鉴价值。

## 二十二、Sweep 与 GarbageCollector 协同垃圾回收

Firebird 的垃圾回收分**两层**：
- **Sweep（sweep 线程）**：推进 OIT（Oldest Interesting Transaction），标记不再需要的死事务；周期性触发（默认每 20000 事务）。
- **GarbageCollector（后台 GC 线程）**：在线程级**增量回收**具体数据页的死版本记录，不推进 OIT。

二者**正交运行**：Sweep 是数据库级的"里程碑"，GarbageCollector 是页级的"勤杂工"。

### 22.1 Sweep 主流程与 OIT 推进

源码入口：`src/jrd/tra.cpp:1799-1932`（`TRA_sweep`）。伪代码：

```c
// src/jrd/tra.cpp:1813-1904 (TRA_sweep 主流程)
void TRA_sweep(thread_db* tdbb)
{
    dbb->initiateSweepRun(tdbb);
    fb_assert(dbb->dbb_flags & DBB_sweep_in_progress);

    Jrd::Attachment* attachment = tdbb->getAttachment();
    jrd_tra* tdbb_old_trans = tdbb->getTransaction();
    jrd_tra* transaction = NULL;

    try {
        // 1) 标识 sweeper 线程：让事务 precommitted + JRD_reschedule 让出 CPU
        ThreadSweepGuard sweepGuard(tdbb);
        TraceSweepEvent traceSweep(tdbb);

        // 2) 启动只读"扫描事务"
        transaction = TRA_start(tdbb, sizeof(sweep_tpb), sweep_tpb);
        TraNumber transaction_oldest_active = transaction->tra_oldest_active;
        tdbb->setTransaction(transaction);

        // 3) 关掉 GC 通知，自己同步清完所有 dead记录
        attachment->att_flags &= ~ATT_notify_gc;

        if (VIO_sweep(tdbb, transaction, &traceSweep))     // 实际扫描死记录
        {
            // 4) 找出 [oldest, top) 范围内最老的 limbo 事务
            int oldest_state = 0;
            const TraNumber oldest_limbo =
                TPC_find_states(tdbb, transaction->tra_oldest, transaction->tra_top - 1,
                                1 << tra_limbo, oldest_state);
            const TraNumber active = oldest_limbo ? oldest_limbo : transaction->tra_top;

            // 5) 关键：先 CCH_flush(FLUSH_SWEEP) 再推进 OIT，
            //    否则宕机后死记录的 TID < OIT，会被当作 committed
            CCH_flush(tdbb, FLUSH_SWEEP, 0);

            WIN window(HEADER_PAGE_NUMBER);
            header_page* header = (header_page*) CCH_FETCH(tdbb, &window, LCK_write, pag_header);

            if (header->hdr_oldest_transaction < --transaction_oldest_active)
            {
                CCH_MARK_MUST_WRITE(tdbb, &window);
                header->hdr_oldest_transaction = MIN(active, transaction_oldest_active);
            }

            traceSweep.update(header);
            CCH_RELEASE(tdbb, &window);
            traceSweep.finish();
        }

        TRA_commit(tdbb, transaction, false);
        tdbb->setTransaction(tdbb_old_trans);
        dbb->clearSweepFlags(tdbb);
    }
    catch (const Firebird::Exception& ex) { ... }
}
```

### 22.2 Sweep 触发时机与阈值

- **触发方式**：
  1. **事务数阈值**：每次 `TRA_start` 检查 `header->hdr_oldest_transaction` 与 `latest_transaction` 差值；若 > 20000 则触发 sweep（默认 `configurable_scan_threshold = 20000`，见 `Firebird.conf`）。
  2. **手动触发**：`gfix -sweep`。
  3. **后台线程**：`SweepParameter::runSweep`（`tra.cpp:2685-2771`）封装为线程，可配置自动间隔。
- **三种 sweep 状态机**：通过 `Database::initiateSweepRun` / `clearSweepFlags` / `clearSweepStarting` 控制 `DBB_sweep_in_progress` 等标志。

### 22.3 GarbageCollector 页映射表与异步回收

源码：`src/jrd/GarbageCollector.cpp:39-83`。GarbageCollector 维护一张 `PageTranMap`：键 = `(relID, pageno)`，值 = 该页上最近写入的最小 TraNumber。

```c
// src/jrd/GarbageCollector.cpp:39-83 (RelationData::findPage / addPage / swept)
TraNumber GarbageCollector::RelationData::findPage(const ULONG pageno, const TraNumber tranid)
{
    PageTranMap::Accessor pages(&m_pages);
    if (!pages.locate(pageno))
        return MAX_TRA_NUMBER;

    // 已有记录时取 min(已有 tranid, 当前 tranid)
    if (pages.current().tranid > tranid)
        pages.current().tranid = tranid;
    return pages.current().tranid;
}

TraNumber GarbageCollector::RelationData::addPage(const ULONG pageno, const TraNumber tranid)
{
    TraNumber findTran = findPage(pageno, tranid);
    if (findTran != MAX_TRA_NUMBER)
        return findTran;
    m_pages.add(PageTran(pageno, tranid));     // 懒注册
    return tranid;
}

void GarbageCollector::RelationData::swept(const TraNumber oldest_snapshot, PageBitmap** bm)
{
    PageTranMap::Accessor pages(&m_pages);
    bool next = pages.getFirst();
    while (next) {
        if (pages.current().tranid < oldest_snapshot) {  // 死记录可回收
            if (bm) PBM_SET(&m_pool, bm, pages.current().pageno);   // 收集位图
            next = pages.fastRemove();
        } else
            next = pages.getNext();
    }
}
```

### 22.4 GarbageCollector 与 Sweep 协同

分工与时序：

```
┌───────────────────────────────────────────────────────────────────┐
│ 写入路径 (vio.cpp:3264)                                           │
│  attach 时如果 DBB_garbage_collector 开启                         │
│  attachment->att_flags |= ATT_notify_gc                          │
│                                                                   │
│ 读取路径 (vio.cpp:1377-1381, 1494-1495)                          │
│  当 VIO_fetch 看到 rpb_transaction_nr 是 dead 且 gcPolicyBackground│
│  时调用 notify_garbage_collector(tdbb, rpb)                      │
│   → GC.addPage(relId, pageno, rpb->rpb_transaction_nr)           │
└─────────────────────┬─────────────────────────────────────────────┘
                      │
┌─────────────────────▼─────────────────────────────────────────────┐
│ GC 后台线程 (GarbageCollector 共享)                                │
│  周期调用 getPages(oldest_snapshot, &relID)                       │
│   → RelationData::swept(oldest_snapshot, &bm) 返回 PageBitmap      │
│  对 bm 中每页执行 VIO 标记/清除死版本                              │
│  Sweep 后清扫 Oldest Transaction 之前的所有 mapping                  │
└───────────────────────────────────────────────────────────────────┘
```

具体对应源码（实测引用）：
- `vio.cpp:1377-1381`：读取时通知 GC（`gcPolicyBackground` + `ATT_notify_gc`）。
- `vio.cpp:3260-3265`：attach 时打开 GC 通知开关。
- `GarbageCollector.cpp:106-156`：`addPage` 与 `getPages` 的锁与位图生成。
- `GarbageCollector.cpp:65-83`：`RelationData::swept` 把 `tranid < oldest_snapshot` 的页收集到 PageBitmap 等待清理。

### 22.5 Sweep vs GarbageCollector 对比

| 维度 | Sweep | GarbageCollector |
|------|-------|------------------|
| 范围 | 数据库级（全表扫死记录） | 关系级（PageBitmap 增量） |
| 推进 OIT | 是 | 否（不修改 hdr_oldest_transaction） |
| 触发 | 20000 事务阈值 / 手动 / 后台 | 读取路径通知 + 后台循环 |
| 锁模型 | 持有 sweep 期间独占 header 页 | 共享 `ATT_notify_gc`，不影响 sweeper |
| 与快照交互 | 必须等所有活跃快照结束（oldest_snapshot）才能推进 OIT | 同样必须等 oldest_snapshot |
| 性能特征 | 一次性长事务（可能抖动） | 持续小开销（后台摊销） |
| 失败回滚 | 在 catch 块调 `TRA_commit` 兜底（`tra.cpp:1915-1925`） | 单页级失败不致命 |

### 22.6 复杂度对比表

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| `TRA_sweep` | O(D + L) | D = 数据页总数，L = limbo 事务区间长度 |
| `VIO_sweep`（`tra.cpp:1860` 调用） | O(R × V) | R = 关系数，V = 平均版本数/记录 |
| `GarbageCollector::addPage` | O(log P) | P = 该关系页映射数，BePlusTree 查找 |
| `RelationData::swept` | O(P) | 全扫页映射，过滤 `tranid < oldest_snapshot` |
| `getPages`（`GarbageCollector.cpp:124`） | O(R) | 轮询所有关系，直到找到一个有可回收页 |

### 22.7 本项目对应

| Firebird 机制 | 本项目对应 | 差距 |
|--------------|-----------|------|
| Sweep + OIT 推进 | 无 Sweep | 未实现死事务检测与 OIT 推进 |
| GarbageCollector 异步页回收 | 无 GC | `engineering/src/db/storage/access/heap/heapam.c` 单版本堆无需回收 |
| `ATT_notify_gc` 写路径旁路 | 无对应 | 无 MVCC 故无 GC 触发点 |
| `hdr_oldest_transaction` 头页字段 | `dbb->dbb_oldest_xid`（简化） | 仅一字段，无 sweep 检查逻辑 |

> **设计建议**：本项目若引入 MVCC，可借鉴 GC 与 Sweep 双层分工——Sweep 推进 OIT（全局里程碑），GC 回收具体页死版本（局部清扫），二者通过共享 `oldest_snapshot` 协调。

## 二十三、PSQL 编译路径与 Autonomous Transaction 嵌套执行模型

PSQL（Procedural SQL）是 Firebird 的过程语言，编译路径与 PG 的 PL/pgSQL 完全不同：PG 把 PL/pgSQL 编译成 `PlannedStmt`（运行时 AST），而 Firebird 把 PSQL 编译成 **BLR（Binary Language Representation，二进制 IR）**，直接持久化在系统表 `RDB$PROCEDURES.RDB$PROCEDURE_BLR` / `RDB$TRIGGERS.RDB$TRIGGER_BLR` 中，由 caller (`src/jrd/cmp.cpp`) 解释执行。

### 23.1 编译路径：DSQL AST → BLR

编译入口分两类：

#### (A) 存储过程 / 触发器（CREATE PROCEDURE / CREATE TRIGGER）

源码：`src/dsql/DdlNodes.epp:3628-3680` + `:4195-4286`。

```c
// src/dsql/DdlNodes.epp:4195-4286 (CreateAlterTriggerNode::compile)
void CreateAlterTriggerNode::compile(thread_db* /*tdbb*/, DsqlCompilerScratch* dsqlScratch)
{
    if (invalid)
        status_exception::raise(Arg::Gds(isc_dyn_invalid_ddl_trig) << name.toQuotedString());
    if (compiled)
        return;
    compiled = true;
    invalid = true;

    if (body)
    {
        dsqlScratch->beginDebug();
        dsqlScratch->getBlrData().clear();

        // 1) 创建 OLD/NEW 上下文
        if (dsqlScratch->contextNumber)
            dsqlScratch->resetTriggerContextStack();

        if (relationName.object.hasData()) {
            RelationSourceNode* relationNode = FB_NEW_POOL(...) RelationSourceNode(...);
            if (hasOldContext(type.value())) {
                relationNode->alias = OLD_CONTEXT_NAME;
                dsql_ctx* oldContext = PASS1_make_context(dsqlScratch, relationNode);
                oldContext->ctx_flags |= CTX_system | CTX_cursor;
            }
            if (hasNewContext(type.value())) {
                relationNode->alias = NEW_CONTEXT_NAME;
                dsql_ctx* newContext = PASS1_make_context(dsqlScratch, relationNode);
                newContext->ctx_flags |= CTX_system | CTX_cursor;
            }
        }

        // 2) 写入 BLR header + psql flag
        if (dsqlScratch->isVersion4())  dsqlScratch->appendUChar(blr_version4);
        else                            dsqlScratch->appendUChar(blr_version5);
        dsqlScratch->appendUChar(blr_begin);
        dsqlScratch->setPsql(true);

        // 3) 局部声明与主体语句编译为 BLR
        if (localDeclList) {
            localDeclList = localDeclList->dsqlPass(dsqlScratch);
            localDeclList->genBlr(dsqlScratch);
        }

        dsqlScratch->loopLevel = 0;
        dsqlScratch->cursorNumber = 0;
        dsqlScratch->scopeLevel++;
        StmtNode* stmtNode = body->dsqlPass(dsqlScratch);

        dsqlScratch->putOuterMaps();
        GEN_hidden_variables(dsqlScratch);

        // 4) EXIT 兼容 + 写入 body BLR + 收尾
        dsqlScratch->appendUChar(blr_label);
        dsqlScratch->appendUChar(0);
        stmtNode->genBlr(dsqlScratch);
        dsqlScratch->scopeLevel--;
        dsqlScratch->appendUChar(blr_end);
        dsqlScratch->appendUChar(blr_eoc);
    }
    invalid = false;
}
```

关键节点（`StmtNodes.cpp`）：
- `BlockNode::genBlr`（`:537-551`） → `blr_block ... blr_end`：BEGIN..END 块 + 异常处理器列表。
- `AssignmentNode::genBlr`（`:353`）：赋值。
- `IfNode::genBlr`（`:5429`）：`blr_if ... blr_end`。
- `CursorStmtNode::genBlr`（`:1464`）：`FOR SELECT ... INTO` 游标。
- `ErrorHandlerNode::genBlr`（`:3719`）：`WHEN ... DO` 异常处理器。
- `InAutonomousTransactionNode::genBlr`（`:5518-5547`）：`blr_auto_trans` 包装。

#### (B) DSQL 动态 SQL 语句（SELECT/INSERT/UPDATE 等）

`src/dsql/DsqlStatements.cpp:152-185`：

```c
// src/dsql/DsqlStatements.cpp:152-185 (DSQL prepare 路径)
DSQL_prepare(tdbb, ..., &blr, ..)
{
    // ... (SQL → Algebra tree → Optimizer)
    statement = CMP_compile(tdbb, blr.begin(), blr.getCount(), ...);
    // CMP_compile 在 src/jrd/cmp.cpp 把 BLR 编码为 Request
}
```

### 23.2 BLR 二进制 IR 简表

BLR 是单字节操作码 + 参数（变长整数/字符串）的紧凑格式。常用操作码 | 含义（参考 `src/dsql/Keywords.cpp` 与 `src/jrd/blp.h`）：

| BLR 操作码 | 含义 | 引用 |
|-----------|------|------|
| `blr_begin` / `blr_end` | 块开始 / 结束 | DdlNodes.epp:4253, 4285 |
| `blr_eoc` | BLR 流结束 | DdlNodes.epp:4286 |
| `blr_version4` / `blr_version5` | BLR 协议版本 | DdlNodes.epp:4249, 4251 |
| `blr_block` / `blr_end` | BEGIN..END 嵌套 | StmtNodes.cpp:539, 550 |
| `blr_label` (1B) | 标签（EXIT/BREAK 跳转目标） | DdlNodes.epp:4279 |
| `blr_auto_trans` | IN AUTONOMOUS TRANSACTION 包装 | StmtNodes.cpp:5543 |
| `blr_if` / `blr_else` | 条件分支 | IfNode::genBlr |
| `blr_loop` / `blr_break` / `blr_continue` | 循环 | WhileNode / LeaveNode |
| `blr_select` / `blr_for` | FOR SELECT 游标 | CursorStmtNode |
| `blr_modify` / `blr_erase` / `blr_store` | DML 触发器语句 | GenNode 子类 |

复杂度：编译时 O(N) AST 节点，每个节点 → 1 个或多个 BLR 字节。运行时 `CMP_compile` → `Request`（`src/jrd/cmp.cpp`）→ 链表式 StatementNode 树（`StmtNodes.cpp`），由 `BlockNode::execute` 等 `execute()` 方法在 `req_evaluate`/`req_unwind` 状态机下推/拉。

### 23.3 Autonomous Transaction 嵌套栈

PSQL 支持 `IN AUTONOMOUS TRANSACTION DO ...`：在已有事务栈之上嵌套**独立事务**，可独立 commit/rollback 而不污染外层事务。源码：`src/dsql/StmtNodes.cpp:5518-5597`。

#### BLR 编码（编译期）

```c
// src/dsql/StmtNodes.cpp:5518-5547 (InAutonomousTransactionNode::genBlr)
void InAutonomousTransactionNode::genBlr(DsqlCompilerScratch* dsqlScratch)
{
    bool useInternalAutoTrans = false;

    if (dsqlScratch && !dsqlScratch->isPsql()) {  // 非 PSQL 单条 DML + RETURNING 时
        if (const auto modifyNode = nodeAs<ModifyNode>(action))
            useInternalAutoTrans = modifyNode->dsqlReturning && modifyNode->dsqlCursorName.isEmpty();
        else if (const auto eraseNode = nodeAs<EraseNode>(action))
            useInternalAutoTrans = eraseNode->dsqlReturning && eraseNode->dsqlCursorName.isEmpty();
        else if (const auto storeNode = nodeAs<StoreNode>(action))
            useInternalAutoTrans = storeNode->dsqlReturning != nullptr;
        else if (const auto updateOrInsertNode = nodeAs<UpdateOrInsertNode>(action))
            useInternalAutoTrans = updateOrInsertNode->returning != nullptr;
        else if (const auto mergeNode = nodeAs<MergeNode>(action))
            useInternalAutoTrans = mergeNode->returning != nullptr;
    }

    if (useInternalAutoTrans) {
        AutoSetRestoreFlag autoTrans(&dsqlScratch->flags,
            DsqlCompilerScratch::FLAG_IN_AUTO_TRANS_BLOCK, true);
        action->genBlr(dsqlScratch);
    } else {
        dsqlScratch->appendUChar(blr_auto_trans);
        dsqlScratch->appendUChar(0);    // to extend syntax in the future
        action->genBlr(dsqlScratch);
    }
}
```

#### 运行时执行（Statement-level 嵌套）

```c
// src/dsql/StmtNodes.cpp:5562-5611 (InAutonomousTransactionNode::execute)
const StmtNode* InAutonomousTransactionNode::execute(thread_db* tdbb, Request* request, ExeState*) const
{
    Database* const dbb = tdbb->getDatabase();
    Jrd::Attachment* const attachment = tdbb->getAttachment();
    Impure* const impure = request->getImpure<Impure>(impureOffset);

    if (request->req_operation == Request::req_evaluate)
    {
        // 强制重新调度：防止 attach/db shutdown 后又启新事务
        JRD_reschedule(tdbb, true);

        jrd_tra* const org_transaction = request->req_transaction;
        fb_assert(tdbb->getTransaction() == org_transaction);

        ULONG transaction_flags = org_transaction->tra_flags;

        // 若原事务是 read_consistency，降级为 concurrency（避免锁升级）
        if (transaction_flags & TRA_read_consistency)
            transaction_flags &= ~(TRA_read_committed | TRA_read_consistency);

        // 1) 启动嵌套自治事务
        jrd_tra* const transaction = TRA_start(tdbb, transaction_flags,
                                               org_transaction->tra_lock_timeout,
                                               org_transaction);

        // 2) push 事务栈 + 替换当前事务
        request->pushTransaction();
        TRA_attach_request(transaction, request);
        tdbb->setTransaction(transaction);

        try {
            JRD_run_trans_start_triggers(tdbb, transaction);
        }
        catch (Exception&) { ... }

        return action;
    }
    ...
}
```

关键设计：
- **`org_transaction` 不被挂起**——自治事务提交/回滚后，**外层事务的快照仍然指向自治事务启动前的 CommitNumber**（由 `TRA_start` 拿新 traNumber + tip_cache snapshot 完成）。
- **`pushTransaction()`/`popTransaction()` 栈**：`request` 维护事务栈，多层嵌套自治事务不会互相串扰。
- **`TRA_run_trans_start_triggers`**：嵌套时也触发 `ON TRANSACTION START` 触发器。

### 23.4 Statement-level Auto-Commit（TRA_perform_autocommit）

Firebird 的"语句级事务"通过 `TRA_autocommit` + `TRA_perform_autocommit` 两个 flag 协调：
- `TRA_autocommit`：TPB 设置，事务本身是 autocommit 模式（每条语句是一个事务边界）。
- `TRA_perform_autocommit`：语句成功执行完后置位，告诉执行器"这条语句完成，应该 commit"。

```c
// src/jrd/Attachment.cpp / vio.cpp 等多处：
// 修改完成后置位（vio.cpp:2484, 3867, 4784, 5074）
if (transaction->tra_flags & TRA_autocommit)
    transaction->tra_flags |= TRA_perform_autocommit;
```

提交入口（`src/jrd/jrd.cpp:1363-1374`）：

```c
// src/jrd/jrd.cpp:1363-1374 (语句末尾提交检查)
if (transaction->tra_flags & TRA_perform_autocommit)
{
    if (!(tdbb->getAttachment()->att_flags & ATT_no_db_triggers) &&
        !(transaction->tra_flags & TRA_prepared))
    {
        // run ON TRANSACTION COMMIT triggers
        run_commit_triggers(tdbb, transaction);
    }

    transaction->tra_flags &= ~TRA_perform_autocommit;
    TRA_commit(tdbb, transaction, true);   // retaining 提交（保留事务对象，节省重新分配）
}
```

关键差异：
- **retaining commit**（`TRA_commit(tdbb, transaction, true)`）：提交事务但**保留 traNumber、保留锁状态、保留 savepoint 栈**——这与 PG/MySQL "每条语句一个事务"完全不同。
- **读一致性覆盖**：autocommit 模式下，若启用 `ReadConsistency` 配置（`dbb_config->getReadConsistency()`），则 `TRA_read_committed | TRA_read_consistency | TRA_rec_version` 被自动设置（`tra.cpp:3492-3498`）。

### 23.5 PSQL 编译与执行的复杂度

| 操作 | 复杂度 | 备注 |
|------|--------|------|
| DDL 编译（CREATE PROCEDURE） | O(N) AST 节点 | 每个节点 → 1 BLR 调用 |
| BLR → Request 编码（`CMP_compile`） | O(B) BLR 字节 | 单遍扫描，链表化 StatementNode |
| 运行时 `BlockNode::execute` | O(1) per call | 状态机：req_evaluate / req_unwind 切换 |
| Savepoint 嵌套 | O(S) S=嵌套块数 | 每次 `req_evaluate` 起 1 个 savepoint |
| Autonomous Transaction 启动 | O(1) | `TRA_start` + `pushTransaction` |
| Savepoint 回滚（异常处理器） | O(S) | unwind 时逐级回滚 savepoint |

### 23.6 本项目对应

| PSQL 机制 | 本项目对应 | 差距 |
|----------|-----------|------|
| BLR 二进制 IR + CMP_compile | `engineering/src/db/sql/executor.c` 直 AST 求值 | 未做二进制 IR，无持久化 |
| PSQL 存储过程 | 无对应 | 未实现过程语言层 |
| Autonomous Transaction 嵌套 | 无对应 | 仅单层事务栈，无 savepoint 链 |
| `TRA_perform_autocommit` 语句级提交 | 无对应 | 单一 commit 边界 |

> **设计建议**：本项目如需引入 PL/pgSQL-like 过程语言，可参考 BLR 模式——AST 在编译期序列化为字节流，存储在系统表，执行器为字节码解释器。这种"持久化 IR"模式比 PG 的 PlannedStmt 持久化更紧凑，但代价是 BLR 协议版本升级需数据迁移（类似 ODS 升级）。

---