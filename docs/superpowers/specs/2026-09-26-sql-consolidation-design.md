# SQL 栈全收敛设计（SQL Modality Consolidation）

- **日期**：2026-09-26
- **状态**：已批准（设计评审通过）
- **所属分解**：数据库内核全栈深化的第 1 个子项目（S0）；后续子项目：S1 执行器/表达式补完、S2 sqllogictest 测试体系、S3 SQL 覆盖面扩展、S4 查询优化器、S5 存储底座深化、S6 事务与并发

## 1. 背景与问题

当前 `engineering/src/db` 存在**三套并行的 SQL 栈**，集成断裂：

| 栈 | 组成 | 状态 |
|---|---|---|
| A. 简化路径 | 手写解析器（仅 6 种语句）+ KV 执行器（`sqlExec.c`） | CLI 实际在用，能力最弱 |
| B. PG 风格引擎 | planner + volcano 算子（17 种）+ cost（`src/db/sql/`） | 驱动入口 `sql_driver.c` 被 CMake 排除；多个核心算子是返回 NULL 的 TODO 桩 |
| C. Flex/Bison 完整语法 | `gram.y`（851 行）+ `scan.l` | 语法最完整（JOIN/子查询/GROUP BY/HAVING/ORDER BY/LIMIT/CASE），但 flex/bison 为可选依赖，Windows 构建未编译 |

同时，`MMDB_ENABLE_*` 模态隔离体系（11 个模态开关，声明于 `cmake/MultimodalConfig.cmake`）**骨架已存在但未生效**：

1. CMake 生成的 `multimodal_config.h` 从未进入任何 target 的 include path；实际编译使用源码树中一份过期提交副本 `include/db/multimodal_config.h`。`-DMMDB_ENABLE_KV=OFF` 会链接失败——隔离从构造上是坏的。
2. 全仓仅 `src/db/core/engine_registry.c` 真正使用这些宏；SQL 执行器、parser、validator、catalog 全部无条件编译且硬引用 KV 符号。
3. CMake 层自相矛盾：DOCUMENT/TIMESERIES 空操作；ST/RDF 检查从未声明的变量（死分支）；txn 子目录被注释掉。

KV 引擎本身是完整的 Redis 式产品（put/get/delete/scan/cas/watch/ttl/batch/LSM/wide_row），被 graph、CF、sharding、txn、multimodal_rag 应用显式依赖。**"SQL 执行器跑在 KV 上"（`sqlExec.c`/`table.c`）是历史遗留的畸形耦合**；heapam 路径（slotted-page on buf/WAL/MVCC）本来就独立于 KV。

## 2. 目标与范围

**总目标**：三套 SQL 栈收敛为一条 canonical 路径；`MMDB_ENABLE_*` 隔离体系真正接通，每个数据模态可独立编译、独立运行。一次到位，不留后续任务。

**目标态数据流**（收敛后唯一 SQL 路径）：

```
SQL 文本
  → ① Bison 解析器（gram.y/scan.l，预生成 .c 入仓）→ PG 风格 AST（parsenodes.h NodeTag）
  → ② 语义分析（catalog 校验、类型检查）
  → ③ planner（逻辑/物理计划 + cost）
  → ④ volcano 执行器（SeqScan/IndexScan/HashJoin/Agg/Sort/Limit/ModifyTable…）
  → ⑤ heapam 存储（slotted-page on buf/WAL/MVCC）
  ← 结果集 → CLI / 未来 pgwire
```

**模态隔离目标态**：

- `MMDB_ENABLE_RELATIONAL`：SQL 全链路 + catalog/buf/WAL/heapam 底座，编译期不依赖 KV
- `MMDB_ENABLE_KV`：独立 Redis 式引擎，被 graph/CF/sharding/multimodal_rag 显式依赖
- 其余开关：CMake 逻辑修正到"声明即生效"，消除死分支与空操作
- 配置头单一来源：CMake 生成的 `multimodal_config.h`；删除源码树过期副本

**移出（归档至 `archive/`，不删除）**：`sqlExec.c`、`table.c`、手写解析器全套（`sql_lexer.c`/`sql_parser.c`/`makefuncs.c`/`parse_analyze.c`/`parse_expr.c`/`parse_node.c`）、2099 行被排除解析器（`src/db/sql/sql_parser.c`）、`parse_expr.c.bak` 等残留。`gram.y`/`scan.l` 保留并成为 canonical。

**非目标**（留给后续子项目）：优化器规则实装（S4）、新 SQL 语法扩展（S3）、事务/并发深化（S6）、算子 TODO 超出"接通主路径所需"的深度补完（S1）。

**已确认的关键决策**：

1. 每模态独立 CMake 开关，默认全开，任一模态可单独编译运行
2. KV 保留为独立 Redis 式模态；SQL↔KV 全部解耦
3. canonical 解析器 = Bison 语法 + 预生成 `gram.c`/`scan.c` 提交入仓（Windows 构建零新工具链依赖）
4. 全收敛一次到位：宏骨架修复 + 解耦 + 解析器统一 + 驱动接通 + 测试绿化，一个子项目完成

## 3. 组件级改动清单（六个工作流，按依赖顺序）

### WS-A · 宏骨架修复

| 改动 | 文件 |
|---|---|
| 生成头接入：`${CMAKE_BINARY_DIR}/generated/include` 加入相关 target include path，链接 `multimodal_config_gen` | `cmake/MultimodalConfig.cmake`、各层 `CMakeLists.txt` |
| 删除源码树过期副本 | `include/db/multimodal_config.h`（git rm） |
| 修死分支/空操作：ST/RDF 未声明变量、DOCUMENT/TIMESERIES 空转——接入或标记暂不支持且默认 OFF | `src/db/storage/CMakeLists.txt` |
| `engine_registry.c` 注册逻辑与生成头对齐验证 | `src/db/core/engine_registry.c` |

### WS-B · SQL↔KV 解耦

| 改动 | 文件 |
|---|---|
| 归档（移出构建，进 `archive/db-sql-over-kv/`） | `src/db/executor/sql/sqlExec.c`、`src/db/storage/rel/table.c`、`src/db/parser/sql/` 下的**手写版文件**（`sql_lexer.c`、`sql_parser.c`、`makefuncs.c`、`parse_analyze.c`、`parse_expr.c`、`parse_node.c`——`gram.y`/`scan.l` 保留原位并新增 `generated/`）、`src/db/sql/sql_parser.c`、`parse_expr.c.bak` |
| validator 解耦：`kv_t` 依赖改为 catalog 接口 | `src/db/validator/sql_semantic.c`、`semantic_analyzer.c` |
| 清除对 kv 的引用 | `include/db/sql/sql_driver.h` |

### WS-C · 解析器统一（canonical = Bison）

- WSL2 用 flex/bison 预生成 `gram.c`/`scan.c`，提交至 `src/db/parser/sql/generated/`；Windows 构建零新依赖
- CMake 默认使用预生成文件；`REGENERATE_PARSER=ON` 选项供维护时重新生成（流程写入文档）
- 解析入口统一为 `sql_parse(const char*) → Node*`，消除 `sql_parse_one` 命名冲突；对外只暴露一个入口

### WS-D · 驱动与执行器接通

- 复活 `sql_driver.c`：修复被排除的根因（"Expr 类型冲突"），接通 解析→语义→planner→executor 全链路，对外暴露 `execute_sql()`
- 修 `sqlExecutor.c` 的 API 漂移，对齐 catalog/heapam/WAL 当前接口
- 主路径必需的算子最小修补（Agg/Limit/Result 等 TODO，仅限接通所需深度）
- **CLI 重接线**：从 `kv_open + sql_exec` 切到 `execute_sql()`（heapam 后端）；CLI 初始化改为 buf pool/WAL/catalog 启动序列；KV 命令保留为 CLI 内独立子命令（kv API 直连）

### WS-E · CMake 排除项清理

- `src/db/sql/CMakeLists.txt` 排除清单逐项复核：随 `sql_driver.c` 复活移除；`db_executor` 不再无条件 glob 两个执行器；各子目录加 `MMDB_ENABLE_*` 守卫
- **约束**：`apps/multimodal_rag/CMakeLists.txt` 的 EXCLUDE 规则不动（每条都有历史，见团队记忆）

### WS-F · 测试绿化

- 消费旧 AST 的测试（`test_planner.cpp`、`sql_parser.cpp` 等）：迁移到 canonical AST 或随归档移出
- `sql_integration.cpp`（51 个 CLI 端到端用例）改走新 CLI 路径
- 验收基线：Windows + WSL2 Linux 双平台 `ctest` 全绿；模态开关组合构建通过

## 4. 数据流细节

### 4.1 CLI 启动序列（收敛后）

```
cli_main()
  ├─ 解析参数（db_path 等）
  ├─ IF MMDB_ENABLE_RELATIONAL:
  │    ├─ buf_pool_init()        — buffer pool（Clock-Sweep）
  │    ├─ wal_init(db_path)      — WAL 打开/恢复（redo）
  │    ├─ catalog_bootstrap()    — 系统目录加载
  │    └─ 进入 SQL REPL：每条输入 → execute_sql()
  └─ IF MMDB_ENABLE_KV:
       └─ KV 子命令（PUT/GET/DEL/SCAN…）→ kv_open() 直连 kv API
```

CLI 不再无条件 `kv_open`：SQL REPL 的存储上下文是 buf/WAL/catalog 三件套；KV 只在 `MMDB_ENABLE_KV` 开启且用户发 KV 子命令时才打开。

### 4.2 一条 SELECT 的生命周期

```
"SELECT name FROM users WHERE age > 30"
  ① sql_parse()        → SelectStmt{targetList, fromClause, whereClause: A_Expr(age>30)}
  ② 语义分析           → catalog 校验表/列存在、类型可比较；失败报带位置的错误
  ③ planner            → SeqScan(users) + Filter(age>30) + Project(name)
                          （索引选择接口保留，行为以现有 cost 代码为准；实装属 S4）
  ④ executor           → volcano: SeqScan 经 heapam 逐 tuple → Filter 求值 → Project
  ⑤ heapam             → buf_pool 读页 → MVCC 可见性 → 返回可见 tuple
  结果集 → CLI 表格化打印（列头 + 行 + "N rows"）
```

DML 末端算子换 `ModifyTable`，写路径经 WAL → heapam → buf pool 刷盘。DDL 走 catalog 变更 + 存储对象创建。

## 5. 错误处理与边界情况

### 5.1 错误分层

| 层 | 错误类型 | 处理策略 |
|---|---|---|
| 解析层 | 语法错误 | Bison `yyerror` 带 line/column；不崩溃、不退出 REPL |
| 语义层 | 表/列不存在、类型不匹配、约束违反 | 统一错误码 + 消息，经 `execute_sql()` 返回值上抛；REPL 继续 |
| 计划/执行层 | 不支持的算子组合、资源不足 | 明确报错；禁止静默返回 NULL 结果集 |
| 存储层 | IO 失败、WAL 写失败、页损坏 | 错误上冒到语句边界；宁可 fail-fast 也不带病运行（进程级恢复属 S6） |

### 5.2 关键边界情况

1. **模态关闭时**：`MMDB_ENABLE_RELATIONAL=OFF` 的 CLI 收到 SQL → 明确提示 `relational modality not enabled in this build`；KV 同理。靠 CLI 侧 `#ifdef` 守卫 + 测试验证
2. **空结果集 vs 错误**：无命中行 = 正常 0 行；表不存在 = 错误，严格区分
3. **类型相碰**：`WHERE age > 'abc'` 语义层拒绝；不做隐式转换（对齐 PG 严格性）
4. **WAL 恢复失败**：启动即报错拒绝开门（fail-fast）
5. **多语句输入**：REPL 按 `;` 分割逐条执行，前条失败不阻断后条；脚本模式（`cli < file`）遇错即停、非零退出码（便于 CI）
6. **归档代码的测试**：随归档源文件一并搬走，不用 EXCLUDE 隐藏

### 5.3 回归防护原则

任何"先返回 NULL/空以后再补"的桩：要么实装、要么显式硬报错（`elog(ERROR, "not implemented: %s")` 式）。**禁止静默假成功。** 此原则为后续 S1–S6 共享。

## 6. 测试策略与验收标准

### 6.1 测试分层

| 层 | 内容 | 载体 |
|---|---|---|
| 单元 | 解析器（grammar 各产生式）、语义分析、算子、heapam | 现有 gtest，迁移到 canonical AST |
| 集成 | `execute_sql()` 端到端 DDL→DML→SELECT（含 JOIN/聚合用例，真实通过） | `test/db/sql/` 改造 |
| CLI 端到端 | `sql_integration.cpp` 51 用例走新 CLI；脚本模式纳入 CI | 现有套件改造 |
| 模态组合构建 | `{RELATIONAL×KV}×{ON,OFF}` 四组合 configure+build 通过；RELATIONAL=ON/KV=OFF 时 SQL 测试绿 | 新增 CTest/CI 步骤 |
| 双平台 | Windows + WSL2 Ubuntu 全绿 | 手动验收 + CI |

### 6.2 Definition of Done

1. **唯一路径**：CLI 的 SQL 只经 Bison(预生成)→语义→planner→volcano→heapam；`sqlExec.c`/`table.c`/两个旧解析器已归档且不在任何构建目标
2. **隔离生效**：`-DMMDB_ENABLE_KV=OFF` 全量构建 + SQL 测试绿；`-DMMDB_ENABLE_RELATIONAL=OFF` 构建出可用 KV CLI；源码树无 `multimodal_config.h` 副本
3. **能力兑现**：CLI 实际跑通 JOIN、GROUP BY/聚合、ORDER BY/LIMIT、子查询，并有通过的 e2e 用例
4. **无静默桩**：主路径 `return NULL` 式 TODO 桩要么实装要么硬报错（grep 可验证）
5. **双平台 ctest 全绿**，无新增警告

### 6.3 风险与对策

| 风险 | 对策 |
|---|---|
| `sql_driver.c` 的 "Expr 类型冲突" 比预期深 | 实施计划第一步做 spike 探明根因；若根因是 AST 类型分裂，由 WS-C 统一解决 |
| 预生成 `gram.c` 与 bison 版本敏感 | 锁定生成环境（WSL2 bison 版本记录于实施文档），生成文件头部标注工具版本 |
| 归档 KV 执行器后发现隐性依赖 | WS-B 先做全仓引用扫描；归档与构建验证同一提交，出问题回退单提交 |
| 本仓库 git 索引操作病理性缓慢 | 小步多提交 + 纯引用操作；避免大 checkout/rebase |

## 7. 附：现状关键事实（设计依据）

- 简化手写解析器：`src/db/parser/sql/sql_parser.c`（935 行，6 种语句）
- Bison 语法：`src/db/parser/sql/gram.y`（851 行）+ `scan.l`（251 行）
- PG 风格引擎：`src/db/sql/`（planner.c 1441 行、executor.c、17 个 node*.c 算子、cost）
- 被排除的驱动入口：`src/db/sql/sql_driver.c`（"Expr 类型冲突"）
- 被排除的 2099 行手写解析器：`src/db/sql/sql_parser.c`（"兼容性问题"）
- KV 执行器：`src/db/executor/sql/sqlExec.c`（行存为 KV 对）、`src/db/storage/rel/table.c`
- heapam：`src/db/storage/access/heap/heapam.c`（独立于 KV，on buf/WAL/MVCC）
- 宏声明：`cmake/MultimodalConfig.cmake:9-27`；生成头模板：`cmake/multimodal_config.h.in`
- 唯一用宏的文件：`src/db/core/engine_registry.c`
- 测试：`test/db/sql/` 约 49 个 gtest 文件、~1.5 万行
