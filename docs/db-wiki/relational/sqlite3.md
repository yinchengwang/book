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