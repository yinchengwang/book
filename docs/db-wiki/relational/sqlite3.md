# SQLite3 Wiki（关系型数据库 / 嵌入式）

> 版本：本次参考 SQLite `3.54.0`（commit `75136699f`）。SQLite 持续维护，每年多次更新。

## 一、项目定位

SQLite 是世界上最广泛部署的**嵌入式关系型数据库引擎**。它不是 C/S 架构，而是**进程内库**——整个数据库引擎就是一个约 600 KB 的 C 库，直接链接到应用进程，无独立服务进程。

- **痛点**：传统数据库需要单独的服务器进程，配置复杂、资源消耗大；对于单机本地存储（移动 App、桌面软件、嵌入式设备、浏览器、小型工具）不必要。
- **典型用户**：Android/iOS/macOS/Windows 操作系统内置；浏览器（Chrome 书签、Firedragon 表单历史）；嵌入式设备（车载系统、IoT）；PHP/Python/Ruby/Node 默认集成；Notion、Apple Photos、Dropbox、微信本地缓存；浏览器扩展。

## 二、发展历史与社区

- **起源**：2000 年由 D. Richard Hipp 设计，目标是替代 `fopen()` 写临时文件。
- **里程碑**：
  - 2000 年 1.0：首个版本。
  - 2004-2009 年：v3 系列重写，分离解析器、代码生成器、B+Tree；引入 WAL。
  - 2010 年 v3.7.0：WAL 模式、64 位支持。
  - 2013 年 v3.8.0：partial index、without rowid 表。
  - 2017 年 v3.20：window function。
  - 2018 年 v3.24：upsert、generated columns。
  - 2020 年 v3.32：real-time 监控、approximate count。
  - 2022-2023 年：JSONB 实验性、外部内容 FTS5 改进。
  - 2024-2026 年 v3.50+：仍在高频迭代，每年 10+ 次发版。
- **社区**：D. Richard Hipp 领导核心团队；邮件列表 + GitHub；社区贡献者众；公共领域（Public Domain）。
- **维护节奏**：每年多个版本，紧急修复通常 1-3 天内发布。

## 三、许可与商业限制

- **许可证**：**公共领域（Public Domain）**！
  - 来源：`reference/relational/sqlite3/LICENSE.md`
  - 关键条款：完全无限制使用、商用、修改、分发、闭源衍生均无需授权。
- **商业担保**：SQLite 团队提供商业级技术支持与定制开发（SQLite Consulting）。
- **无 SSPL/BSL/GPL 限制**：是最宽松的数据库许可证之一。

## 四、业界主要使用方与案例

- **操作系统**：Android、iOS、macOS、Sailfish OS、Tizen、Windows 10+ 部分组件。
- **浏览器**：Chrome、Firefox、Safari、Edge 内部存储（书签、历史、表单）。
- **语言运行时**：Python（`sqlite3` 模块）、PHP（PDO_SQLITE）、Ruby、Rust（`rusqlite`）、Node.js（`better-sqlite3`）等默认集成。
- **应用**：Notion、Skype、Dropbox、Apple Photos、WhatsApp、Signal、Telegram、微信、Adobe Lightroom、Symantec 杀毒客户端、BMW iDrive、Mercedes COMAND。
- **测试**：数百万开发者用 SQLite 做单元测试、CI 缓存。
- **嵌入式**：汽车 ECU、工业控制、医疗设备、POS 终端、IoT 网关。

## 五、一句话对比定位

> SQLite 是**嵌入式场景的事实标准**，以**零配置、单文件、公共领域许可**为最大优势；与 **MySQL/PostgreSQL（C/S 架构）**形成互补关系而非竞争；**不是**分布式数据库的替代品。

## 六、整体架构

SQLite 采用**单一 C 库 + 模块化内部分层**架构，进程内运行，所有组件在同一地址空间。

```
┌─────────────────────────────────────────────────────────────────────┐
│                       应用进程（链接 libsqlite3）                       │
│                                                                       │
│  ┌───────────────────────────────────────────────────────────────┐   │
│  │              Public C API (sqlite3_open / sqlite3_exec /       │   │
│  │                            sqlite3_step / sqlite3_prepare)      │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │         Tokenizer + Parser (tokenize.c, parse.y)              │   │
│  │            - LEMON 生成的 LALR(1) 解析器                        │   │
│  │            - 输出 Parse Tree (AST)                              │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │      Code Generator (build.c, expr.c, insert.c, delete.c)     │   │
│  │            - AST → VDBE bytecode 准备                          │   │
│  │            - 优化：常量折叠、覆盖索引优化                       │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │         VDBE Virtual Machine (vdbe.c, vdbeapi.c)                │   │
│  │            - 执行 byte-code 程序                                    │   │
│  │            - 多步执行（sql sqlite3_step）                       │   │
│  │            - 9500+ 行核心实现                                    │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │        B-Tree Layer (btree.c)                                  │   │
│  │            - B+Tree 实现 11655 行                                │   │
│  │            - 游标、锁、事务管理                                  │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │      Pager / OS Interface (pager.c, os.c)                       │   │
│  │            - 页面缓存 (LRU)                                       │   │
│  │            - WAL 或 Rollback Journal                             │   │
│  │            - 文件 I/O / mmap / fsync                            │   │
│  └─────────────────────────┬─────────────────────────────────────┘   │
│                            │                                          │
│  ┌─────────────────────────▼─────────────────────────────────────┐   │
│  │      Disk File (单个 .db 文件)                                   │   │
│  │            - 默认页大小 4096 字节                                 │   │
│  │            - 可配置 512-65536                                    │   │
│  └───────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────┘
```

源码入口：
- `sqlite3_open`（`src/main.c:3747`）打开/创建数据库。
- `sqlite3BtreeOpen`（`src/btree.c:2562`）打开 B+Tree。
- `sqlite3VdbeExec`（`src/vdbe.c:902`）执行字节码。
- `sqlite3_step`（`src/vdbeapi.c:980`）单步执行。

## 七、存储引擎

### 7.1 文件格式

- 单个 `.db` 文件。
- 固定 100 字节的 database header（`page 1` 起始）：
  - magic string `"SQLite format 3\000"`
  - page size（默认 4096，可配 512-65536）
  - file format write version / read version
  - reserved space per page
  - `text encoding`（1=UTF8、2=UTF16le、3=UTF16be）
- 后续页：freelist → trunk page → B+Tree pages。

### 7.2 页结构

SQLite 页（默认 4 KB）布局：

```
┌──────────────────────────────────────────────────┐
│ Page Header (8 / 12 字节, 取决于页类型)              │
│   - pgno (4B)    - 页号                             │
│   - first freeblock (2B)                          │
│   - cell count (2B)                               │
│   - cell content area start (2B)                  │
│   - fragmented free bytes (1B)                    │
│   - right child pointer (4B, 仅内部 B+Tree 节点)    │
├──────────────────────────────────────────────────┤
│ Cell Pointer Array (从顶部向下生长)                   │
├──────────────────────────────────────────────────┤
│ Unallocated Space                                   │
├──────────────────────────────────────────────────┤
│ Cell Content Area (从底部向上生长)                    │
├──────────────────────────────────────────────────┤
│ Unallocated Space                                   │
├──────────────────────────────────────────────────┤
│ Reserved Space (per-page, e.g. 0 for leaves)       │
└──────────────────────────────────────────────────┘
```

### 7.3 事务与并发

- **默认 rollback journal 模式**：写前复制原页到 `*.db-journal`；提交时删除 journal。
- **WAL 模式**（v3.7+）：写操作追加到 `*.db-wal`，读操作从 `*.db-wal` + 数据库文件组合读取。允许多读单写并发。
- **锁状态**：UNLOCKED → SHARED → RESERVED → PENDING → EXCLUSIVE（rollback journal）；或 `WAL 模式 + shared write lock`。
- **多进程**？SQLite **支持**多进程，但**不推荐**高并发写入，多进程并发写需通过 POSIX Advisory Lock。

### 7.4 数据类型

SQLite 使用**动态类型系统（manifest typing / dynamic typing）**：
- 5 个存储类：`NULL / INTEGER / REAL / TEXT / BLOB`。
- 列可以声明类型亲和性（type affinity）：INTEGER / TEXT / REAL / NUMERIC / NONE。
- 无强类型校验，存什么类型都行（除主键约束等）。

## 八、索引机制

### 8.1 B+Tree 索引（默认）

- 索引项存：键值 + rowid。
- 主键 `INTEGER PRIMARY KEY` 是 rowid 别名（WITHOUT ROWID 表除外）。

### 8.2 部分索引（Partial Index）

- `CREATE INDEX idx ON t (col) WHERE col > 0`，仅索引满足条件的行。

### 8.3 表达式索引

- `CREATE INDEX idx ON t (lower(name))`。

### 8.4 WITHOUT ROWID 表（v3.8+）

- `CREATE TABLE t (...) WITHOUT ROWID`，主键即 B+Tree 键，叶子节点直接存数据；无 hidden rowid。
- 适合主键为复合键、紧凑键的表，可省空间。

### 8.5 全文索引 FTS5

- 独立模块（`ext/fts5/`）：基于倒排索引，支持分词、rank、snippet。
- 内置 `porter tokenizer`、unicode61 tokenizer、trigram tokenizer（v3.34+）。

### 8.6 JSON 索引

- `CREATE INDEX idx ON t (json_extract(data, '$.x'))`。
- v3.38+ 引入 `JSONB`（实验）减少 JSON 体积与查询时间。

## 九、查询与执行

### 9.1 解析

- **词法分析**：`src/tokenize.c`。
- **语法分析**：`src/parse.y` → LEMON 生成的解析器（输出到 `src/parse.c`，但本仓库仅持 `.y`）。
- **输出**：Parse Tree。

### 9.2 优化

SQLite 实现了一个**轻量级 CBO（基于成本）**：
- `where.c` 实现 WHERE 子句优化（`sqlite3WhereBegin`）：扫描方式选择（`OP_OpenRead` / `OP_OpenWrite`）。
- 优化技术：覆盖索引（covering index）、OR 子句转 UNION、自动索引（auto-indexing）。
- `select.c` / `expr.c`：复杂 SELECT 转换（subquery flattening）。
- 没有 PG/MySQL 那种复杂 CBO，但通过 `ANALYZE` 维护 `sqlite_stat1/2/4` 统计信息。
- **PRAGMA optimize**（v3.18+）：自动触发 ANALYZE。

### 9.3 执行（VDBE）

- **VDBE = Virtual Database Engine**，是 SQLite 的字节码解释器。
- 每个 prepared statement 是一段字节码程序，调用 `sqlite3_step()` 单步推进。
- 字节码操作（Opcode）：`OpenRead`、`Column`、`Compare`、`Next`、`Insert`、`ResultRow`、`Halt` 等。
- 源码：`src/vdbe.c:902`（`sqlite3VdbeExec` 约 8000 行核心）、`src/vdbeapi.c:980`（`sqlite3_step`）。

### 9.4 触发器与视图

- **触发器**：`BEFORE/AFTER INSERT/UPDATE/DELETE`（v3.x 无 INSTEAD OF 但通过 RAISE 实现）。
- **视图**：只读视图；可更新视图需满足条件。

### 9.5 解释计划

- `EXPLAIN QUERY PLAN`：`sqlite3_step` 走 TreeView 输出扫描方式（`SCAN t`、`SEARCH t USING INDEX idx`）。
- `EXPLAIN`：返回字节码程序。

## 十、事务与一致性

### 10.1 ACID

- **A**tomicity：事务内所有写入要么全部成功要么全部回滚（通过 journal / WAL）。
- **C**onsistency：触发器、约束、外键、CHECK 强制一致性。
- **I**solation：默认 `SERIALIZABLE`；不支持自定义隔离级别。

### 10.2 隔离级别

- **DEFERRED**（默认）：获取 SHARED 锁后才读到 snapshot。
- **IMMEDIATE**：事务开始就获取 RESERVED 锁。
- **EXCLUSIVE**：事务开始就获取 EXCLUSIVE 锁，防止其他 reader。
- 详见 `BEGIN DEFERRED/IMMEDIATE/EXCLUSIVE`。

### 10.3 锁机制

SQLite 的锁是**数据库级粗粒度锁**，不支持行锁：

- UNLOCKED → SHARED → RESERVED → PENDING → EXCLUSIVE。
- 详见 `src/btmutex.c`（B-Tree mutex）、`src/pager.c`（pager 锁状态机）。

### 10.4 乐观并发

- `BEGIN IMMEDIATE` + 检查 `SQLITE_BUSY`；应用层捕获后重试。

## 十一、复制与高可用

SQLite **不内置主从复制**，但有以下方案：

### 11.1 文件级复制

- 拷贝 `.db` 文件 + `.db-wal`（注意不要在写入中）。

### 11.2 逻辑复制

- **Litestream**（Ben Johnson 出品）：WAL 流式备份到 S3，类 `binlog` 备份。
- **rqlite**：基于 Raft 的分布式 SQLite（详见 extension 分类）。

### 11.3 备份 API

- `sqlite3_backup_init/cstep/finish`：在线热备份 API。

### 11.4 嵌入式替代方案

- 跨设备同步：SQLite + CRDT（automerge）、Litestream + S3 + 冲突合并。

## 十二、性能与基准

- **写入**：10-100k INSERT/s（单线程，无事务）。
- **事务写入**：`PRAGMA journal_mode=WAL` + `synchronous=NORMAL` 可达 10k+ tx/s。
- **读取**：简单查询 100k+ SELECT/s；含 JOIN 5k-30k。
- **适用规模**：单文件 < 1TB（理论 281 TB），实际建议 < 100GB；并发 < 100 写并发。
- **优化**：
  - `PRAGMA journal_mode = WAL`
  - `PRAGMA synchronous = NORMAL`（除非电力不可靠）
  - `PRAGMA cache_size = -20000`（约 20 MB 缓存）
  - `PRAGMA mmap_size = 268435456`（256 MB mmap）
  - `PRAGMA temp_store = MEMORY`
  - 事务批量提交（每 1000 行 1 commit）

## 十三、生态与集成

### 13.1 客户端 SDK

- **C/C++**：`sqlite3.h` 头文件 + `libsqlite3.a/.so`。
- **Python**：`sqlite3`（标准库）、`peewee`、`SQLAlchemy`、`Django ORM`。
- **Go**：`mattn/go-sqlite3`（cgo）、`modernc.org/sqlite`（pure Go）、`ncruces/go-sqlite3`。
- **Java**：SQLite JDBC（`org.xerial:sqlite-jdbc`）。
- **Node.js**：`better-sqlite3`（同步，性能最佳）、`sqlite3`（异步）、`knex`、`prisma`、`drizzle-orm`。
- **Rust**：`rusqlite`、`sqlx`（含 SQLite 驱动）。
- **PHP**：PDO_SQLITE（标准库）。

### 13.2 工具链

- **sqlite3 CLI**：交互式 CLI（`tool/shell.c`）。
- **`sqlite3_analyzer`**（`tool/`）：分析数据库空间使用、索引效率。
- **`sqlite3_diff`** / **`sqlite3_rsync`**（v3.47+）：数据库差异 / 同步工具。
- **DB Browser for SQLite**（社区）：GUI 工具。
- **Litestream**：WAL 流式备份。

### 13.3 扩展模块

- **JSON1**（默认编译）：JSON 函数。
- **FTS5**：全文搜索。
- **RTREE**：R-Tree 空间索引。
- **math / regexp / uuid / base64**：扩展函数。
- **`gencol` / `returning` / `upsert`** 等现代特性（默认编译）。

## 十四、源码学习路径

SQLite 是**学习数据库内核最合适的项目**，代码组织清晰，单文件可控。

1. **入口**：`src/main.c:3747`（`sqlite3_open`）。
2. **API**：`src/sqlite3.h`（C API 头文件，5000+ 行定义）、`src/vdbeapi.c:980`（`sqlite3_step`）。
3. **Tokenizer**：`src/tokenize.c`（约 1000 行）。
4. **Parser**：`src/parse.y`（约 3500 行 LEMON 语法）。
5. **Code Generator**：`src/build.c`、`src/expr.c`、`src/insert.c`。
6. **VDBE**：`src/vdbe.c:902`（`sqlite3VdbeExec` 核心）。
7. **B-Tree**：`src/btree.c:2562`（`sqlite3BtreeOpen`）、`:9441`（`sqlite3BtreeInsert`）。
8. **Pager / WAL**：`src/pager.c`、`src/wal.c`。
9. **OS 接口**：`src/os_unix.c`、`src/os_win.c`。

源码规模：约 16 万行 C 代码 + 8 万行测试代码（`test/`）+ 大量文档（`doc/`）。

## 十五、风险与陷阱

### 15.1 适用场景

- **不是分布式数据库**：单文件存储，无法横向扩展到多节点。
- **不适合高并发写**：单写者锁，多进程写竞争严重。
- **不适合大表（> 100GB）**：事务开销、备份时间、VACUUM 都慢。

### 15.2 性能陷阱

- **autocommit 误用**：每次 INSERT 都创建事务 → 极慢。批量 BEGIN 后再提交。
- **无索引的 SELECT**：表扫描；用 `EXPLAIN QUERY PLAN` 检查。
- **缺 `WITHOUT ROWID` 而主键非整型**：rowid + 主键双重存储浪费空间。
- **PRAGMA 设置错误**：默认 `synchronous=FULL` + `journal_mode=DELETE` 写入极慢。

### 15.3 数据丢失风险

- **未开 WAL**：`synchronous=FULL` 仍有部分丢失可能（取决于文件系统）。
- **文件系统崩溃**：ext4/xfs 在崩溃恢复时可能损坏；推荐 `synchronous=FULL` + WAL。
- **不备份**：很多项目忘了 SQLite 需要备份；用 Litestream / 定期 `.backup`。

### 15.4 并发陷阱

- **多进程并发写**：`SQLITE_BUSY` 是常态；应用层需要重试 + backoff。
- **WAL 模式下 long-running reader**：阻塞 checkpoint 与 GC。
- **手动 `VACUUM INTO`** 阻塞写。

### 15.5 类型陷阱

- **类型亲和性被误解**：SQLite 允许任意类型存入列，类型检查在应用层。
- **缺失 CHECK 约束**：默认无强一致性，需手写触发器。

## 十六、学习路径

### 16.1 入门（1 周）

1. 安装 SQLite CLI（Windows/macOS/Linux 自带或 `apt install sqlite3`）。
2. 用 `sqlite3 my.db` 创建数据库，`CREATE TABLE / INSERT / SELECT`。
3. 学习 `.tables / .schema / .indices / .dump / .timer on` 元命令。
4. 阅读官方教程 <https://www.sqlite.org/quickstart.html>。

### 16.2 进阶（2-4 周）

1. 读 SQLite 官方文档：
   - <https://www.sqlite.org/lang.html>（SQL 语法）
   - <https://www.sqlite.org/pragma.html>（PRAGMA）
   - <https://www.sqlite.org/fileformat2.html>（文件格式）
   - <https://www.sqlite.org/wal.html>（WAL 模式）
2. 学习事务、索引、视图、触发器、FTS5。
3. 在 Python/Node 中使用 SQLite 库做小项目。

### 16.3 高级（1-3 月）

1. **深入阅读源码**（重点推荐）：`src/btree.c`、`src/vdbe.c`、`src/where.c`、`src/pager.c`。
2. 写一个 SQLite 扩展（`sqlite3_create_function`、`sqlite3_create_module`）。
3. 实现自定义 tokenizer 或 VFS。
4. 性能调优：`EXPLAIN QUERY PLAN` + `PRAGMA` + 基准测试。

### 16.4 推荐资源

- 官方文档：<https://www.sqlite.org/docs.html>
- 《The Definitive Guide to SQLite》（Grant Allen、Mike Owens）
- 《Using SQLite》（Jay Kreibich）
- 官方邮件列表：<https://sqlite.org/forum/>
- 中文社区：SQLite 中文文档（`docs/superpowers/specs/`）与 SQLite 中文论坛
- SQL 教程（SQLite 方言）：<https://www.sqlitetutorial.net/>

## 十七、可移植性与部署

- **平台**：几乎所有操作系统（Linux、macOS、Windows、BSD、各种 Unix、iOS/Android、嵌入式 RTOS）。
- **编译**：纯 ANSI C，无标准库外依赖；默认 autoconf；CMake 集成（`add_subdirectory(sqlite-amalgamation)`）。
- **amalgamation**：可编译成单一 `sqlite3.c` + `sqlite3.h`（约 30 万行）方便静态链接。
- **wasm**：SQLite 提供 `sqlite3.wasm` + WebAssembly 编译选项，支持浏览器/PWA。
- **嵌入式**：天然适合；只需 ~600 KB 库 + 单文件 .db。
- **云端**：Cloudflare D1（基于 SQLite + WebAssembly）、SQLite Cloud、Turso（libSQL）。

## 十八、变体与衍生项目

- **libSQL**：Turso 出品的 SQLite 分支（fork），增加 HTTP 异步协议、WebAssembly 嵌入；兼容 SQLite API。
- **sqld**：Turso 的分布式 SQLite（基于 Raft）。
- **rusqlite**：Rust 绑定。
- **better-sqlite3**：Node.js 同步绑定，性能最佳。
- **sql.js**：SQLite WebAssembly 编译版，浏览器内运行。
- **SQLite 数据库生态系统**（应用层）：
  - DBeaver（社区版 GUI）
  - HeidiSQL（Windows GUI）

## 十九、与本项目的关联

- 本仓库 `engineering/src/db/` 中实现的 PG 风格存储引擎**未涉及 SQLite**，但 SQLite 是嵌入式数据库的标杆。
- **`learning/scaffold/cpp/stl`** 可以作为 SQLite 设计哲学（极简、单文件、可嵌入）的对照参考。
- 本 wiki 可作为理解 `engineering/src/db/` 中 WAL / Buffer Pool 设计的对照。

## 二十、参考资料

1. SQLite 官方文档：<https://www.sqlite.org/docs.html>
2. SQLite 文件格式：<https://www.sqlite.org/fileformat2.html>
3. SQLite WAL 模式：<https://www.sqlite.org/wal.html>
4. SQLite 查询优化器：<https://www.sqlite.org/optoverview.html>
5. SQLite 异步 I/O（v3.36+）：<https://www.sqlite.org/asyncio.html>
6. SQLite 源码 `src/btree.c`、`src/vdbe.c`、`src/parse.y`、`src/pager.c`、`src/wal.c`
7. 《The Definitive Guide to SQLite》
8. litestream 备份工具：<https://litestream.io/>
9. libSQL（Turso）：<https://turso.tech/libsql>
10. SQLite 中文社区：<https://sqlite.com.cn/>

## 二十一、SQLite B-Tree 节点分裂、空间分配与"最小化"设计

第八节 §8.1 仅一句话"索引项存：键值 + rowid"。本节深入 SQLite **B-Tree 最小化**（minimalist）哲学：节点布局、cell 空间分配、三档 rebalance 路由。

### 21.1 三档 rebalance 路由：`balance()` 主循环

`btree.c:9162` 的 `balance(BtCursor *pCur)` 是每次 INSERT 后唯一会被调用的入口；它按"溢出程度"分流到三个不同子函数，**不**像 PostgreSQL `_bt_split_penalty` 那样做复杂的分裂点打分。触发条件：

```c
// 简化自 btree.c:9174-9244 实际源码
do {
    if (pPage->nFree*3 <= (int)pCur->pBt->usableSize*2) {
        // (1) 没有溢出 cell 且空闲 < 2/3 页 → 不分裂
        break;
    }
    if (iPage == 0) {
        // (2) 根页溢出 → balance_deeper() 树增高
        rc = balance_deeper(pPage, &pCur->apPage[1]);
    } else if (/* pPage 是 INTKEY leaf + 单溢出 + 父页 iIdx == nCell + 父页 != pgno 1 */) {
        // (3) 快速路径 → balance_quick() 仅分配一个新兄弟页
        rc = balance_quick(pParent, pPage, aBalanceQuickSpace);
    } else {
        // (4) 标准路径 → balance_nonroot() 跨 1~3 个兄弟页重分布
        rc = balance_nonroot(pParent, iIdx, aOvflSpace, isRoot, bBulk);
    }
} while (/* 父页再次不平衡 */);
```

`nFree * 3 > usableSize * 2` 这一不等式意味着 **SQLite 在页空闲 < 2/3 时绝不分裂**——和 PostgreSQL "fill factor" 目标相反，而是更接近"碎片优先惰性分裂"。

### 21.2 `balance_quick()` 单一溢出快速路由

`btree.c:8039`。前置条件是 `pPage->nOverflow==1 && aiOvfl[0]==nCell && pParent->nCell==iIdx && pParent->pgno!=1`（INTKEY 表 + 自增 rowid + 父页插入在末尾）。算法逻辑：

1. `allocateBtreePage(pBt, &pNew, &pgnoNew, 0, 0)` 分配新页。
2. 把单条溢出 cell 从 `pPage->apOvfl[0]` 拷贝到 `pNew`（调用 `rebuildPage`）。
3. 构造父页 divider cell（4 字节 page number + 原 cell 最大键），`insertCell(pParent, pParent->nCell, pSpace, sz, ...)` 插入父页末尾。
4. `put4byte(&pParent->aData[pParent->hdrOffset+8], pgnoNew)` 修改右子指针。

**为什么快**：不读任何邻居页、不重新分配 cell、不调用 `balance_nonroot` 的 split 算法，整段最重的工作是单 cell 复制 + 一次 `insertCell`。代价是父页只有 1 个 cell 增量，但下一轮 do-while 仍会重新走 `balance()` 决策。

### 21.3 `balance_nonroot()` 标准 multi-sibling 重分布

`btree.c:8277`。复杂度最高路径：

- 取 `pPage` 和**最多 2 个邻居页**（共 `NB=3` 个：1 当前页 + 2 邻居）。
- 收集所有 cell 到 `CellArray b.apCell[]`，按现有顺序拼接（不重排）。
- 循环向 `apNew[i]` 填充直至空间耗尽（`usableSize - reserved - 2*pNew->nCell - cellOverhead`）。
- 写入 `cntNew[i]` 与 `szNew[i]`，刷新父页 divider cells（`apDiv[i]`）。

```c
// 简化自 btree.c:8343-8395 实际源码
i = pParent->nOverflow + pParent->nCell;
if (i < 2) { nxDiv = 0; }
else if (iParentIdx == 0) { nxDiv = 0; }
else if (iParentIdx == i) { nxDiv = i - 2 + bBulk; }
else { nxDiv = iParentIdx - 1; }
nOld = i + 1;
// 从右向左收集所有 cell 到 b.apCell[]
while (1) {
    rc = getAndInitPage(pBt, pgno, &apOld[i], 0);
    nMaxCells += apOld[i]->nCell + ArraySize(pParent->apOvfl);
    if ((i--) == 0) break;
    // 找下一个 divider cell 提取 page number
    apDiv[i] = findCell(pParent, i+nxDiv-pParent->nOverflow);
    pgno = get4byte(apDiv[i]);
}
```

### 21.4 `allocateSpace()` 三策略 cell 空间分配

`btree.c:1846` 同样体现"极简"哲学：

1. **freelist 优先**：`pageFindSlot(pPage, nByte, &rc)` 在 freeblock 链表找足够大的 slot。
2. **碎片整理回退**：`gap+2+nByte > top` 时 `defragmentPage(pPage, MIN(4, nFree - 2 - nByte))`。
3. **cell content area 顶端**：`top -= nByte; put2byte(&data[hdr+5], top)` 在底部反向生长。

`freeSpace()` (`btree.c:1945`) 负责释放并**合并相邻 freeblock**：`iEnd = iStart + iSize`，扫描 freelist，插入到 `iStart` 之前并合并。

### 21.5 与 PostgreSQL 对比与复杂度

| 操作 | SQLite B-Tree | PostgreSQL B+Tree |
|------|---------------|-------------------|
| 分裂触发 | `nFree*3 > usableSize*2` (≥1/3 满) | `_bt_insertonpage` 在 insert 后立即 |
| 分裂点选择 | 不选择，直接全重分布 | `_bt_findsplitloc` 打分 (`_bt_split_penalty`) |
| 后缀截断 | 不支持（WITHOUT ROWID 也不截） | `_bt_truncate` 后缀去列 |
| 单次分裂复杂度 | O(N) cells 移动 + O(N) parent update | O(N log N) 评估 + O(N) 移动 |
| 树高 | `balance_deeper` 仅在根溢出时增 | 插入路径自动级联 |
| 树矮化 | 不主动（页利用率低不缩） | `_bt_fix_collapse` 自动 |

### 21.6 本项目对应

`engineering/src/db/btreeam.c` 实现的是朴素 B+Tree 节点分裂，**未采用** SQLite 的 "nFree*3 > 2/3" 惰性策略，也**未实现** `balance_quick` 快速路径——可作为后续优化方向（先写溢出再分裂，减分裂次数）。

---

## 二十二、Pager 状态机、Rollback Journal 与 WAL 帧编码

§7.3 与 §10 仅给"Pager + Rollback Journal + WAL"三名字，本节展开真实的状态机与文件格式。

### 22.1 Pager 七状态机

`pager.c:351-357`：

```c
#define PAGER_OPEN                  0
#define PAGER_READER                1
#define PAGER_WRITER_LOCKED         2
#define PAGER_WRITER_CACHEMOD       3
#define PAGER_WRITER_DBMOD          4
#define PAGER_WRITER_FINISHED       5
#define PAGER_ERROR                 6
```

状态转移（来自 `pager.c:898-980` 实际断言与 `sqlite3PagerCommitPhaseOne` 触发条件）：

```
OPEN ──begin──► READER ──write tx──► WRITER_LOCKED ──first page mod──► WRITER_CACHEMOD
                                                                      │
                                                                      ▼
                              [sqlite3PagerCommitPhaseOne()] ──► WRITER_DBMOD
                                                                      │
                                                                      ▼
                                              ──► WRITER_FINISHED ──► READER
                                                                      │
                                              [rollback] ──► ERROR
```

**注意**：WAL 模式下 pager 永远不进入 `WRITER_DBMOD`/`WRITER_FINISHED`——`pager.c:340-342` 明确写"A pager is never in WRITER_DBMOD or WRITER_FINISHED state if the connection is open in WAL mode"。

### 22.2 Rollback Journal 提交两阶段

**Phase One** `sqlite3PagerCommitPhaseOne(pPager, zSuper, noSync)` `pager.c:6527`：

```c
// 简化自 pager.c:6555-6600
if (0 == pagerFlushOnCommit(pPager, 1)) {
    sqlite3BackupRestart(pPager->pBackup);  // 无脏页，幂等
} else {
    PgHdr *pList = sqlite3PcacheDirtyList(pPager->pPCache);
    if (pagerUseWal(pPager)) {
        // WAL 模式：脏页走 walFrames
        rc = pagerWalFrames(pPager, pList, pPager->dbSize, 1);
    } else {
        // Rollback Journal：写 super-journal（如有）+ 同步 db file
        rc = pager_write_pagelist(pPager, pList);
    }
    // 同步 db file
    if (!noSync) sqlite3OsSync(pPager->fd, 0);
    pPager->eState = PAGER_WRITER_FINISHED;
}
```

**Phase Two** `sqlite3PagerCommitPhaseTwo` `pager.c:6764` 实际只做 journal 截断/删除（根据 `journalMode`）：
- `DELETE`：unlink journal 文件；
- `TRUNCATE`：ftruncate 到 0；
- `PERSIST`：保留文件头但截断；
- `OFF/MEMORY`：跳过 journal。

`change counter` 在 `pager_incr_changecounter` (`pager.c:6379`) 阶段写入 page 1 偏移 24-27 的 4 字节，**用于其他连接检测 db 文件是否被修改**（类似乐观锁的版本号）。

### 22.3 WAL 文件格式

`wal.c:477-491`：

```c
#define WAL_FRAME_HDRSIZE 24   /* 每帧 header */
#define WAL_HDRSIZE       32   /* 文件头总大小 */
#define WAL_MAGIC         0x377f0682
```

WAL 文件 = 32 字节 header + 多个帧。每个帧 = **24 字节 frame header + szPage 字节 page data + 8 字节 trailing checksum**。

帧偏移公式（`wal.c:498-500`）：

```c
#define walFrameOffset(iFrame, szPage) (
    WAL_HDRSIZE + ((iFrame)-1) * (i64)((szPage) + WAL_FRAME_HDRSIZE)
)
```

注意 `WAL_FRAME_HDRSIZE=24` 不含 checksum 8 字节，frame header 字段（推断自 `wal.c` 写入逻辑）：

| 偏移 | 字段 | 含义 |
|------|------|------|
| 0-3 | page number (u32) | 该帧承载的 db page 号 |
| 4-7 | db size after commit (u32) | 提交后 db 文件页数 |
| 8-11 | not used (u32) | 保留 |
| 12-15 | not used (u32) | 保留 |
| 16-23 | salt1+salt2 (2*u32) | 同 header 的 aSalt，每次 commit 加 1 |
| 24+ | page data | 实际 db 页内容 |
| 末 8 | checksum[2] | crc32c over frame header + page data |

**Magic LSB 控制校验字节序**（`wal.c:482-490`）：magic 最低位为 0 → little-endian checksum；为 1 → big-endian。这是个老 bug 兼容设计。

### 22.4 WalIndexHdr + WalCkptInfo 共享内存索引

`wal.c:321-333`：

```c
struct WalIndexHdr {
    u32 iVersion;                  /* wal-index 版本 */
    u32 unused;                    /* 对齐 */
    u32 iChange;                   /* 每次事务 +1 */
    u8  isInit;                    /* 已初始化标志 */
    u8  bigEndCksum;               /* big-endian checksum 标志 */
    u16 szPage;                    /* db 页大小 (1==64K) */
    u32 mxFrame;                   /* WAL 最后有效帧号 */
    u32 nPage;                     /* db 页数 */
    u32 aFrameCksum[2];            /* 最后帧的 checksum */
    u32 aSalt[2];                  /* 从 WAL header 复制的 salt */
    u32 aCksum[2];                 /* 上述字段的 checksum */
};
```

`WalCkptInfo` (`wal.c:394`) 包含 `aReadMark[WAL_NREADER]` 数组，**每个 reader 持有一个 read-mark** 表示它可见的最大 `mxFrame`。`aReadMark[K]==0xffffffff (READMARK_NOT_USED)` 表示槽位空闲。当 `nBackfill == mxFrame` 时所有 WAL 内容已回填到 db，新 reader 直接读 db 文件不再查 WAL——这是 WAL 模式能"reset 回 0 帧"的触发条件。

### 22.5 WAL hash table 分页

`wal.c:615`：

```c
#define HASHTABLE_NPAGE      4096                 /* 2 的幂 */
#define HASHTABLE_NPAGE_ONE  (HASHTABLE_NPAGE - (WALINDEX_HDR_SIZE/sizeof(u32)))
```

每张 wal-index 页 32 KB（8192 个 `u32`），前几张存 `WalIndexHdr*2 + WalCkptInfo`，第一张剩余位置存 page-number array。第 2+ 张存 hash table + page-number array。`walFramePage(iFrame)` (`wal.c:1203`) 根据帧号计算在哪张 wal-index 页：

```c
int iHash = (iFrame + HASHTABLE_NPAGE - HASHTABLE_NPAGE_ONE - 1) / HASHTABLE_NPAGE;
```

### 22.6 复杂度表

| 操作 | 复杂度 | 说明 |
|------|--------|------|
| 单页读 | O(1) + fsync | wal-index 内存查找 → 直接读页 |
| 单帧写 | O(1) + fsync(可选) | 追加到 WAL 末尾 |
| 全帧扫描 | O(N) | N = mxFrame 帧数 |
| Checkpoint | O(mxFrame - nBackfill) | 回填增量 |
| Hash 查找页所在帧 | O(1) 平均 | hash collision 退化为链式 |

### 22.7 本项目对应

`engineering/src/db/wal.h` + `wal_buf.h` 简化版（pg_xlog 风格），**未实现**：
- WAL frame trailing checksum 8 字节；
- `aReadMark[]` 多 reader slot（用 1 个全局 `latestValid` 替代）；
- WAL hash 分页（直接 linear scan）。

---

## 二十三、Where 优化器与 Nested-Loop + Co-routine Join 算法

§9.2 仅 5 行提到 `where.c` + `sqlite3WhereBegin`。本节展开 SQLite 唯一**不依赖 RBO 规则的轻量级 CBO** 与其**只有 Nested-Loop** 的执行策略。

### 23.1 `sqlite3WhereBegin` 总体流程

`where.c:6838` 入口。**SQLite 没有传统意义的 query rewrite**，所有优化集中在 WHERE 子句扫描方式选择：

```c
// 简化自 where.c:6838-6898 实际源码
WhereInfo *sqlite3WhereBegin(Parse *pParse, SrcList *pTabList,
                             Expr *pWhere, ExprList *pOrderBy,
                             ExprList *pResultSet, Select *pSelect,
                             u16 wctrlFlags, int iAuxArg) {
    nTabList = (wctrlFlags & WHERE_OR_SUBCLAUSE) ? 1 : pTabList->nSrc;
    if (pTabList->nSrc > BMS) { /* BMS=63, 一个 Bitmask 能容纳的位数 */
        sqlite3ErrorMsg(pParse, "at most %d tables in a join", BMS);
        return 0;
    }
    // 三阶段：
    // 1) whereLoopAddAll() — 为每个表枚举所有可能扫描方式（顺序/索引/rowid）
    // 2) wherePathSolver() — DP 求 N-best join order
    // 3) sqlite3WhereCodeOneLoopStart() — 生成 VDBE 字节码
}
```

### 23.2 `wherePathSolver` N-best Paths DP

`where.c:5846`。SQLite 不像 PostgreSQL 那样做 O(3^N) 全枚举，而是**每层保留 N 条最优 path**：

```c
// 简化自 where.c:5871-5888 实际源码
/* TUNING: mxChoice is the maximum number of possible paths to preserve
** at each step.  Based on the number of loops in the FROM clause:
**
**     nLoop      mxChoice
**     -----      --------
**       1            1            // the most common case
**       2            5
**       3+        12 or 18        // see computeMxChoice()
*/
if (nLoop <= 1)     mxChoice = 1;
else if (nLoop == 2) mxChoice = 5;
else                 mxChoice = computeMxChoice(pWInfo);  /* 通常 12，复杂情形 18 */

nSpace = (sizeof(WherePath) + sizeof(WhereLoop*) * nLoop) * mxChoice * 2;
nSpace += sizeof(LogEst) * nOrderBy;
```

**复杂度**：`O(mxChoice^2 * nLoop^2)` 实际开销远低于 `O(3^N)`，代价是 3+ 表 join 时**可能错过全局最优 order**。`aTo[mxChoice]` + `aFrom[mxChoice]` 双缓冲，每轮迭代：

```c
for (iLoop=0; iLoop<nLoop; iLoop++) {
    for (ii=0, pFrom=aFrom; ii<nFrom; ii++, pFrom++) {
        for (pWLoop=pFrom->aLoop[0]; pWLoop; pWLoop=pWLoop->pNextLoop) {
            // 尝试把 pWLoop 加到 pFrom 的末尾
            cost = pFrom->rCost + pWLoop->rSetup + pWLoop->rRun * pFrom->nRow;
            // 选择 mxChoice 条最小 cost 的 path 写入 aTo[]
        }
    }
    // 交换 aFrom / aTo，进入下一层
}
```

### 23.3 `whereLoopAddBtreeIndex` 成本估算

`where.c:3223`。每条候选扫描方式（顺序扫 / rowid / 索引）都构造一个 `WhereLoop`，记录成本 `rRun` 与 `rSetup`：

```c
// 简化自 where.c:3282-3311
pTerm = whereScanInit(&scan, pBuilder->pWC, pSrc->iCursor, saved_nEq,
                      opMask, pProbe);
pNew->rSetup = 0;
rSize = pProbe->aiRowLogEst[0];   /* LogEst 类型，base-2 对数 */
rLogSize = estLog(rSize);
for (; rc==SQLITE_OK && pTerm!=0; pTerm = whereScanNext(&scan)) {
    u16 eOp = pTerm->eOperator;
    LogEst rCostIdx;
    LogEst nOutUnadjusted;
    int nIn = 0;
    // 根据 eOp 累加 rRun（每条 WHERE 条件贡献 LogEst 减少）
    // 公式：nOut -= log(table_size / distinct_count[col])
}
```

**`LogEst`** 是定点表示（实际为 `i64`，乘以 10 后取 log2），SQLite **不存浮点成本**，仅排序用。`WHERE_VIRTUALTABLE` / `WHERE_TOP_LIMIT` / `WHERE_BTM_LIMIT` 等 wsFlags 标志位组合决定选哪种扫描方式。

### 23.4 ANALYZE / sqlite_stat1 统计信息

`analyze.c:1593` `analysisLoader` callback（被 `sqlite3AnalysisLoad` `analyze.c:1942` 调用）：

```c
// 简化自 analyze.c:1616-1647
z = argv[2];   /* sqlite_stat1 表的 stat 列（文本格式整数数组）*/
if (pIndex) {
    tRowcnt *aiRowEst = 0;
    int nCol = pIndex->nKeyCol + 1;
    if (pIndex->aiRowEst == 0) {
        pIndex->aiRowEst = sqlite3MallocZero(sizeof(tRowcnt) * nCol);
    }
    aiRowEst = pIndex->aiRowEst;
    pIndex->bUnordered = 0;
    decodeIntArray((char*)z, nCol, aiRowEst, pIndex->aiRowLogEst, pIndex);
    pIndex->hasStat1 = 1;
} else {
    Index fakeIdx;
    fakeIdx.szIdxRow = pTable->szTabRow;
    decodeIntArray((char*)z, 1, 0, &pTable->nRowLogEst, &fakeIdx);
    pTable->szTabRow = fakeIdx.szIdxRow;
}
```

**stat1 格式**：`<table> <index> <stat>`，其中 `stat` 是空格分隔的整数（`LogEst` 缩放 10 倍）。`nKeyCol+1` 个数字，第一列是表总行数对数，后续每列是该索引前导列的 distinct count 对数。`sqlite_stat4`（启用 `SQLITE_ENABLE_STAT4`）额外存 `nSample` 条采样（key, rowid, nEq, nLt, nDLt）。

### 23.5 Nested-Loop Join + Co-routine

SQLite **只支持 Nested-Loop Join**——没有 Hash Join、没有 Merge Join。每个 loop 通过 `OP_Gosub` + `OP_Return` 实现为**字节码子例程**，外层 loop 用 `OP_Yield` 调起。

`wherecode.c:1533` `sqlite3WhereCodeOneLoopStart` 的控制流：

```
┌─────────────────────────────────────────────────────────────────┐
│ Outer loop (iLevel=0)                                           │
│   InitCoroutine regYield addrTop                                │
│   ... open cursor, position ...                                 │
│   Gosub regReturn addrBody                                      │
│   ... continue/yield ...                                        │
│ addrBody:                                                       │
│   ... loop body (read columns, evaluate WHERE, emit result) ... │
│   Yield regYield addrCont                                       │
│   Return regReturn  (-- 跳回 Gosub 之后 --)                     │
└─────────────────────────────────────────────────────────────────┘
```

`wherecode.c:2346-2563` 中：

```c
// 简化自 wherecode.c:2346-2563 实际源码
int regReturn = ++pParse->nMem;          /* Gosub/Return 寄存器 */
int regRowset = 0;                       /* OR-clause 优化 RowSet */
int regRowid = 0;
int iLoopBody = sqlite3VdbeMakeLabel(pParse);   /* 子例程入口 */
int iRetInit;

// 生成 rowset 测试 + Gosub
sqlite3VdbeAddOp2(v, OP_Gosub, regReturn, iLoopBody);
// ... 继续外层 loop ...

// 标号 A: 子例程体
//   <loop body>
//   Return regReturn  → 跳回 Gosub 之后
```

OR-clause（`a OR b OR c`）通过 `RowSet` 去重：每条子句执行后把 rowid 写入 RowSet，下条子句先 `RowSetTest` 跳过已见 rowid。

### 23.6 复杂度对比

| 数据库 | Join 算法 | N 表复杂度 | 适用 |
|--------|-----------|-----------|------|
| SQLite | Nested-Loop only | O(∏\|Ri\|) | 小数据集，1-3 表 join |
| PostgreSQL | NLJ + MergeJoin + HashJoin | O(N log N)（merge/hash） | 通用 |
| MySQL 8.0+ | BNL + Hash Join (8.0.18+) | O(N+M) | 通用 |

**SQLite 没有 `LIMIT push-down` 之外的优化**，所有 join 都是字面意义的逐行嵌套循环。

### 23.7 本项目对应

`engineering/src/db/sql/sql_planner.c` + `engineering/src/db/sql/sql_exec.c` 是从零实现的极简 CBO+执行器：
- 优化器仅支持**枚举全部 join order**（不超过 5 表）；
- Join 算法仅 Nested-Loop；
- **未实现** OR-clause 转 RowSet 去重、覆盖索引自动识别、co-routine 字节码生成。

`engineering/src/db/btreeam.c` + `engineering/src/db/heapam.c` 提供表/索引扫描原语，对应 SQLite `sqlite3BtreeCursor` + `sqlite3BtreeFirst/Next`。