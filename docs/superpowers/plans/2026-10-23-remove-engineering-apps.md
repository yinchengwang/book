# engineering/apps 整目录删除 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `engineering/apps/` 从仓库中彻底删除，同时下线只服务于它的 4 个向量持久化通用接口，保持引擎既有持久化能力与全量构建测试通过。

**Architecture:** 采用「先解耦、再删测试、最后删目录」的三段式顺序，保证每一步结束时构建都是绿的。核心动作是移除 `vector_engine.c` 中对 `hnsw_persist_*` 的 5 处外部引用（该实现位于待删的 `apps/multimodal_rag/`），使 `vector_persist` 库不再编译 apps 源码。引擎真正的持久化路径 `vector_engine_save_index/load_index` 自包含，不受影响。

**Tech Stack:** C11 / C++17、CMake 3.x、GoogleTest（vendored）、Git

**Spec:** `docs/superpowers/specs/2026-10-23-remove-engineering-apps-design.md`

## Global Constraints

- 构建目录统一用 `build/engineering`（已配置，增量构建；重配约 15–17 秒）
- 每个验证步骤必须同时满足：`Configuring done` + 目标构建成功 + **target 清单差集可解释**；只验退出码不算通过
- **删 API 而非 stub**：让遗漏调用点在编译期暴露，而非运行时静默失败
- **必须保留** 后缀版独立实现：`vector_index_save_hnsw` / `vector_index_load_hnsw` / `vector_index_save_diskann` / `vector_index_load_diskann` / `vector_index_save_ivf` / `vector_index_load_ivf`（位于 `src/db/storage/vector/vector_index_persist.c`，直接用 `faiss_hnsw`，不依赖 apps）
- 每个 Task 一个 commit，message 前缀 `refactor(apps):`
- 不修改 `docs/superpowers/plans/2026-10-22-*` 的历史正文，只追加回填区块

## Review Focus

- **遗漏的 `hnsw_persist_*` 调用点** —— 该文件用 `extern` 声明而非 include 头文件，头文件删除不会报错，必须靠 grep 全量核对；遗漏会导致链接期 `undefined reference`
- **误删后缀版 persist 函数** —— 若把 `vector_index_save_hnsw` 等一并删除，向量索引持久化能力会真的丢失（这正是方案 C 要避免的）
- **target 静默消失** —— 上次教训：构建"不报错"但少了 31 个 target。必须做前后差集，48 个 apps 相关 target 消失、其余一个不少
- **`test/apps` 删除后的残留引用** —— `apps_all_tests` 自定义目标若残留，`cmake` 配置期会报错
- **文档遗留 `engineering/apps` 有效引用** —— 除历史计划与迁移说明外，grep 应为空

---

### Task 1: 下线 vector 通用持久化接口，断开对 apps 的编译依赖

**Files:**
- Modify: `engineering/src/db/storage/vector/CMakeLists.txt:15-17`（源列表）、`:22-26`（include 目录）
- Modify: `engineering/src/db/storage/vector/vector_engine.c:186-206`（extern 块）、`:2583-2713`（4 个函数，至文件尾）
- Modify: `engineering/include/db/vector_engine.h:187-225`（4 个声明及注释）
- Modify: `engineering/include/db/index/vector_index/persist/vector_index_persist.h:298-303`（`vector_index_is_valid` 声明）

**Interfaces:**
- Consumes: 无（首个任务）
- Produces: `vector_persist` 库不再编译 `apps/` 下任何源码；此后 `engineering/src/` 与 `engineering/include/` 中不存在 `hnsw_persist` 符号。`vector_engine_save_index/load_index`、`vector_index_save_hnsw/load_hnsw` 等保持原样

- [ ] **Step 1: 记录删除前的 target 基线**

```bash
cd /c/code/book
cmake --build build/engineering --target help > /tmp/targets-before.txt 2>&1
wc -l /tmp/targets-before.txt
grep -cE "apps_|games_|vdb_cli|todo-app|multimodal|test_common" /tmp/targets-before.txt
```

Expected: 文件已生成；apps 相关计数为 **48**

- [ ] **Step 2: 从 `vector_persist` 移除 apps 源码与 include**

修改 `engineering/src/db/storage/vector/CMakeLists.txt`：

```cmake
set(VECTOR_PERSIST_SOURCES
    vector_page.c
    vector_segment.c
    vector_index_persist.c
    vector_wal.c
    checkpoint_manager.c  # P1-B Task 7：后台 CheckpointManager
    vector_buf.c
    vector_engine.c
    vec_page.c
)

target_include_directories(vector_persist PUBLIC
    ${ENGINEERING_SOURCE_DIR}/include
    ${ENGINEERING_SOURCE_DIR}/src
)
```

即：删掉 `hnsw_persist.c` 那一行源文件与 `apps/multimodal_rag/include` 那一行 include（连同其上方 `# 直接编译 hnsw_persist.c（从 multimodal_rag 模块）` 注释）。`checkpoint_manager.c` 的行尾注释保留。

- [ ] **Step 3: 删除 `vector_engine.c` 中的 extern 声明与类型定义**

删除 `engineering/src/db/storage/vector/vector_engine.c` 的 L186–206，即：

```c
/* HNSW 持久化外部函数声明（来自 rag 模块） */
extern int hnsw_persist_save(void *hnsw, const char *path, void *result);
extern void *hnsw_persist_load(const char *path, void *result);
extern int hnsw_persist_is_valid(const char *path);
extern int hnsw_persist_get_meta(const char *path, void *meta);
extern void hnsw_persist_stats(void *hnsw, int32_t *total, int32_t *deleted, uint64_t *mem);

/* HNSW 持久化结果类型 */
typedef struct {
    int error_code;
    char error_msg[256];
    uint64_t bytes_written;
    uint64_t bytes_read;
} hnsw_persist_result_t;

/* HNSW 持久化元数据类型 */
typedef struct {
    int32_t dims;
    int32_t n_total;
    int32_t metric;
} hnsw_persist_meta_t;
```

保留其后的 `#define VECTOR_ENGINE_NAME ...`。

- [ ] **Step 4: 删除 `vector_engine.c` 末尾的 4 个持久化函数**

删除从 L2583 的分节注释起到文件末尾（L2713）的全部内容：

```c
/* ========================================================================
 * 持久化接口实现
 * ======================================================================== */

int vector_index_save(void *rel, const char *path, vector_persist_result_t *result) { ... }
int vector_index_load(void *rel, const char *path, vector_persist_result_t *result) { ... }
bool vector_index_is_valid(const char *path) { ... }
int vector_index_get_meta(const char *path, int32_t *dims, int32_t *n_total, int32_t *metric) { ... }
```

删除后文件应以 `return 0;\n}` 结尾（即 L2581 处 `vector_engine_*` 某个函数结束）。

- [ ] **Step 5: 删除 `include/db/vector_engine.h` 中的 4 个声明**

删除 L187–225（从 `vector_index_save` 的 `/**` 注释起，到 `int vector_index_get_meta(...);` 及其后空行），保留 L226 起的 `/* ==== 工具函数 ==== */` 分节。

- [ ] **Step 6: 删除 `vector_index_persist.h` 中重复的 `vector_index_is_valid` 声明**

删除 `engineering/include/db/index/vector_index/persist/vector_index_persist.h` 的 L298–303：

```c
/**
 * @brief 检查索引文件是否有效
 * @param path 文件路径
 * @return 有效返回 true
 */
bool vector_index_is_valid(const char *path);
```

- [ ] **Step 7: 验证源码中已无 hnsw_persist 残留**

```bash
cd /c/code/book/engineering
grep -rn "hnsw_persist" --include="*.c" --include="*.h" --include="*.cpp" src include
```

Expected: 无输出（退出码 1）。若有输出，逐个确认是注释还是真实引用，真实引用必须处理。

- [ ] **Step 8: 验证 `vector_persist` 仍可配置与构建**

```bash
cd /c/code/book
cmake -S engineering -B build/engineering 2>&1 | tail -5
cmake --build build/engineering --target vector_persist --parallel 4 2>&1 | tail -5
```

Expected: `Configuring done` + `Generating done`；构建输出 `[100%] Built target vector_persist`（或 `libvector_persist.a`），无 `undefined reference`

- [ ] **Step 9: Commit**

```bash
cd /c/code/book
git add engineering/src/db/storage/vector/CMakeLists.txt \
        engineering/src/db/storage/vector/vector_engine.c \
        engineering/include/db/vector_engine.h \
        engineering/include/db/index/vector_index/persist/vector_index_persist.h
git commit -m "refactor(apps): 下线 vector 通用持久化接口，断开对 apps 的编译依赖"
```

---

### Task 2: 删除 apps 相关测试

**Files:**
- Delete: `engineering/test/apps/`（整目录：5 个 `.cpp` + `CMakeLists.txt`）
- Modify: `engineering/test/CMakeLists.txt:18`
- Delete: `engineering/test/db/storage/vector_index_persist_test.cpp`
- Modify: `engineering/test/db/storage/vector_persist_test.cpp:250`（过时注释）

**Interfaces:**
- Consumes: Task 1 已下线 `vector_index_save/load/is_valid/get_meta`，本任务清理引用它们的测试
- Produces: 构建目标 `test_common_terminal_test` / `test_common_menu_test` / `games_g2048_core_test` / `games_snake_core_test` / `apps_all_tests` 全部消失

- [ ] **Step 1: 删除 `test/apps` 目录**

```bash
cd /c/code/book
git rm -r engineering/test/apps
```

- [ ] **Step 2: 移除 `test/CMakeLists.txt` 中的子目录引用**

删除 `engineering/test/CMakeLists.txt` 第 18 行：

```cmake
add_subdirectory(apps)            # 独立应用测试（游戏、工具等）
```

- [ ] **Step 3: 删除引用已下线 API 的死测试文件**

```bash
cd /c/code/book
git rm engineering/test/db/storage/vector_index_persist_test.cpp
```

理由：该文件调用 `vector_index_save` / `vector_index_load` / `vector_index_is_valid`（Task 1 已删除），且**未在 CMake 中登记**，属未构建的死代码。

- [ ] **Step 4: 更新 `vector_persist_test.cpp` 的过时注释**

`engineering/test/db/storage/vector_persist_test.cpp` L249–252 原注释提到 `vector_index_is_valid() 调用 hnsw_persist_is_valid()`。将该注释改为：

```cpp
    // 注意: 完整文件校验由 vector_index_verify_header() 覆盖；
    //     hnsw_persist 相关接口已随 apps 移除而下线。
```

- [ ] **Step 5: 验证配置成功且 4 个测试目标消失**

```bash
cd /c/code/book
cmake -S engineering -B build/engineering 2>&1 | tail -5
cmake --build build/engineering --target help > /tmp/targets-mid.txt 2>&1
for t in test_common_terminal_test test_common_menu_test games_g2048_core_test games_snake_core_test apps_all_tests; do
  echo "$t: $(grep -c "$t" /tmp/targets-mid.txt)"
done
```

Expected: `Configuring done`；五个目标计数均为 **0**

- [ ] **Step 6: Commit**

```bash
cd /c/code/book
git add -A engineering/test
git commit -m "refactor(apps): 删除 apps 相关测试与已下线持久化 API 的死测试"
```

---

### Task 3: 删除 `engineering/apps` 目录与 CMake 入口

**Files:**
- Delete: `engineering/apps/`（整目录）
- Modify: `engineering/CMakeLists.txt:2`（注释）、`:153-155`（EXISTS 守卫）

**Interfaces:**
- Consumes: Task 1 与 Task 2 已移除全部对 apps 的编译期引用
- Produces: `engineering/apps/` 不存在；`engineering/CMakeLists.txt` 不再引用 apps

- [ ] **Step 1: 删除目录**

```bash
cd /c/code/book
git rm -r engineering/apps
```

- [ ] **Step 2: 移除 `engineering/CMakeLists.txt` 的 apps 入口**

删除第 153–155 行：

```cmake
if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/apps/CMakeLists.txt")
    add_subdirectory(apps)
endif()
```

- [ ] **Step 3: 更新 `engineering/CMakeLists.txt` 第 2 行注释**

将：

```cmake
# 工程轨道 CMake 入口（PG 风格存储、MiniVecDB、apps/、rag/、sdk/）
```

改为：

```cmake
# 工程轨道 CMake 入口（PG 风格存储、MiniVecDB、rag/、sdk/）
```

- [ ] **Step 4: 验证配置成功且 target 差集可解释**

```bash
cd /c/code/book
cmake -S engineering -B build/engineering 2>&1 | tail -5
cmake --build build/engineering --target help > /tmp/targets-after.txt 2>&1
diff <(sort /tmp/targets-before.txt) <(sort /tmp/targets-after.txt) | grep "^<" | wc -l
diff <(sort /tmp/targets-before.txt) <(sort /tmp/targets-after.txt) | grep "^>" | wc -l
```

Expected:
- `Configuring done` + `Generating done`
- 减少行数（`^<`）**大于 0**，且每一条都能归因到 apps（`apps_*` / `games_*` / `vdb_cli` / `todo-app` / `multimodal` / `mmrag` / `test_common_*`）
- **新增行数（`^>`）为 0** —— 若出现新增，说明配置发生了变化，必须查明原因

- [ ] **Step 5: 逐条核对差异只涉及 apps**

```bash
cd /c/code/book
diff <(sort /tmp/targets-before.txt) <(sort /tmp/targets-after.txt) | grep "^<" | grep -vE "apps_|games_|vdb_cli|todo-app|multimodal|mmrag|test_common"
```

Expected: 无输出。若有输出，逐条解释；无法解释则回滚并排查。

- [ ] **Step 6: 验证引擎持久化能力仍在（方案 C 的关键校验）**

```bash
cd /c/code/book/engineering
grep -c "vector_engine_save_index\|vector_engine_load_index" src/db/storage/vector/vector_engine.c
grep -c "vector_index_save_hnsw\|vector_index_load_hnsw" src/db/storage/vector/vector_index_persist.c
```

Expected: 两个计数均 **≥ 2**（定义 + 调用或头文件引用），确认自包含持久化路径未被误删

- [ ] **Step 7: 全量构建**

```bash
cd /c/code/book
cmake --build build/engineering --parallel 4 2>&1 | tail -15
```

Expected: 构建完成，无 `error`；若因耗时较长被中断，改用 `--target` 分批构建关键库（`vector_persist`、`db_storage`、`db_core`）

- [ ] **Step 8: Commit**

```bash
cd /c/code/book
git add -A engineering/apps engineering/CMakeLists.txt
git commit -m "refactor(apps): 删除 engineering/apps 整目录与 CMake 入口"
```

---

### Task 4: 文档同步

**Files:**
- Modify: `AGENTS.md`（删除「Knowledge Hub API Server」整节）
- Modify: `engineering/README.md`（删除 `apps/` 行）
- Modify: `CLAUDE.md:380`（新独立项目约定）
- Modify: `docs/superpowers/plans/2026-10-22-miniprogram-native-rewrite-master-plan.md`（追加回填）
- Modify: `docs/superpowers/plans/2026-10-22-miniprogram-native-rewrite-execution-plan.md`（追加回填）
- Modify: `docs/architecture/knowledge-hub/README.md`、`docs/architecture/reading-radar/README.md`（补充 apps 已删）

**Interfaces:**
- Consumes: Task 3 已删除目录，文档中的引用成为失效引用
- Produces: 除历史计划与迁移说明外，全仓无 `engineering/apps` 有效引用

- [ ] **Step 1: 删除 `AGENTS.md` 的 api-server 整节**

删除从 `## Knowledge Hub API Server` 到「参数：`-p <port>` ... 静态目录。」为止的整节（含「构建」「运行」「API 模块」三个小节）。api-server 已随 apps 删除，其构建命令与模块表均已失效。

- [ ] **Step 2: 更新 `engineering/README.md`**

删除目录表中的 `| \`apps/\` | 独立应用（...） |` 一行，并检查正文中其他 `apps` 描述。

- [ ] **Step 3: 更新根 `CLAUDE.md` 第 380 行**

将：

```markdown
- **新独立项目** → 在 `engineering/apps/` 下创建文件夹及自带 `CMakeLists.txt`
```

改为：

```markdown
- **新独立项目** → 在 `engineering/` 下创建直属目录及自带 `CMakeLists.txt`
```

- [ ] **Step 4: 追加回填到总纲与执行计划**

在两个文件末尾各追加一节「第二轮执行结果回填（2026-10-23）」：

```markdown
## 第二轮执行结果回填（2026-10-23）

`engineering/apps/` 已**整目录删除**。与首轮回填（只删 3 个 Web 源）的差异及原因：

- 首轮保留 apps 的理由是「含活跃 C++ 构建，整删破坏 CMake」
- 复核后确认：`vector_engine.c` 对 `hnsw_persist_*` 的 5 处引用中，
  通用 `vector_index_save/load/is_valid/get_meta` 的**唯一生产消费者就是 apps/multimodal_rag 自身**；
  引擎真正的持久化路径 `vector_engine_save_index/load_index` 自包含、不依赖 apps
- 因此按方案 C 删除：下线 4 个通用持久化接口 + 删 apps 测试 + 删目录
- 设计文档：`docs/superpowers/specs/2026-10-23-remove-engineering-apps-design.md`
```

- [ ] **Step 5: 补充架构文档说明**

在 `docs/architecture/knowledge-hub/README.md` 与 `docs/architecture/reading-radar/README.md` 的迁移说明块中，各追加一行：

```markdown
> 其所在目录 `engineering/apps/` 已于 2026-10-23 整目录删除。
```

- [ ] **Step 6: 全仓引用复查**

```bash
cd /c/code/book
grep -rn "engineering/apps" --include="*.md" --include="*.txt" --include="*.cmake" --include="CMakeLists.txt" . 2>/dev/null \
  | grep -vE "^\./\.git|^\./build|^\./docs/superpowers|^\./\.workbuddy-ai|^\./\.claude|^\./\.superpowers|node_modules"
```

Expected: 无输出，或仅剩「已删除 / 已迁移」类说明性语句。逐条确认后记录。

- [ ] **Step 7: Commit**

```bash
cd /c/code/book
git add AGENTS.md CLAUDE.md engineering/README.md \
        docs/superpowers/plans/2026-10-22-miniprogram-native-rewrite-master-plan.md \
        docs/superpowers/plans/2026-10-22-miniprogram-native-rewrite-execution-plan.md \
        docs/architecture/knowledge-hub/README.md \
        docs/architecture/reading-radar/README.md
git commit -m "docs(apps): 同步 apps 整目录删除，清理失效引用"
```

---

### Task 5: 最终全量验证

**Files:**
- 无文件改动（纯验证）

**Interfaces:**
- Consumes: Task 1–4 的全部产物
- Produces: 一份可核对的验收结论

- [ ] **Step 1: 干净重配**

```bash
cd /c/code/book
rm -rf /tmp/cmake-final-apps
cmake -S engineering -B /tmp/cmake-final-apps 2>&1 | tail -6
```

Expected: `Configuring done` + `Generating done`，无 `Error`、无 `apps` 相关警告

- [ ] **Step 2: target 总数对比**

```bash
cd /c/code/book
echo "before: $(wc -l < /tmp/targets-before.txt)"
cmake --build /tmp/cmake-final-apps --target help > /tmp/targets-final.txt 2>&1
echo "after:  $(wc -l < /tmp/targets-final.txt)"
```

Expected: 后者小于前者，差额与 Task 3 Step 4 记录的 apps 目标减少数一致

- [ ] **Step 3: 关键库逐个构建**

```bash
cd /c/code/book
for t in vector_persist db_core db_storage algo-prod; do
  echo "=== $t ==="
  cmake --build /tmp/cmake-final-apps --target $t --parallel 4 2>&1 | tail -3
done
```

Expected: 每个都 `Built target <t>`，无 `undefined reference`

- [ ] **Step 4: 运行既有测试**

```bash
cd /tmp/cmake-final-apps
ctest --output-on-failure 2>&1 | tail -20
```

Expected: 无**新增**失败。若存在与 apps 无关的既有失败，记录其名称并说明为既有问题。

- [ ] **Step 5: 确认目录已消失**

```bash
cd /c/code/book
test -d engineering/apps && echo "仍存在（失败）" || echo "已删除 ✓"
git status --short engineering/ | head
```

Expected: `已删除 ✓`；`git status` 中 engineering 下无未提交改动

---

## 完成定义

1. `engineering/apps/` 不存在，`engineering/CMakeLists.txt` 无 apps 引用
2. `cmake -S engineering -B <新目录>` 干净通过，`Generating done`
3. `vector_persist` / `db_core` / `db_storage` 构建通过，无 `undefined reference`
4. target 差集仅包含 apps 相关目标，无意外新增
5. `vector_engine_save_index/load_index` 与 `vector_index_save_hnsw/load_hnsw` 均保留
6. 全仓无 `engineering/apps` 有效引用（历史计划与迁移说明除外）
