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