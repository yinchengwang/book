# SQL 栈全收敛实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将三套并行 SQL 栈收敛为一条 canonical 路径（Bison 预生成解析器 → 语义 → planner → volcano → heapam），接通 `MMDB_ENABLE_*` 模态隔离体系，SQL↔KV 完全解耦，双平台测试绿化。

**Architecture:** 以现有 PG 风格引擎（`src/db/sql/`，planner 头文件已 include `parsenodes.h`——本就是为 Bison AST 设计）为 canonical 核心；Bison 语法经 WSL2 预生成为 C 文件入仓，Windows 构建零新依赖；KV 引擎保留为独立 Redis 式模态。

**Tech Stack:** C11、CMake ≥ 3.16、Flex/Bison（仅 WSL2 重新生成时需要）、gtest、双平台（Windows + WSL2 Ubuntu）。

**Spec:** `docs/superpowers/specs/2026-09-26-sql-consolidation-design.md`（commit 624548544）

**Spec 修正（计划编写时发现，已获 grounding 证实）：** spec §3 WS-B 归档清单中的 `makefuncs.c`/`parse_node.c`/`parse_analyze.c`/`parse_expr.c` **不归档**——它们是 canonical Bison 解析器（`db_parser_sql_full`）的共享 helper，gram.y 直接 include `makefuncs.h`。实际归档的是 `sql_lexer.c` + `sql_parser.c`（简化前端）两个文件。

## Global Constraints

- 归档用 `git mv` 到 `archive/db-sql-over-kv/`，**不删除**；归档代码不进任何构建目标（不用 EXCLUDE 隐藏）
- 主路径禁止静默假成功：`return NULL` 式 TODO 桩要么实装要么硬报错
- `apps/multimodal_rag/CMakeLists.txt` 的 EXCLUDE 规则一律不动
- git 操作：本仓库 `status`/`diff`/`checkout` 病理缓慢；用单文件 `git add <path>` + `git commit`（已验证可用），避免全量扫描命令
- 配置头单一来源：CMake 生成的 `multimodal_config.h`；源码树不得再出现该文件的提交副本
- 模态开关默认值保持现状（RELATIONAL/KV/GRAPH/VECTOR/SPATIAL/TREE/SPARSE/DISTRIBUTED=ON；STREAM/COLUMNAR/DISTRIBUTED_RAFT/DISTRIBUTED_SHARD=OFF；TIMESERIES/DOCUMENT 改为 OFF 并明示暂不支持）
- 构建目录约定：`engineering/` 下 `cmake -B build -S . -DBUILD_TESTING=ON`；Linux 用 `build-linux/`
- 每个 Task 结束必须 `cmake --build` 通过 + 相关测试绿 + 提交

---

## 文件结构总览

| 文件 | 责任 | 所属 Task |
|---|---|---|
| `cmake/MultimodalConfig.cmake` | 模态开关声明（已有，不动） | — |
| `src/db/core/CMakeLists.txt` 或顶层 | 把 `multimodal_config_gen` 接上 include 路径 | T2 |
| `include/db/multimodal_config.h` | **删除**（过期副本） | T2 |
| `src/db/storage/CMakeLists.txt` | 修 ST/RDF 死分支、DOCUMENT/TIMESERIES 默认 OFF | T3 |
| `archive/db-sql-over-kv/` | 归档：sqlExec.c、table.c、sql_lexer.c、sql_parser.c(简化)、src/db/sql/sql_parser.c、parse_expr.c.bak、semantic_analyzer.c | T4 |
| `src/db/validator/sql_semantic.c` | 重写为 catalog API | T5 |
| `src/db/parser/sql/generated/{gram.c,gram.h,scan.c}` | 预生成解析器（WSL2 产出，提交入仓） | T6 |
| `src/db/parser/sql/sql_parse.c` + `include/db/parser/sql/sql_parse.h` | 统一解析入口 `sql_parse()` | T6 |
| `src/db/parser/sql/gram.y` | 小改：暴露 parsetree 访问器 + 错误缓冲 | T6 |
| `src/db/parser/sql/CMakeLists.txt` | canonical `db_parser_sql` 目标 | T6 |
| `src/db/sql/sql_driver.c` | 复活：Expr 冲突修复 + 结果收集 | T7, T9 |
| `src/db/executor/sql/sqlExecutor.c` | API 漂移修复 | T8 |
| `src/db/sql/node{Limit,Result,Agg,Hashjoin}.c` | 主路径算子最小实现 | T9 |
| `src/db/cli/cli.c`、`src/db/cli/CMakeLists.txt` | CLI 重接线 + 重新启用 | T10 |
| `src/db/executor/CMakeLists.txt` 等 | glob 拆分 + 模态守卫 | T11 |
| `test/db/sql/`、`scripts/check_modality_matrix.sh` | 测试迁移 + 模态矩阵验收 | T12 |

---

### Task 1: 基线确认 + 三个 spike

**Files:**
- Create: `docs/superpowers/plans/notes/2026-09-26-sql-consolidation-spike.md`

**Interfaces:**
- Produces: spike 笔记，包含 (a) `Expr` 冲突的完整 include 图；(b) `sqlExecutor.c` 漂移点清单；(c) WSL2 flex/bison 版本；(d) 基线构建/测试状态。T4/T6/T7/T8 依赖这些结论。

- [ ] **Step 1: 基线构建（Windows）**

```bash
cd /d/code/book/engineering && cmake -B build -S . -DBUILD_TESTING=ON 2>&1 | tail -5
cmake --build build --parallel 4 2>&1 | tail -20
```

Expected: configure 输出 `Multimodal Config:` 各开关状态；build 完成。记录警告/错误基线到 spike 笔记。

- [ ] **Step 2: 基线测试**

```bash
ctest --test-dir build --output-on-failure 2>&1 | tail -15
```

Expected: 记录通过/失败数作为基线（本计划不允许比基线更差）。

- [ ] **Step 3: Spike A — Expr 冲突 include 图**

```bash
cd /d/code/book/engineering
# 谁定义了 Expr（typedef struct Expr）
grep -rn "typedef struct Expr {" include/ | head
grep -rn "typedef struct Expr_s" include/ | head
# 谁 include 了 parse_node.h（简化 AST 头）
grep -rln "parser/sql/parse_node.h" src/ include/ 
# 谁 include 了 parsenodes.h（canonical AST 头）
grep -rln "parser/sql/parsenodes.h" src/ include/ | head -30
# 验证冲突：手动编译 sql_driver.c
gcc -c src/db/sql/sql_driver.c -Iinclude -Iinclude/db -o /tmp/sql_driver_test.o 2>&1 | head -30
```

记录：每个 Expr 定义的文件:行、两组 include 链、gcc 报的确切冲突错误。预期冲突为 `parse_node.h:123` 的 `typedef struct Expr {...} Expr` vs `expr.h`/`sql_planner.h` 的 `typedef struct Expr_s Expr`。

- [ ] **Step 4: Spike B — sqlExecutor.c 漂移点**

```bash
gcc -c src/db/executor/sql/sqlExecutor.c -Iinclude -Iinclude/db -o /tmp/sqlExecutor_test.o 2>&1 | head -40
```

记录全部报错（API 漂移点清单），T8 按此清单修复。

- [ ] **Step 5: Spike C — WSL2 工具链版本**

```bash
wsl -e bash -lc "flex --version && bison --version"
```

记录版本号（写入 generated 文件头注释和 spike 笔记）。若未安装：`wsl -e bash -lc "sudo apt-get install -y flex bison"`。

- [ ] **Step 6: 提交 spike 笔记**

```bash
git add docs/superpowers/plans/notes/2026-09-26-sql-consolidation-spike.md
git commit -m "docs: spike notes for SQL consolidation (Expr conflict map, drift list, toolchain)"
```

---

### Task 2: 接通生成配置头（WS-A 核心）

**Files:**
- Modify: `src/db/core/CMakeLists.txt`（或顶层定义 `project_includes` 处）
- Delete: `include/db/multimodal_config.h`
- Create: `test/db/core/test_multimodal_config.cpp`

**Interfaces:**
- Consumes: `multimodal_config_gen` INTERFACE target（`cmake/MultimodalConfig.cmake:48`）
- Produces: 任何 target `#include "db/multimodal_config.h"` 时获得 **build 树生成版**；宏值随 `-DMMDB_ENABLE_*` 变化

- [ ] **Step 1: 定位 `project_includes` 定义**

```bash
grep -rn "add_library(project_includes" /d/code/book/engineering/ --include=CMakeLists.txt
grep -rn "project_includes" /d/code/book/engineering/CMakeLists.txt | head -5
```

Expected: 找到其定义点（大概率是 INTERFACE 库）。

- [ ] **Step 2: 写失败测试——证明当前 include 的是过期副本**

Create `test/db/core/test_multimodal_config.cpp`:

```cpp
/* 验证 multimodal_config.h 来自 build 树生成版而非源码树过期副本。
 * 判定方式：生成版包含 CMake 注释标记 "// generated by CMake" 不可得，
 * 改用行为验证：配置 -DMMDB_ENABLE_KV=OFF 重新 configure 后，
 * MMDB_ENABLE_KV 宏应当消失。本测试在默认配置（全开）下运行，
 * 只验证头可用且 RELATIONAL/KV 已定义；OFF 行为由 T12 矩阵脚本验证。 */
#include <gtest/gtest.h>
#include "db/multimodal_config.h"

TEST(MultimodalConfig, HeaderIsAvailable) {
#ifdef MMDB_ENABLE_RELATIONAL
    SUCCEED();
#else
    FAIL() << "MMDB_ENABLE_RELATIONAL not defined - stale or missing config header";
#endif
}

TEST(MultimodalConfig, KvEnabledByDefault) {
#ifdef MMDB_ENABLE_KV
    SUCCEED();
#else
    FAIL() << "MMDB_ENABLE_KV not defined in default build";
#endif
}
```

- [ ] **Step 3: 接通生成头**

在 `project_includes` 定义处（若为 INTERFACE 库）追加：

```cmake
# 生成配置头全局可见（模态开关宏的唯一来源）
target_link_libraries(project_includes INTERFACE multimodal_config_gen)
```

若 `project_includes` 不是 INTERFACE 或不存在，改为对 `db_core` 添加（其余 target 已通过 db_core 传递）：

```cmake
target_link_libraries(db_core PUBLIC multimodal_config_gen)
```

- [ ] **Step 4: 删除过期副本并验证**

```bash
cd /d/code/book
git rm engineering/include/db/multimodal_config.h
cd engineering && cmake -B build -S . -DBUILD_TESTING=ON 2>&1 | grep -i "multimodal\|error" | head
cmake --build build --parallel 4 2>&1 | tail -10
```

Expected: configure 无 "multimodal_config.h not found" 类错误；build 通过。若某文件报找不到头 → 该文件的 target 未链接到 `project_includes`/`db_core`，给其 target 补 `target_link_libraries(<t> PRIVATE multimodal_config_gen)`。

- [ ] **Step 5: 验证 OFF 开关真正生效（核心验收）**

```bash
cd /d/code/book/engineering
cmake -B build-nokv -S . -DBUILD_TESTING=OFF -DMMDB_ENABLE_KV=OFF 2>&1 | grep "KV:"
```

Expected: 输出 `KV: OFF`。然后检查生成头内容：

```bash
grep "MMDB_ENABLE_KV" build-nokv/generated/include/db/multimodal_config.h
```

Expected: `/* #undef MMDB_ENABLE_KV */`（证明开关→生成头链路通了；完整 OFF 构建绿是 T11/T12 的目标，此处可能有 KV 依赖文件的编译错误，属预期，记录即可）。

- [ ] **Step 6: 跑新测试 + 提交**

```bash
# 把测试挂到 test/db/core/CMakeLists.txt（参照同目录现有测试的写法），然后：
cmake --build build --parallel 4 && ctest --test-dir build -R MultimodalConfig --output-on-failure
git add engineering/src/db/core/CMakeLists.txt engineering/test/db/core/ engineering/cmake
git commit -m "feat(db): wire generated multimodal_config.h as single source; remove stale copy"
```

---

### Task 3: 修 storage CMake 死分支与空操作（WS-A 收尾）

**Files:**
- Modify: `src/db/storage/CMakeLists.txt`
- Modify: `cmake/MultimodalConfig.cmake`

**Interfaces:**
- Produces: `MMDB_ENABLE_ST`/`MMDB_ENABLE_RDF` 成为已声明 option（默认 OFF）；`MMDB_ENABLE_DOCUMENT`/`MMDB_ENABLE_TIMESERIES` 默认 OFF 且打印"暂不支持"

- [ ] **Step 1: 声明缺失的 option**

`cmake/MultimodalConfig.cmake` 在 `MMDB_ENABLE_SPARSE` 行后追加：

```cmake
option(MMDB_ENABLE_ST "Enable Spatio-Temporal Model" OFF)  # 未就绪，默认关闭
option(MMDB_ENABLE_RDF "Enable RDF Knowledge Graph Model" OFF)  # 未就绪，默认关闭
```

同时在 `multimodal_config.h.in` 末尾 `#endif` 前追加：

```c
#cmakedefine MMDB_ENABLE_ST
#cmakedefine MMDB_ENABLE_RDF
```

- [ ] **Step 2: DOCUMENT/TIMESERIES 默认 OFF**

`cmake/MultimodalConfig.cmake` 中：

```cmake
option(MMDB_ENABLE_TIMESERIES "Enable Timeseries Model (not ready)" OFF)
option(MMDB_ENABLE_DOCUMENT "Enable Document Model (not ready)" OFF)
```

`src/db/storage/CMakeLists.txt:86-93` 的两个 `if` 块保持 `message(STATUS ...)` 不变（option OFF 后自然不再触发打印）。

- [ ] **Step 3: 验证**

```bash
cd /d/code/book/engineering
cmake -B build -S . -DBUILD_TESTING=ON 2>&1 | grep -E "ST|RDF|Document|Timeseries"
cmake -B build-st -S . -DMMDB_ENABLE_ST=ON 2>&1 | grep "ST engine"
```

Expected: 默认配置无 Document/Timeseries 的"not enabling"噪音；`-DMMDB_ENABLE_ST=ON` 时打印 `ST engine: enabled`（若 st/ 目录存在；不存在则无输出且 configure 不报错——`if(EXISTS)` 守卫已在）。

- [ ] **Step 4: 全量构建 + 提交**

```bash
cmake --build build --parallel 4 2>&1 | tail -5
git add engineering/cmake/MultimodalConfig.cmake engineering/cmake/multimodal_config.h.in engineering/src/db/storage/CMakeLists.txt
git commit -m "fix(db): declare ST/RDF options, default DOCUMENT/TIMESERIES OFF"
```

---

### Task 4: 归档 SQL-over-KV 路径（WS-B 核心）

**Files:**
- Move to `archive/db-sql-over-kv/`: `src/db/executor/sql/sqlExec.c`、`src/db/storage/rel/table.c`、`src/db/parser/sql/sql_lexer.c`、`src/db/parser/sql/sql_parser.c`、`src/db/sql/sql_parser.c`、`src/db/parser/sql/parse_expr.c.bak`
- Modify: `src/db/parser/sql/CMakeLists.txt`、`src/db/executor/CMakeLists.txt`、`src/db/storage/rel/CMakeLists.txt`（若显式列了 table.c）、`src/db/sql/CMakeLists.txt`

**Interfaces:**
- Consumes: T1 spike 笔记（确认归档清单无隐藏依赖）
- Produces: 构建中不再存在 `sql_exec(`/`sql_exec_ddl(`/`sql_parse_one(`（简化版）/`sql_exec_create(` 符号；`sqlExec`/`table` 相关 header（`include/db/executor/sql/sql_exec.h`、`include/db/table.h` 若无人用）一并归档

- [ ] **Step 1: 归档前全仓引用扫描（最后防线）**

```bash
cd /d/code/book/engineering
grep -rln "sql_exec\.h\|sql_exec_create\|sql_exec_ddl\|table_create(" src/ apps/ test/ tests/ --include="*.c" --include="*.cpp" --include="*.h" | grep -v archive
```

Expected: 仅 `src/db/cli/cli.c`（T10 重写）、`test/db/sql/sql_parser.cpp`+`test_planner.cpp`（T12 迁移）、`src/db/executor/sql/sqlExec.c` 自身。出现其他文件 → 记录并评估（不得贸然归档）。

- [ ] **Step 2: git mv 归档**

```bash
cd /d/code/book
mkdir -p archive/db-sql-over-kv
git mv engineering/src/db/executor/sql/sqlExec.c archive/db-sql-over-kv/
git mv engineering/src/db/storage/rel/table.c archive/db-sql-over-kv/
git mv engineering/src/db/parser/sql/sql_lexer.c archive/db-sql-over-kv/
git mv engineering/src/db/parser/sql/sql_parser.c archive/db-sql-over-kv/
git mv engineering/src/db/sql/sql_parser.c archive/db-sql-over-kv/
git mv engineering/src/db/parser/sql/parse_expr.c.bak archive/db-sql-over-kv/
# 头文件（若 Step 1 确认无引用）
git mv engineering/include/db/executor/sql/sql_exec.h archive/db-sql-over-kv/ 2>/dev/null || true
```

- [ ] **Step 3: 更新 `src/db/parser/sql/CMakeLists.txt`**

删除 `SQL_PARSER_SIMPLIFIED_SOURCES` 块和 `db_parser_sql_simplified` 目标（canonical 目标在 T6 建立；本 Task 先移除旧的）。同时删除 `src/db/executor/CMakeLists.txt:21` 和 `src/db/sql/CMakeLists.txt:48` 中对 `db_parser_sql_simplified` 的链接引用（暂留空，T6 补回 canonical 目标名）。

- [ ] **Step 4: 验证构建到达预期状态**

```bash
cd /d/code/book/engineering && cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build --parallel 4 2>&1 | tail -15
```

Expected: 可能出现链接错误（`sql_parse_one` 未定义，来自 CLI/旧测试）——逐一记录；CLI 目标当前整体注释掉故不应报错；报错的旧测试在本 Task 内用 `set_source_files_properties` 从其测试 target 移除或直接在该测试的 CMakeLists 注释（T12 重写）。**最终本 Task 要求 build 全绿**（通过暂时摘除旧测试达成）。

- [ ] **Step 5: 提交**

```bash
git add archive/ engineering/src/db/ engineering/include/db/
git commit -m "refactor(db): archive SQL-over-KV executor and simplified parser front-end"
```

---

### Task 5: validator 解耦——重写为 catalog API（WS-B 收尾）

**Files:**
- Move: `src/db/validator/semantic_analyzer.c` → `archive/db-sql-over-kv/`（与 `sql_semantic.c` 逐行重复，保留一份即可）
- Modify: `src/db/validator/sql_semantic.c`
- Modify: `include/db/executor/sql/sql_semantic.h`（签名去 KV 化）
- Test: `test/db/validator/test_sql_semantic.cpp`（无则新建，参照 `test/db/storage/catalog_test.cpp` 的 catalog 用法）

**Interfaces:**
- Consumes: `catalog_init(void) → int`、`catalog_lookup_table(const char *name) → Oid`（`include/db/catalog.h:152,191`）、`catalog_get_table(Oid) → table_info_t*`
- Produces: `sql_semantic_t *sql_semantic_create(void)`（不再收 `kv_t*`）；表存在性校验走 catalog

- [ ] **Step 1: 归档重复文件 + 写失败测试**

```bash
cd /d/code/book && git mv engineering/src/db/validator/semantic_analyzer.c archive/db-sql-over-kv/
```

新建测试（关键用例）：

```cpp
TEST(SqlSemantic, UnknownTableRejected) {
    ASSERT_EQ(catalog_init(), 0);
    sql_semantic_t *sem = sql_semantic_create();   // 新签名：无 kv_t
    ASSERT_NE(sem, nullptr);
    /* "no_such_table" 不在 catalog → 校验必须报错而非通过 */
    sql_node_t *ast = /* 构造或解析 SELECT * FROM no_such_table（T6 前可手工构造最小 AST） */;
    int rc = sql_semantic_check(sem, ast);
    EXPECT_NE(rc, 0) << "不存在的表必须被语义层拒绝";
    sql_semantic_destroy(sem);
}
```

- [ ] **Step 2: 重写 `sql_semantic.c` 的 KV 依赖点**

已知耦合点（`:19,:41,:76,:271,:305`）。改造模式——表存在性从 `kv_exists(meta_key)` 改为：

```c
#include "db/catalog.h"

/* 旧：snprintf(key, "meta:table:%s", name); kv_exists(ctx->db, key, ...) */
/* 新： */
static bool sem_table_exists(const char *name) {
    Oid oid = catalog_lookup_table(name);
    return oid != 0 && oid != InvalidOid;  /* 以 catalog.h 实际的无效 OID 约定为准 */
}
```

`sql_semantic_create(kv_t *db)` 改为 `sql_semantic_create(void)`，struct 中 `kv_t *db` 字段删除。列存在性校验同理走 `catalog_get_table(oid)` 的列列表。头文件签名同步更新。

- [ ] **Step 3: 修复所有调用点**

```bash
grep -rn "sql_semantic_create\|sql_semantic_check" src/ test/ --include="*.c" --include="*.cpp" | grep -v archive
```

逐处改为新签名。调用点若在已归档/已摘除代码中则跳过。

- [ ] **Step 4: 构建 + 测试 + 提交**

```bash
cd /d/code/book/engineering && cmake --build build --parallel 4 && ctest --test-dir build -R "Semantic|semantic" --output-on-failure
git add engineering/src/db/validator/ engineering/include/db/ engineering/test/db/validator/ archive/
git commit -m "refactor(db): decouple validator from kv_t, use catalog API; drop duplicate semantic_analyzer.c"
```

---

### Task 6: canonical 解析器——预生成 + 统一入口（WS-C）

**Files:**
- Modify: `src/db/parser/sql/gram.y`（暴露 parsetree/错误访问器）
- Create: `src/db/parser/sql/generated/gram.c`、`generated/gram.h`、`generated/scan.c`（WSL2 产出）
- Create: `include/db/parser/sql/sql_parse.h`、`src/db/parser/sql/sql_parse.c`
- Modify: `src/db/parser/sql/CMakeLists.txt`
- Test: `test/db/parser/test_sql_parse_canonical.cpp`

**Interfaces:**
- Produces（后续所有 Task 依赖）:

```c
/* include/db/parser/sql/sql_parse.h */
#ifndef DB_PARSER_SQL_PARSE_H
#define DB_PARSER_SQL_PARSE_H
#include "db/parser/sql/parsenodes.h"
#ifdef __cplusplus
extern "C" {
#endif
Node *sql_parse(const char *sql);            /* 失败返回 NULL */
const char *sql_parse_last_error(void);      /* 带 line:column 的错误串 */
#ifdef __cplusplus
}
#endif
#endif
```

- CMake target `db_parser_sql`（STATIC），链接者获得 `sql_parse()` 与全部 AST 节点构造 helper（makefuncs 等）

- [ ] **Step 1: 修改 gram.y（三处小改）**

`%{ %}` 块内，`static Node *parsetree;` 之后追加：

```c
/* 解析结果与错误的对外访问器（供 sql_parse.c 使用） */
static char yy_error_buf[512];
Node *sql_yy_get_parsetree(void) { return parsetree; }
const char *sql_yy_get_error(void) { return yy_error_buf; }
```

`yyerror` 改为写缓冲而非 stderr：

```c
void yyerror(const char *msg) {
    snprintf(yy_error_buf, sizeof(yy_error_buf),
             "%s (line %d, column %d)", msg, sql_yylloc.line, sql_yylloc.column);
}
```

确认语法规则的动作代码把结果赋给 `parsetree`（检查 `input`/`statement` 顶层规则；若当前没有赋值，在顶层规则加 `parsetree = $1;`）。

- [ ] **Step 2: WSL2 预生成**

```bash
wsl -e bash -lc "cd /mnt/d/code/book/engineering/src/db/parser/sql && \
  bison --defines=generated/gram.h --output=generated/gram.c gram.y && \
  flex -o generated/scan.c scan.l && \
  echo GENERATED-OK"
```

（先 `mkdir -p generated`。）在三个生成文件头部追加注释（用 sed 或生成后立即编辑）：

```c
/* PRE-GENERATED by WSL2: bison <version> / flex <version> (见 spike 笔记)
 * 重新生成: cmake -DREGENERATE_PARSER=ON 或手动执行 T6 Step 2 命令 */
```

- [ ] **Step 3: 验证生成文件含所需符号**

```bash
grep -c "sql_yyparse\|sql_yy_get_parsetree" /d/code/book/engineering/src/db/parser/sql/generated/gram.c
grep -c "sql_yy_scan_string" /d/code/book/engineering/src/db/parser/sql/generated/scan.c
```

Expected: 均 ≥1。若 `sql_yy_scan_string` 缺失 → scan.l 缺 buffer 支持，在 scan.l 的 `%option` 区无冲突情况下 flex 默认生成之；确认 grep 的是生成文件而非源文件。

- [ ] **Step 4: 写 `sql_parse.c`**

```c
#include "db/parser/sql/sql_parse.h"
#include <stddef.h>

/* flex/bison 生成符号（prefix=sql_yy） */
typedef void *YY_BUFFER_STATE;
extern YY_BUFFER_STATE sql_yy_scan_string(const char *str);
extern void sql_yy_delete_buffer(YY_BUFFER_STATE b);
extern int sql_yyparse(void);
extern Node *sql_yy_get_parsetree(void);
extern const char *sql_yy_get_error(void);

Node *sql_parse(const char *sql) {
    if (sql == NULL) return NULL;
    YY_BUFFER_STATE buf = sql_yy_scan_string(sql);
    if (buf == NULL) return NULL;
    int rc = sql_yyparse();
    sql_yy_delete_buffer(buf);
    if (rc != 0) return NULL;
    return sql_yy_get_parsetree();
}

const char *sql_parse_last_error(void) {
    return sql_yy_get_error();
}
```

- [ ] **Step 5: 重写 `src/db/parser/sql/CMakeLists.txt` 为 canonical 目标**

```cmake
# parser/sql 子模块：canonical SQL 解析器（Bison 语法 + 预生成文件）
# 预生成文件默认使用；维护时 -DREGENERATE_PARSER=ON 且安装 flex/bison 后重新生成

option(REGENERATE_PARSER "Regenerate gram.c/scan.c from gram.y/scan.l (requires flex/bison)" OFF)

if(REGENERATE_PARSER)
    find_program(FLEX_EXECUTABLE flex REQUIRED)
    find_program(BISON_EXECUTABLE bison REQUIRED)
    set(GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(MAKE_DIRECTORY ${GEN_DIR})
    add_custom_command(
        OUTPUT ${GEN_DIR}/gram.c ${GEN_DIR}/gram.h
        COMMAND ${BISON_EXECUTABLE} --defines=${GEN_DIR}/gram.h --output=${GEN_DIR}/gram.c
                ${CMAKE_CURRENT_SOURCE_DIR}/gram.y
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gram.y VERBATIM)
    add_custom_command(
        OUTPUT ${GEN_DIR}/scan.c
        COMMAND ${FLEX_EXECUTABLE} -o ${GEN_DIR}/scan.c ${CMAKE_CURRENT_SOURCE_DIR}/scan.l
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/scan.l VERBATIM)
else()
    set(GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
endif()

add_library(db_parser_sql STATIC
    ${GEN_DIR}/gram.c
    ${GEN_DIR}/scan.c
    sql_parse.c
    makefuncs.c
    parse_node.c
    parse_analyze.c
    parse_expr.c
)
target_include_directories(db_parser_sql PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${GEN_DIR}          # gram.h（token 定义）对 scan.c/调用者可见
)
target_link_libraries(db_parser_sql PRIVATE project_includes)
```

- [ ] **Step 6: 更新链接引用 + 失败测试先行**

`src/db/sql/CMakeLists.txt:48` 和 `src/db/executor/CMakeLists.txt:21` 的 `db_parser_sql_simplified` 改为 `db_parser_sql`。

写 `test/db/parser/test_sql_parse_canonical.cpp`：

```cpp
#include <gtest/gtest.h>
#include "db/parser/sql/sql_parse.h"

TEST(SqlParseCanonical, SelectStar) {
    Node *ast = sql_parse("SELECT * FROM users");
    ASSERT_NE(ast, nullptr);
    EXPECT_EQ(ast->type, T_SelectStmt);   /* parsenodes.h 的 NodeTag */
}

TEST(SqlParseCanonical, JoinParses) {
    Node *ast = sql_parse("SELECT a.id FROM t1 a JOIN t2 b ON a.id = b.id");
    ASSERT_NE(ast, nullptr) << sql_parse_last_error();
}

TEST(SqlParseCanonical, GroupByHavingOrderLimit) {
    Node *ast = sql_parse(
        "SELECT dept, COUNT(*) FROM emp GROUP BY dept HAVING COUNT(*) > 1 "
        "ORDER BY dept DESC LIMIT 10 OFFSET 5");
    ASSERT_NE(ast, nullptr) << sql_parse_last_error();
}

TEST(SqlParseCanonical, SyntaxErrorHasPosition) {
    Node *ast = sql_parse("SELEKT * FORM t1");
    EXPECT_EQ(ast, nullptr);
    const char *err = sql_parse_last_error();
    ASSERT_STRNE(err, "");
    EXPECT_NE(strstr(err, "line"), nullptr) << "错误必须带位置: " << err;
}
```

- [ ] **Step 7: 构建 + 测试 + 提交**

```bash
cd /d/code/book/engineering && cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build --parallel 4 2>&1 | tail -10
ctest --test-dir build -R SqlParseCanonical --output-on-failure
```

Expected: 4/4 通过。若 `T_SelectStmt` 等枚举名不匹配 → 以 `parsenodes.h` 实际枚举为准修正测试。提交：

```bash
git add engineering/src/db/parser/sql/ engineering/include/db/parser/sql/sql_parse.h engineering/test/db/parser/
git commit -m "feat(db): canonical Bison parser with pre-generated sources and unified sql_parse() entry"
```

---

### Task 7: 复活 sql_driver.c——Expr 冲突修复 + 端到端烟测（WS-D 上半）

**Files:**
- Modify: `src/db/sql/sql_driver.c`、`src/db/sql/CMakeLists.txt`（取消排除）
- 按 spike 结论 Modify: `include/db/parser/sql/parse_node.h` 或 `include/db/sql/expr.h`（二选一，见 Step 2）
- Test: `test/db/sql/test_driver_smoke.cpp`

**Interfaces:**
- Consumes: `sql_parse()`（T6）、planner API（`planner_create/planner_logical_plan/planner_optimize/planner_physical_plan/planner_create_plan_state`）、executor API（`CreateQueryDesc/ExecutorStart/ExecutorRun/ExecutorFinish/ExecutorEnd`）
- Produces: `QueryResult *execute_sql(const char *sql, void *db)` 可编译、可链接、DDL 烟测通过

- [ ] **Step 1: 先把 sql_driver.c 加回构建，复现冲突**

`src/db/sql/CMakeLists.txt` 去掉 `#   sql_driver.c` 注释使其进入 `sql_engine` 源列表，构建并记录错误：

```bash
cd /d/code/book/engineering && cmake -B build -S . && cmake --build build 2>&1 | grep -A3 "error" | head -40
```

- [ ] **Step 2: 按 T1 spike 的 include 图消除冲突**

T4 归档后，简化 AST（`sql.h`/`parse_node.h` 的 `typedef struct Expr {...} Expr`）应当只剩 helper 四文件可能引用。按 spike 结论执行其一：

- **若 `parse_node.h` 仍被 helper 引用且与 `expr.h` 冲突**：把 `parse_node.h` 的 `Expr` 改名为 `ParseExpr`（struct tag 与 typedef 同步改），波及文件逐一编译修正；
- **若 helper 实际用 `parsenodes.h`**（更可能，因 gram.y 走 parsenodes）：直接删除 `parse_node.h` 中对 `Expr` 的 typedef（或整头归档），`sql_driver.c` 的 `#include "db/parser/sql/sql.h"` 改为 `#include "db/parser/sql/sql_parse.h"`。

同时把 `sql_driver.c:173` 的 `sql_parse_one(sql)` 改为 `sql_parse(sql)`，`:183` 的 `sql_node_free(parse_result)` 按 parsenodes 体系的释放函数替换（spike 确认名字；parsenodes 体系通常由 memctx/查询上下文统一释放，则直接删除该调用）。

- [ ] **Step 3: 端到端烟测（失败先行）**

`test/db/sql/test_driver_smoke.cpp`（bootstrap 序列复制自 `test/db/sql/test_sql_storage_integration.cpp:38-42`）：

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/sql/sql_driver.h"
#include "db/catalog.h"
#include "db/buf.h"
#include "db/heapam.h"
#include "db/rel.h"
}

class DriverSmoke : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(catalog_init(), 0);
        ASSERT_EQ(buf_init(1024), 0);
        ASSERT_EQ(heapam_init(), 0);
        ASSERT_EQ(btreeam_init(), 0);
        ASSERT_EQ(rel_init(), 0);
    }
    static void TearDownTestSuite() { heapam_shutdown(); buf_shutdown(); }
};

TEST_F(DriverSmoke, CreateTableDdl) {
    QueryResult *r = execute_sql("CREATE TABLE smoke_t (id INT, name TEXT)", nullptr);
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->error_msg, nullptr) << (r->error_msg ? r->error_msg : "");
    EXPECT_NE(catalog_lookup_table("smoke_t"), 0) << "DDL 后 catalog 必须能查到表";
    FreeQueryResult(r);
}
```

（`btreeam_init` 的头文件以 `test_sql_storage_integration.cpp` 的 include 为准。）

- [ ] **Step 4: 迭代修复直至烟测通过**

DDL 路径涉及 planner/executor 对 `CreateStmt` 的支持。若 planner 尚不接受 DDL 节点：`execute_sql()` 中对 `T_CreateStmt`/`T_DropStmt` 走**直通道**（不经 planner，直接语义校验 + `catalog_create_table` + 存储对象创建），SELECT/DML 走 planner 通道。这是本计划的既定设计（DDL 直通、DQL/DML 走全链路），不是偷工减料。

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/sql/ engineering/include/db/ engineering/test/db/sql/test_driver_smoke.cpp
git commit -m "feat(db): resurrect sql_driver.c on canonical AST; DDL smoke test green"
```

---

### Task 8: sqlExecutor.c API 漂移修复（WS-D 中段）

**Files:**
- Modify: `src/db/executor/sql/sqlExecutor.c`
- Modify: `include/db/executor/sql/sql_executor.h`（若声明漂移）
- Test: 复用 T7 的 `test_driver_smoke.cpp` + 新增 DML 用例

**Interfaces:**
- Consumes: T1 spike B 的漂移点清单；catalog/heapam/WAL 当前签名
- Produces: `sqlExecutor.c` 零警告编译；`execute_sql("INSERT ...")` / `execute_sql("SELECT ...")` 行为正确

- [ ] **Step 1: 按 spike 清单逐一修复编译错误**

```bash
cd /d/code/book/engineering
gcc -c src/db/executor/sql/sqlExecutor.c -Iinclude -Iinclude/db -o /tmp/e.o 2>&1 | head -30
```

每处错误对照当前头文件签名修正（典型漂移：函数更名、参数增删、结构字段改名）。**禁止**为消错误而 `#if 0` 功能代码——功能缺失改为硬报错 `fprintf(stderr, "not implemented: ...")` + 返回错误。

- [ ] **Step 2: 写 DML 失败测试**

在 `test_driver_smoke.cpp` 追加：

```cpp
TEST_F(DriverSmoke, InsertThenSelect) {
    execute_sql("CREATE TABLE dml_t (id INT, name TEXT)", nullptr);
    QueryResult *ri = execute_sql("INSERT INTO dml_t VALUES (1, 'alice')", nullptr);
    ASSERT_NE(ri, nullptr);
    EXPECT_EQ(ri->error_msg, nullptr);
    EXPECT_EQ(ri->nrows, 1);
    FreeQueryResult(ri);

    QueryResult *rs = execute_sql("SELECT id, name FROM dml_t", nullptr);
    ASSERT_NE(rs, nullptr);
    EXPECT_EQ(rs->error_msg, nullptr);
    EXPECT_EQ(rs->nrows, 1);
    FreeQueryResult(rs);
}
```

- [ ] **Step 3: 结果收集落地**

当前 `execute_sql()` 只从 `es_processed` 取行数（`sql_driver.c:238-242`），`SELECT` 的行数据未收集。在 `ExecutorRun` 之后增加收集循环（API 名以 `include/db/sql/executor.h` 实际为准，先读该头文件再写码）：

```c
/* ExecutorRun 之后：逐 tuple 拉取并物化到 QueryResult */
TupleTableSlot *slot;
while ((slot = ExecProcNode(query_desc->planstate)) != NULL) {
    /* 1. 首 tuple 时按 slot 描述符分配 col_names
     * 2. 每列取值→字符串（按类型 OID 格式化），strdup 进 rows[i][j]
     * 3. result->nrows++ */
}
```

列取值/格式化的具体 accessor 以 `executor.h`/`tuptable` 相关头为准；找不到现成格式化函数时，本阶段只支持 INT/TEXT/VARCHAR 三种类型的 `snprintf` 格式化，其余类型输出 `<type %u>` 占位字符串（**显式可见**，不是静默 NULL）。

- [ ] **Step 4: 构建 + 测试 + 提交**

```bash
cmake --build build --parallel 4 && ctest --test-dir build -R DriverSmoke --output-on-failure
git add engineering/src/db/executor/sql/ engineering/include/db/ engineering/src/db/sql/ engineering/test/db/sql/
git commit -m "fix(db): repair sqlExecutor.c API drift; materialize SELECT results into QueryResult"
```

---

### Task 9: 主路径算子最小实现（WS-D 深水区）

**Files:**
- Modify: `src/db/sql/nodeLimit.c`（`:47` TODO）、`nodeAgg.c`（`:47` TODO）、`nodeResult.c`、`nodeHashjoin.c`（`:401,408` 表达式编译）、`src/db/sql/executor.c`（`:875` Result 节点 NULL）
- Test: `test/db/sql/test_e2e_capability.cpp`

**Interfaces:**
- Consumes: T8 的 `execute_sql()` 全链路
- Produces: spec §6.2-3 的能力兑现——JOIN/GROUP BY+聚合/ORDER BY/LIMIT/子查询 e2e 通过

- [ ] **Step 1: 先写全部能力测试（失败先行，一次写齐）**

`test/db/sql/test_e2e_capability.cpp`（bootstrap 同 DriverSmoke）：

```cpp
class E2ECapability : public ::testing::Test { /* 同 DriverSmoke 的 SetUp/TearDown */ };

static QueryResult *run(const char *sql) {
    QueryResult *r = execute_sql(sql, nullptr);
    EXPECT_NE(r, nullptr);
    EXPECT_EQ(r->error_msg, nullptr) << (r && r->error_msg ? r->error_msg : "");
    return r;
}

TEST_F(E2ECapability, OrderByLimit) {
    run("CREATE TABLE t (id INT, v INT)");
    run("INSERT INTO t VALUES (1, 30)"); run("INSERT INTO t VALUES (2, 10)"); run("INSERT INTO t VALUES (3, 20)");
    QueryResult *r = run("SELECT id FROM t ORDER BY v DESC LIMIT 2");
    ASSERT_EQ(r->nrows, 2);          /* v=30,20 → id=1,3 */
    FreeQueryResult(r);
}

TEST_F(E2ECapability, LimitOffset) {
    /* 同上数据 */ QueryResult *r = run("SELECT id FROM t ORDER BY v LIMIT 1 OFFSET 1");
    ASSERT_EQ(r->nrows, 1);          /* v 升序第 2 个 → id=3 */
    FreeQueryResult(r);
}

TEST_F(E2ECapability, GroupByAggregate) {
    run("CREATE TABLE emp (dept INT, salary INT)");
    run("INSERT INTO emp VALUES (1, 100)"); run("INSERT INTO emp VALUES (1, 200)"); run("INSERT INTO emp VALUES (2, 50)");
    QueryResult *r = run("SELECT dept, COUNT(*), SUM(salary) FROM emp GROUP BY dept ORDER BY dept");
    ASSERT_EQ(r->nrows, 2);
    FreeQueryResult(r);
}

TEST_F(E2ECapability, InnerJoin) {
    run("CREATE TABLE a (id INT, x INT)"); run("CREATE TABLE b (id INT, y INT)");
    run("INSERT INTO a VALUES (1, 10)"); run("INSERT INTO b VALUES (1, 20)"); run("INSERT INTO b VALUES (2, 30)");
    QueryResult *r = run("SELECT a.id, b.y FROM a JOIN b ON a.id = b.id");
    ASSERT_EQ(r->nrows, 1);
    FreeQueryResult(r);
}

TEST_F(E2ECapability, SubqueryInFrom) {
    QueryResult *r = run("SELECT s.c FROM (SELECT COUNT(*) AS c FROM emp) s");
    ASSERT_EQ(r->nrows, 1);
    FreeQueryResult(r);
}
```

- [ ] **Step 2: 实现 nodeLimit（范例，其余算子同模式）**

`nodeLimit.c:43-54` 的 `exec_limit_impl` 替换为：

```c
static TupleTableSlot *exec_limit_impl(PlanState *pstate) {
    LimitState *node = (LimitState *)pstate;

    if (node->ps.lefttree == NULL) return NULL;

    /* 1. 跳过 OFFSET 个元组 */
    while (node->position < node->limitOffset) {
        TupleTableSlot *skip = ExecProcNode(node->ps.lefttree);
        if (skip == NULL) { node->position = -1; return NULL; }  /* 子节点耗尽 */
        node->position++;
    }

    /* 2. 到达 LIMIT 上限则终止（noCount 表示无 LIMIT 只 OFFSET） */
    if (!node->noCount &&
        node->position >= node->limitOffset + node->limitCount) {
        return NULL;
    }

    /* 3. 拉取并返回下一个元组 */
    TupleTableSlot *slot = ExecProcNode(node->ps.lefttree);
    if (slot == NULL) return NULL;
    node->position++;
    return slot;
}
```

（字段名以 `include/db/sql/nodeLimit.h` 的 `LimitState` 实际定义为准；`limitOffset/limitCount/position/noCount` 在 `ExecInitLimit` 中已出现，类型若为无符号需调整比较写法。）

- [ ] **Step 3: 逐算子实现最小逻辑**

按测试失败顺序逐个实现（每完成一个跑对应 TEST_F）：

| 算子 | TODO 位置 | 最小实现要点 |
|---|---|---|
| Result | `executor.c:875` | 无输入一次性返回投影行（用于 `SELECT 1` 类） |
| Agg（无 GROUP BY） | `nodeAgg.c:47` | 扫空子节点，累计 COUNT/SUM/AVG/MIN/MAX，返回单行 |
| HashAgg/Group | `nodeHashagg.c` | 按 group key 哈希分桶，每桶累计聚合，逐桶输出 |
| HashJoin 表达式编译 | `nodeHashjoin.c:401,408` | 用 `expr.c`/`expr_interp.c` 现有表达式求值编译 join qual；先支持等值 JOIN |
| Sort | `nodeSort.c`（若无 TODO 则验证即可） | 物化子节点全部元组后 qsort |

每个算子的实现模式 = nodeLimit 同款：`ExecInit<Node>`（已有框架）+ `exec_<n>_impl`（填 TODO）。**子节点返回 NULL 即耗尽；本节点语义完成后也必须返回 NULL 而非空 slot。**

- [ ] **Step 4: 构建 + 全部能力测试 + 提交**

```bash
cmake --build build --parallel 4 && ctest --test-dir build -R E2ECapability --output-on-failure
```

Expected: 5/5 通过。若某能力需要 planner 侧支持（如子查询未进物理计划），在 planner 对应分支补最小分派，**不得**在 executor 里特判绕过。

```bash
git add engineering/src/db/sql/ engineering/test/db/sql/
git commit -m "feat(db): minimal main-path operators (Limit/Result/Agg/HashAgg/HashJoin) with e2e capability tests"
```

---

### Task 10: CLI 重接线与重新启用（WS-D 出口）

**Files:**
- Modify: `src/db/cli/cli.c`（重写执行路径与启动序列）
- Modify: `src/db/cli/CMakeLists.txt`（启用，去掉 vector_api 依赖）
- Test: `test/db/cli/test_cli_smoke.cpp`（重新启用原被注释的 CLI 测试目录）

**Interfaces:**
- Consumes: `execute_sql()`（T7-9）、bootstrap 序列、`catalog_lookup_table`（实现 `.tables`/`.schema` 可后置）
- Produces: `db_cli` 可执行程序；REPL 走 canonical 路径；KV 子命令在 `MMDB_ENABLE_KV` 下可用

- [ ] **Step 1: 重写 cli.c 的结构与 includes**

```c
#include <db/cli/cli.h>
#include <db/multimodal_config.h>
#ifdef MMDB_ENABLE_RELATIONAL
#include <db/sql/sql_driver.h>
#include <db/catalog.h>
#include <db/buf.h>
#include <db/heapam.h>
#include <db/rel.h>
#endif
#ifdef MMDB_ENABLE_KV
#include <db/kv.h>
#endif
```

`struct db_cli_s` 改为：

```c
struct db_cli_s {
    db_cli_config_t config;
#ifdef MMDB_ENABLE_KV
    kv_t *kv;                     /* 仅在用到 KV 子命令时懒打开 */
#endif
    line_buffer_t multi_line;
    bool in_multiline;
};
```

- [ ] **Step 2: 启动序列与 execute_sql 重写**

`db_cli_create` 中替换 `kv_open + sql_exec_create`：

```c
#ifdef MMDB_ENABLE_RELATIONAL
    if (catalog_init() != 0 || buf_init(1024) != 0 || heapam_init() != 0 ||
        btreeam_init() != 0 || rel_init() != 0) {
        fprintf(stderr, "存储引擎初始化失败\n");
        free(cli);
        return NULL;
    }
#endif
```

旧 `execute_sql()` 静态函数整体替换为：

```c
static int execute_sql(db_cli_t *cli, const char *sql)
{
#ifdef MMDB_ENABLE_RELATIONAL
    clock_t start = clock();
    QueryResult *r = execute_sql_canonical(sql, NULL);  /* 见下方说明 */
    ...
#else
    (void)cli;
    printf("relational modality not enabled in this build\n");
    return 1;
#endif
}
```

注意命名冲突：静态 `execute_sql` 与 `sql_driver.h` 的 `execute_sql` 同名——把 CLI 静态函数改名 `cli_run_sql`，内部调 `execute_sql(sql, NULL)`。打印逻辑复用现有 `print_header/print_row/print_json_result`，但其参数从 `sql_result_t*` 改为 `QueryResult*`（字段对应：`nrows/ncols/col_names/rows`，字符串二维数组结构一致，改动是机械式的）。

- [ ] **Step 3: KV 子命令（模态隔离的出口）**

`handle_command` 中 `.tables` 分支后追加：

```c
#ifdef MMDB_ENABLE_KV
    if (strncmp(input, ".kvput ", 7) == 0) { /* 解析 k v，懒 kv_open，kv_put */ return 0; }
    if (strncmp(input, ".kvget ", 7) == 0) { /* kv_get 并打印 */ return 0; }
    if (strncmp(input, ".kvdel ", 7) == 0) { /* kv_delete */ return 0; }
    if (strcmp(input, ".kvstats") == 0)     { /* kv_stats */ return 0; }
#endif
```

（实现各 20 行内；`cli->kv == NULL` 时先 `kv_open(cli->config.db_path)`。）

- [ ] **Step 4: 重写 `src/db/cli/CMakeLists.txt`**

```cmake
# CLI 交互界面模块（canonical SQL 路径；vector_cli 待 vector_api 恢复后另行接入）
add_library(db_cli_lib STATIC cli.c)
target_include_directories(db_cli_lib PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/include/db
)
target_link_libraries(db_cli_lib PUBLIC
    sql_engine
    db_storage
    db_parser_sql
    db_core
    project_includes
    multimodal_config_gen
)

add_executable(db_cli cli_main.c)
target_link_libraries(db_cli PRIVATE db_cli_lib)
```

- [ ] **Step 5: CLI 烟测（脚本模式，非交互）**

`test/db/cli/test_cli_smoke.cpp`：

```cpp
#include <gtest/gtest.h>
#include "db/cli/cli.h"

TEST(CliSmoke, ExecCreateInsertSelect) {
    db_cli_config_t cfg = {};
    cfg.db_path = "./cli_smoke.db";
    cfg.prompt = "";
    cfg.echo = false; cfg.json_output = false; cfg.show_timing = false;
    db_cli_t *cli = db_cli_create(&cfg);
    ASSERT_NE(cli, nullptr);
    EXPECT_EQ(db_cli_exec(cli, "CREATE TABLE c (id INT)"), 0);
    EXPECT_EQ(db_cli_exec(cli, "INSERT INTO c VALUES (1)"), 0);
    EXPECT_EQ(db_cli_exec(cli, "SELECT id FROM c"), 0);
    EXPECT_NE(db_cli_exec(cli, "SELEKT nonsense"), 0) << "语法错误必须非零返回";
    db_cli_destroy(cli);
}
```

（`db_cli_exec` 已存在，cli.c:419。）

- [ ] **Step 6: 构建 + 测试 + 提交**

```bash
cmake --build build --parallel 4 && ctest --test-dir build -R CliSmoke --output-on-failure
# 手工冒烟：echo "SELECT 1;" | ./build/bin/db_cli 或对应输出路径
git add engineering/src/db/cli/ engineering/test/db/cli/
git commit -m "feat(db): rewire CLI to canonical execute_sql; KV dot-commands under MMDB_ENABLE_KV"
```

---

### Task 11: CMake 排除项清理与模态守卫（WS-E）

**Files:**
- Modify: `src/db/executor/CMakeLists.txt`、`src/db/sql/CMakeLists.txt`、`src/db/storage/CMakeLists.txt`、`src/db/cli/CMakeLists.txt`

**Interfaces:**
- Produces: `db_executor` 不再无条件 glob 两个执行器；RELATIONAL 相关目录受 `MMDB_ENABLE_RELATIONAL` 守卫；KV 相关受 `MMDB_ENABLE_KV` 守卫

- [ ] **Step 1: db_executor glob 拆分**

`src/db/executor/CMakeLists.txt:5` 的 `GLOB_RECURSE "*.c"` 后追加排除与守卫：

```cmake
# sql/ 子目录仅含 canonical sqlExecutor.c（sqlExec.c 已归档）；
# RELATIONAL 关闭时整个 sql/ 子目录不编译
if(NOT MMDB_ENABLE_RELATIONAL)
    list(FILTER EXECUTOR_SOURCE_FILES EXCLUDE REGEX ".*/sql/.*\\.c$")
endif()
```

`db_executor` 的链接行去掉 `db_parser_sql`（RELATIONAL 关闭时该目标不存在，需 `if(TARGET db_parser_sql)` 守卫）：

```cmake
if(TARGET db_parser_sql)
    target_link_libraries(db_executor PRIVATE db_parser_sql)
endif()
```

- [ ] **Step 2: RELATIONAL 守卫**

`src/db/CMakeLists.txt` 中 `add_subdirectory(sql)`、`add_subdirectory(parser)`（SQL 部分）、`add_subdirectory(validator)` 外包 `if(MMDB_ENABLE_RELATIONAL)`（注意确认 parser 下 graph/ 解析是否应留在守卫外——GQL 属于 GRAPH 模态，用独立守卫或保留现状并在注释说明）。

- [ ] **Step 3: KV 守卫验证**

`src/db/cf/`、`src/db/sharding/`、`src/db/storage/graph/` 等 KV 依赖方在 `MMDB_ENABLE_KV=OFF` 时必须不编译或降级。逐一检查其 CMakeLists，对硬依赖 KV 的目录在 `src/db/CMakeLists.txt` 对应 `add_subdirectory` 外包 `if(MMDB_ENABLE_KV)`。multimodal_rag 应用依赖 KV——**只读确认**其 CMake 有 `if(MMDB_ENABLE_KV)` 类守卫，没有则在报告中记录（不修改其 EXCLUDE 规则，仅可加外层开关守卫）。

- [ ] **Step 4: OFF 组合构建验证**

```bash
cd /d/code/book/engineering
cmake -B build-nokv -S . -DMMDB_ENABLE_KV=OFF -DBUILD_TESTING=ON && cmake --build build-nokv --parallel 4 2>&1 | tail -10
cmake -B build-norel -S . -DMMDB_ENABLE_RELATIONAL=OFF -DBUILD_TESTING=ON && cmake --build build-norel --parallel 4 2>&1 | tail -10
```

Expected: 两个配置都 build 通过（这是本 Task 的硬验收；修不掉的残余耦合按"硬报错 + 守卫"原则处理并记录）。

- [ ] **Step 5: 提交**

```bash
git add engineering/src/db/ engineering/apps/CMakeLists.txt 2>/dev/null; git add engineering/apps/multimodal_rag/CMakeLists.txt 2>/dev/null || true
git commit -m "feat(db): modality guards for RELATIONAL/KV; clean CMake exclusion leftovers"
```

---

### Task 12: 测试迁移、模态矩阵与最终验收（WS-F）

**Files:**
- Modify/Move: `test/db/sql/test_planner.cpp`、`test/db/sql/sql_parser.cpp`（迁移到 `sql_parse()`/parsenodes AST；无法迁移的用例随归档移出到 `archive/db-sql-over-kv/tests/`）
- Modify: `test/db/sql/CMakeLists.txt`（重新启用 sql_integration 走 db_cli_lib）
- Create: `scripts/check_modality_matrix.sh`
- Modify: `docs/superpowers/specs/2026-09-26-sql-consolidation-design.md`（追加"实施完成"状态行）

**Interfaces:**
- Consumes: 全部前序 Task
- Produces: spec §6.2 五条 DoD 的可验证证据

- [ ] **Step 1: 迁移旧 AST 测试**

`test_planner.cpp` 中 `sql_parse_one("SELECT ...")` → `sql_parse(...)`；断言里的 `sql_node_t`/`SQL_NODE_*` → `Node*`/`T_*`。无法语义对应的用例（依赖简化 AST 特有结构）`git mv` 到 `archive/db-sql-over-kv/tests/` 并在文件头注明原因。

- [ ] **Step 2: 重新启用 sql_integration**

`test/db/sql/CMakeLists.txt:85-87` 的注释块解开，`db_cli_lib` 已在 T10 建立。跑通 51 个 CLI 端到端用例；失败用例分两类处理：能力缺口 → 记入 `docs/known-limitations.md`（新建）并将该用例标记 `GTEST_SKIP()` 附原因；回归 → 修复。

- [ ] **Step 3: 模态矩阵脚本**

`scripts/check_modality_matrix.sh`：

```bash
#!/usr/bin/env bash
# 模态组合构建矩阵验收（spec §6.2-2）
set -e
cd "$(dirname "$0")/../engineering"
for rel in ON OFF; do
  for kv in ON OFF; do
    [ "$rel" = OFF ] && [ "$kv" = OFF ] && continue   # 全关无意义
    dir="build-matrix-r${rel}-k${kv}"
    echo "=== RELATIONAL=$rel KV=$kv ==="
    cmake -B "$dir" -S . -DBUILD_TESTING=ON \
          -DMMDB_ENABLE_RELATIONAL=$rel -DMMDB_ENABLE_KV=$kv > /dev/null
    cmake --build "$dir" --parallel 4 > /dev/null
    if [ "$rel" = ON ]; then
      ctest --test-dir "$dir" -R "DriverSmoke|E2ECapability|SqlParseCanonical|CliSmoke" \
            --output-on-failure | tail -3
    fi
  done
done
echo "MATRIX-OK"
```

- [ ] **Step 4: DoD 五条逐条验证并记录**

```bash
cd /d/code/book/engineering
# DoD-1 唯一路径：旧符号在构建中不存在
grep -rn "sql_exec_create\|sql_exec_ddl" src/ --include="*.c" | grep -v archive   # 期望空
# DoD-2 隔离生效：Step 3 矩阵 + 源码树无配置头副本
ls include/db/multimodal_config.h 2>&1                                            # 期望不存在
# DoD-4 无静默桩：主路径 grep
grep -rn "TODO.*实现\|框架版本返回 NULL" src/db/sql/node*.c src/db/sql/executor.c # 期望空或仅剩硬报错
# DoD-5 双平台
ctest --test-dir build --output-on-failure | tail -5
wsl -e bash -lc "cd /mnt/d/code/book/engineering && cmake -B build-linux -S . -DBUILD_TESTING=ON && cmake --build build-linux --parallel 4 && ctest --test-dir build-linux | tail -5"
```

- [ ] **Step 5: 更新 spec 状态 + 最终提交**

spec 文件头 `状态：已批准` 改为 `状态：已实施（YYYY-MM-DD 验收通过）`。

```bash
git add engineering/test/ engineering/scripts/ engineering/docs/ docs/
git commit -m "test(db): migrate AST tests, enable sql_integration, modality matrix acceptance"
```

---

## Self-Review 记录

- **Spec 覆盖**：WS-A→T2/T3，WS-B→T4/T5，WS-C→T6，WS-D→T7/T8/T9/T10，WS-E→T11，WS-F→T12；§4 数据流→T10（启动序列）/T7-T9（生命周期）；§5 错误处理→T5（语义拒绝）/T6（位置错误）/T10（模态关闭提示、非零退出）/T9（禁静默桩）；§6 验收→T12。无遗漏。
- **Spec 修正**：WS-B 归档清单中四个 helper 文件实为 canonical 解析器依赖，已在本计划头部注明修正（归档仅 `sql_lexer.c`+`sql_parser.c`）。
- **占位符扫描**：T8/T9 的个别实现步骤引用了"以头文件实际签名为准"——这是复活式工程的真实不确定点，均已配 spike（T1）+ 失败测试（先行）+ 明确行为约束（禁静默桩），非 TBD 类占位。
- **类型一致性**：`sql_parse()`/`QueryResult`/`execute_sql()`/bootstrap 序列（`catalog_init→buf_init→heapam_init→btreeam_init→rel_init`）在 T6-T12 间一致。
