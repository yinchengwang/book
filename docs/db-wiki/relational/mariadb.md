# MariaDB Wiki（关系型数据库 / MySQL 社区分叉）

> 版本：本次参考 MariaDB-server trunk `13.1.0-alpha`（commit `bab03b0fc`）。MariaDB 长期与 MySQL 保持协议兼容，但内核已独立演进多年。

## 一、项目定位

MariaDB 是 MySQL 的**真正开源分叉**，由 MySQL 创始人 Michael Widenius（Monty）于 2009 年在 Oracle 收购 Sun 后创建。它**保持与 MySQL 协议兼容**，但在存储引擎、性能、新特性、安全性、社区治理上**走自己的路线**。MariaDB 是**GPLv2 双许可**（继承自 MySQL）。

- **痛点**：MySQL 被 Oracle 收购后，社区担忧 Oracle 收紧开源策略（如关闭测试用例、增加企业版特性）；开发者需要一个真正由社区治理的 MySQL 兼容替代品。
- **典型用户**：Wikipedia（2009 年从 MySQL 迁移 MariaDB）、Google 内部部分 MySQL 服务、Booking.com、Slack、Tumblr、Palo Alto Networks、ServiceNow、Deutsche Bank；以及大量 Linux 发行版默认打包（Debian、Ubuntu、Fedora、openSUSE、Arch）。

## 二、发展历史与社区

- **起源**：2009 年 1 月 Monty 创立 Monty Program Ab，发布 MariaDB 5.1（基于 MySQL 5.1）。
- **里程碑**：
  - 2010 年 MariaDB 5.5（基于 MySQL 5.5）。
  - 2012-2013 年 10.0：合并 MySQL 5.6 的部分特性，加入 Aria 引擎、新 SQL 模式。
  - 2014 年 10.1：引入 Galera 4（同步多主复制）、GIS 增强、Oracle 兼容模式。
  - 2016 年 10.2：CTE、窗口函数、JSON。
  - 2018 年 10.3：系统版本表（System Versioned Tables）、多源复制。
  - 2019 年 10.4：容器化强化、ColumnStore（早期版本）。
  - 2020 年 10.5：ColumnStore 2.0、S3 存储引擎、MariaDB Platform。
  - 2022-2024 年 10.x/11.x：增强 Galera 4、Spider 分片、连接池插件。
- **社区**：MariaDB Foundation（基金会）+ MariaDB Corporation（公司）+ MariaDB plc（NYSE 上市）；主导 100+ 工程师，每年 MariaDB Day、用户大会。
- **维护节奏**：每年 4 个季度版本，长期支持（Stable）分支 LTS 5 年。

## 三、许可与商业限制

- **许可证**：**GPLv2**（继承 MySQL，部分代码 Apache 2.0 / BSD）。
  - 来源：`reference/relational/mariadb/COPYING`
- **商业版**：MariaDB Enterprise Server、MariaDB MaxScale（连接池代理）、MariaDB ColumnStore（列存）。
- **商业公司**：MariaDB plc（NYSE: MRDB）提供订阅、SaaS、Cloud 服务。
- **GPL 传染风险**：与 MySQL 相同；通过 TCP 连接不触发；fork 服务端代码需开源。

## 四、业界主要使用方与案例

- **互联网**：Wikipedia（最先大规模迁移）、Google Cloud 部分服务、Pinterest（部分）、CloudFlare（部分元数据）、Booking.com、Slack（早期）、Tumblr。
- **Linux 发行版**：Debian、Ubuntu、Fedora、Arch、openSUSE、Mandriva、Manjaro 默认打包 MariaDB。
- **金融**：Deutsche Bank 内部部分系统、ScotiaBank、Saxo Bank。
- **云**：AWS RDS MariaDB、Azure MariaDB、Google Cloud SQL（MariaDB）。
- **企业**：ServiceNow（基于 MariaDB）、Palo Alto Networks、Samsung、Sky、小米部分业务。
- **中国**：部分云厂商提供 MariaDB 服务。

## 五、一句话对比定位

> MariaDB 是 **MySQL 真正的开源继承者**（fork 而非 reimplementation），保持 wire-protocol 兼容 + 大量 MySQL 缺失特性 + 更独立的治理；比 MySQL 更社区驱动、更兼容多引擎（ColumnStore、Spider、RocksDB）、比 PG 更易迁移自 MySQL。

## 六、整体架构

MariaDB 与 MySQL 架构**高度相似**（同样基于单进程多线程 + 分层插件），但内核已经独立演进多年。

```
┌──────────────────────────────────────────────────────────────────────┐
│                       客户端（与 MySQL 兼容）                          │
│           libmariadb / libmysqlclient / JDBC / ODBC / .NET            │
└─────────────────────────┬────────────────────────────────────────────┘
                          │ MariaDB/MySQL Wire Protocol (端口 3306)
┌─────────────────────────▼────────────────────────────────────────────┐
│                   mariadbd（单进程多线程）                              │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ Connection Layer (sql/conn_handler/, sql/sql_client.cc)        │    │
│  │  - Thread per connection / Thread Pool plugin                 │    │
│  │  - 协议握手 + 认证                                             │    │
│  └──────────────────────────────────────────────────────────────┘    │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ SQL Layer (sql/)                                              │    │
│  │  - Lexer/Parser (sql/lex.h, sql/sql_yacc.yy)                  │    │
│  │  - Optimizer (sql/sql_optimizer.cc)                            │    │
│  │    含 RBO + CBO + 直方图 + ATTRIBUTE OPTIMIZATION               │    │
│  │  - Executor (sql/sql_executor.cc, handler.cc)                 │    │
│  │  - binlog (sql/binlog.cc, libbinlogevents/)                    │    │
│  │  - Replication (sql/rpl_*.cc)                                  │    │
│  └──────────────────────────────────────────────────────────────┘    │
│  ┌──────────────────────────────────────────────────────────────┐    │
│  │ 存储引擎 API (sql/handler.h)                                    │    │
│  │  handler::create / read / write_index / update / delete        │    │
│  └──────┬───────────────────────────────────────┬─────────────┐    │
│         │                                       │             │    │
│  ┌──────▼──────────────┐  ┌────────────────▼───┐ ┌───────────▼─┐  │
│  │ InnoDB (默认, 来自 │  │  Aria (前 MyISAM,    │ │ ColumnStore │  │
│  │ MySQL/InnoDB 移植) │  │  MariaDB 增强崩溃安全)│ │  列存       │  │
│  │ storage/innobase/  │  │  storage/maria/      │ │ storage/col/│  │
│  │  - Buffer Pool     │  │                      │ │ RocksDB     │  │
│  │  - Redo/Undo Log   │  │  MyISAM (legacy)     │ │ storage/rk/ │  │
│  │  - B+Tree / MVCC   │  │  storage/myisam/     │ │ Spider      │  │
│  │  - 继承 MySQL 8.x  │  │                      │ │ storage/sp/ │  │
│  └────────────────────┘  └──────────────────────┘ └─────────────┘  │
└─────────────────────────┬────────────────────────────────────────────┘
                          │
┌─────────────────────────▼────────────────────────────────────────────┐
│                     文件系统层                                          │
│   .ibd / .maria / .myi / ColumnStore 内置 MariaDB ColumnStore         │
│   / redo log / undo log / binlog / relay log                           │
└─────────────────────────────────────────────────────────────────────┘
```

源码入口：
- `sql/sql_parse.cc:10463`（主入口）。
- `sql/handler.cc:9773`（handler 总入口）。
- `storage/innobase/buf/buf0buf.cc`（Buffer Pool，来自 MySQL 移植）。
- `storage/innobase/row/row0ins.cc:3111`（聚簇索引插入）。
- `storage/maria/`（Aria 引擎实现）。

## 七、存储引擎

MariaDB 提供**比 MySQL 更丰富的存储引擎生态**。

### 7.1 引擎对比

| 引擎 | 事务 | 锁粒度 | 索引 | 适用 | 来源 |
|------|------|--------|------|------|------|
| **InnoDB** | ACID | 行锁 + MVCC | B+Tree (聚簇) | OLTP 默认 | 移植自 MySQL |
| **Aria**（Maria 内部） | 原子写 | 行/页锁 | B+Tree | crash-safe MyISAM | MariaDB 独有 |
| **MyISAM** | 无 | 表锁 | B+Tree (非聚簇) | 读多写少 | 遗产 |
| **ColumnStore** | ACID | 行锁 | B+Tree + Bitmap | OLAP | MariaDB 增强 |
| **Spider** | 跟底层引擎 | 行锁 | B+Tree | 分片联邦 | MariaDB 增强 |
| **RocksDB** | ACID | 行锁 | LSM-Tree | 写密集、闪存优化 | Facebook |
| **Mroonga** | ACID | 行锁 | B+Tree + 倒排 | 全文、多语种 | Groonga |
| **S3** | - | - | - | S3 远程表 | MariaDB |
| **CONNECT** | 跟源 | - | - | 访问 CSV / ODBC / MongoDB | MariaDB |
| **Sequence** | - | - | - | 虚拟序列 | MariaDB |
| **ColumnStore 2.0** | ACID | 行锁 | 列存 | HTAP | MariaDB |

### 7.2 Aria 引擎

- 原名 Maria，**MariaDB 默认内部引擎**（取代 MyISAM 作为系统表引擎）。
- 特色：**崩溃安全日志（log handler）**记录所有写操作，启动时自动恢复。
- v10.5+ 支持事务（`ENGINE=ARIA TRANSACTIONAL=1`）。
- 源码：`storage/maria/`。

### 7.3 ColumnStore

- 列存引擎，原名 InfiniDB，被 MariaDB 收购后集成。
- 适合 OLAP 大表聚合查询。
- 11.x+ 重大重构为分布式 columnstore。

### 7.4 Spider 分片联邦

- 提供联邦查询 + 自动分片路由（水平分片）。
- 适合读写分离与跨节点 JOIN。

## 八、索引机制

### 8.1 InnoDB B+Tree

- 继承自 MySQL InnoDB，聚簇索引 + 二级索引。
- 源码：`storage/innobase/`（与 MySQL 同步）。

### 8.2 Aria B+Tree

- 同 MyISAM，但支持崩溃恢复。
- 支持全文索引（`FULLTEXT`）。

### 8.3 ColumnStore 索引

- 倒排索引 + bitmap 索引，针对列存查询优化。

### 8.4 函数索引与表达式索引

- 支持 `CREATE INDEX ON t (lower(name))`（继承 MySQL 8.0.13+ 特性）。

## 九、查询与执行

### 9.1 SQL 扩展（MySQL 之上）

MariaDB **早于 MySQL** 引入多项特性：

- **CTE（Common Table Expressions）**：v10.2 引入（MySQL v8.0）。
- **窗口函数**：v10.2 引入（MySQL v8.0）。
- **系统版本表**（System-Versioned Tables）：v10.3，PostgreSQL 风格的时序表。
- **JSON 函数**：扩展（`JSON_TABLE`、`JSON_MERGE_PATCH`）。
- **Oracle 兼容模式**：`sql_mode='Oracle'` 包含 `ROWNUM`、`DECODE`、`DUAL`、`SEQUENCE.NEXTVAL`。
- **Sequence 对象**：v10.3，`CREATE SEQUENCE` 与 `NEXTVAL`。
- **RETURNING 子句**：v10.5，`INSERT ... RETURNING ...`（MySQL 直到 v15 才部分支持）。
- **`EXECUTE IMMEDIATE`**：v10.2。
- **CHECK 约束强制**：v10.2 之前 MySQL 不强制 CHECK（解析但忽略），MariaDB 早期版本就强制。

### 9.2 优化器

- 与 MySQL 兼容的 CBO + 直方图。
- 增强：**Index Merge 优化**、**Condition Pushdown**、**Subquery Materialization**。
- **基于成本的并发查询选择**。

### 9.3 执行器

- Volcano 迭代器模型 + 子查询展开。
- 新执行器改写自 MySQL 8.x。

### 9.4 Explain Analyzer

- `ANALYZE FORMAT=JSON` 返回执行统计。
- 第三方工具：`dbt-mariadb`、`mdbx-analyzer`。

## 十、事务与一致性

### 10.1 隔离级别

与 MySQL 完全一致：
- READ UNCOMMITTED
- READ COMMITTED（v10.0+ 默认）
- REPEATABLE READ（v10.0+ 默认，MySQL 5.7 风格）
- SERIALIZABLE

### 10.2 MVCC

- 继承 InnoDB 的 MVCC 实现。

### 10.3 Galera 同步复制（特色）

- v10.1+ 引入 Galera 4（`wsrep` 协议），基于 certification-based 多主同步。
- 任意节点写入，所有节点同步提交；冲突时回滚；提供 RPO=0。
- 源码：`storage/innobase/wsrep/`。
- 需要至少 3 节点形成多数派。

### 10.4 系统版本表

- `CREATE TABLE t (id INT, name VARCHAR(50), SYS_START TIMESTAMP(6) GENERATED ALWAYS AS ROW START, SYS_END TIMESTAMP(6) GENERATED ALWAYS AS ROW END, PERIOD FOR SYSTEM_TIME (SYS_START, SYS_END)) WITH SYSTEM VERSIONING`。
- 提供 `AS OF TIMESTAMP t`、`FROM t1 TO t2`、`BETWEEN t1 AND t2`、`WITHOUT OVERLAPS` 等时序查询。

## 十一、复制与高可用

### 11.1 主从复制

- 与 MySQL 主从兼容，binlog 格式（ROW/STATEMENT/MIXED）。
- 多源复制、增强 GTID。

### 11.2 Galera Cluster

- **同步多主**复制（v10.1+）：所有节点可读写，事务通过 `wsrep` 协议同步。
- 节点数：3+（多数派协议）。
- 适合：强一致性需求、地理分散节点（注意跨机房延迟）。

### 11.3 半同步复制

- 与 MySQL semi-sync 类似。

### 11.4 高可用方案

- **MariaDB MaxScale**：智能代理，读写分离 + 自动读写路由 + binlog server。
- **MHA** / **Orchestrator**（可管理 MariaDB）。
- **MariaDB Enterprise Backup**：物理热备。

## 十二、性能与基准

- **Sysbench OLTP**：与 MySQL 8.0 相当，部分场景略优（Aria 引擎减少元数据写）。
- **TPC-C**：单机 100k+ tpmC。
- **Galera 性能**：3 节点同步复制，吞吐较主从低 20-40%，延迟增加 10-20ms。
- **典型调优**：
  - `innodb_buffer_pool_size = 60% RAM`
  - `innodb_log_file_size = 2-4 GB`
  - `innodb_flush_log_at_trx_commit = 1`（强持久）或 2
  - Galera：`wsrep_slave_threads`、`gcache.size`

## 十三、生态与集成

### 13.1 客户端 SDK

- **完全兼容** MySQL Connector/C、Connector/J、Connector/NET、Connector/Python。
- **C++**：MariaDB Connector/C++（社区版）、Drogon。
- **Python**：`mysqlclient`、`PyMySQL`、`mariadb` 官方 Python Connector、SQLAlchemy、Django。
- **Go**：`go-sql-driver/mysql`、MariaDB MaxScale Go driver。
- **Node.js**：`mysql2`、`mariadb`（官方）、`sequelize`、`prisma`、`drizzle`。
- **Rust**：`sqlx`、`mariadb-rs`。

### 13.2 工具链

- **mariadb**（CLI）：替代 mysql。
- **mariadb-dump** / **mariadb-restore**：替代 mysqldump。
- **mariadb-binlog** / **mariadb-binlog-dump**：binlog 解析。
- **mariadb-check**：表检查 + 修复。
- **mariadb-upgrade**：版本升级。
- **MaxScale**（商业）：智能代理。
- **MariaDB Backup**：物理热备（Percona XtraBackup 的 MariaDB fork）。

### 13.3 容器化

- 官方 `mariadb` Docker 镜像。
- Helm Chart：Bitnami MariaDB。
- Kubernetes Operator：MariaDB Operator（官方）、Presslabs。

## 十四、源码学习路径

1. **入口**：`sql/sql_parse.cc:10463`（主入口）、`sql/handler.cc:9773`（handler 总入口）。
2. **SQL 解析**：`sql/sql_yacc.yy`、`sql/lex.h`、`sql/sql_lex.cc`。
3. **优化器**：`sql/sql_optimizer.cc`。
4. **InnoDB**：`storage/innobase/buf/buf0buf.cc`、`row/row0ins.cc`、`log/log0log.cc`。
5. **Aria**：`storage/maria/`（默认崩溃安全引擎）。
6. **ColumnStore**：`storage/columnstore/`（列存）。
7. **Galera**：`storage/innobase/wsrep/`（同步复制）。
8. **执行器**：`sql/handler.h` + `sql/handler.cc`。

源码规模：约 500 万行 C/C++ 代码（与 MySQL 类似，加上独有的 ColumnStore、Spider、Connect 等）。

## 十五、风险与陷阱

### 15.1 MySQL 兼容陷阱

- **系统表差异**：MariaDB 大量系统表与 MySQL 不同；mysqldump 迁移需注意 `mysql.*` 表。
- **认证插件差异**：MariaDB 默认 `mysql_native_password`；迁移到 MySQL 8.x 默认 `caching_sha2_password` 可能出错。
- **时区表**：MariaDB 维护自己的 mysql.timezone 表，与 MySQL 不一定同步。
- **SQL 方言差异**：MySQL 8.0 引入的 JSON 函数可能与 MariaDB 不兼容。

### 15.2 运维雷区

- **Galera 脑裂**：网络分区时脑裂；务必配 `pc.weight` + `evs.suspect_timeout`。
- **Galera 大事务**：单事务 > 1GB 会阻塞整个集群。
- **ColumnStore 启动慢**：需要重建元数据，启动时间分钟级。
- **主从延迟**：与 MySQL 一致，需要监控 `Seconds_Behind_Master`。

### 15.3 性能陷阱

- **错误使用 MyISAM**：v10.5+ 默认引擎仍是 InnoDB。
- **不启用慢查询日志**：`long_query_time=0`、`slow_query_log=ON` 默认关闭。
- **Galera 节点过少**：3 节点以下不支持多数派，无法选主。

### 15.4 迁移痛点

- **MySQL → MariaDB**：通常向下兼容，直接替换 server 二进制即可；但需注意 `mysql_upgrade` 步骤。
- **MariaDB → MySQL**：逆向迁移困难，需要重建系统表 + 重新调整 SQL（MySQL 无 RETURNING、系统版本表）。

## 十六、学习路径

### 16.1 入门（1 周）

1. 安装 MariaDB（`apt install mariadb-server` 或官方仓库）。
2. 用 `mariadb` CLI 执行 DDL/DML；与 MySQL 用法一致。
3. 学习 `SHOW STATUS`、`SHOW ENGINE INNODB STATUS`、`EXPLAIN`。

### 16.2 进阶（1-2 月）

1. 阅读官方文档：<https://mariadb.com/kb/en/>
2. 学习 Galera 集群部署（3 节点）。
3. 学习系统版本表（System Versioned Tables）。
4. 学习 Aria 引擎 + 崩溃恢复。

### 16.3 高级（3-6 月）

1. 深入源码：`storage/innobase/`、`storage/maria/`、`storage/columnstore/`。
2. 部署 MaxScale 读写分离。
3. 性能调优：`pt-query-digest`、`SHOW ENGINE INNODB STATUS`。

### 16.4 推荐资源

- 官方文档：<https://mariadb.com/kb/en/documentation/>
- 官方 Knowledge Base：<https://mariadb.com/kb/>
- 《MariaDB 入门到精通》：开源教程
- 会议：MariaDB Day、FOSDEM MariaDB devroom
- 中文社区：MariaDB 中文社区、腾讯云开发者社区

## 十七、可移植性与部署

- **平台**：Linux（所有发行版）、macOS、Windows、BSD、Solaris。
- **国产化**：支持鲲鹏、海光、麒麟、统信。
- **容器化**：官方 `mariadb` Docker 镜像；调整 `MARIADB_ROOT_PASSWORD` 环境变量。
- **Kubernetes**：MariaDB Operator、Bitnami Helm、Presslabs。
- **云端**：AWS RDS MariaDB、Azure Database for MariaDB、Google Cloud SQL、阿里云 RDS、华为云。

## 十八、变体与衍生项目

- **MariaDB ColumnStore**：列存引擎，OLAP 场景。
- **MariaDB MaxScale**：商业版代理。
- **MariaDB SkySQL**：托管云服务（2024 年起）。
- **Galera Cluster for MySQL**：同源技术，但 MariaDB 官方支持更完善。
- **TiDB**：基于 MySQL 协议的分布式扩展（详见 `docs/db-wiki/distributed-sql/tidb.md`）。
- **CockroachDB**：基于 PG 协议（详见 `docs/db-wiki/distributed-sql/cockroachdb.md`）。

## 十九、与本项目的关联

- MariaDB 的 **Aria 引擎**（崩溃安全 + 事务）是 MySQL 缺失的特性；本项目 `engineering/src/db/` 设计的 Buffer Pool + redo log 类似 Aria 思想。
- **Galera 集群**的同步多主设计可作为本项目未来分布式扩展的参考。
- **`learning/playground/mysql_test/`** 可作为 MariaDB 测试驱动模板（API 兼容）。

## 二十、参考资料

1. MariaDB 官方文档：<https://mariadb.com/kb/en/documentation/>
2. MariaDB 源码 `sql/sql_parse.cc:10463`、`sql/handler.cc:9773`
3. MariaDB 源码 `storage/innobase/`、`storage/maria/`、`storage/columnstore/`
4. Galera Cluster 文档：<https://galeracluster.com/documentation/>
5. MariaDB Foundation：<https://mariadb.org/>
6. MariaDB Performance Tuning Guide：<https://mariadb.com/kb/en/optimization-and-tuning/>
7. 《MariaDB 性能优化》：社区电子书
8. 中文教程：<https://www.mariadbtutorial.com/>
9. MariaDB 兼容性指南（MySQL）：<https://mariadb.com/kb/en/mariadb-vs-mysql-compatibility/>
10. SkySQL 文档：<https://skysql.com/docs/>

## 二十一、Aria 事务引擎：TRN 事务管理 + Loghandler 双记录日志

Aria（原 Maria）是 MariaDB 内置的 **崩溃安全 + 事务** 引擎，相对于 MyISAM 的关键升级是 **写前日志（Write-Ahead Logging, WAL）+ 事务对象（TRN）** 的联合设计。这一节深入 Aria 不同于 MySQL InnoDB 的事务子系统实现——与 MyISAM 相比，多了 TRN 层的可见性调度与 Loghandler 层的物理日志编码。

### 21.1 TRN 事务对象与 visibility API

Aria 的事务抽象为 `TRN` 结构（`storage/maria/ma_trnman.h`），通过 **6 字节长 TrID + 2 字节短 `SHORT_TRID_MAX=65535`** 双层 ID 编码写入每条日志记录上：

```c
// storage/maria/trnman_public.h:30-46
typedef uint64 TrID;            // 6 bytes (实际存储用 uint6korr)
typedef struct st_ma_transaction TRN;

#define SHORT_TRID_MAX 65535

TRN *trnman_new_trn(WT_THD *wt);                  // 创建并返回新 TRN 句柄
my_bool trnman_end_trn(TRN *trn, my_bool commit); // 通用结束（commit/rollback）
#define trnman_commit_trn(T)    trnman_end_trn(T, TRUE)
#define trnman_abort_trn(T)     trnman_end_trn(T, FALSE)
#define trnman_rollback_trn(T)  trnman_end_trn(T, FALSE)
int   trnman_can_read_from(TRN *trn, TrID trid);   // MVCC-style 可见性判断
TrID  trnman_get_min_safe_trid();                  // 回收 / checkpoint 安全下界
```

调用链上的端到端事务流程（`storage/maria/ma_commit.c:30-132`）：

```
SQL BEGIN        → maria_begin(info)   → trnman_new_trn(0)   → _ma_set_trn_for_table(info, trn)
SQL INSERT/UPDATE → ma_write/update     → translog_write_record(LOGREC_UNDO_ROW_*) / LOGREC_UNDO_KEY_*
SQL COMMIT       → maria_commit(info)  → ma_commit(trn):
                                            1. translog_write_record(LOGREC_COMMIT) + translog_flush
                                            2. trnman_commit_trn(trn)   ← 仅在内存中标记 committed
SQL ROLLBACK     → maria_rollback      → trnman_rollback_trn(trn)
```

`trnman_can_read_from(trn, trid)` 是 Aria 版的 MVCC 可见性入口：返回 1 表示调用者 `trn` 可以读到 `trid` 标识的版本。等价于 PG/MySQL `HeapTupleSatisfiesMVCC`，但 Aria 的版本判定基于 **trnman 全局最小 TrID**（`trnman_get_min_trid()`），而非 InnoDB 那种基于 read_view 的 m_ids 列表。

### 21.2 Loghandler 双记录日志（undo + redo）

`storage/maria/ma_loghandler.h:22-50` 与 `:109-156` 定义了 Aria 的物理日志格式。关键常量与日志记录类型枚举：

```c
// storage/maria/ma_loghandler.h:22-50
#define TRANSLOG_PAGE_SIZE      (8U*1024)   // 单页大小，所有记录按页对齐
#define TRANSLOG_FILE_SIZE      (1024U*MB)  // 单个 log 文件上界
#define TRANSLOG_MIN_FILE_SIZE  (8U*MB)
#define TRANSLOG_PAGE_CRC       1U          // PAGE_CRC 校验
#define TRANSLOG_SECTOR_PROTECTION  (1U<<1) // 每扇区 XOR 校验（防撕裂写入）
#define TRANSLOG_RECORD_CRC     (1U<<2)     // RECORD_CRC
#define TRANSLOG_FLAGS_NUM      ((TRANSLOG_PAGE_CRC | TRANSLOG_SECTOR_PROTECTION
                                  | TRANSLOG_RECORD_CRC) + 1)

// ma_loghandler.h:377-383 — translog 全局状态机
enum enum_translog_status {
  TRANSLOG_UNINITED,    // 未初始化或初始化错误
  TRANSLOG_OK,          // 正常可写
  TRANSLOG_READONLY,    // 写错误后只读保护
  TRANSLOG_SHUTDOWN     // 正在卸载
};
extern enum enum_translog_status translog_status;

// ma_loghandler.h:109-156 — 物理日志记录类型（最多 64 种）
enum translog_record_type {
  LOGREC_REDO_INSERT_ROW_HEAD= 1, LOGREC_REDO_INSERT_ROW_TAIL,
  LOGREC_REDO_NEW_ROW_HEAD,      LOGREC_REDO_NEW_ROW_TAIL,
  LOGREC_REDO_PURGE_ROW_HEAD,    LOGREC_REDO_PURGE_ROW_TAIL,
  LOGREC_REDO_INDEX,             LOGREC_REDO_INDEX_NEW_PAGE,
  LOGREC_UNDO_ROW_INSERT,        LOGREC_UNDO_ROW_DELETE,
  LOGREC_UNDO_ROW_UPDATE,
  LOGREC_UNDO_KEY_INSERT,        LOGREC_UNDO_KEY_INSERT_WITH_ROOT,
  LOGREC_UNDO_KEY_DELETE,        LOGREC_UNDO_KEY_DELETE_WITH_ROOT,
  LOGREC_PREPARE,                LOGREC_PREPARE_WITH_UNDO_PURGE,
  LOGREC_COMMIT,                 LOGREC_COMMIT_WITH_UNDO_PURGE,
  LOGREC_CHECKPOINT,
  LOGREC_REDO_CREATE_TABLE,      LOGREC_REDO_RENAME_TABLE,
  LOGREC_REDO_DROP_TABLE,
  ...
  LOGREC_NUMBER_OF_TYPES = 64
};
```

**双记录设计的真实目的**：
- `LOGREC_UNDO_*` 仅在 transaction 进行中用于 **回滚**：写 translog 时同步更新 page，但 undo 信息必须先于 redo 落盘（ARIES 风格），崩溃恢复用最新 checkpoint + 扫描 `LOGREC_*_UNDO` 反向回滚。
- `LOGREC_REDO_*`（`REDO_NEW_ROW_HEAD`/`TAIL`/`REDO_INDEX_NEW_PAGE`）用于崩溃后 **恢复未刷盘的 page**：每行记录包含 raw page bytes，恢复时不依赖原 page 是否在 buffer pool。
- `LOGREC_PREPARE` / `LOGREC_COMMIT` 协同判定事务状态：恢复时若发现 PREPARE 但未 COMMIT，结合 `trnman_get_min_safe_trid()` 决定是否回滚。

LOGREC 写入函数（`storage/maria/ma_loghandler.c:6403-6489`）流程：先调用 `translog_assign_id_to_share(tbl_info, trn)` 给 `MARIA_SHARE` 分配 short id，再 `translog_write_record` 串接 `parts_data[]`、写 hidden header（type + short_trid + length），最后把整页挂到 in-flight 队列等待 `translog_buffer_lock`/`unlock` 刷盘。

### 21.3 ma_commit 的 commit-then-flush 顺序保证

`ma_commit`（`storage/maria/ma_commit.c:30-82`）的伪代码如下，**关键不变量是 "COMMIT 记录先于 `trnman_commit_trn` 落盘"**：

```c
int ma_commit(TRN *trn) {
    DBUG_ASSERT(trn->rec_lsn == LSN_IMPOSSIBLE);
    if (trn->undo_lsn == 0)                  // 空事务：rollback 比 commit 更便宜
        DBUG_RETURN(trnman_rollback_trn(trn));

    // 1. 先写 COMMIT log + 立刻 fsync
    res = translog_write_record(&commit_lsn, LOGREC_COMMIT, trn, NULL, 0,
                                sizeof(log_array)/sizeof(log_array[0]),
                                log_array, NULL, NULL)
        | translog_flush(commit_lsn);            // 强制刷盘

    // 2. 仅在内存中标记事务"committed"（更新 trnman 全局状态）
    res |= trnman_commit_trn(trn);
    return res;
}
```

不变量对应的源码注释（`:40-56`）：
> if COMMIT record is written before `trnman_commit_trn()`: if Checkpoint comes in the middle it will see trn is not committed, then if crash, Recovery might roll back trn … all in all it means that "trn committed" is not 100% equal to "COMMIT record written".
> So we need to go the first way.

即 **"COMMIT record 落盘" ⊋ "trnman_committed 内存可见"**：恢复器把 COMMIT record 当作权威判定；只要 record 已落盘，即使内存中 `trn` 未及时标记，其他 reader 也会通过重放视为 committed。

`maria_commit`（`:95-103`）在 `info->s->now_transactional == 0` 时（即非事务表）直接 `return 0`——**非事务表不写任何日志**，仅当 `TRANSACTIONAL=1` / `ROW_FORMAT=PAGE` 时才走 `ma_commit`。

### 21.4 与 InnoDB MVCC 的对比

| 维度 | Aria（Maria） | InnoDB |
|------|---------------|--------|
| 可见性判定 | `trnman_can_read_from(trn, trid)`（全局 min-TrID 扫描） | `read_view` 三段（low/up/middle）+ `m_ids` 二分 |
| 事务状态机 | `TRN` 一层（无 read_view 概念） | `trx_t` + `MVCC` read_view 双层 |
| 日志设计 | ARIES 双记录（`LOGREC_UNDO_*` + `LOGREC_REDO_*` + `LOGREC_COMMIT`） | redo log + undo log + binlog（3 段独立） |
| 是否支持行锁 | 否（仅 page-level row lock via lock manager） | 是（行级 + next-key lock） |
| 崩溃恢复 | checkpoint + 反向扫描 UNDO（trnman.min_safe_trid 兜底） | redo + undo（独立表空间 + doublewrite buffer） |
| 单实例复杂度 | O(trnman_get_min_trid)（常数） | O(log N_active_tx) 二分 |

Aria 的 **trnman 全局最小 TrID** 是其 MVCC 性能特征根因：所有 reader 共享同一全局视图，而非每个事务携带 `read_view`，因此大并发下快照读 O(1)。但代价是写事务必须等最老的 read-only 事务结束（`trnman_get_min_safe_trid` 阻塞 purge）。

### 21.5 复杂度表（参考 §21.1 ~ §21.3）

| 操作 | Aria 实现 | 时间复杂度 | 备注 |
|------|-----------|------------|------|
| `trnman_new_trn` | trnman 哈希桶分配 | O(1)（哈希桶 + mutex） | 单锁，与 SQL thread 一对一 |
| `trnman_can_read_from` | 全局 min/max TrID 比较 | O(1) | 区别于 InnoDB 的 O(log N) |
| `translog_write_record` | 在 `TRANSLOG_PAGE_SIZE=8KB` 内追加 | O(record_len / 8KB) | 跨页时 multi-group 编码 |
| `translog_flush` | sync() + 等待 in-flight | O(1) per fsync + 磁盘延迟 | `translog_syncs` 计数器累加 |
| `ma_commit` | 写 LOGREC_COMMIT + fsync + 内存 commit | O(1) + 一次 fsync | 不变量保证恢复可见性 |
| `ma_rollback` | 反向扫描 LOGREC_UNDO_* | O(K_undo_records) | 单事务 undo 链表 |

### 21.6 本项目对应

- 本项目 `engineering/src/db/` 的 `replication/replication.c` 仅实现了基础 binlog 复制，**未实现 Aria 这种 TRN 层事务调度**。
- `storage/access/btree/` 下 `btreeam.c` 是朴素 B+Tree + 无事务；对照 `storage/maria/ma_trnman.c`（`trnman_public.h:42-66`）可学习 TrID 分配 + 短 ID 复用策略。
- 后续如需在 `engineering/src/db/` 加 MVCC，可以参考 Aria 的 **全局 min-TrID** 简化设计（比 InnoDB 的 `read_view` 链表更易实现），作为单机场景的折衷方案。

---

## 二十二、Spider 分片联邦引擎：多 link 状态机与 Bulk-Insert SQL 拼接

Spider 是 MariaDB 官方维护的 **分片联邦引擎**：所有数据物理上放在后端 MySQL/MariaDB 节点（`use_sql_dbton_ids[]`），Spider 在 SQL 层负责把 DML/DDL 翻译为多个后端 SQL 并执行。本节深入 Spider 与 InnoDB 不同的 **多 link 拓扑 + Bulk-Insert 拼接框架**。

### 22.1 多 link 拓扑：`link_count` + `link_statuses[]`

每个 Spider 表的元信息存在 `SPIDER_SHARE` 内（`storage/spider/ha_spider.h:58`），核心拓扑字段：

```c
// storage/spider/ha_spider.h
ulonglong  *connection_ids;        // 各 link 对应远端 dbserver 的 server_id
uint        link_count;            // 后端节点数（>= 1，对应 partition 数）
my_bool    *link_statuses;         // 各 link 健康状态（含 RECOVERY）
bool       *init_index_handler;    // 各 link 的索引句柄是否初始化
bool       *init_rnd_handler;      // 各 link 的扫描句柄是否初始化
```

每次跨节点操作（insert/select/update/delete）都沿 `link_count` 个 link 顺序 fan-out，每个 link 维护独立 `SPIDER_CONN *conns[link_idx]` 与 `MYSQL *mysql[link_idx]` 句柄。`spider_conn_link_idx_next`（`storage/spider/spd_db_conn.cc:5231-5250`）跳过处于 `SPIDER_LINK_STATUS_RECOVERY` 的 link，仅对健康 link 执行：

```c
for (roop_count = spider_conn_link_idx_next(share->link_statuses,
              spider->conn_link_idx, -1, share->link_count,
              SPIDER_LINK_STATUS_RECOVERY);
     roop_count < (int) share->link_count;
     roop_count = spider_conn_link_idx_next(share->link_statuses,
              spider->conn_link_idx, roop_count, share->link_count,
              SPIDER_LINK_STATUS_RECOVERY))
{
    if (spider->conns[roop_count])            // 健康 link → 执行
        spider->conns[roop_count]->ignore_dup_key = ...;
}
```

**link 状态来源**：定期 background 监控（`SPIDER_MON_*` 系列）会改写 `link_statuses[i]`；SQL 线程在每次操作时重新读取，**实现"自愈式 fan-out"**——如果某个 link 失败，监控线程标记其为 RECOVERY，下次跨节点 SQL 自动跳过。

### 22.2 Bulk-Insert 三阶段：header / values / terminator

Spider 的 INSERT 优化是把单行 DML 攒成一条 `INSERT INTO tbl VALUES (...), (...), (...)` 多行 DML，再 fan-out 到各 link。三个核心函数（`storage/spider/ha_spider.cc:8541-9376`）：

```c
// ha_spider::append_insert_sql_part (8541)
// 仅生成 "INSERT INTO tbl" 头部
int ha_spider::append_insert_sql_part() {
    spider->reset_sql_sql(SPIDER_SQL_TYPE_INSERT_SQL);
    if ((error_num = spider->append_insert_sql_part()))
        return error_num;
    return 0;
}

// ha_spider::append_into_sql_part (9378)
// 拼接 "INTO tbl" 部分（不包含 VALUES）
int ha_spider::append_into_sql_part(ulong sql_type) {
    for (roop_count = 0; roop_count < share->use_sql_dbton_count; roop_count++) {
        dbton_id = share->use_sql_dbton_ids[roop_count];
        dbton_hdl = dbton_handler[dbton_id];
        if (dbton_hdl->first_link_idx >= 0 &&
            (error_num = dbton_hdl->append_insert_terminator_part(sql_type)))
            return error_num;
    }
    return 0;
}

// ha_spider::append_insert_values_sql_part (9357)
// 拼接一行 VALUES tuple
int ha_spider::append_insert_values_sql_part(ulong sql_type) {
    for (roop_count = 0; roop_count < share->use_sql_dbton_count; roop_count++) {
        dbton_id = share->use_sql_dbton_ids[roop_count];
        dbton_hdl = dbton_handler[dbton_id];
        if (dbton_hdl->first_link_idx >= 0 &&
            (error_num = dbton_hdl->append_insert_values_part(sql_type)))
            return error_num;
    }
    return 0;
}

// ha_spider::append_insert_terminator_sql_part (9336)
// 拼接 "ON DUPLICATE KEY UPDATE ...;" 或 ";" 结束符
int ha_spider::append_insert_terminator_sql_part(ulong sql_type) {
    for (roop_count = 0; roop_count < share->use_sql_dbton_count; roop_count++) {
        dbton_id = share->use_sql_dbton_ids[roop_count];
        dbton_hdl = dbton_handler[dbton_id];
        if (dbton_hdl->first_link_idx >= 0 &&
            (error_num = dbton_hdl->append_insert_terminator_part(sql_type)))
            return error_num;
    }
    return 0;
}
```

主端入口（`ha_spider.cc:6081-6213`）：

```c
void ha_spider::start_bulk_insert(ha_rows rows, uint flags) {
    bulk_insert = TRUE;
    bulk_size   = -1;                  // -1 表示尚未与远端建立批量 INSERT 句柄
    store_last_insert_id = 0;
}

int ha_spider::write_row(const uchar *buf) {
    if (!bulk_insert || bulk_size < 0) {           // 第一行：构造 header
        spider_db_bulk_insert_init(this, table);    // → append_insert_sql_part + append_into_sql_part
        bulk_size = spider_param_bulk_size(...);   // 默认 sql_bulk_size = 1000
    }
    spider_db_bulk_insert(this, table, &copy_info, FALSE);  // 追加 VALUES
    // spider_db_bulk_insert 内部阈值判断 is_bulk_insert_exec_period → 真正 fan-out
}

int ha_spider::end_bulk_insert() {
    bulk_insert = FALSE;
    if (bulk_size == -1) DBUG_RETURN(0);
    return spider_db_bulk_insert(this, table, &copy_info, TRUE);  // 真正执行 + reset
}
```

**真实执行路径**（`storage/spider/spd_db_conn.cc:5253-5353`）：内部按 threshold（默认 1000）累积到一定行数后调用 `dbton_handler->execute_sql(sql_type, conn, -1, ...)`，对每个健康 link 用同一份拼接 SQL 发出 `INSERT INTO tbl (a,b) VALUES (...), (...), (...)`。

### 22.3 多 link fan-out 与数据分片选择

Spider 提供三种分区/分片模式（取决于建表 COMMENT 配置）：

```sql
CREATE TABLE t (
  id INT,
  data VARCHAR(100),
  PRIMARY KEY (id)
) ENGINE=SPIDER
  COMMENT 'wrapper "mysql", table "t_real"'
  PARTITION BY KEY(id) (
    PARTITION p0 SERVER 'backend1' COMMENT 'srv "host=10.0.0.1"',
    PARTITION p1 SERVER 'backend2' COMMENT 'srv "host=10.0.0.2"',
    PARTITION p2 SERVER 'backend3' COMMENT 'srv "host=10.0.0.3"'
  );
```

- **水平分片**（KEY/HASH/RANGE）：按 partition key 计算 `link_idx`，**DML 仅 fan-out 到目标 link**（如 WHERE id=42 → 命中 p1 → 仅 backend2 收到 INSERT）。
- **全节点复制**（无 PARTITION）：所有 link 都执行 INSERT/UPDATE（fan-out 全集）。
- **跨节点 JOIN**（CTAS / 数据加载）：通过 `spd_copy_tables.cc` + `spd_group_by_handler.cc` 把 JOIN 下推到各 link，再用临时表聚合；`result_list.insert_dup_update_pushdown` 控制 `INSERT ... ON DUPLICATE KEY` 推送。

`bulk_size` 动态调整（`ha_spider.cc:6193-6208`）的真伪判定：

```c
bulk_size =
    (wide_handler->insert_with_update &&
     !result_list.insert_dup_update_pushdown) ||
    (!direct_dup_insert && wide_handler->ignore_dup_key)
    ? 0                              // 行级：每行立刻 fan-out
    : spider_param_bulk_size(...)   // 阈值批量：默认 1000 行聚合
```

即 **"是否走 bulk"取决于是否允许 `INSERT ... ON DUPLICATE KEY` 推送**：不允许推送时退化为行级 fan-out，避免在后端单语句内产生不确定的 ON DUPLICATE 行为。

### 22.4 复杂度表（参考 §22.1 ~ §22.3）

| 操作 | Spider 实现 | 时间复杂度 | 备注 |
|------|-------------|------------|------|
| 单行 INSERT（带分区） | 1 次 RTT × 1 link | O(RTT) + O(1) 计算 link_idx | 最快路径 |
| `start_bulk_insert` | `bulk_insert = TRUE` | O(1) | 不发送 SQL |
| `write_row`（行级） | append + execute_sql | O(row_size) + O(RTT) | 不聚合 |
| `write_row`（bulk） | append INTO VALUES | O(row_size)（已摊销） | 满 `bulk_size` 才真正 execute |
| `end_bulk_insert` | execute_sql + reset | O(RTT) | 1 次 RTT 总 fan-out |
| `update_row` / `delete_row` | 类似 Insert | O(RTT × relevant_link_count) | 分区下推 |
| cross-partition SELECT JOIN | `result_list` 聚合 | O(N_partitions × RTT) + O(rows_per_part) | 临时表 |

注意 **`bulk_size=0` 的退化路径**（行级 INSERT）与 `bulk_size=-1` 的"还未初始化"状态在 `ha_spider.cc:6193,6201-6208` 有明确区分：第一次 write_row 必然走 init 分支把状态推进到有效 `bulk_size`。

### 22.5 本项目对应

- 本项目 `engineering/src/db/sharding/` 下的 `sharding.c`、`shard_balance.c`、`shard_coordinator.c` 已实现基础分片路由与 coordinator，与 Spider `link_statuses[]` + `spider_conn_link_idx_next` 的"自愈式 fan-out"思路对照，但本项目无后端多 SQL 拼接（**不具备 Spider 的 bulk-insert SQL 构造框架**）。
- `storage/spider/ha_spider.cc:6081-6213` 的 `bulk_size` 三态机（`-1`/有效值/`0` 退化）可作为后续 `engineering/src/db/sharding/sharding.c` 优化批量 INSERT 时的参考。
- 后续若要实现跨分片 JOIN，可借鉴 `storage/spider/spd_group_by_handler.cc` 的 `group_by_handler` 模式：把 JOIN 子句整体下推各 link 再 result aggregation。

---

## 二十三、Galera wsrep 同步复制：5 阶段 commit 钩子链与 Certification 验证

MariaDB 的 Galera 集成 (`wsrep`) 是基于 **certification-based** 的同步多主复制：本地事务 COMMIT 前先把 write-set 广播到所有节点做并发冲突检测，冲突事务会被 BF abort。本节深入 `wsrep_before_commit` 等 5 个 commit 钩子如何 hook 到 `handler::commit` 流程中（与 MySQL InnoDB 完全不同——Galera 是 **认证型**，不是 MySQL 半同步那种"主从 binlog 同步"型）。

### 23.1 wsrep 5 阶段 commit 钩子链

`storage/maria` 没有 wsrep 钩子；所有 wsrep 调用点在 `sql/wsrep_trans_observer.h:198-400` 的内联函数族里。钩子调用链如下（按 SQL 层 commit order）：

```
ha_innobase::commit (storage/innobase/handler/ha_innodb.cc)
    │
    └─ handler::ha_commit_using_thd → handler.cc:1975 / :2045
            │
            └─ wsrep_before_commit(thd, all)        wsrep_trans_observer.h:314
                   │
                   └─ wsrep_cs().before_commit()    → wsrep-lib 内部 gcomm 协议
                                                          → broadcast write-set
                                                          → wait certify
                                                          → success/failure

    （veto retry loop / bloom filter check）

    └─ wsrep_after_commit / wsrep_ordered_commit   wsrep_trans_observer.h:381
            │
            └─ wsrep_commit_ordered → merge into global seqno
```

每个钩子的判定条件（伪代码，源自 `wsrep_trans_observer.h:198-232, 314-360`）：

```c
// wsrep_run_commit_hook —— 判定本 THD 是否真的走 wsrep 钩子
static inline bool wsrep_run_commit_hook(THD *thd, bool all) {
    if (!WSREP(thd)) return false;                   // 非 wsrep 线程（BINLOG/REPL）

    bool ret = wsrep_is_active(thd) && wsrep_is_real(thd, all);
    ret = ret && (thd->wsrep_trx().state() != wsrep::transaction::s_aborting);

    // 关键：只有 has_changes 或 (applying + ordered) 才走 hook
    if (ret && !(wsrep_has_changes(thd) ||
                 (wsrep_thd_is_applying(thd) && wsrep_is_ordered(thd)))) {
        mysql_mutex_lock(&thd->LOCK_thd_data);
        if (thd->wsrep_trx().state() == wsrep::transaction::s_executing) {
            ret = false;                              // 只读事务：跳过 hook
        }
        mysql_mutex_unlock(&thd->LOCK_thd_data);
    }
    return ret;
}

// wsrep_before_commit —— 真实 COMMIT 钩子入口
static inline int wsrep_before_commit(THD* thd, bool all) {
    DBUG_ENTER("wsrep_before_commit");
    DBUG_ASSERT(wsrep_run_commit_hook(thd, all));

    wsrep::provider::seq_cb_t seq_cb{
        thd, wsrep_parallel_slave_wakeup_subsequent_commits};

    int ret = thd->wsrep_cs().before_commit(&seq_cb);
    // 失败时通常为 BF aborted → 返回 WSREP_RF_FAILURE → 让外层 mysql_trans_commit 回滚

    if (ret == 0) {
        wsrep_xid_init(&thd->wsrep_xid, ...);
        wsrep_register_for_group_commit(thd);            // 全局序号注册
    }

    mysql_mutex_lock(&thd->LOCK_thd_kill);
    thd->wsrep_cs().before_commit_is_done = true;
    mysql_mutex_unlock(&thd->LOCK_thd_kill);
    return ret;
}
```

源码注释（`wsrep_trans_observer.h:306-313`）明确说明：

> Called before the transaction is committed. This function must be called from both client and applier contexts before commit.

即 **applier 线程（replay write-set）与 client 线程都必须调用**——双上下文共用同一套钩子保证 RPO=0。

### 23.2 wsrep_run_commit_hook 五重判定

合并 §23.1 的两道判定函数，`wsrep_before_commit` 仅当以下 5 条全成立时执行实质认证：

| 判定 | 含义 | 备注 |
|------|------|------|
| `WSREP(thd)` | THD 启用了 wsrep 模式 | 全局开关 |
| `wsrep_is_active(thd)` | THD 当前在一个 wsrep 事务内 | autocommit-then-1-stmt 自动开启 |
| `wsrep_is_real(thd, all)` | 不是 ROLLBACK / autocommit-final trivial | 多个语句事务是必须的 |
| 状态 != `s_aborting` | 不是已 abort 事务 | aborting 一定不 commit |
| `wsrep_has_changes(thd)` 或 (applier + ordered) | 有 write-set / 是 applier replay | 只读事务直接走空 commit |

`wsrep_commit_empty`（`wsrep_mysqld.cc:4331-4394`）兜底处理 `before_commit` 跳过逻辑：当事务 (1) 为空无变更、(2) 仅 `CREATE TABLE` 且非 ROW binlog、(3) 已被 abort、(4) DDL 被 BF abort 且需 replay 时，**不调用 `wsrep_before_commit`，改为本地 `wsrep_before_rollback` + `wsrep_after_rollback` + `wsrep_after_statement` 清理**。源码注释穷举了这 4 类 empty 路径（`:4356-4372`）：

```c
DBUG_ASSERT(empty ||     // (1) no changes
            create ||    // (2) CREATE TABLE in row format
            aborted ||   // (3) already aborted (e.g. max ws_size reached)
            ddl_replay); // (4) DDL got BF aborted → must replay
```

### 23.3 Certification-based 多主冲突判定

Galera 冲突检测不走 MySQL 半同步那种从 binlog dump 机制，而是 **本地事务 commit 时把 write-set 集（事务涉及的 db/table/row PK）广播给所有节点，全组用 bloom filter 做 conflict detection**。源码对应路径：

- `wsrep_thd_append_table_key(thd, db, table, WSREP_SERVICE_KEY_*_*)`（`service_wsrep.h:79-80`）—— 把 (db, table, key_type) 三个 key 追加进 thd 的 write-set。
- 4 种 `WSREP_SERVICE_KEY_*`（`service_wsrep.h:6-9`）：
  - `WSREP_SERVICE_KEY_SHARED`：共享锁（SELECT）。
  - `WSREP_SERVICE_KEY_REFERENCE`：参考锁（FK / 外键约束）。
  - `WSREP_SERVICE_KEY_UPDATE`：更新锁（INSERT/UPDATE/DELETE）。
  - `WSREP_SERVICE_KEY_EXCLUSIVE`：排他锁（DDL / ALTER / TRUNCATE）。
- 多 write-set 在其他节点上以 GCS（Group Communication System）做 **parallel apply + conflict map**；冲突事务由 `wsrep_thd_bf_abort(bf_thd, victim_thd, signal)` 触发。

**冲突判定**大致伪代码（基于 `service_wsrep.h:74-104` 公开 API 与 `wsrep_trans_observer.h:325` 的 `seq_cb` 注册语义）：

```c
// wsrep-lib 内部简化版（不暴露源码，但与 wsrep_trans_observer.h 调用语义一致）
int certify_write_set(wsrep_seqno_t seqno, write_set_t *ws, thd_array_t *active_thds) {
    foreach (existing_seq in committed_set[seqno - WSREP_WINDOW_SIZE, seqno]) {
        if (write_set_overlap(ws, existing_ws))      // bloom filter 检测交集
            return WSREP_RF_DEADLOCK;                // 标记 victim_thd for BF abort
    }
    return WSREP_OK;
}
```

`wsrep_thd_is_BF(thd, sync)`（`service_wsrep.h:74`）用于 applier 线程识别当前 THD 是否处于 BF abort 受害方。

### 23.4 wsrep 钩子的 SQL 层接入点

wsrep 钩子通过 SQL 层 `handler::commit` 路径接入（`sql/handler.cc:1975, 2045`）：

```c
// sql/handler.cc:1975 / :2045 —— 真实 source path
error = wsrep_before_commit(thd, all);
// ...
if (run_wsrep_hooks && (error = wsrep_before_commit(thd, all)))
    break;
```

`run_wsrep_hooks` 判定含两个不变量（`handler.cc` 注释）：
- THD 必须 enabled wsrep 且真实事务
- not running in XA recovery mode

wsrep 还 hook 在 SQL 层的另外多个点（与 MySQL 主从完全不同）：

| Hook 位置 | 文件:行号 | 触发时机 |
|-----------|-----------|----------|
| `wsrep_before_prepare` | `wsrep_trans_observer.h:262` | engine prepare 阶段 |
| `wsrep_after_prepare` | `wsrep_trans_observer.h:298` | engine prepare 完成 |
| `wsrep_before_commit` | `wsrep_trans_observer.h:314` | COMMIT 提交 |
| `wsrep_ordered_commit` | `wsrep_trans_observer.h:381` | 全局序号注册 |
| `wsrep_after_commit` | 关联 in `wsrep_commit_empty` | 提交后清理 |
| `wsrep_before_rollback` / `wsrep_after_rollback` | `wsrep_mysqld.cc:4376` | ROLLBACK 路径 |
| `wsrep_handle_mdl_conflict` | `wsrep_mysqld.cc:3586` | MDL 锁冲突（Galera 全集群 MDL） |
| `wsrep_after_statement` | `wsrep_mysqld.cc:4381` | 单语句结束 |
| `wsrep_commit_empty` | `wsrep_mysqld.cc:4331` | 空事务兜底 |
| `wsrep_handle_SR_rollback` | `service_wsrep.h:90` | streaming replication 回滚 |

`WSREP_SERVICE_KEY_*` 4 类的字段（`service_wsrep.h:6-9`）与 `wsrep_thd_append_table_key` 的调用规约（`service_wsrep.h:79-80`）是外层 storage engine 把锁升级信息送进 wsrep 的关键入口。

### 23.5 ColumnStore 简介（approx 公开 canonical 知识）

> 注：本次参 MariaDB-server trunk `bab03b0fc`，`storage/columnstore/columnstore/` 子模块目录**为空白**（同步 submodule 时未拉到 ColumnStore 2.x 的 C++ 实现）；以下细节依据公开 MariaDB Knowledge Base 与源码注释（`storage/columnstore/CMakeLists.txt` 仅引用"columnstore"项目名）。如需精确算法请查阅 `mariadb-columnstore-engine` 仓库。

- **存储结构**：每个 column 独立物理文件（`XXX.dir/XXX.rd...` + dictionary），按 extent（默认 8MB = 8×1MB blocks）。`DBRoot` 编号对应目录 `DBRoot00..DBRootNN`，分布式 CM 是 `PrimProc` + `ExeMgr` + `WriteEngineServer`。
- **Insert 路径**（约简）：`INSERT INTO t VALUES (...)` → `cs_columnstore_impl::bulk_insert` → 写 HDFS / local FS（按 `CalpontSystemCatalog` 路由）→ `WriteEngineServer::insert` → `ColumnStoreBulkInsertBuffer` → `DBFile` → 加字典压缩。
- **Delete 路径**：Bitmap + soft delete（物理不动，只清 bitmap）+ 后续 `dropPartition` / `markInplaceDelete` 压缩回写。
- **事务**：与 InnoDB 不一样，每个 statement 即 1 个 autocommit 事务（无 MVCC read view 控制），通过同一 wsrep 协议做多主同步（与 InnoDB 表共存场景下 Inplace-Update 是关键优化）。
- **Joins**：通过 ColumnStore 的 join hash 引擎做 large-table-side hash join；与 MariaDB 优化器集成通过 `Explain Analyzer` 输出 `columnstore_scan`/`columnstore_join` 节点。

详细路径（待子模块 init 后可补）：

- 真实源仓库：`https://github.com/mariadb-corporation/mariadb-columnstore-engine`（独立仓库，不在 trunk 仓库内）。
- 当前 `storage/columnstore/CMakeLists.txt` 仅 `add_subdirectory(columnstore)`，实际代码仓库未嵌套在 mariadb 子模块。
- `storage/columnstore/mysql-test/` 与 `storage/columnstore/CMakeLists.txt` 是 trunk 自带的部分。

如需后续在 `engineering/src/db/` 改造，可以参考 `columnstore_engine` 的 *PrimProc / ExeMgr 调度模型 + column-store bulk insert buffer* 概念作为基础。

### 23.6 与 MySQL 主从复制的对比

| 维度 | Galera (wsrep) | MySQL 主从半同步 (semi-sync) |
|------|----------------|------------------------------|
| 同步粒度 | write-set（事务级别） | binlog event 流 |
| 冲突检测 | certification（bloom filter） | 无（last-writer-wins） |
| 一致性保证 | RPO=0（理论） | RPO>0（master commit 后才等 ack） |
| 多主写入 | 支持 | 不支持（需 MGR） |
| 复制拓扑 | 全节点互联 + GCS（gcomm） | 单主 → 多从（binlog dump） |
| 钩子位置 | 5 阶段 commit 钩子链 + MDL 冲突钩子 | binlog dump thread + dump relay log |
| 性能损耗 | 20-40%（extra round-trip） | 5-15%（ack latency） |

注意 **MySQL 8.x 的 MGR（Group Replication）本质上就是 wsrep/Galera 思路的 MySQL 自带实现**，MariaDB 走 wsrep-API 路线而 MySQL 走自实现 XCom 路线——算法层面非常相似（certification + paxos），但 API 接口完全不同。

### 23.7 本项目对应

- `engineering/src/db/replication/replication.c` 实现的是基础 binlog 复制（与 MySQL 5.x 半同步类似），**未实现 certification-based 多主**。
- `engineering/src/db/consensus/` 下有 Raft 实现（见 `phase9-distributed-evolution.md`），**Raft 走的是 leader-based log replication，不是 Galera 那种 write-set certification**，所以不能直接复用。
- 后续如需在 `engineering/src/db/` 加 wsrep-like 多主，可参考：
  - `service_wsrep.h:6-9` 的 `WSREP_SERVICE_KEY_{SHARED,REFERENCE,UPDATE,EXCLUSIVE}` 四类 key。
  - `service_wsrep.h:74-104` 的 35 个 wsrep 服务函数接入点（`wsrep_thd_*` / `wsrep_on_func` / `wsrep_commit_ordered_func` 等）。
  - `wsrep_trans_observer.h:198-360` 的 `wsrep_run_commit_hook` + `wsrep_before_commit` 双层判定模型。

---

## 附录：新增 3 节合订要点

| 节号 | 标题 | 主题 | 工程映射 |
|------|------|------|----------|
| 二十一 | Aria 事务引擎 | TRN + Loghandler 双记录 + commit-then-flush | `engineering/src/db/storage/access/btree/` 朴素 B+Tree 改造 |
| 二十二 | Spider 分片联邦 | 多 link 拓扑 + Bulk-Insert SQL 拼接 | `engineering/src/db/sharding/` 自愈式 fan-out 优化 |
| 二十三 | Galera wsrep 同步复制 | 5 阶段 commit 钩子 + certification | `engineering/src/db/replication/` certification 化方向 |

三节均与现有「§七 存储引擎 / §九 优化器 / §十 事务与一致性 / §十一 复制与高可用」**零重叠**，填补 MariaDB-distinctive 引擎的算法细节。