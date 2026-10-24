# SQL 合并 Spike 笔记（Task 1）

日期：2026-09-26
分支：feat/sql-consolidation
执行环境：Windows 11 + Git Bash（MinGW gcc 14.2.0），WSL2 Ubuntu
依赖方：T4（Expr 冲突图）、T6、T7（工具链）、T8（漂移清单）

---

## 1. 基线构建状态（Windows）

### 1.1 Configure

命令：`cd /d/code/book/engineering && cmake -B build -S . -DBUILD_TESTING=ON`

使用了已有的 `build/` 目录（非 `build-spike`），configure 成功：

```
-- Multimodal Config:
--   Relational: ON
--   KV: ON
--   Graph: ON
--   Vector: ON
--   Timeseries: ON
--   Document: ON
--   Spatial: ON
--   Tree: ON
--   Stream: OFF
--   Columnar: OFF
--   Sparse: ON
--   Distributed: ON
-- Document engine: code exists but has build issues, not enabling
-- Timeseries engine: code exists but has build issues, not enabling
-- Spatial engine: enabled
-- Tree engine: enabled
-- Sparse + BM25 hybrid engine: enabled
-- Flex not found - Flex-based lexer will not be built
-- Bison not found - Bison-based parser will not be built
-- Build type: Release
-- Build testing: ON
-- Configuring done (14.6s)
-- Generating done (21.5s)
```

注意：configure 输出中 `Document/Timeseries` 开关显示 ON，但紧随其后的引擎检测行显示
"Document engine: code exists but has build issues, not enabling" 与
"Timeseries engine: code exists but has build issues, not enabling" —— 开关与实际编入目标不一致，属既有状态。

### 1.2 Build（基线即失败）

命令：`cmake --build build --parallel 4` → **退出码 1（失败）**

这是增量构建（既有 build/ 目录中大部分目标已编译），本轮仅尝试 5 个编译步骤，
其中 **4 个 FAILED、81 个 error、16 个 warning**：

```
FAILED: src/db/sql/CMakeFiles/sql_engine.dir/cost.c.obj
FAILED: src/db/sql/CMakeFiles/sql_engine.dir/nodeSeqscan.c.obj
FAILED: src/db/index/storage_backend/CMakeFiles/storage_backend_memory.dir/storage_mmap.c.obj
FAILED: src/db/index/storage_backend/CMakeFiles/storage_backend_common.dir/storage_mmap.c.obj
ninja: build stopped: subcommand failed.
```

三类失败（详细错误见 §3、§4）：

| 文件 | 错误数 | 性质 |
|---|---|---|
| `src/db/sql/cost.c` | 1 | Expr 冲突的实际发作点：`invalid use of undefined type 'struct Expr'`（cost.c:212） |
| `src/db/sql/nodeSeqscan.c` | 8 | API 漂移：`EState` 未知、`struct TableScanDescData` 未定义、`mvcc_current_xid` 隐式声明、多余 `}` |
| `src/db/index/storage_backend/storage_mmap.c` | ~70（x2 目标） | Windows 平台头缺失：`HANDLE`/`CreateFileA`/`GENERIC_READ`/`INVALID_HANDLE_VALUE` 等全部未声明（疑缺 `#include <windows.h>` 或 guard 错误），与本 SQL 合并计划无直接关联 |

**基线结论：Windows 全量构建当前就是失败的。** 后续任务的验收标准应是
"不比此基线更差"，即这 4 个 FAILED 目标之外不得新增失败；
`sql_engine`（cost.c、nodeSeqscan.c）的修复本身就在计划范围内（T4/T8）。

### 1.3 基线测试

命令：`ctest --test-dir build/test --output-on-failure --timeout 60`

重要发现 1：brief 中的 `ctest --test-dir build` **找不到任何测试**
（`No tests were found!!!`）—— `build/` 根目录没有 `CTestTestfile.cmake`，
测试注册在 `build/test/CTestTestfile.cmake`（含 subdirs: algo, self_made_cpp, db,
kbase, db/consensus, apps, sdk, sdk/integration）。后续任务必须用
`ctest --test-dir build/test`。

重要发现 2：不带超时运行时，`TerminalTest.GetchNoBlock`（#415/465）会无限阻塞
（等待控制台输入，非交互 shell 下永远等不到），必须以 `--timeout 60` 运行。

**基线测试结果（权威数字）：**

```
75% tests passed, 112 tests failed out of 447
Total Test time (real) = 234.44 sec
CTEST_EXIT=8
```

按状态行精确计数（共 465 个注册测试；ctest 统计口径为 447，另有 18 个
未进入统计，疑为 fixture/disabled）：

| 状态 | 数量 |
|---|---|
| Passed | 335 |
| ***Failed | 45 |
| ***Timeout（60s） | 3（TerminalTest.GetchNoBlock #415、TerminalTest.GetchWithTimeout #416、TerminalTest.FullWorkflow #421 —— 均为控制台输入阻塞型测试，非交互 shell 下必然超时） |
| ***Not Run | 81（其中 23 个为 *_NOT_BUILT —— 测试二进制未构建，与 §1.2 构建失败直接相关；其余为可执行文件缺失的引擎测试 test_sparse/test_graph/test_vector_index 等） |

实跑失败（45 个）按套件聚类：

```
9  IVFPQTest
9  IVFFlatTest
7  CopySqlTest
7  CopyBinaryTest
6  CopyJsonTest
5  ExplainYamlTest
4  CopyTextExportTest
2  CopyTextRoundtripTest
2  CopyCsvRoundtripTest
2  BgWorkerTest
1  xquery_test / xquery_id_len_test / txn_test 等
```

**"不得比基线更差"的量化基线：Passed >= 335，实跑 Failed <= 45，
Timeout <= 3（TerminalTest 三个为环境性超时，交互终端下可能通过），
Not Run <= 81。注意构建失败修复后 *_NOT_BUILT 测试会重新出现，
其通过/失败需并入对比。**

---

## 2. Spike A — Expr 冲突 include 图

### 2.1 两组互斥的 Expr 定义（文件:行）

**组 1：简化 AST（parser 侧，struct tag = `Expr`）**

- `include/db/parser/sql/parse_node.h:123-125`
  ```c
  typedef struct Expr {
      NodeTag type;
  } Expr;
  ```
  仅有 `NodeTag type` 一个字段。同文件 `parse_node.h:24` 还有一行前向声明 `struct Expr;`。
  同族定义（同文件内冲突类型）：`parse_node.h:96 typedef uint32_t Oid;`、
  `parse_node.h:299 } TargetEntry;`（struct tag = `TargetEntry`）。

**组 2：planner/executor 侧（struct tag = `Expr_s`）**

- `include/db/sql/sql_planner.h:166-193`：完整定义 `typedef struct Expr_s { NodeTag type; SqlExprType expr_type; ... union { const_val / var / paramno / op / func } val; } Expr;`
- `include/db/sql/expr.h:38-41`：`#ifndef EXPR_DEFINED` 保护的前向声明 `typedef struct Expr_s Expr;`
- `include/db/sql/expr.h:54`：**同一 typedef 又出现一次（无 guard）**（C11 允许相同 typedef 重复，合法但说明 guard 意图落空）
- `include/db/sql/sql_executor.h:590`：又一处前向声明 `typedef struct Expr_s Expr;`
- 同族定义：`include/db/sql/sql_types.h:18 typedef uint64_t Oid;`（与组 1 的 `uint32_t Oid` 直接冲突！）、`sql_planner.h:202 } TargetEntry;`（tag = `TargetEntry_s`）
- `include/db/sql/cost.h:98`：前向声明 `struct Expr;`（组 1 的 tag）—— cost.c 踩坑的根源，见 §3

### 2.2 include 链

谁 include 了 `parser/sql/parse_node.h`（组 1）：
```
src/db/parser/sql/parse_analyze.c
src/db/parser/sql/parse_expr.c
src/db/parser/sql/parse_expr.c.bak   ← 仓库里残留的 .bak 文件
src/db/parser/sql/parse_node.c
```

谁 include 了 `parser/sql/parsenodes.h`（NodeTag 枚举等，两组共用）：
```
src/db/parser/sql/gram.y
src/db/parser/sql/scan.l
src/db/parser/sql/parse_analyze.c
src/db/parser/sql/parse_expr.c(.bak)
src/db/sql/materialized_view.c
src/db/sql/planner.c
src/db/sql/sql_parser.c
include/db/parser/sql/makefuncs.h
include/db/parser/sql/parse_node.h        ← parse_node.h:12 自己 include 它
include/db/sql/memctx.h
include/db/sql/nodes/nodetags.h
include/db/sql/sql_executor.h
include/db/sql/sql_planner.h
```

谁 include 了 `sql_planner.h`（组 2 完整定义）：
```
src/db/sql/executor.c, expr.c, planner.c, sql_driver.c, sql_executor.c
include/db/core/cross_model_optimizer.h
include/db/sql/expr.h          ← expr.h:34 经 execnodes.h 链？实际 expr.h:31-35 include execnodes.h
include/db/sql/nodes/execnodes.h（组 2 阵营）
```

谁 include 了 `expr.h`（组 2 前向声明）：
```
src/db/sql/expr.c, expr_interp.c
include/db/sql/executor.h, jit.h, nodeAgg.h, nodeHashjoin.h, nodeLimit.h,
nodeModifyTable.h, nodeProjectSet.h, nodeResult.h, nodeSort.h, window.h
```

**关键事实：当前没有任何 .c 文件同时 include 两组头** —— parse_*.c 只碰组 1，
sql/*.c 只碰组 2。因此 `typedef Expr` 的直接冲突是**潜在（latent）**的，
一旦 T4 把两组头拉进同一个 TU 就会爆发。

### 2.3 直接冲突的验证（探针编译）

探针（/tmp/conflict_probe.c，仅两行 include + main，未改动仓库源码）：
```c
#include "db/parser/sql/parse_node.h"
#include "db/sql/sql_planner.h"
int main(void){return 0;}
```
`gcc -c /tmp/conflict_probe.c -Iinclude -Iinclude/db` → 确切错误（两种 include 顺序对称爆发）：

```
include/db/sql/sql_types.h:18:18: error: conflicting types for 'Oid'; have 'uint64_t' {aka 'long long unsigned int'}
include/db/parser/sql/parse_node.h:96:18: note: previous declaration of 'Oid' with type 'Oid' {aka 'unsigned int'}
include/db/sql/sql_planner.h:193:3: error: conflicting types for 'Expr'; have 'struct Expr_s'
include/db/parser/sql/parse_node.h:125:3: note: previous declaration of 'Expr' with type 'Expr'
include/db/sql/sql_planner.h:202:3: error: conflicting types for 'TargetEntry'; have 'struct TargetEntry_s'
include/db/parser/sql/parse_node.h:299:3: note: previous declaration of 'TargetEntry' with type 'TargetEntry'
```

**冲突面比 brief 预期更大**：不止 Expr，还有 Oid（uint32 vs uint64，ABI 级不一致）
和 TargetEntry。T4 的统一类型方案必须同时处理这三组。

### 2.4 sql_driver.c 探针编译（brief 指定命令）—— 暴露出第二个独立问题

命令：`gcc -c src/db/sql/sql_driver.c -Iinclude -Iinclude/db -o /tmp/sql_driver_test.o`
（用完整构建 include 集 `-Iinclude -Iinclude/db/parser -Iinclude/db/storage -Iinclude/db/core` 结果相同）

退出码 1，**43 个 error**，但**没有出现** `conflicting types for 'Expr'` ——
编译在更早的头文件阶段就崩了。根因是 **`PLANSTATE_DEFINED` include-guard 冲突**：

1. `sql_driver.c:16` include `sql_planner.h` → `sql_planner.h:626-630`：
   ```c
   #ifndef PLANSTATE_DEFINED
   #define PLANSTATE_DEFINED
   struct PlanState_s;
   typedef struct PlanState_s PlanState;   /* 只提供了 PlanState 一个 */
   #endif
   ```
2. `sql_driver.c:17` include `executor.h` → `execnodes.h` → `execnodes.h:45-62` 的
   `#ifndef PLANSTATE_DEFINED` 整块前向声明被**跳过**
   （Plan/ExprState/PlanState/EState/ExprContext/TupleTableSlot/TupleDesc/Snapshot/List 等全部丢失）。
3. `execnodes.h` 随后的完整结构体定义引用这些类型 → 连锁爆炸。错误开头：
   ```
   include/db/sql/nodes/execnodes.h:128:21: error: unknown type name 'TupleTableSlot'
   include/db/sql/nodes/execnodes.h:201:5:  error: unknown type name 'Relation'
   include/db/sql/nodes/execnodes.h:252:5:  error: unknown type name 'Snapshot'; did you mean 'SnapshotData'?
   include/db/sql/expr.h:197:1:  error: unknown type name 'ExprState'
   include/db/sql/executor.h:93:38: error: unknown type name 'Plan'
   include/db/sql/executor.h:274:29: error: invalid use of incomplete typedef 'PlanState' {aka 'struct PlanState_s'}
   ...
   src/db/sql/sql_driver.c:221:29: error: implicit declaration of function 'CreateQueryDesc'
   src/db/sql/sql_driver.c:221:46: error: 'Plan' undeclared
   src/db/sql/sql_driver.c:241:48: error: request for member 'es_processed' in something not a structure or union
   ```

附加发现（同族潜在冲突）：`execnodes.h:52` 前向声明
`typedef struct TupleTableSlot TupleTableSlot;`（tag 无后缀），而
`sql_executor.h:131` 完整定义为 `typedef struct TupleTableSlot_s {...} TupleTableSlot;`
（tag 有 `_s` 后缀）—— 即使修好 PLANSTATE_DEFINED guard，这两个 tag 不一致仍会引发
`conflicting types for 'TupleTableSlot'`。PlanState 同样：execnodes.h 用
`struct PlanState`（:49、:281），sql_planner.h 用 `struct PlanState_s`（:628）。

### 2.5 Spike A 结论（给 T4）

1. 需要统一的类型冲突共三组：**Expr**（parse_node.h:123 vs sql_planner.h:166）、
   **Oid**（parse_node.h:96 uint32 vs sql_types.h:18 uint64）、
   **TargetEntry**（parse_node.h:299 vs sql_planner.h:202）。
2. 第二个独立坑：**`PLANSTATE_DEFINED` 同一宏守护两份不同的前向声明集**
   （execnodes.h:45 全集 vs sql_planner.h:626 单类型），include 顺序决定编译成败；
   并伴随 `PlanState`/`PlanState_s`、`TupleTableSlot`/`TupleTableSlot_s` 的 tag 不一致。
3. `expr.h:38-41` 的 `EXPR_DEFINED` guard 与 :54 的无 guard 重复 typedef 说明
   guard 方案在历史上已失控，T4 应以单一 canonical 头取代而不是再加 guard。
4. `src/db/parser/sql/parse_expr.c.bak` 残留在源码树，grep 时会干扰，建议 T4 清理。

---

## 3. Spike A 附带：cost.c 是 Expr 冲突的实际发作点（基线构建失败 1/2）

`src/db/sql/cost.c` 只 include `db/sql/cost.h`；`cost.h:98` 前向声明 `struct Expr;`
（组 1 的 tag）。`cost.c:203` 的函数签名为
`double estimate_selectivity(AttStats *stats, struct Expr *clause)`，
但 `cost.c:212` 却访问 `clause->val.const_val` —— `val.const_val` 只存在于
**组 2** 的 `struct Expr_s`（sql_planner.h:176）。编译错误：

```
src/db/sql/cost.c: In function 'estimate_selectivity':
src/db/sql/cost.c:212:63: error: invalid use of undefined type 'struct Expr'
  212 |         if (stats->mcv_freq && stats->mcv_nitems > 0 && clause->val.const_val) {
      |                                                               ^~
```

即：cost.c 是按组 2 的结构体形状写的，类型却用了组 1 的 tag，且任何一组头都没被
include 进来。T4 统一 Expr 后此文件应自然修复。

## 4. Spike B — sqlExecutor.c 漂移点清单

### 4.1 brief 指定探针的结果：编译通过（0 错误 0 警告）

```
gcc -c src/db/executor/sql/sqlExecutor.c -Iinclude -Iinclude/db -o /tmp/sqlExecutor_test.o
→ 退出码 0（用 db_executor 的完整 include 集复测同样通过）
```

`src/db/executor/CMakeLists.txt:28` 的注释称
"（header 冲突 + sqlExecutor API 漂移，归 Gap03 统一执行器变更处理）"，
但**以当前头文件状态，sqlExecutor.c 本身已能独立编译** —— 注释中的漂移可能已被
后续提交修复，或漂移实际位于同库其他 TU / 链接阶段。`db_executor` 目标由
`file(GLOB_RECURSE ... "*.c")` 收集 `executor/sql/*.c`（含 sqlExecutor.c、sqlExec.c）；
本次基线构建在到达该目标前已被 sql_engine 的失败截停，无法确认 db_executor 全库状态。

### 4.2 实际漂移点清单（T8 按此修复）

真正漂移的是 `src/db/sql/` 下进入 `sql_engine` 目标的两个文件（完整错误文本）：

**nodeSeqscan.c（8 个 error）**
```
src/db/sql/nodeSeqscan.c:102:5:  error: unknown type name 'EState'; did you mean 'AggState'?
    EState *estate_p = (EState *)estate;
src/db/sql/nodeSeqscan.c:102:25: error: 'EState' undeclared (first use in this function)
src/db/sql/nodeSeqscan.c:102:33: error: expected expression before ')' token
src/db/sql/nodeSeqscan.c:103:44: error: request for member 'es_query_cxt' in something not a structure or union
    MemoryContext ctx = estate_p ? estate_p->es_query_cxt : NULL;
src/db/sql/nodeSeqscan.c:242:63: error: invalid use of undefined type 'struct TableScanDescData'
    slot->tts_tid.ip_blkid = ext_state->ss_currentScanDesc->rs_curr_blk;
src/db/sql/nodeSeqscan.c:243:63: error: invalid use of undefined type 'struct TableScanDescData'
    slot->tts_tid.ip_posid = ext_state->ss_currentScanDesc->rs_curr_off;
src/db/sql/nodeSeqscan.c:258:31: error: implicit declaration of function 'mvcc_current_xid'
    int64_t current_xid = mvcc_current_xid();
src/db/sql/nodeSeqscan.c:269:1:  error: expected identifier or '(' before '}' token
```
nodeSeqscan.c 只 include 了 `db/sql/nodes/nodeSeqscan.h` 和 `db/sql/memctx.h`。
漂移点：(a) `EState`/`es_query_cxt` —— 需要 execnodes.h 的完整 EState（被 §2.4 的
guard 冲突阻断或根本未 include）；(b) `struct TableScanDescData` 仅前向声明，
`rs_curr_blk`/`rs_curr_off` 字段访问需要完整定义；(c) `mvcc_current_xid()` 无声明头；
(d) 269 行多了一个 `}`（语法错误）。

**cost.c（1 个 error）**：见 §3，Expr tag/形状错配。

**sql_driver.c（43 个 error，不进 sql_engine 目标但同属执行链）**：见 §2.4，
PLANSTATE_DEFINED guard 冲突 + `CreateQueryDesc` 未声明 + `Plan`/`es_processed` 漂移。

**storage_mmap.c（~70 个 error，与 SQL 合并无关但计入基线）**：
`HANDLE`/`CreateFileA`/`GENERIC_READ`/`INVALID_HANDLE_VALUE` 等 Win32 符号全部未声明，
在 `storage_backend_memory` 和 `storage_backend_common` 两个目标中重复编译、重复失败。

---

## 5. Spike C — WSL2 工具链版本

命令：`wsl -e bash -lc "flex --version && bison --version"`

**结果：flex 与 bison 均未安装。**

```
bash: line 1: flex: command not found
bash: line 1: bison: command not found
```

尝试 `sudo apt-get install -y flex bison` 失败：WSL 用户 `yinch` 的 sudo 需要交互式
密码（`sudo: interactive authentication is required`），非交互环境无法安装。
Windows 侧同样没有 flex/bison（`where flex bison win_flex win_bison` 全无，
configure 输出 "Flex not found / Bison not found"）。

WSL 附注：每次 wsl 调用都打印一条 localhost 代理/NAT 警告
（"wsl: 检测到 localhost 代理配置，但未镜像到 WSL。NAT 模式下的 WSL 不支持 localhost 代理。"），
不影响命令执行。

**给 T7 的处置建议（三选一）**：
1. 用户手工执行一次：`wsl -e bash -lc "sudo apt-get install -y flex bison"`（需输密码）；
2. Windows 侧 `choco install winflexbison3`（configure 提示的官方路径，
   src/db/parser/sql/CMakeLists.txt:43-44）；
3. T7 不重新生成，直接维护手写的 `sql_lexer.c`/`sql_parser.c`
   （当前 CMake 在找不到 flex/bison 时优雅回退，构建系统已按此路径工作）。

CMake 回退逻辑（src/db/parser/sql/CMakeLists.txt:25-46）：`find_program(FLEX_EXECUTABLE flex)`
找不到则跳过 `lex.sql.c`/`gram.c` 生成，仅编译 `db_parser_sql_simplified`
（makefuncs.c、parse_analyze.c、parse_expr.c、parse_node.c 等手写文件）。
因此 flex/bison 缺失**不阻塞**当前构建，只阻塞 gram.y/scan.l 的重新生成。

生成文件头注释所需版本号：**待安装后补充**（当前无版本可记）。

---

## 6. 编译器信息

- 探针编译器：MinGW-W64 gcc 14.2.0（Git Bash `gcc`，`gcc.exe (MinGW-W64 x86_64-msvcrt-posix-seh, built by Brecht Sanders, r3) 14.2.0`）
- 主构建：同 gcc（经 ccache），`-std=gnu11 -Wall -Wextra -Wpedantic -O2 -DNDEBUG`
- WSL2：flex/bison 缺失（见 §5）
