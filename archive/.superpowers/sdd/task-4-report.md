# Task 4 报告：归档 SQL-over-KV 路径（WS-B 核心）

日期：2026-09-26
分支：feat/sql-consolidation
执行环境：Windows 11 + Git Bash（MinGW gcc 14.2.0）

（注：本路径此前存有 intent-gate 项目的旧 task-4 报告，已保留为
`task-4-report.intent-gate-legacy.md`，未删除。）

---

## 1. Step 1 — 归档前全仓引用扫描

### 1.1 brief 指定扫描的结果

按符号逐项扫描（engineering 全树，含 src/ apps/ test/ tests/ sdk/ tools/）：

| 符号 | 引用者 | 处置 |
|---|---|---|
| `db/executor/sql/sql_exec.h` | `src/db/executor/sql/sqlExec.c`（自身，归档）、`src/db/cli/cli.c`（CLI 目标整体注释，T10 重写） | 头文件随源码归档 |
| `sql_parse_one`（简化版，`sql_node_t*(const char*)`） | 定义于 `src/db/parser/sql/sql_parser.c:817`（归档）；调用者仅 `test/db/sql/sql_parser.cpp`、`test/db/sql/test_planner.cpp` | 两个测试按 controller 调整 #3 摘除 |
| `sql_exec_create` / `sql_exec_ddl` / `sql_exec(` | 全仓（src/apps/test/tests/sdk）**零调用者**（sqlExec.c 自身除外） | 安全归档 |
| `table_create(` / `table_open(`（rel 版，`kv_t*` 签名） | 定义于 `src/db/storage/rel/table.c`（归档）；调用者：`sqlExec.c`（归档）、`src/db/validator/sql_semantic.c:52`、`src/db/validator/semantic_analyzer.c:52` | 见「意外发现 1」 |
| `db/table.h` 头 | `rel_engine.c`、`sqlExecutor.c`（仅 include，无调用）、`sql_semantic.c`、`semantic_analyzer.c`、`table.c`、`sqlExec.c` | **保留不归档**（仍被在编的 rel_engine.c / sqlExecutor.c include） |
| `db/storage/rel/table.h` 头 | `engineering/tests/test_relational.c`（该目录无 CMakeLists、不在构建内，是手工 .o 的暂存目录） | 保留 |
| `db/parser/sql/sql.h` 头 | `cli.c`（禁用）、`optimizer.c`、`planner.c`、`sql_driver.c`、三个归档文件 | 保留（planner.c/optimizer.c 仍在 sql_engine/db_optimizer 中 include 它） |

### 1.2 意外发现（brief 预期之外）

1. **validator 也依赖 table.c 符号**：`sql_semantic.c` 与 `semantic_analyzer.c` 调用 `table_open`。
   评估结论：可安全归档 table.c，因为——
   - 这两个文件是**内容完全相同的重复副本**（同行号同函数，均定义 `sql_semantic_create` 等；
     GLOB 同时编入 db_validator 静态库，靠静态库成员去重侥幸不炸）；
   - `sql_semantic_*` 全仓唯一调用者是 sqlExec.c（随之归档）；CLI（另一潜在拉动者）目标禁用；
   - 因此归档后没有任何存活链接路径会拉入 validator 的 `table_open` 引用。
   已验证：libdb_validator.a 与 13 个 db_executor 测试 exe 全部构建成功。
2. **两个不同签名的 `sql_parse_one`**：`parser/sql/sql_parser.c:817` 为
   `sql_node_t*(const char*)`；`src/db/sql/sql_parser.c:1976` 为
   `SqlParseResult*(const char*, int)`。若两库同时被链接即冲突——归档后该隐患消除。
3. **`db_parser_sql_simplified` 的 CMake 引用面远超 brief 所列**：共 ~30 处
   （executor:21、sql:48、validator:13、test/db/sql 全部 22 个目标、test/db/sql/benchmark、
   test/db/parser）。已全部清理（见 §3）。
4. **`parse_expr.c.bak` 未被 git 跟踪**（且 `*.bak` 在 .gitignore 中）：用普通 `mv` + `git add -f` 归档。
5. **两个 `sql_parser.c` 同名冲突**：`src/db/sql/sql_parser.c` 归档时改名为
   `archive/db-sql-over-kv/src_db_sql__sql_parser.c`。
6. **`include/db/table.h` 与 `include/db/storage/rel/table.h` 是同一 API 的重复声明**
   （均于 :61 声明 `table_create(kv_t*,...)`）。本 Task 不处理，记录备查。

---

## 2. Oid 普查（controller 新增交付物）

### 2.1 typedef 站点全表（共 17 处；uint32_t ×13、uint64_t ×4、unsigned int ×1）

| # | 文件：行 | 宽度 | 所在模块 |
|---|---|---|---|
| 1 | engineering/include/db/catalog.h:24 | uint32_t | catalog（顶层） |
| 2 | engineering/include/db/storage/catalog/catalog.h:24 | uint32_t | storage/catalog（与 #1 重复声明） |
| 3 | engineering/include/db/graph/index.h:16 | uint32_t | graph 索引 |
| 4 | engineering/include/db/parser/sql/parse_node.h:96 | uint32_t | parser（简化 AST 阵营） |
| 5 | engineering/include/db/sql/nodes/nodeSeqscan.h:27 | uint32_t | sql 执行节点 |
| 6 | engineering/include/db/sql/nodes/nodeIndexscan.h:34 | uint32_t | sql 执行节点 |
| 7 | engineering/include/db/sql/sql_types.h:18 | **uint64_t** | sql 公共类型（planner/executor 阵营） |
| 8 | engineering/include/db/sql/cost.h:28 | **uint64_t** | 代价模型 |
| 9 | engineering/include/db/sql/trigger.h:142 | **uint64_t** | 触发器 |
| 10 | engineering/include/db/sql/trigger_functions.h:42 | **uint64_t** | 触发器函数 |
| 11 | engineering/include/db/tools/sys_catalog.h:23 | uint32_t | tools/sys_catalog |
| 12 | engineering/src/db/sql/nodeSeqscan.c:20 | uint32_t | .c 内重复 typedef |
| 13 | engineering/src/db/sql/nodeIndexscan.c:19 | uint32_t | .c 内重复 typedef |
| 14 | engineering/src/db/storage/graph/graph_index.c:17 | uint32_t | .c 内重复 typedef |
| 15 | engineering/test/db/sql/test_data_helper.h:15 | uint32_t | 测试辅助头 |
| 16 | learning/scaffold/db/storage_overview/main.c:18 | uint32_t | learning 脚手架（教学代码，独立轨道） |
| 17 | learning/scaffold/db/catalog_system/main.c:15 | unsigned int（32 位） | learning 脚手架（教学代码，独立轨道） |

**宽度结论：engineering 生产/测试代码中 uint32_t ×11、uint64_t ×4；冲突核心仍是 spike 已定位的
parse_node.h(uint32) vs sql_types.h/cost.h/trigger*.h(uint64)。无其他宽度。**
（按任务要求未统一、未修改任何定义；宽度决策留待与用户讨论。）

### 2.2 Oid 使用密度（`\bOid\b` 出现次数，按区域聚合）

| 区域 | 文件数 | 总次数 | 重度用户 |
|---|---|---|---|
| engineering/include/db | 24 | 164 | parse_node.h(29)、catalog.h(25)、storage/catalog/catalog.h(25)、rel.h×2(12+12)、trigger_functions.h(10) |
| engineering/src/db | 14 | 76 | storage/catalog/catalog.c(21)、sqlExecutor.c(10)、graph_index.c(10)、trigger_functions.c(6) |
| engineering/test | 18 | 100 | catalog_test.cpp(18)、test_storage_integration.cpp(11)、storage_integration_test.cpp(8) |
| learning（脚手架/笔记） | ~8 | ~25 | scaffold/db/*、scaffold/cpp/pimpl、scaffold/linux/rcu |

观察：Oid 的重度使用集中在 **catalog（两个 catalog.h 各 25 次）与 parser AST（parse_node.h 29 次）**；
sql 执行节点阵营（nodes/、trigger、cost）次数不多，但恰恰是 uint64 阵营——ABI 冲突面集中在
「catalog/parser(uint32) ↔ sql 执行链(uint64)」的接缝处。graph（graph_index.c/index.h）自成 uint32 一派。

---

## 3. Step 2/3 — 归档与 CMake 变更清单

### 3.1 git mv 归档（archive/db-sql-over-kv/）

| 原路径 | 归档后文件名 |
|---|---|
| engineering/src/db/executor/sql/sqlExec.c | sqlExec.c |
| engineering/src/db/storage/rel/table.c | table.c |
| engineering/src/db/parser/sql/sql_lexer.c | sql_lexer.c |
| engineering/src/db/parser/sql/sql_parser.c | sql_parser.c |
| engineering/src/db/sql/sql_parser.c | src_db_sql__sql_parser.c（重命名避免同名冲突） |
| engineering/src/db/parser/sql/parse_expr.c.bak | parse_expr.c.bak（未跟踪文件，git add -f） |
| engineering/include/db/executor/sql/sql_exec.h | sql_exec.h |

按 controller 调整 #2：`makefuncs.c`、`parse_node.c`、`parse_analyze.c`、`parse_expr.c`
**保留在源码树**（canonical Bison parser 的依赖，T6 编入 canonical 目标）。
按扫描结果调整：`include/db/table.h`、`include/db/storage/rel/table.h` **不归档**
（仍被在编代码 include，归档会产生新的编译失败）。

### 3.2 CMake 修改（7 个文件）

| 文件 | 修改 |
|---|---|
| engineering/src/db/parser/sql/CMakeLists.txt | 删除 `SQL_PARSER_SIMPLIFIED_SOURCES` 块与 `db_parser_sql_simplified` 目标，加 T4 注释；Flex/Bison 块原样保留 |
| engineering/src/db/executor/CMakeLists.txt | db_executor 链接行移除 `db_parser_sql_simplified` |
| engineering/src/db/sql/CMakeLists.txt | sql_engine 链接行移除 `db_parser_sql_simplified` |
| engineering/src/db/validator/CMakeLists.txt | db_validator 链接行移除 `db_parser_sql_simplified`（brief 未列但必需） |
| engineering/test/db/sql/CMakeLists.txt | test_sql_parser、test_sql_planner 整段注释摘除（`# T4: archived with stack A, rewritten in T12`）；其余 20 个目标链接行移除 `db_parser_sql_simplified`；自定义目标 test_sql 同步移除两个被摘除测试 |
| engineering/test/db/sql/benchmark/CMakeLists.txt | sql_benchmark 链接行移除 `db_parser_sql_simplified` |
| engineering/test/db/parser/CMakeLists.txt | db_parser_sql_test 注释摘除（`# T4: archived with stack A, rewritten in T12`；它使用 makefuncs/parse_node 符号，helper 源文件保留但 T6 前无库编译它们） |

注：`src/db/parser/CMakeLists.txt:19` 的 `if(NOT TARGET db_parser_sql_simplified ...)` 回退分支
现在会求值，但 `src/db/parser/` 顶层无 .c 文件，不会创建 db_parser 目标，无影响。

---

## 4. Step 4 — 构建验证（基线对比）

基线（spike §1.2，权威）：FAILED 目标 = sql_engine(cost.c, nodeSeqscan.c)、storage_backend_memory、
storage_backend_common（storage_mmap.c）。验收标准（controller 调整 #1）：**不得出现新的失败目标**。

本次用 `-k 0` 全量构建枚举了所有失败目标（覆盖面大于 spike 的增量构建，因此暴露出大量 spike
未触及的**既有 bit-rot**，均与本 Task 无关）：

### 4.1 与本 Task 变更域相关的目标——全部通过

- `libdb_executor.a` 构建成功（无 sqlExec.c、无 simplified 链接引用）✓
- `libdb_validator.a` 构建成功 ✓
- `storage_rel`（OBJECT 库）编译成功（无 table.c）✓
- 13 个 db_executor 测试 exe 全部链接成功（px_queue_test、shard_prune_test、distributed_join_test 等）✓
- 全部失败日志中**无任何**对 `sql_parse_one`/`sql_exec_create`/`sql_exec_ddl`/
  `table_create`/`table_open`/`table_insert`/`table_scan`/`table_drop` 的 undefined reference ✓

### 4.2 失败目标分类

| 类别 | 目标 | 与本 Task 关系 |
|---|---|---|
| 基线既有（spike 已记录） | sql_engine(cost.c 1 错、nodeSeqscan.c 8 错，错误文本与 spike 逐字一致）、storage_backend_memory/common（+mmap/pagefile 变体，同因 storage_mmap.c 缺 Win32 头） | 无关；sql_engine 修复属 T8 |
| 既有 bit-rot（-k 0 才暴露，spike 增量构建未到达） | test/kbase 全部 42 个 .obj、aging、ledger、netconf、subscription、yang、spatial、graph tests、columnar/stream/sparse/rdf/tid_pipe/kv 测试、games_server、multimodal_rag_server、sdk aggregate_test、consistency_test | 无关（错误均为 strndup/kv_open/mm_storage_init/spatial_engine_get_ops/heap_insert 签名漂移等，均不触及归档符号） |
| test/db/sql 域编译失败 | test_sql_storage_integration.cpp、test_storage_integration.cpp、benchmark_scan.cpp、benchmark_aggregate.cpp（`heap_insert` 参数个数漂移 + int→const char* 转换） | **经逐条核对错误文本确认与本 Task 无关**：不引用任何归档头/符号；属既有 API 漂移，spike 未跑到这些目标。建议并入 T12 重写范围 |
| 链接失败 exe | guc、raft_test、sq_test、engine_registry_test、security_test、test_sparse、test_rdf_engine、test_sharding、test_vector_index | 错误符号为 strndup/mm_storage_init/kv_open/kv_scan/spatial_engine_get_ops 等，与归档符号零交集 |

**结论：无新增失败目标。** 所有失败要么逐字匹配 spike 基线，要么为与本变更域零交集的既有 bit-rot。

### 4.3 ctest 对比

基线（spike §1.3，`ctest --test-dir build/test --timeout 60`）：
Passed 335 / Failed 45 / Timeout 3 / NotRun 81（注册 465，统计 447）。

本次结果：见 §4.4 提交前补记。

预期差异：test_sql_parser、db_parser_sql_test、test_sql_planner 从注册表消失（已按调整 #3 摘除）；
build/test/db/sql/test_sql_parser.exe 为基线遗留产物，不再重建。

### 4.4 ctest 实跑数字

命令：`ctest --test-dir build/test --timeout 60`。注册测试 465（与基线相同）。

| 指标 | 基线（spike） | 本次 | 说明 |
|---|---|---|---|
| Passed | 335 | **413** | +78 |
| Failed（实跑） | 45 | 52（含 3 个 TerminalTest 环境性超时） | 见下 |
| Not Run | 81 | **0** | 本次 `-k 0` 全量构建把基线缺失的测试二进制都编出来了，故无 Not Run |

失败明细对比：

- **与基线完全一致的失败簇**：CopySqlTest 7、CopyBinaryTest 7、CopyJsonTest 6、ExplainYamlTest 5、
  CopyTextExportTest 4、CopyTextRoundtripTest 2、CopyCsvRoundtripTest 2、BgWorkerTest、
  TerminalTest 3（GetchNoBlock/GetchWithTimeout/FullWorkflow，控制台输入阻塞型，非交互 shell 必超时）。
- **基线失败、本次转为通过**：IVFPQTest 9、IVFFlatTest 9（疑与本次 configure 为 Debug、基线为 Release 有关）。
- **本次新出现的失败**（均为基线 Not Run、本次新编出二进制后实跑的结果，非回归）：
  - CopyCsvExportTest 7：失败于 `temp_fp != nullptr`（临时文件创建失败，环境/tmpdir 问题），与归档符号无关；
  - NSWTest 3（vector 索引）、cross_modality_test、test_doc_pipeline、test_rest_api、test_storage_wal：
    均为 T4 变更域之外的引擎/应用测试。
- **T4 变更域内测试**：无任何失败；test_sql_parser / test_sql_planner / db_parser_sql_test
  已按调整 #3 从注册表摘除（不出现，符合预期）。

**结论：ctest 维度同样无可归因于本 Task 的新增失败；Passed 数大幅上升源于构建覆盖完整化。**

---

## 5. 遗留事项 / Concerns

1. `test/db/sql` 下 4 个测试源（test_sql_storage_integration、test_storage_integration、
   benchmark_scan、benchmark_aggregate）存在既有 `heap_insert` API 漂移编译错误——非本 Task 引入，
   但会在 T8 修复 sql_engine 后成为该域的下一批阻塞点，建议并入 T12 重写范围时一并处理。
2. validator 的 `sql_semantic.c` 与 `semantic_analyzer.c` 是完全相同的重复文件；
   `include/db/table.h` 与 `include/db/storage/rel/table.h` 是重复声明——均不在本 Task 范围，
   建议 T8/T12 清理。
3. `build/src/db/storage/rel/CMakeFiles/storage_rel.dir/table.c.obj` 为归档前遗留的过期 .obj
   （ninja 不引用它，无害）；如需洁癖可删 build 目录重配。
4. `src/db/parser/sql/` 下 4 个 helper .c 在 T6 前不被任何目标编译——期间若有人新建引用
   makefuncs 符号的测试会链接失败（test/db/parser 的 db_parser_sql_test 已因此摘除）。
5. cli.c 仍 include 已归档的 `db/executor/sql/sql_exec.h`——CLI 目标当前整体注释不受影响，
   T10 重写时需改用新头。
