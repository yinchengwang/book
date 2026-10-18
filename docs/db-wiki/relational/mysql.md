# MySQL Wiki（关系型数据库 / 全维度覆盖）

> 版本：本次参考 MySQL trunk（`9.x` 系列，`MYSQL_VERSION` 显示 `26.10.0 INNOVATION`，上一 LTS 为 9.7.0），GPLv2 双许可。

## 一、项目定位

MySQL 是世界上最流行的开源**关系型数据库管理系统（RDBMS）**之一，由瑞典 MySQL AB 公司 1995 年首发，后被 Sun（2008）、Oracle（2010）收购。它以**易用性、高性能（Web 场景读取）、庞大生态**著称，是 LAMP（Linux+Apache+MySQL+PHP）时代的标志性组件，至今仍主导互联网中小型应用与云托管数据库市场。

- **痛点**：商业数据库昂贵且锁定（Oracle/SQL Server）；开发者需要一款快速上手、社区庞大、配套工具齐全的 RDBMS。
- **典型用户**：Web 应用（WordPress、Discuz、电商网站、博客）、SaaS 服务、游戏后端、日志存储与分析（中等规模）、金融核心账务（部分中国大行）。

## 二、发展历史与社区

- **起源**：1995 年瑞典 Michael Widenius（"Monty"）、David Axmark 创立 MySQL AB。
- **里程碑**：
  - 2001 年 v3.23：InnoDB 引入（事务/行锁/MVCC），奠定现代 MySQL 形态。
  - 2008 年被 Sun Microsystems 收购。
  - 2010 年被 Oracle 收购，引发社区担忧，Monty 主导 MariaDB 分叉（见 mariadb wiki）。
  - 2013 年 5.6：InnoDB 性能显著提升、`performance_schema`。
  - 2015 年 5.7：原生 JSON 类型、Generated Column、Group Replication 起步。
  - 2018 年 8.0：原子 DDL、Window Function、CTE（递归）、InnoDB 自增 ID 持久化、MGR 升级。
  - 2023-2024 年 8.4 LTS、9.0（向 HTAP 演进）；长期支持分支为 LTS（8.4、9.7 等）。
  - 2025-2026 年 9.x Innovation（含 HeatWave Lakehouse、向量等扩展）。
- **社区**：Oracle 维护主线；社区 fork 包括 MariaDB、Percona Server（带 Percona XtraDB）、TiDB（基于 MySQL 协议）。
- **维护节奏**：每年约 4 个 Innovation 版本 + LTS 每 2-3 年一个，长期支持 8 年。

## 三、许可与商业限制

- **许可证**：GPLv2（双重许可），允许商业使用但衍生产品也必须开源。
  - 来源：`reference/relational/mysql/LICENSE`
- **商业版差异**（MySQL Enterprise Edition）：闭源插件（Thread Pool、Audit、Enterprise Backup、Firewall、Transparent Data Encryption）、Oracle Premier Support。
- **GPL 触发风险**：应用通过 TCP 连接 MySQL 不触发 GPL（Oracle 明确 FSF 解释），但若静态链接 libmysqlclient 且不分发则通常无碍；GPL 真正风险是 fork/修改服务端代码。
- **生态中立**：MySQL 仍归 Oracle，但因协议兼容性产生 MariaDB、TiDB、Aurora MySQL 等大量替代品。

## 四、业界主要使用方与案例

- **互联网**：Facebook（数十万台 MySQL 实例）、Twitter、YouTube、Wikipedia、Airbnb、Uber、Netflix 部分服务、Pinterest。
- **中国互联网**：阿里早期去 IOE 前的关键数据库、字节跳动部分业务、美团点评（MTProxy）、小米、京东商城。
- **金融**：部分券商交易系统（中国银河）、中小银行账务、上海证券交易所行情子系统。
- **SaaS**：WordPress.com、GitHub 早期部分数据、Slack 部分服务、Zoom 部分元数据。
- **云**：AWS Aurora MySQL、Aliyun RDS-MySQL、Azure Database for MySQL、Google Cloud SQL、Tencent CDB。

## 五、一句话对比定位

> MySQL 是**Web 应用优先的 RDBMS**，以读取性能、连接并发、复制简便为核心优势；在事务严谨、复杂查询、扩展性上**不及 PG**，但生态与上手门槛遥遥领先。

## 六、整体架构

MySQL 采用**经典的分层插件化架构**，**单进程多线程**，每个客户端连接由独立 Thread 处理（区别于 PG 的多进程）。

```
┌──────────────────────────────────────────────────────────────────────┐
│                       客户端（任意编程语言）                            │
│           libmysqlclient / JDBC / ODBC / .NET / Python                │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ MySQL Wire Protocol (端口 3306)
┌─────────────────────────▼────────────────────────────────────────────┐
│                   mysqld 主进程（单进程多线程）                          │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ Connection Layer (sql/conn_handler/)                            │    │
│  │  - Thread per connection (默认)                                │    │
│  │  - 处理握手、认证、协议解析（sql/protocol*.cc）                  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ SQL Layer (sql/)                                               │    │
│  │  - Lexer & Parser  (sql/sql_lex.cc, sql/sql_yacc.yy)            │    │
│  │  - Optimizer      (sql/sql_optimizer.cc)                       │    │
│  │    包含 RBO + CBO（基于 cost 模型 + 直方图）                     │    │
│  │  - Executor       (sql/sql_executor.cc, handler.cc)             │    │
│  │  - binlog         (sql/binlog.cc, libbinlogevents/)             │    │
│  └──────────────────────────────────────────────────────────────┘    │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ 存储引擎 API (sql/handler.h)                                     │    │
│  │   handler::create / read / write_index / update / delete        │    │
│  └──────┬─────────────────────────────────┬──────────────────────┘    │
│         │                                 │                            │
│  ┌──────▼────────────┐         ┌──────────▼──────────┐                │
│  │ InnoDB  (默认)     │         │  MyISAM / Memory /   │                │
│  │ storage/innobase/ │         │  NDB / Archive 等     │                │
│  │ - Buffer Pool     │         │  storage/myisam/      │                │
│  │ - Redo / Undo Log │         │  storage/heap/        │                │
│  │ - B+Tree          │         │  storage/ndb/         │                │
│  │ - MVCC            │         │                       │                │
│  └──────┬────────────┘         └──────────────────────┘                │
└─────────┼────────────────────────────────────────────────────────────┘
          │
┌─────────▼────────────────────────────────────────────────────────────┐
│                     文件系统层                                          │
│   .ibd（独立表空间）/ .frm（8.0+ 取消）/ redo log / undo log /         │
│   binlog / slow log / error log / relay log                            │
└─────────────────────────────────────────────────────────────────────┘
```

### 6.1 进程与线程模型

- **mysqld 单进程**，内部线程：
  - **主线程**：启动、关闭、信号处理。
  - **IO 线程**：InnoDB 的 `read_io_thread / write_io_thread`（默认各 4 个）。
  - **Purge 线程**：回收 Undo log。
  - **Page Cleaner 线程**：刷脏页。
  - **Log Writer**：异步写 redo log。
  - **Master/Slave IO/SQL**：复制线程。
  - **用户连接线程**（`sql/conn_handler/`）：`one-thread-per-connection`（默认），可选 `thread_pool` 插件。

## 七、存储引擎

### 7.1 引擎对比

| 引擎 | 事务 | 锁粒度 | 索引 | 适用 | 现状 |
|------|------|--------|------|------|------|
| **InnoDB** | ACID | 行锁 + MVCC | B+Tree (聚簇) | OLTP 默认 | 默认且唯一推荐 |
| **MyISAM** | 无 | 表锁 | B+Tree (非聚簇) | 读多写少、压缩报表 | 已废弃 |
| **MEMORY/HEAP** | 无 | 表锁 | Hash/B+Tree | 临时表、缓存 | 慎用，重启丢失 |
| **CSV** | 无 | 无 | 无 | 数据交换 | 极少 |
| **ARCHIVE** | 无 | 行锁 | 无 | 日志归档 | 极少 |
| **BLACKHOLE** | 无 | - | - | 主从中转 / binlog 转发 | 特殊 |
| **NDB Cluster** | ACID | 行锁 | B+Tree + Hash | 分布式集群 | MySQL Cluster |

### 7.2 InnoDB 关键架构

源码位于 `storage/innobase/`，模块化极强：

- **缓冲池**（`buf/buf0buf.cc`，7072 行）：管理数据页与索引页的内存缓存。
- **B+Tree 实现**（`btr/`）：聚簇索引按主键组织，二级索引叶子节点存 PK。
- **事务系统**（`trx/`）：`trx0trx.cc`（事务结构）、`trx0undo.cc`（Undo log）。
- **锁系统**（`lock/`）：行锁（lock_rec）、意向锁、表锁。
- **日志**（`log/`）：Redo log（`log0log.cc`），循环写入。
- **数据字典**（`dict/`）：8.0 后使用 InnoDB 自身存储数据字典（`mysql.ibd`）。
- **行操作**（`row/`）：`row0ins.cc:3111`（`row_ins_clust_index_entry` 聚簇索引插入）、`row0upd.cc`、`row0sel.cc`。

### 7.3 InnoDB 页（Page）

默认 16 KB（`innodb_page_size`），布局：

```
┌──────────────────────────────────────────────────┐
│ Fil Header (38B)         - checksum, lsn, page id│
├──────────────────────────────────────────────────┤
│ Page Header (56B)        - n_recs, n_heap, etc.  │
├──────────────────────────────────────────────────┤
│ Infimum + Supremum 记录（虚拟边界记录）            │
├──────────────────────────────────────────────────┤
│ User Records (向上生长, 按主键顺序)                  │
├──────────────────────────────────────────────────┤
│ 空闲空间                                          │
├──────────────────────────────────────────────────┤
│ Page Directory (slot 数组, 加速二分查找)            │
├──────────────────────────────────────────────────┤
│ Fil Trailer (8B)          - checksum             │
└──────────────────────────────────────────────────┘
```

每条记录格式：`Compact (5.0+)`，含变长字段长度列表、null bitmap、record header（`info_bits`、`n_owned`、`heap_no`、`record_type`、`next_record`）。

### 7.4 Redo / Undo / Binlog 三日志协作

- **Redo Log**（`storage/innobase/log/log0log.cc`）：保证持久性，物理日志，循环写入（默认 `innodb_log_file_size * 2`），WAL 思想。
- **Undo Log**（`storage/innobase/trx/trx0undo.cc`）：保证事务回滚 + MVCC 多版本，含 `INSERT_UNDO` / `UPDATE_UNDO`。
- **Binlog**（`sql/binlog.cc`、`libbinlogevents/`）：Statement / Row / Mixed 三种格式，主从复制 + 时间点恢复。
- **两阶段提交**：`Prepare`（InnoDB redo 写盘） → `Commit`（binlog 写盘） → `Commit`（redo commit 标记）保证主从一致性。

源码：`sql/handler.cc:9193`（handler 总入口）。

### 7.5 表空间

- **系统表空间**：`ibdata1`（8.0 后改为 `mysql.ibd` + undo tablespaces）。
- **独立表空间**（默认）：每个表一个 `.ibd`（`innodb_file_per_table=ON`）。
- **通用表空间**：`CREATE TABLESPACE` 共享 `.ibd`。
- **Undo 表空间**：`innodb_undo_tablespaces`（默认 2 个）。

## 八、索引机制

### 8.1 聚簇索引 vs 二级索引

- **聚簇索引（clustered index）**：按主键组织，叶子节点直接存行数据；推荐**自增主键**避免页分裂。
- **二级索引（secondary index）**：叶子节点存 `主键值 + 行指针`，回表（`lookup`）开销大。

### 8.2 B+Tree 索引

- 源码：`storage/innobase/btr/btr0cur.cc`、`btr0pcur.cc`、`btr0sea.cc`（自适应哈希索引）。
- **自适应哈希索引**：InnoDB 自动为热点页建立内存哈希，加速等值查询。
- **Change Buffer**：写二级索引时缓存变更，合并到磁盘，减少随机 IO。
- **Multi-Range Read (MRR)**：优化器对范围查询排序后再回表。

### 8.3 全文索引

- v5.6+ 内置 FULLTEXT；v8.0+ 支持 `ngram parser`（中文分词）和 `InnoDB FTS`。
- 倒排索引，存储在 `FTS_*` 系列系统表。

### 8.4 索引扩展（Extensions）

- **生成列索引**：`CREATE INDEX idx ON t (JSON_EXTRACT(data, '$.x'))`。
- **函数索引**（v8.0.13+）：`CREATE INDEX idx ON t ((UPPER(name)))`。
- **降序索引**（v8.0+）：`CREATE INDEX idx ON t (a DESC, b ASC)`。

## 九、查询与执行

### 9.1 解析

- 词法分析：`sql/lex_symbol.h`、`sql/sql_lex.cc`。
- 语法分析：`sql/sql_yacc.yy`（bison 生成），生成 `SELECT_LEX` 结构。
- 预处理：`sql/sql_prepare.cc`，绑定变量、处理 `@var`。

### 9.2 优化器

- **入口**：`sql/sql_optimizer.cc`，主函数 `Sql_cmd_dml::optimize`。
- **RBO**：规则化（外连接转内连接、子查询展开、谓词下推）。
- **CBO**：基于成本（`opt_costmodel.cc`），读取直方图（`histograms/`）。
- **Hint**：`INDEX_MERGE`、`JOIN_ORDER`、`SEMIJOIN`、`NO_RANGE_OPTIMIZATION` 等。

### 9.3 执行器

- **嵌套循环（Nested Loop Join）**：默认。
- **Block Nested Loop (BNL)**：v8.0.20+ 弃用，改用 Hash Join（`sql/join_iterator.h`）。
- **Hash Join**（v8.0.18+）：优化器可选用 `hash_join` 替代 BNL，含经典 grace hash 与 chained hash。
- **Iterator 框架**（v8.0+）：`sql/iterators/` 用 C++ 迭代器模型重写执行器。

入口：`sql/handler.cc:9193`（`handler` 接口）。SQL 解析入口：`sql/sql_parse.cc:7425`。

### 9.4 视图 / 触发器 / 存储过程

- **视图**：v5.0+ 视图算法 `MERGE / TEMPTABLE / UNDEFINED`，8.0 引入可更新视图的 `WITH CHECK OPTION`。
- **触发器**：`BEFORE/AFTER INSERT/UPDATE/DELETE`，每事件每动作最多 6 个（v5.7.2+）。
- **存储过程 / 函数**：基于 SQL/PSM 风格，`sql/sp_*` 模块。

## 十、事务与一致性

### 10.1 隔离级别

| 级别 | InnoDB 实现 | 现象 |
|------|------------|------|
| READ UNCOMMITTED | 直接读取最新版本 | 脏读 |
| READ COMMITTED | 每语句快照（v5.0+ 默认） | 不可重复读 |
| REPEATABLE READ | 事务开始快照（v5.7+ 默认） | 幻读（InnoDB 通过 Next-Key Lock 解决） |
| SERIALIZABLE | RR + 所有 SELECT 转 `SELECT ... LOCK IN SHARE MODE` | 无并发 |

### 10.2 MVCC

- 每行记录 `DB_TRX_ID`（最后修改事务）、`DB_ROLL_PTR`（指向 Undo log）、`DB_ROW_ID`（无 PK 时）。
- **快照读**：读取 `trx_id < m_up_limit_id` 且 `not in m_ids` 的版本。
- **当前读**：`SELECT ... FOR UPDATE / LOCK IN SHARE MODE / UPDATE / DELETE`，加行锁。

### 10.3 锁机制

源码：`storage/innobase/lock/lock0lock.cc`。

- **行锁**：`LOCK_REC_NOT_GAP` / `LOCK_GAP` / `LOCK_ORDINARY`（Next-Key Lock）/ `LOCK_INSERT_INTENTION`。
- **意向锁**：表级 `IS / IX`，与行锁兼容。
- **自增锁**（v8.0 改进）：`innodb_autoinc_lock_mode` 0/1/2 模式，v8.0 默认 2（轻量互斥）。

### 10.4 死锁检测

- 自动检测 + 回滚代价小的事务。
- 关闭检测（`innodb_deadlock_detect=OFF`）时依赖 lock wait timeout。

## 十一、复制与高可用

### 11.1 主从复制（Asynchronous Replication）

- **基于 binlog**：binlog dump thread（`Binlog Dump`）→ 从库 IO Thread 写入 `relay-log` → SQL Thread 回放。
- **多源复制**（v5.7+）、**并行复制**（v5.6 schema 级，v5.7 database 级，v8.0 writeset-based）。

### 11.2 半同步复制（Semi-Synchronous）

- `sql/semisync*.cc`：主库等待至少一个从库 ACK 才提交。
- v8.0+ 默认 `rpl_semi_sync_replica_wait_for_ack` 控制等待模式。

### 11.3 组复制（Group Replication, MGR）

- v5.7 引入，v8.0 成熟；基于 Paxos 变种 `XCom`（`plugin/group_replication/`）。
- 单主或多主模式，自动故障转移，RPO ≈ 0。
- **使用案例**：金融级强一致场景、分布式 MySQL。

### 11.4 高可用方案

- **MHA / MHA + VIP**：传统主从 failover。
- **Orchestrator**：GitHub 出品的拓扑感知 + 自动恢复。
- **MySQL InnoDB Cluster**（8.0+）：MGR + MySQL Router + MySQL Shell 一体化。
- **ProxySQL**：读写分离 + 查询分流。
- **MaxScale**（MariaDB 商业版）：智能代理。

## 十二、性能与基准

- **Sysbench OLTP_READ_WRITE**：单机 v8.0 在 NVMe SSD 上 30-80k TPS。
- **TPC-C**：8.0 单机 100k+ tpmC（4 节点）。
- **典型调优参数**：
  - `innodb_buffer_pool_size = 60-80% RAM`
  - `innodb_log_file_size = 2-4 GB`（写入密集）
  - `innodb_flush_log_at_trx_commit = 1`（强持久）或 2（折衷）
  - `sync_binlog = 1`（强持久）或 1000（折衷）
  - `innodb_flush_neighbors = 0`（SSD）
- **监控视图**：`performance_schema`、`sys` schema（`sys.*` 视图）、`information_schema`。

## 十三、生态与集成

### 13.1 客户端 SDK

- **C/C++**：`libmysqlclient`（`libmysql/`）、MySQL Connector/C++。
- **Java**：MySQL Connector/J。
- **Python**：mysqlclient、PyMySQL、mysql-connector-python、aiomysql、SQLAlchemy、Django ORM。
- **Go**：`go-sql-driver/mysql`、GORM、XORM、Ent。
- **Node.js**：`mysql2`、`sequelize`、`typeorm`、`prisma`、`drizzle-orm`。
- **Rust**：`sqlx`、`mysql_async`。

### 13.2 工具链

- **mysql（CLI）**：`client/mysql.cc`。
- **mysqldump**：逻辑备份。
- **mysqlbinlog**：binlog 解析。
- **mysqladmin**：运维管理。
- **MySQL Shell**：mysqlsh，集成 JavaScript / Python / SQL REPL。
- **MySQL Router**：透明读写分离。
- **MySQL Enterprise Backup**（商业）/ **Percona XtraBackup**：物理热备。

### 13.3 容器化

- 官方 `mysql` Docker 镜像（基于 Oracle Linux / Ubuntu）。
- Helm Chart：Bitnami MySQL、官方 Operator。
- Operator：MySQL Operator for Kubernetes（Oracle）、Percona Operator。

## 十四、源码学习路径

推荐顺序：

1. **入口与握手**：`sql/sql_parse.cc:7425`、`sql/mysqld.cc`（主入口）。
2. **存储引擎 API**：`sql/handler.h` 定义 `handler` 接口；`sql/handler.cc:9193` 总入口。
3. **InnoDB Buffer Pool**：`storage/innobase/buf/buf0buf.cc:7072`。
4. **InnoDB 行插入**：`storage/innobase/row/row0ins.cc:3111`（`row_ins_clust_index_entry`）。
5. **Redo Log**：`storage/innobase/log/log0log.cc`。
6. **B+Tree**：`storage/innobase/btr/btr0cur.cc`（游标操作）。
7. **优化器**：`sql/sql_optimizer.cc`（入口）+ `sql/join_iterator/`（v8.0 新执行器）。
8. **复制**：`sql/rpl_*.cc`、`sql/binlog.cc`、`storage/innobase/log/log0recv.cc`（备机回放）。

源码规模：trunk 约 470 万行 C/C++ 代码（其中 InnoDB ~38 万行，SQL 层 ~24 万行），构建系统 CMake。

## 十五、风险与陷阱

### 15.1 运维雷区

- **autocommit 误用**：DML 默认 autocommit=on，长事务忘记提交导致锁等待。
- **`wait_timeout`**（默认 8 小时）：空闲连接被服务端关闭，但客户端不知 → 应用侧使用连接池时务必关闭空闲连接。
- **`max_connections`** 默认 151：每连接消耗 ~250 KB，过万需用 ProxySQL。
- **`binlog_format`**：ROW 模式下大事务 binlog 爆炸；MIXED 在某些场景出现不一致。
- **`sync_binlog=0`**：操作系统崩溃可能丢 binlog，导致主从不一致。

### 15.2 迁移痛点

- **MySQL 5.x → 8.x**：utf8 → utf8mb4（emoji 必需）、浮点数舍入规则变化、保留字增加。
- **MySQL → PG**：自增 ID 行为、`GROUP BY` 严格性、`GROUP_CONCAT`、`FIND_IN_SET` 等差异。
- **MySQL → TiDB**：协议兼容但 TiDB 不支持事务 + DDL 混合、`AUTO_INCREMENT` 行为。

### 15.3 性能陷阱

- **SELECT \***：可能让大字段（TEXT）拖慢；用 `EXPLAIN FORMAT=JSON` 检查实际使用列。
- **大 OFFSET 分页**：`LIMIT 1000000, 10` 全扫；用 **seek pagination**（基于上次最大 ID）。
- **隐式类型转换**：WHERE 字符列 = 数字 → 索引失效。
- **N+1 查询**：ORM 未开启 eager loading。
- **无索引排序**：`ORDER BY ... LIMIT N` 全文件排序。

### 15.4 主从不一致

- 复制链路延迟导致读已提交不一致 → 应用侧 `SELECT ... WAIT 5` 或 `read_after_write=true`。
- binlog 不完整（`expire_logs_days=0` + 备份缺失）→ 时间点恢复失败。

### 15.5 安全

- 默认空口令 root：`mysql_secure_installation` 必须执行。
- `GRANT ... @'%'`：远程 root 必须禁用，改用白名单 + 强密码。

## 十六、学习路径

### 16.1 入门（1-2 周）

1. 安装 MySQL 8.0（macOS brew / Linux apt / 官方 docker）。
2. 用 `mysql` CLI 执行 DDL/DML；学习 `EXPLAIN`、`SHOW PROCESSLIST`。
3. 学习 SQL 基础、JOIN、索引原理。

### 16.2 进阶（1-2 月）

1. 阅读官方手册《MySQL Reference Manual》对应章节。
2. 学习 InnoDB 架构：MVCC、锁、日志、Checkpoint。
3. 实战：搭建主从复制、`pt-online-schema-change` 在线 DDL、慢查询分析。

### 16.3 高级（3-6 月）

1. 深入源码：`storage/innobase/buf/buf0buf.cc`、`log/log0log.cc`、`sql/handler.cc`。
2. 写自定义插件：`daemon plugin`、`audit plugin`、`information_schema engine`。
3. 部署 MGR 高可用集群、ProxySQL 读写分离。
4. 性能调优：`performance_schema` + `sys schema`、`EXPLAIN ANALYZE`。

### 16.4 推荐资源

- 官方文档：<https://dev.mysql.com/doc/refman/8.0/en/>
- 《高性能 MySQL》（Baron Schwartz 等，第 4 版）
- 《MySQL 内核：InnoDB 存储引擎》（姜承尧）
- 《MySQL 运维内参》（宁海元）
- 官方论坛：<https://forums.mysql.com/>
- 公众号：InsideMySQL、MySQL 运维、MySQL 技术
- 知乎：MySQL 话题、InnoDB 话题

## 十七、可移植性与部署

- **平台**：Linux（所有发行版）、macOS、Windows（MSI 安装器）、FreeBSD、OpenBSD。
- **容器化**：官方 `mysql` Docker 镜像；可调整 `MYSQL_ROOT_PASSWORD`、`MYSQL_DATABASE` 等环境变量。
- **Kubernetes**：MySQL Operator（Oracle）、Percona Operator、Bitnami Helm Chart。
- **云托管**：AWS RDS / Aurora MySQL、阿里云 RDS、Azure Database for MySQL、Google Cloud SQL、Tencent CDB。
- **嵌入式**？否，MySQL 是 C/S 架构；嵌入式场景请用 SQLite 或 BerkeleyDB。

## 十八、变体与衍生项目

- **MariaDB**：原 MySQL 创始人 Monty 创建的分支，GPLv2，详见 `docs/db-wiki/relational/mariadb.md`。
- **Percona Server for MySQL**：Percona 公司维护，含 XtraDB 引擎与工具链。
- **Aurora MySQL**：AWS 自研云原生版本（计算存储分离，6 副本）。
- **TiDB**：PingCAP 出品的分布式 HTAP 数据库，MySQL 协议兼容（详见 `docs/db-wiki/distributed-sql/tidb.md`）。
- **CockroachDB**：协议兼容 PostgreSQL，但 fork 自 PG（详见 `docs/db-wiki/distributed-sql/cockroachdb.md`）。
- **PlanetScale（Vitess）**：MySQL 水平扩展方案，YouTube 出品。
- **MySQL Cluster（NDB）**：分布式内存引擎，电信运营商常用。

## 十九、与本项目的关联

- 本仓库 `engineering/src/db/` 中实现的 **PG 风格存储引擎**借鉴了 InnoDB 的 Buffer Pool Clock-Sweep 与 WAL 设计。
- **`engineering/apps/db_driver/`** 可作为 MySQL 客户端测试的简易驱动（基于 libmysqlclient）。
- **`engineering/rag/`** 中的 MiniVecDB 数据库后端可以兼容 MySQL（详见 Task 7 MiniVecDB 计划）。

## 二十、参考资料

1. MySQL 官方文档 8.0：<https://dev.mysql.com/doc/refman/8.0/en/>
2. MySQL 官方文档 9.x：<https://dev.mysql.com/doc/refman/9.0/en/>
3. MySQL Server 源码 `storage/innobase/buf/buf0buf.cc`、`storage/innobase/log/log0log.cc`、`storage/innobase/row/row0ins.cc`、`sql/handler.cc`、`sql/sql_parse.cc`
4. 《高性能 MySQL》（第 4 版）：<https://www.highperformanceMySQL.com/>
5. MySQL Server Blog：<https://mysqlserverteam.com/>
6. Percona Database Performance Blog：<https://www.percona.com/blog/>
7. MariaDB 知识库（部分 MySQL 兼容特性）：<https://mariadb.com/kb/en/>
8. TiDB Documentation（MySQL 兼容性参考）：<https://docs.pingcap.com/>
9. 知乎专栏「MySQL 入门教程」：<https://zhuanlan.zhihu.com/c_117287938>
10. MySQL 实战 45 讲（极客时间，林晓斌）

## 二十一、InnoDB B+Tree 页面分裂与插入路径算法

§8.2 提到了 B+Tree 与自适应哈希索引，但**没展开「插入一个记录究竟走哪些函数」「页面满了怎么选 split_rec」**这些工程细节。本节聚焦 InnoDB 的页面插入与分裂算法。

### 21.1 单页内插入：`page_cur_insert_rec_low`

源码：`reference/relational/mysql/storage/innobase/page/page0cur.cc:1239`（行号对应 MySQL 9.x trunk，`MYSQL_VERSION` = 26.10.0 INNOVATION）。

页面内插入按「优先复用空闲链表、其次堆顶分配」的策略：

```text
function page_cur_insert_rec_low(current_rec, index, rec, offsets, mtr):
    page = page_align(current_rec)
    rec_size = rec_offs_size(offsets)

    # 1. 优先从 PAGE_FREE 空闲链表复用等大的 slot
    free_rec = page_header_get_ptr(page, PAGE_FREE)
    if free_rec != NULL and rec_size_of(free_rec) >= rec_size:
        insert_buf = free_rec - rec_offs_extra_size(free_rec)
        page_mem_alloc_free(page, free_rec.next, rec_size)
    else:
        # 2. 否则从堆顶向下分配
        insert_buf = page_mem_alloc_heap(page, rec_size, &heap_no)
        if insert_buf == NULL: return NULL  # 页面已满

    # 3. 拷贝记录内容到 insert_buf
    insert_rec = rec_copy(insert_buf, rec, offsets)

    # 4. 串入单向链表：current_rec -> insert_rec -> next_rec
    next_rec = page_rec_get_next(current_rec)
    page_rec_set_next(insert_rec, next_rec)
    page_rec_set_next(current_rec, insert_rec)

    # 5. 维护 Page Directory slot（n_owned 超阈值则分裂 slot）
    owner_rec = page_rec_find_owner_rec(insert_rec)
    n_owned = rec_get_n_owned(owner_rec)
    rec_set_n_owned(owner_rec, n_owned + 1)
    if n_owned == PAGE_DIR_SLOT_MAX_N_OWNED:
        page_dir_split_slot(page, owner_slot)

    # 6. 写 mini-transaction redo 日志
    page_cur_insert_rec_write_log(insert_rec, rec_size, current_rec, index, mtr)
    return insert_rec
```

**复杂度**：单页插入主路径 O(1)（指针改写 + memcpy），触发 `page_dir_split_slot` 时 O(slot 数)。

### 21.2 页面已满：`btr_page_split_and_insert` 的 split_rec 选择

源码：`reference/relational/mysql/storage/innobase/btr/btr0btr.cc:2315`。

`page_cur_insert_rec_low` 返回 NULL 说明页面放不下，进入分裂路径。**split_rec 的选择是分裂算法的核心**，MySQL 走「先尝试右兄弟 → 失败则按方向探测 → 仍失败则取中间记录」：

```text
function btr_page_split_and_insert(flags, cursor, offsets, heap, tuple, mtr):
    block = btr_cur_get_block(cursor); page = block.frame

    # 第 0 步：先尝试「偷」一个 slot 给右页（避免真分裂）
    rec = btr_insert_into_right_sibling(...)
    if rec != NULL: return rec   # 成功偷到 slot，直接插入

    # 第 1 步：根据迭代次数与上一次分裂方向决定 split_rec
    if n_iterations > 0:                    # retry 路径
        direction = FSP_UP                  # 强制向上分裂
        split_rec = btr_page_get_split_rec(cursor, tuple)
        insert_left = (tuple < split_rec)
    elif btr_page_get_split_rec_to_right(cursor, &split_rec):   # 右倾
        direction = FSP_UP
    elif btr_page_get_split_rec_to_left(cursor, &split_rec):    # 左倾
        direction = FSP_DOWN
    else:                                  # 兜底：取中间
        if page_get_n_recs(page) > 1:
            split_rec = page_get_middle_rec(page)
        elif tuple < first_user_rec:
            split_rec = first_user_rec       # 1 条记录 + 偏向左
        else:
            split_rec = NULL                 # 1 条记录 + 偏向右（新记录独占新页）

    # 第 2 步：在 hint_page_no 附近申请新页
    new_block = btr_page_alloc(cursor->index, hint_page_no, direction,
                                btr_page_get_level(page), mtr, mtr)

    # 第 3 步：把 [split_rec, end] 移到新页
    if split_rec: first_rec = move_limit = split_rec
    else:         first_rec = page_rec_get_next(page_get_infimum_rec(page))

    # 第 4 步：插入 tuple 到合适一侧
    insert_left = (tuple < first_rec)
    page_cur_insert_rec_low(left_or_right_block, ..., tuple, mtr)

    # 第 5 步：递归向上更新父节点（可能引发父页级联分裂）
    btr_attach_half_pages(...)
```

**关键设计**：
- **`insert_left` 优化**：若 tuple < split_rec 就直接放左页，避免在新页二次分裂。压缩页 `page_zip` 时若已分过一次，直接清空右页让 tuple 占整新页。
- **retry loop（`func_start` 标号）**：若新申请页与现有页重叠（rare，缓冲池驱逐极端情况），回到 `func_start` 重选 split_rec，限制 n_iterations 防死循环。
- **`btr_insert_into_right_sibling`**：避免真分裂的「偷 slot」路径——`innodb_index_change_buffering` 与右兄弟页面有空 slot 时生效。

### 21.3 复杂度分析

| 阶段 | 复杂度 | 备注 |
|------|--------|------|
| 单页插入（不分裂） | O(1) 均摊 | 含 slot 维护 |
| 单页插入（分裂） | O(n) | n = 页内记录数，需移动后半段到新页 |
| 父页级联分裂 | O(h × n) | h = 树高，InnoDB 通常 3-4 |
| 偷 slot（right sibling） | O(1) | 命中时最优 |

**与自增主键的关系**：自增主键总是追加到最右叶子页，使 `split_rec` 偏向「中间靠右」且右兄弟多为新页（顺序追加），写入放大最小——这是 §15.3「推荐自增主键」的算法层根据。

### 21.4 本项目对应

`engineering/src/db/storage/bufmgr.c`（Clock-Sweep）与 `engineering/src/db/btreeam.c` 的 B+Tree 实现可参考 InnoDB 的 split_rec 选择策略（特别在 `btr_page_get_split_rec_to_left/right`），但因本项目是教学版，目前按 `page_get_middle_rec` 兜底实现，未做方向探测。

## 二十二、InnoDB MVCC 可见性算法与 ReadView 二分查找

§10.2 提到「快照读」「`DB_TRX_ID` / `DB_ROLL_PTR`」「`trx_id < m_up_limit_id && not in m_ids`」三条结论，但**没展开 ReadView 的数据结构与判定流程**。本节聚焦可见性的实现细节。

### 22.1 ReadView 数据结构

源码：`reference/relational/mysql/storage/innobase/include/read0types.h:49`（类定义）、`:244-280`（私有字段）。

ReadView 在事务首次一致性读（SELECT 不带锁）时构造，包含四个关键字段：

```cpp
class ReadView {
 private:
    trx_id_t m_low_limit_id;   // 高水位：read view 不应见 >= 此 id 的事务
    trx_id_t m_up_limit_id;    // 低水位：read view 应见 < 此 id 的事务
    ids_t     m_ids;           // 创建 read view 时仍活跃的事务 id 列表（已排序）
    trx_id_t m_creator_trx_id; // 创建本 view 的事务 id（自己改的对自己可见）
};
```

**构造时机**：`trx_assign_read_view()` 在 REPEATABLE READ 下事务首个 SELECT 时调用一次（RC 下每语句重新构造）。

### 22.2 可见性判定公式：`ReadView::changes_visible`

源码：`reference/relational/mysql/storage/innobase/include/read0types.h:159-175`。

```cpp
bool ReadView::changes_visible(trx_id_t id) const override {
    ut_ad(id > 0);

    // 1. 低水位之下 或 自己 → 可见
    if (id < m_up_limit_id || id == m_creator_trx_id)
        return true;

    // 2. 高水位之上 → 不可见
    if (id >= m_low_limit_id)
        return false;

    // 3. 中间地带：在创建快照时活跃的事务列表里二分查找
    //    m_ids 已排序 → std::binary_search，O(log N)
    if (m_ids.empty())
        return true;
    const auto *p = m_ids.data();
    return !std::binary_search(p, p + m_ids.size(), id);
}
```

**判定流程伪代码**（更直观版）：

```text
function is_visible(trx_id, undo_chain_head) -> (visible: bool, next_undo: undo_ptr):
    # 1. 快速判定（直接基于 ReadView）
    if trx_id < view.m_up_limit_id: return TRUE
    if trx_id >= view.m_low_limit_id: return FALSE
    if trx_id in view.m_ids (binary search): return FALSE

    # 2. 该版本对当前快照可见
    return TRUE
```

### 22.3 Undo log 链回溯

若当前记录的 `DB_TRX_ID` 对快照不可见，沿着 `DB_ROLL_PTR` 在 undo log 中找更老版本：

```text
function find_visible_version(record, view) -> tuple:
    cur = record
    trx_id = read_DB_TRX_ID(cur)

    while not view.is_visible(trx_id):
        roll_ptr = read_DB_ROLL_PTR(cur)
        if roll_ptr == NULL: raise error "no visible version"
        cur = undo_log_read(roll_ptr)        # 沿 DB_ROLL_PTR 链回溯
        trx_id = read_DB_TRX_ID(cur)

    return cur
```

**Undo log 类型**（源码 `storage/innobase/trx/trx0undo.cc`）：
- `TRX_UNDO_INSERT`：仅 INSERT 事务产生，回滚后即可丢弃（无 MVCC 价值）
- `TRX_UNDO_UPDATE`：UPDATE/DELETE 产生，构成 MVCC 链，需等所有可能访问该版本的 read view 关闭后才能 purge

### 22.4 复杂度分析

| 操作 | 复杂度 | 工程含义 |
|------|--------|----------|
| `changes_visible` 判定 | O(log N) | N = `m_ids.size()`，通常 ≤ 活跃事务数 |
| 单行可见性查找 | O(L) | L = undo 链长度（长事务下恶化） |
| ReadView 创建 | O(N) | N = 当前活跃事务数（需拷贝 id 列表并排序） |
| Purge undo | O(K) | K = 待回收 undo 页数，受 `innodb_purge_threads` 与 `innodb_purge_batch_size` 控制 |

**关键优化**：
- **Hint Bits**：在 tuple header 上缓存 `HEAP_XMIN_IS_VISIBLE` / `HEAP_XMAX_IS_VISIBLE`，命中后跳过 ReadView 判定（直读 tuple 头）。
- **快照复用**：REPEATABLE READ 下 read view 全事务复用，RC 下每语句新 view（隔离级别越严格，read view 越长寿）。
- **`MVCC::get_view_trx_ids`**：purge 线程据此找出「最老的 read view」，回收早于该 view 的 undo。

### 22.5 本项目对应

`engineering/src/db/heapam.c` 的 MVCC 实现可参考 InnoDB 的三段式判定（low/high/liddle），但目前用简化版「事务 id 单调递增 + last_committed 比较」即可满足教学需求，未实现完整 ReadView 与 undo 链。

## 二十三、Hash Join 算法与查询代价模型

§9.3 仅说「Hash Join（v8.0.18+）含经典 grace hash 与 chained hash」，**没展开算法细节与 cost 公式**。MySQL 8.0.20 起 BNL 被淘汰，Hash Join 成为默认的 equi-join 实现。

### 23.1 经典 Grace Hash Join（内存充足）

源码：`reference/relational/mysql/sql/iterators/hash_join_iterator.h:50-99`（类注释文档）。

**两阶段算法**（build + probe）：

```text
function classic_grace_hash_join(build_input, probe_input, join_cond):
    # Phase 1: BUILD —— 把内表装入哈希表
    hash_table = empty
    for row in build_input:
        key = extract_join_key(row, join_cond)
        if hash_table.has(key):
            hash_table[key].append(row)      # chaining 处理重复 key
        else:
            hash_table[key] = [row]
        if hash_table.estimated_size > join_buffer_size:
            spill_to_disk()                   # 转 on-disk 分区路径

    # Phase 2: PROBE —— 探测外表
    result = []
    for row in probe_input:
        key = extract_join_key(row, join_cond)
        if hash_table.has(key):
            for match in hash_table[key]:
                if satisfies_other_predicates(row, match):
                    result.append(combine(row, match))
    return result
```

**复杂度**：
- 时间：O(|build| + |probe|) 均摊（哈希表期望 O(1) 查找）
- 空间：O(|build|) 内存 + O(partition_count × avg_chunk) 磁盘（溢出时）

### 23.2 磁盘溢出：分区 + 多趟合并

源码：`reference/relational/mysql/sql/iterators/hash_join_iterator.h:99-115`（类注释 step 1-3）、`hash_join_iterator.cc` 中的 `InitRowBuffer/InitProbeIterator/InitWritingToProbeRowSavingFile`。

当 build 输入超过 `join_buffer_size`（默认 256 KB，可调）时，进入 grace hash 的磁盘溢出路径：

```text
function on_disk_hash_join(build_input, probe_input, n_chunks = kMaxChunks):
    # Step 1: 分区 —— 用 hash(key, partition_id) 把 build 与 probe 同时分到 n_chunks 对文件中
    for row in build_input:
        chunk_id = partition_hash(extract_join_key(row))
        write_to_chunk_file('build', chunk_id, row)
    for row in probe_input:
        chunk_id = partition_hash(extract_join_key(row))
        write_to_chunk_file('probe', chunk_id, row)

    # Step 2: 递归处理每一对 chunk
    for chunk_id in 0..n_chunks-1:
        if chunk_size > memory_limit:
            recurse_with_smaller_n_chunks(chunk_id)   # 嵌套分区
        else:
            classic_grace_hash_join(
                build_chunk(chunk_id),
                probe_chunk(chunk_id),
                join_cond)
```

**Probe Row Saving**（源码 `hash_join_iterator.h:194-220`）：当 build chunk 大到同一 probe 行需读多次（如 semijoin 不允许重复输出），把 probe 行先写到「save file」，每次重读时从 save file 取，确保不重复输出。

### 23.3 JoinType 与算法选择

源码：`reference/relational/mysql/sql/join_type.h:28-43`：

```cpp
enum class JoinType {
  INNER,           // 内连接
  OUTER,           // 左外连接
  ANTI,            // 反连接 (NOT EXISTS)
  SEMI,            // 半连接 (EXISTS)
  FULL_OUTER       // 全外连接
};
```

**Hash Join 选择判据**（来自优化器 `sql/join_optimizer/`）：
- 等值连接（`join_cond` 是 `=`）：Hash Join 候选
- 非等值（`<`、`>`）：退化 NLJ
- 驱动表行数 × 内表行数极大 且 索引不可用：CBO 倾向 Hash Join

### 23.4 Hypergraph Cost 模型公式

源码：`reference/relational/mysql/sql/join_optimizer/cost_constants.h:62-145`（MySQL 8.0.31+ 新代价模型，hypergraph-based）。

MySQL 8.0.31 引入「Hypergraph」优化器（`optimizer_switch=hypergraph_optimizer=ON`），cost 常量重新校准：

| 常量 | 值（单位成本 = kUnitCostInMicroseconds = 0.434 μs） | 含义 |
|------|----------|------|
| `kReadOneRowCost` | 0.1 / 0.434 ≈ 0.230 | 读取一行的固定成本 |
| `kReadOneFieldCost` | 0.02 / 0.434 ≈ 0.046 | 每读一列的增量成本 |
| `kReadOneByteCost` | 0.001 / 0.434 ≈ 0.0023 | 每读一字节的增量成本 |
| `kApplyOneFilterCost` | 0.025 / 0.434 ≈ 0.058 | 单谓词单行评估成本 |
| `kIndexLookupPageCost` | 0.5 / 0.434 ≈ 1.152 | 索引查找每页成本 |
| `kSortOneRowCost` | 0.15 / 0.434 ≈ 0.346 | 排序读入缓冲区的固定成本 |
| `kSortComparisonCost` | 0.014 / 0.434 ≈ 0.032 | 排序比较成本 |

**单行读取代价公式**（`cost_constants.h:71-72`）：

```
cost_read_row = kReadOneRowCost
              + kReadOneFieldCost × num_fields_in_read_set
              + kReadOneByteCost × length_of_record_buffer
```

**IO 代价公式**（`cost_model.h:74`，不在 `cost_constants.h`）：

```
io_cost = kIOStartCost + n_bytes × kIOByteCost
       = 937.0 + n_bytes × 0.0549
```

`kIOStartCost{937.0}` 与 `kIOByteCost{0.0549}` 定义在 `reference/relational/mysql/sql/join_optimizer/cost_model.h:85,88`。

**代价单位校准**（`cost_constants.h:57-61`）：`1.0 unit = 0.434 μs`，定义为「InnoDB 上 10 列整数表 1M 行的 `SELECT * FROM t` 表扫描单行平均耗时」。

### 23.5 Hash Join 复杂度对比

| 算法 | 时间 | 空间 | 适用 |
|------|------|------|------|
| **NLJ（Nested Loop）** | O(\|outer\| × \|inner\|) | O(1) | 小驱动表 / 内表有索引 |
| **BNL（Block NL，已废弃）** | O(\|outer\| × \|inner\| / block_size) | O(block_size) | 8.0.20 前 |
| **Hash Join（内存）** | O(\|build\| + \|probe\|) 均摊 | O(\|build\|) | 等值连接、内存够 |
| **Hash Join（磁盘溢出）** | O(\|build\| + \|probe\|) × partition_pass | O(chunk_size) | 超大表连接 |

**AHI 影响**：自适应哈希索引使索引查找代价介于「AHI 命中（接近常数）」与「AHI 未命中（树高 × 页查找）」之间，MySQL 故意取两者插值（`cost_model.h::IndexLookupCost`）。

### 23.6 本项目对应

`engineering/src/db/sql/sql_planner.c` 的代价模型只实现了简单「rows × cost_factor」估算，未实现 hypergraph join ordering（基于 DP 或 GEQO 遗传算法），亦无 Hash Join 算子——本项目 join 直接走 Nested Loop，这是教学取舍；商用 MySQL 的 Hypergraph 优化器可作为未来扩展方向（`sql/join_optimizer/join_optimizer.cc`）。