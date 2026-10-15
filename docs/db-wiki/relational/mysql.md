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