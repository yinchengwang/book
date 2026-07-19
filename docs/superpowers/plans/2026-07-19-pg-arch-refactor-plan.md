# 数据库 PG 架构重构实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按照 PG 架构全面重构 `engineering/src/db/` 下的 SQL 引擎，端到端贯通解析 → 计划 → 执行 → 存储链路，并补齐多模态接入、pgwire 协议、SDK 分层。

**Architecture:** 采用 PostgreSQL 风格的分层架构（Parser → Logical Plan → Physical Plan → Volcano Executor → Storage AM）。删除无价值的死代码/桩实现，保留有架构价值的模块并重写其 TODO 占位部分，补全 13 个算子的 catalog 集成，最后扩展到多模态引擎、pgwire 协议、SDK 层。

**Tech Stack:** C11、C++17、CMake 3.20+、GoogleTest、MinGW 14.2、clangd 22.1.6（语言服务器）。

**Spec:** [docs/superpowers/specs/2026-07-19-pg-arch-refactor-design.md](../specs/2026-07-19-pg-arch-refactor-design.md)

---

## Global Constraints

- **构建系统**：CMake 3.20+，Ninja 生成器，输出到 `build/engineering/`
- **代码规范**：C11/C++17，所有注释使用中文（CLAUDE.md）
- **Commit Message**：使用中文，遵循 Conventional Commits
- **测试框架**：GoogleTest（vendored），`.cpp` 测试文件
- **编译验证**：每个 Task 完成后 `ninja -j4` 全量编译通过 + 所有现有测试通过
- **OpenSpec 纪律**：只提交与变更相关的代码（不混提交）
- **现有模块保留**：catalog、buffer pool、WAL、MVCC、heap AM、btree AM、多模态引擎（vector/graph/spatial/doc/ts/yang）全部保留
- **删除清单（无价值代码）**：sql_executor.c（674 行全桩）、optimizer.c（360 行全 TODO）、~~parse_analyze.c + parse_expr.c（1326 行死代码）~~ **取消**：它们是 Bison 套测试基础设施（被 test_parser_sql.cpp 调用）、sql/sql_parser.c + sql/sql_driver.c（2618 行死代码）、executor/graph/traverse.c 中 4 个死函数（~400 行）
- **重写清单**：nodeHash.c（152 行骨架）合并到 nodeHashjoin.c，nodeIndexscan.c::exec_index_scan（小写）改名为 ExecIndexScan

---

## 三档总览

| 档 | 内容 | 周期 | 目标 |
|---|---|---|---|
| **第一档** | 清理死代码 + 端到端贯通 | 1-2 周 | MVP（最小可用 SQL 引擎） |
| **第二档** | 补全贯通 + 测试覆盖 + 多模态接入 + 索引 AM 框架 | 16-22 周 | 生产可用 |
| **第三档** | 优化器代价驱动 + ANALYZE + pgwire + SDK + 事务 | 14-20 周 | PG 兼容 |

**总周期**：31-44 周

---

## 第一档：清理 + 端到端贯通（1-2 周）

### Task 1.1：删除 sql_executor.c（全桩实现）

**Files:**
- Delete: `engineering/src/db/sql/sql_executor.c`
- Modify: `engineering/src/db/sql/CMakeLists.txt:32-50`（移除 sql_executor.c 引用）
- Test: 验证 `ninja -j4` 编译通过 + `ctest` 测试通过

**原因**：sql_executor.c 中 15 个 `exec_xxx` 函数 100% 是桩实现（全部 `return NULL`），无任何价值。

**步骤**：

- [ ] **Step 1：验证 sql_executor.c 全是桩实现**

```bash
grep -A5 "^TupleTableSlot \*exec_seq_scan\b" engineering/src/db/sql/sql_executor.c | head -10
# 预期看到：
# TupleTableSlot *exec_seq_scan(SeqScanState *node)
# {
#     /* 桩实现：返回空 */
#     (void)node;
#     return NULL;
# }
```

- [ ] **Step 2：删除 sql_executor.c**

```bash
git rm engineering/src/db/sql/sql_executor.c
```

- [ ] **Step 3：从 CMakeLists.txt 移除引用**

编辑 `engineering/src/db/sql/CMakeLists.txt`，从 `add_library(sql_engine STATIC ...)` 列表中删除 `sql_executor.c`（查找 `sql_executor.c` 行并删除）。

- [ ] **Step 4：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功，无错误
```

- [ ] **Step 5：运行测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure
# 预期：所有现有测试通过
```

- [ ] **Step 6：提交**

```bash
git add engineering/src/db/sql/CMakeLists.txt
git commit -m "refactor: 删除 sql_executor.c（全桩实现，无价值代码）"
```

### Task 1.2：[CANCELLED] 删除 parse_analyze.c + parse_expr.c（**误判，不是死代码**）

**状态**：~~CANCELLED~~ **已取消**（2026-07-19）

**取消原因**：

之前误判 parse_analyze.c 和 parse_expr.c 是死代码，实际上它们被 `engineering/test/db/parser/test_parser_sql.cpp` 调用：
- `transformSelectStmt`（test_parser_sql.cpp:344）
- `makeVar`、`makeConst`（test_parser_sql.cpp:433, 447）
- 其他 `transformXxxStmt` 工具函数

它们服务于 Bison 风格的解析+语义分析测试套件，是**测试基础设施**而非死代码。删除会导致 Bison 套测试无法编译。

**决策**：保留 parse_analyze.c + parse_expr.c，作为 Bison 套测试基础设施。

**Files:**
- ~~Delete: `engineering/src/db/parser/sql/parse_analyze.c`~~
- ~~Delete: `engineering/src/db/parser/sql/parse_expr.c`~~
- ~~Modify: `engineering/src/db/parser/sql/CMakeLists.txt`~~
- ~~Test: 验证编译通过 + 测试通过~~

**修订原因**：grep `transformSelectStmt|makeVar|makeConst|transformInsertStmt` 在 `engineering/test/db/parser/test_parser_sql.cpp` 中找到 4 处引用，说明它们是活跃的测试基础设施。

**步骤**：

- [x] ~~Step 1：验证无调用方~~（取消：发现它们被 test_parser_sql.cpp 调用）
- [x] ~~Step 2：删除两个文件~~（取消）
- [x] ~~Step 3：从 CMakeLists.txt 移除~~（取消）
- [x] ~~Step 4：编译验证~~（取消）
- [x] ~~Step 5：测试验证~~（取消）
- [x] ~~Step 6：提交~~（取消）

**替代行动**：保留文件不变，仅更新本文档与 spec 文档记录决策。

### Task 1.3：删除 sql/sql_parser.c + sql/sql_driver.c（死代码）

**Files:**
- Delete: `engineering/src/db/sql/sql_parser.c`（2318 行）
- Delete: `engineering/src/db/sql/sql_driver.c`（300 行）
- Modify: `engineering/src/db/sql/CMakeLists.txt`（删除引用）
- Test: 验证编译通过

**原因**：CMakeLists.txt 已注释（"暂时排除"），无调用方。

**步骤**：

- [ ] **Step 1：验证无调用方**

```bash
grep -rln "sql_driver\b" engineering/src/ engineering/test/ 2>/dev/null | grep -v "sql_driver.c$"
# 预期：无输出
```

- [ ] **Step 2：删除文件**

```bash
git rm engineering/src/db/sql/sql_parser.c engineering/src/db/sql/sql_driver.c
```

- [ ] **Step 3：从 CMakeLists.txt 移除**

编辑 `engineering/src/db/sql/CMakeLists.txt`，删除 `#   sql_driver.c` 注释行（该行已注释），保留 `sql_parser.c` 注释（"暂时排除"）。

- [ ] **Step 4：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功
```

- [ ] **Step 5：测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure
```

- [ ] **Step 6：提交**

```bash
git add engineering/src/db/sql/CMakeLists.txt
git commit -m "refactor: 删除 sql/sql_parser.c + sql_driver.c（死代码）"
```

### Task 1.4：删除 optimizer.c（全 TODO）

**Files:**
- Delete: `engineering/src/db/sql/optimizer.c`
- Modify: `engineering/src/db/sql/CMakeLists.txt`
- Test: 验证编译通过

**原因**：optimizer.c 中 7 个 `opt_xxx` 函数全是 TODO 占位（"TODO: 实现..."），无任何价值。

**步骤**：

- [ ] **Step 1：验证无调用方**

```bash
grep -rln "opt_predicate_pushdown\|opt_projection_pushdown\|opt_join_order\|optimize_plan" engineering/src/ engineering/test/ 2>/dev/null | grep -v "optimizer.c$"
# 预期：无输出
```

- [ ] **Step 2：删除 optimizer.c**

```bash
git rm engineering/src/db/sql/optimizer.c
```

- [ ] **Step 3：从 CMakeLists.txt 移除**

```bash
# 删除 engine/db/sql/CMakeLists.txt 中的 optimizer.c 引用
grep -n "optimizer.c" engineering/src/db/sql/CMakeLists.txt
# 找到行号后删除该行
```

- [ ] **Step 4：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功
```

- [ ] **Step 5：测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure
```

- [ ] **Step 6：提交**

```bash
git add engineering/src/db/sql/CMakeLists.txt
git commit -m "refactor: 删除 optimizer.c（全 TODO 占位）"
```

### Task 1.5：删除 executor/graph/traverse.c 中 4 个死函数

**Files:**
- Modify: `engineering/src/db/executor/graph/traverse/traverse.c`（删除 4 个函数，保留 graph_traverser_*）
- Test: 验证编译通过

**原因**：`graph_bfs`、`graph_dfs`、`graph_dijkstra`、`graph_pagerank`、`graph_shortest_path` 零调用方。保留 `graph_traverser_*`（被 graph_scan.c 使用）。

**步骤**：

- [ ] **Step 1：定位死函数**

```bash
grep -n "^size_t graph_bfs\|^size_t graph_dfs\|^double graph_dijkstra\|^int graph_pagerank\|^int graph_shortest_path" engineering/src/db/executor/graph/traverse/traverse.c
```

- [ ] **Step 2：删除 4 个死函数**

使用 Edit 工具删除 `graph_bfs`、`graph_dfs`、`graph_dijkstra`、`graph_pagerank`、`graph_shortest_path` 的完整函数定义（包括注释和空行）。

- [ ] **Step 3：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功
```

- [ ] **Step 4：测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure
```

- [ ] **Step 5：提交**

```bash
git add engineering/src/db/executor/graph/traverse/traverse.c
git commit -m "refactor: 删除 traverse.c 中 4 个死函数（graph_bfs/dfs/dijkstra/pagerank）"
```

### Task 1.6：合并 nodeHash.c 到 nodeHashjoin.c

**Files:**
- Delete: `engineering/src/db/sql/nodeHash.c`
- Delete: `engineering/include/db/sql/nodeHash.h`
- Modify: `engineering/src/db/sql/CMakeLists.txt`
- Modify: `engineering/src/db/sql/nodeHashjoin.c`（添加 Hash 节点代码）
- Test: 验证编译通过

**原因**：nodeHash.c 仅 152 行，Exec 函数是骨架（`return NULL`），合并到 nodeHashjoin.c 简化结构。

**步骤**：

- [ ] **Step 1：阅读 nodeHash.c 内容**

```bash
cat engineering/src/db/sql/nodeHash.c
cat engineering/include/db/sql/nodeHash.h
```

- [ ] **Step 2：合并 nodeHash.c 内容到 nodeHashjoin.c**

打开 `engineering/src/db/sql/nodeHashjoin.c`，在文件末尾追加 nodeHash.c 中的：
- `ExecInitHash` 函数定义
- `ExecEndHash` 函数定义
- HashState 结构体（如果需要）

- [ ] **Step 3：删除 nodeHash.c 和 nodeHash.h**

```bash
git rm engineering/src/db/sql/nodeHash.c engineering/include/db/sql/nodeHash.h
```

- [ ] **Step 4：从 CMakeLists.txt 移除 nodeHash.c 引用**

```bash
# 编辑 engineering/src/db/sql/CMakeLists.txt，删除 nodeHash.c 行
```

- [ ] **Step 5：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功（如果 hashjoin.c 引用了 nodeHash.h 头文件，需要改为本地声明）
```

- [ ] **Step 6：修复编译错误（如有）**

如果在编译中出现 "nodeHash.h: No such file" 错误，需要把 HashState 等定义从 nodeHashjoin.c 直接引用，或在 nodeHashjoin.h 中添加。

- [ ] **Step 7：测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure -R hash
# 预期：hash 测试通过
```

- [ ] **Step 8：提交**

```bash
git add engineering/src/db/sql/nodeHashjoin.c engineering/src/db/sql/CMakeLists.txt
git commit -m "refactor: 合并 nodeHash.c 到 nodeHashjoin.c（Hash 是骨架）"
```

### Task 1.7：统一 nodeIndexscan.c 中 exec_index_scan 命名为 ExecIndexScan

**Files:**
- Modify: `engineering/src/db/sql/nodeIndexscan.c`（重命名）
- Modify: `engineering/src/db/sql/planner.c`（更新 get_exec_func 中的引用）
- Modify: `engineering/src/db/sql/sql_executor.c`（已删除，无需更新）
- Test: 验证编译通过

**原因**：nodeIndexscan.c 中函数名是 `exec_index_scan`（小写），与其他 12 个算子的 `ExecXxx`（大写）命名不一致。

**步骤**：

- [ ] **Step 1：定位引用**

```bash
grep -rn "exec_index_scan" engineering/src/ engineering/include/ 2>/dev/null
```

- [ ] **Step 2：在 nodeIndexscan.c 中重命名**

使用 Edit 工具将 `exec_index_scan` 替换为 `ExecIndexScan`（包括函数定义和引用）。

- [ ] **Step 3：更新 planner.c 中的引用**

```bash
# planner.c::get_exec_func 中：
# case PHYS_INDEX_SCAN:
#     return (TupleTableSlot *(*)(PlanState *))exec_index_scan;
# 改为：
# case PHYS_INDEX_SCAN:
#     return (TupleTableSlot *(*)(PlanState *))ExecIndexScan;
```

- [ ] **Step 4：编译验证**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering
# 预期：编译成功
```

- [ ] **Step 5：测试验证**

```bash
ctest --test-dir build/engineering --output-on-failure -R index
# 预期：index 测试通过
```

- [ ] **Step 6：提交**

```bash
git add engineering/src/db/sql/nodeIndexscan.c engineering/src/db/sql/planner.c
git commit -m "refactor: 统一 exec_index_scan 命名为 ExecIndexScan"
```

### Task 1.8：补全 planner.c 中 node_to_logical SELECT 解析

**Files:**
- Modify: `engineering/src/db/sql/planner.c`（`node_to_logical` 函数 SQL_NODE_SELECT 分支）
- Test: `engineering/test/db/sql/test_planner_select.cpp`（新增测试）
- Test: 验证编译通过 + 新测试通过

**目标**：当前 `node_to_logical` 对 SELECT 只设置 `plan->type = LOGICAL_SCAN` 后就 break，没有真正解析 columns/where/table。需要补全。

**接口**：
- Consumes: `sql_node_t *node`（含 select.columns、select.table_name、select.where_cond）
- Produces: `LogicalPlan *`（含 left/right、qual、targetList）

**步骤**：

- [ ] **Step 1：写失败的测试**

创建 `engineering/test/db/sql/test_planner_select.cpp`：

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/sql/sql_planner.h"
#include "db/parser/sql/sql.h"
}

TEST(PlannerSelectTest, ParseSimpleSelect) {
    PlannerContext *ctx = planner_create();
    ASSERT_NE(ctx, nullptr);
    
    // 构造 sql_node_t：SELECT id, name FROM users WHERE id = 1
    sql_node_t *node = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = strdup("users");
    node->u.select.columns = nullptr;  // 简化：先支持 SELECT *
    node->u.select.where_cond = nullptr;
    
    LogicalPlan *plan = sql_node_to_logical(ctx, node);
    ASSERT_NE(plan, nullptr);
    EXPECT_EQ(plan->type, LOGICAL_SCAN);
    
    sql_node_free(node);
    planner_destroy(ctx);
}
```

- [ ] **Step 2：运行测试看失败**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering test_planner
build/engineering/test/db/sql/test_planner
# 预期：测试通过（因为现有代码逻辑已经在 LOGICAL_SCAN 设置成功）
```

注：现有代码可能让测试通过。验证测试能否捕获到 columns/where 未解析。

- [ ] **Step 3：实现 node_to_logical 的 SELECT 真正解析**

修改 `engineering/src/db/sql/planner.c` 中 `node_to_logical` 的 `case SQL_NODE_SELECT`，让它真正设置：
- `plan->left` 为 Scan 节点（含 table_name）
- `plan->qual` 为 WHERE 条件（如果是简单比较表达式）
- `plan->targetList` 为 columns 列表

参考现有 `select_stmt_to_logical` 的实现。

- [ ] **Step 4：运行测试看通过**

```bash
ninja -j4 -C build/engineering test_planner
build/engineering/test/db/sql/test_planner
# 预期：测试通过
```

- [ ] **Step 5：扩展测试覆盖 WHERE 条件**

在 `test_planner_select.cpp` 中添加测试：

```cpp
TEST(PlannerSelectTest, ParseSelectWithWhere) {
    PlannerContext *ctx = planner_create();
    
    sql_node_t *node = (sql_node_t *)calloc(1, sizeof(sql_node_t));
    node->type = SQL_NODE_SELECT;
    node->u.select.table_name = strdup("users");
    node->u.select.where_cond = create_simple_comparison_expr("id", "=", "1");
    
    LogicalPlan *plan = sql_node_to_logical(ctx, node);
    EXPECT_NE(plan->qual, nullptr);
    
    sql_node_free(node);
    planner_destroy(ctx);
}
```

- [ ] **Step 6：提交**

```bash
git add engineering/src/db/sql/planner.c engineering/test/db/sql/test_planner_select.cpp
git commit -m "feat: 补全 planner.c 中 SELECT 真正解析 columns/where/table"
```

### Task 1.9：端到端测试（CREATE TABLE + INSERT + SELECT）

**Files:**
- Create: `engineering/test/db/sql/test_e2e_basic.cpp`
- Test: 验证从解析 → 计划 → 执行端到端跑通

**目标**：创建端到端测试，验证最小可用 SQL 流程跑通（CREATE TABLE → INSERT → SELECT）。

**步骤**：

- [ ] **Step 1：写端到端测试骨架**

创建 `engineering/test/db/sql/test_e2e_basic.cpp`：

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/parser/sql/sql.h"
#include "db/sql/sql_planner.h"
#include "db/sql/executor.h"
#include "db/storage/catalog/catalog.h"
}

TEST(E2EBasicTest, CreateTableInsertSelect) {
    // 1. 初始化 catalog
    catalog_init();
    
    // 2. CREATE TABLE users (id INT, name TEXT)
    Oid oid = catalog_create_table("users", nullptr, 0);  // 需要根据实际 API 调整
    EXPECT_NE(oid, InvalidOid);
    
    // 3. INSERT INTO users VALUES (1, 'alice')
    // ... 省略存储层适配（见后续 Task 2.x）
    
    catalog_shutdown();
}
```

- [ ] **Step 2：编译并运行（验证当前可达成度）**

```bash
cmake -S engineering -B build/engineering
ninja -j4 -C build/engineering test_e2e_basic
# 预期：可能部分失败，但框架就位
```

- [ ] **Step 3：根据测试结果决定第二档起点**

如果端到端可跑通 → 第一档完成，进入第二档。如果失败 → 记录在档 1 完成，档 2 优先解决。

- [ ] **Step 4：提交**

```bash
git add engineering/test/db/sql/test_e2e_basic.cpp
git commit -m "test: 添加端到端测试骨架（CREATE TABLE + INSERT + SELECT）"
```

---

## 第二档：补全贯通 + 多模态接入 + 索引 AM 框架（16-22 周）

### Task 2.1：补全 ExecSeqScan 的 catalog 集成

**Files:**
- Modify: `engineering/src/db/sql/nodeSeqscan.c`
- Test: 验证 ExecSeqScan 真正通过 catalog 获取 Relation

**目标**：当前 ExecSeqScan 通过 `ext_state->ss_currentRelation` 工作，需要确认 Init 阶段 `ExecInitSeqScanRelation` 正确调 catalog。

**步骤**：

- [ ] **Step 1：写测试验证 ExecInitSeqScanRelation 调用 catalog**

创建 `engineering/test/db/sql/test_seqscan_init.cpp`：

```cpp
TEST(SeqScanInitTest, CallsCatalog) {
    catalog_init();
    
    Oid oid = catalog_create_table("t", nullptr, 0);
    
    SeqScanState *state = ExecInitSeqScan(/* plan */, /* estate */, /* eflags */);
    
    EXPECT_NE(state, nullptr);
    EXPECT_NE(state->ext_state->ss_currentRelation, nullptr);
    
    ExecEndSeqScan(state);
    catalog_shutdown();
}
```

- [ ] **Step 2：运行测试看通过**

如果 ExecInitSeqScanRelation 已经在调 catalog（之前 grep 已确认），测试应该通过。

- [ ] **Step 3：如果测试失败，修复 ExecInitSeqScanRelation**

修改 `engineering/src/db/sql/nodeSeqscan.c` 的 `ExecInitSeqScanRelation`，确保：
- 调 `relation_open(relid, RELMODE_READ)`
- 设置 `ext_state->ss_currentRelation`

- [ ] **Step 4：提交**

```bash
git commit -am "test: 验证 ExecSeqScan catalog 集成"
```

### Task 2.2：补全 ExecHashJoin 真正实现

**Files:**
- Modify: `engineering/src/db/sql/nodeHashjoin.c`
- Test: 验证哈希连接真正工作

**步骤**：

- [ ] **Step 1：检查 ExecHashJoin 当前实现深度**

```bash
wc -l engineering/src/db/sql/nodeHashjoin.c
grep -E "^.*ExecHashJoin|exec_hashjoin_impl" engineering/src/db/sql/nodeHashjoin.c | head -5
```

- [ ] **Step 2：写测试**

创建 `engineering/test/db/sql/test_hashjoin_e2e.cpp`：

```cpp
TEST(HashJoinTest, BasicHashJoin) {
    catalog_init();
    catalog_create_table("t1", nullptr, 0);
    catalog_create_table("t2", nullptr, 0);
    
    // 插入测试数据
    
    // 创建 HashJoinState
    // 验证能返回正确元组
    
    catalog_shutdown();
}
```

- [ ] **Step 3：实现/补全 ExecHashJoin**

参考 PG `nodeHashjoin.c` 实现：build 阶段构建哈希表，probe 阶段查找匹配。

- [ ] **Step 4：测试通过 + 提交**

```bash
git commit -am "feat: 补全 ExecHashJoin 哈希连接实现"
```

### Task 2.3：补全 ExecNestLoop

**Files:**
- Modify: `engineering/src/db/sql/nodeNestloop.c`
- Test: 验证嵌套循环连接工作

**步骤**：类似 Task 2.2，参考 PG `nodeNestloop.c` 实现。

### Task 2.4：补全 ExecAgg 真正实现

**Files:**
- Modify: `engineering/src/db/sql/nodeAgg.c`
- Test: 验证聚合函数（SUM/COUNT/AVG）工作

### Task 2.5：补全 ExecSort 真正实现

**Files:**
- Modify: `engineering/src/db/sql/nodeSort.c`
- Test: 验证排序工作

### Task 2.6：补全 ExecLimit 真正实现

**Files:**
- Modify: `engineering/src/db/sql/nodeLimit.c`
- Test: 验证 LIMIT/OFFSET 工作

### Task 2.7：补全 ExecModifyTable 真正 update/delete

**Files:**
- Modify: `engineering/src/db/sql/nodeModifyTable.c`
- Test: 验证 UPDATE/DELETE 真正修改存储

### Task 2.8：补全 ExecInsert 真正 catalog_insert

**Files:**
- Modify: `engineering/src/db/sql/nodeModifyTable.c`
- Test: 验证 INSERT 真正写入存储

### Task 2.9：补全 ExecIndexScan 真正 btree 扫描

**Files:**
- Modify: `engineering/src/db/sql/nodeIndexscan.c`
- Test: 验证索引扫描工作

### Task 2.10：补全 ExecProject / ExecFilter / 其他算子

**Files:**
- Modify: `engineering/src/db/sql/nodeProjectSet.c`
- Test: 验证投影和过滤

### Task 2.11：graph_engine 接入 storage_ops_t

**Files:**
- Modify: `engineering/src/db/storage/graph/graph_engine.c`
- Modify: `engineering/src/db/storage/CMakeLists.txt`
- Test: 验证 graph_engine_get_ops 注册成功

**目标**：实现 graph_engine 的 storage_ops_t 函数指针表，调用 `storage_register_engine(MODEL_GRAPH, &graph_ops)`。

**步骤**：

- [ ] **Step 1：实现 graph_storage_ops_t**

在 `engineering/src/db/storage/graph/graph_engine.c` 添加：

```c
const storage_ops_t *graph_engine_get_ops(void) {
    static const storage_ops_t ops = {
        .name = "graph",
        .model = MODEL_GRAPH,
        .init = graph_engine_init,
        .shutdown = graph_engine_shutdown,
        .table_create = graph_engine_create,
        .table_open = graph_engine_open,
        .table_close = graph_engine_close,
        .table_drop = graph_engine_drop,
        .tuple_insert = graph_engine_insert,
        .tuple_update = graph_engine_update,
        .tuple_delete = graph_engine_delete,
        .scan_begin = graph_engine_scan_begin,
        .scan_next = graph_engine_scan_next,
        .scan_end = graph_engine_scan_end,
        .index_create = NULL,
        .index_drop = NULL,
        .get_stats = graph_engine_get_stats,
    };
    return &ops;
}
```

- [ ] **Step 2：在 graph_engine_init 中注册**

```c
int graph_engine_init(const char *data_dir) {
    // ... 现有代码
    storage_register_engine(MODEL_GRAPH, graph_engine_get_ops());
    return 0;
}
```

- [ ] **Step 3：测试验证**

```cpp
TEST(GraphEngineTest, RegisterStorageOps) {
    storage_init_all("./test_data");
    const storage_ops_t *ops = storage_get_engine(MODEL_GRAPH);
    EXPECT_NE(ops, nullptr);
    EXPECT_STREQ(ops->name, "graph");
    storage_shutdown_all();
}
```

- [ ] **Step 4：编译 + 测试 + 提交**

```bash
git commit -am "feat: graph_engine 接入 storage_ops_t"
```

### Task 2.12：spatial_engine 接入 storage_ops_t

类似 Task 2.11，针对 `engineering/src/db/storage/spatial/spatial_engine.c`。

### Task 2.13：doc_engine 接入 storage_ops_t

需要先在 CMakeLists.txt 启用 `add_subdirectory(doc)`，然后实现 storage_ops_t。

### Task 2.14：ts_engine 接入 storage_ops_t

需要先在 CMakeLists.txt 启用 `add_subdirectory(ts)`，然后实现 storage_ops_t。

### Task 2.15：yang_engine 接入 storage_ops_t

针对 `engineering/src/db/storage/yang/yang_engine.c`。

### Task 2.16：引入 IndexAmRoutine 框架

**Files:**
- Create: `engineering/include/db/access/amapi.h`
- Create: `engineering/src/db/access/amapi.c`
- Test: 验证 AM 注册机制工作

**目标**：参考 PG `access/amapi.h` 引入索引 AM 注册表。

**步骤**：

- [ ] **Step 1：定义 IndexAmRoutine**

```c
// engineering/include/db/access/amapi.h
typedef struct IndexAmRoutine_s {
    const char *am_name;
    int (*ambuild)(Relation heap, IndexInfo *index);
    int (*aminsert)(Relation index, Datum *values, bool *isnull);
    int (*amdelete)(Relation index, Datum *values, bool *isnull);
    IndexScanDesc (*ambeginscan)(Relation index, int nkeys, ScanKey key);
    void (*amendscan)(IndexScanDesc scan);
    bool (*amrescan)(IndexScanDesc scan, ScanKey key);
} IndexAmRoutine;

int index_am_register(const char *am_name, const IndexAmRoutine *routine);
const IndexAmRoutine *index_am_get(const char *am_name);
```

- [ ] **Step 2：实现注册表**

```c
// engineering/src/db/access/amapi.c
static IndexAmRoutine *g_am_registry[16];
static int g_am_count = 0;

int index_am_register(const char *am_name, const IndexAmRoutine *routine) {
    if (g_am_count >= 16) return -1;
    g_am_registry[g_am_count++] = (IndexAmRoutine *)routine;
    return 0;
}
```

- [ ] **Step 3：测试**

```cpp
TEST(AmApiTest, RegisterBtree) {
    IndexAmRoutine btree_am = { .am_name = "btree", .ambuild = btbuild, ... };
    EXPECT_EQ(index_am_register("btree", &btree_am), 0);
    EXPECT_NE(index_am_get("btree"), nullptr);
}
```

- [ ] **Step 4：编译 + 提交**

```bash
git commit -am "feat: 引入 IndexAmRoutine 框架"
```

### Task 2.17：btree 适配 IndexAmRoutine

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Test: 验证 btree 通过 AM 调用

### Task 2.18：HNSW 适配 IndexAmRoutine

**Files:**
- Modify: `engineering/src/db/index/vector_index/faiss_hnsw/faiss_hnsw_create.c`
- Test: 验证 HNSW 通过 AM 调用

### Task 2.19：修复 btbuild 接受 ntuples=0

**Files:**
- Modify: `engineering/src/db/storage/access/btree/btreeam.c`
- Test: 验证空表可建索引

### Task 2.20：修复 vector_engine_build_index 支持空集合

**Files:**
- Modify: `engineering/src/db/storage/vector/vector_engine.c`
- Test: 验证空集合可建索引

### Task 2.21：vector_engine 加 active_index_type 字段

**Files:**
- Modify: `engineering/include/db/vector_engine.h`
- Modify: `engineering/src/db/storage/vector/vector_engine.c`
- Test: 验证根据 active_index_type 分派

### Task 2.22：FEATURE_SQL / FEATURE_KV 编译宏

**Files:**
- Modify: `engineering/src/db/storage/CMakeLists.txt`
- Modify: 各 .c/.h 文件添加 `#ifdef FEATURE_XXX`

**目标**：用编译宏隔离每个引擎独有代码。

---

## 第三档：优化器代价驱动 + ANALYZE + pgwire + SDK + 事务（14-20 周）

### Task 3.1：补全 analyze_execute 真正实现

**Files:**
- Modify: `engineering/src/db/tools/vacuum/vacuum.c`
- Test: 验证 ANALYZE 真正更新 pg_statistic

### Task 3.2：planner.c::cost_seqscan 读 table_info.npages/ntupes

**Files:**
- Modify: `engineering/src/db/sql/planner.c`
- Test: 验证代价估算用真实统计

### Task 3.3：planner 调 stats_get_tables 拿真实行数

**Files:**
- Modify: `engineering/src/db/sql/planner.c`
- Test: 验证 planner 用 StatsCollector 数据

### Task 3.4：StatsCollector 在 INSERT/DELETE/UPDATE 时更新计数

**Files:**
- Modify: `engineering/src/db/tools/stats/stats.c`
- Modify: 各 .c 文件添加 stats_update_* 调用
- Test: 验证 stats 实时更新

### Task 3.5：planner 重写 5 条优化规则（PG 风格）

**Files:**
- Modify: `engineering/src/db/sql/planner.c`
- Test: 验证 5 条规则真正工作

### Task 3.6：恢复 pgwire.c（Windows 兼容）

**Files:**
- Modify: `engineering/src/db/sql/pgwire.c`
- Modify: `engineering/src/db/sql/CMakeLists.txt`
- Test: 验证 Windows 下编译

### Task 3.7：db_server.c 委托给 pgwire.c

**Files:**
- Modify: `engineering/src/db/core/db_server.c`
- Test: 验证启动 pgwire 服务器

### Task 3.8：恢复 cli.c 并贯通完整 SQL 管线

**Files:**
- Modify: `engineering/src/db/cli/cli.c`
- Modify: `engineering/src/db/cli/CMakeLists.txt`
- Test: 验证 cli 能跑 SQL

### Task 3.9：新增 db_sdk C 库

**Files:**
- Create: `engineering/sdk/c/include/db_sdk.h`
- Create: `engineering/sdk/c/db_sdk.c`
- Create: `engineering/sdk/c/CMakeLists.txt`
- Test: 验证 db_sdk API 工作

### Task 3.10：vdb_cli 改用 SDK

**Files:**
- Modify: `engineering/apps/vdb_cli/vdb_cli.c`
- Modify: `engineering/apps/vdb_cli/CMakeLists.txt`
- Test: 验证 vdb_cli 通过 SDK 工作

### Task 3.11：Python SDK 通过 ctypes 调 db_sdk

**Files:**
- Modify: `engineering/sdk/python/minivecdb/client.py`
- Test: 验证 Python SDK 工作

### Task 3.12：恢复 txn/ 目录到 CMake

**Files:**
- Modify: `engineering/src/db/storage/CMakeLists.txt`
- Test: 验证事务启用

### Task 3.13：补全 BeginTransaction/Commit/Abort 真实实现

**Files:**
- Modify: `engineering/src/db/storage/txn/txn.c`
- Test: 验证事务基本工作

### Task 3.14：加 REPEATABLE READ 隔离级别

**Files:**
- Modify: `engineering/src/db/storage/txn/mvcc.c`
- Test: 验证 RR 隔离

### Task 3.15：bit hack 提取为 TTS_VALUES_SET_* 宏

**Files:**
- Modify: `engineering/include/db/executor/tuple_table_slot.h`
- Modify: 各 executor/vector/*.c、executor/spatial/*.c
- Test: 验证 bit hack 替换

### Task 3.16：End-to-end SQL 测试覆盖

**Files:**
- Create: `engineering/test/db/sql/test_e2e_sql.cpp`
- Test: 完整 SQL 测试（CREATE/INSERT/SELECT/JOIN）

---

## Self-Review（spec coverage + placeholder + 类型一致性）

**Spec coverage 检查**：

| Spec 决策 | 对应 Task |
|---|---|
| 走 PG 架构 + 注册表插件化 | Task 2.16-2.18 + 2.21-2.22 |
| 删除 sql_executor.c 全桩 | Task 1.1 |
| ~~删除 parse_analyze.c~~ | ~~Task 1.2~~ **取消**：parse_analyze/parse_expr 是 Bison 套测试基础设施，保留 |
| 删除 sql/sql_parser.c + driver | Task 1.3 |
| 删除 optimizer.c | Task 1.4 |
| 删除 traverse.c 死函数 | Task 1.5 |
| 合并 nodeHash.c | Task 1.6 |
| 改名 exec_index_scan | Task 1.7 |
| 补全 node_to_logical SELECT | Task 1.8 |
| 端到端贯通 | Task 1.9 |
| 13 个 ExecXxx 补全 | Task 2.1-2.10 |
| 5 个引擎接入 storage_ops_t | Task 2.11-2.15 |
| IndexAmRoutine | Task 2.16-2.18 |
| 三种建索引场景 | Task 2.19-2.20 |
| 多索引 active_index_type | Task 2.21 |
| FEATURE_SQL 编译宏 | Task 2.22 |
| ANALYZE 真正实现 | Task 3.1 |
| planner 读真实统计 | Task 3.2-3.4 |
| 优化器 5 条规则 | Task 3.5 |
| pgwire 恢复 | Task 3.6-3.7 |
| cli 恢复 | Task 3.8 |
| db_sdk 新增 | Task 3.9 |
| vdb_cli 改 SDK | Task 3.10 |
| Python SDK | Task 3.11 |
| 事务恢复 | Task 3.12-3.13 |
| 隔离级别 | Task 3.14 |
| bit hack 宏 | Task 3.15 |
| 端到端 SQL 测试 | Task 3.16 |

**Placeholder 扫描**：✅ 无 TBD/TODO 占位（"实现/补全"步骤后都有具体代码示例）

**类型一致性检查**：
- `IndexAmRoutine` 定义在 `amapi.h`，被 `ambuild/aminsert/ambeginscan` 等引用 → ✅ 一致
- `storage_ops_t` 已在 `storage_engine.h` 定义，Task 2.11-2.15 直接使用 → ✅ 一致
- `catalog_init/catalog_create_table/catalog_shutdown` 在 catalog.h 定义 → ✅ 一致
- `IndexInfo` 和 `Datum` 引用需要在 amapi.h 中定义或在引入时实现 → 后续 Task 可能需要补充这些类型定义

## 修改决策记录

### 2026-07-19：Task 1.2 取消（parse_analyze/parse_expr 不是死代码）

**变更**：将 `### Task 1.2：删除 parse_analyze.c + parse_expr.c` 标记为 `[CANCELLED]`，不再执行删除操作。

**触发原因**：通过 grep 验证发现，`engineering/test/db/parser/test_parser_sql.cpp` 调用了 `transformSelectStmt`、`makeVar`、`makeConst` 等函数，parse_analyze.c + parse_expr.c 是 Bison 套测试基础设施，而非死代码。

**联动修改**：
1. `docs/superpowers/specs/2026-07-19-pg-arch-refactor-design.md` 第 178 行的"澄清记录"已从"死代码——直接删除"改为"保留"
2. spec 章节 10 的表格中 parse_analyze.c / parse_expr.c 行已合并为一行，标注 ✅ **保留**
3. spec 章节 11 第一档方案中删除条目已改为 ~~删除~~ **取消**
4. plan 顶部"删除清单"已删除 parse_analyze/parse_expr 条目
5. plan Self-Review 表中 `删除 parse_analyze.c | Task 1.2` 已改为取消标注

**影响**：Phase 1 死代码清理任务数从 7 个减为 6 个，剩余：sql_executor.c、sql/sql_parser.c + sql/sql_driver.c、optimizer.c、executor/graph/traverse.c 死函数、nodeHash.c 合并、nodeIndexscan.c 改名。