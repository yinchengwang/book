# 按模态独立构建 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `engine/` 从「单体静态库 + 14 个近乎无效的开关」改成「每个模态一个独立静态库 + 单一 `MMDB_MODALITY` profile 入口」，并用每模态一个冒烟可执行文件机械地证明该模态单独可用。

**Architecture:** 四层依赖（L0 `mm_base` → L1 存储设施 → L2 模态引擎 → L3 查询/服务），只许向下。去环靠「注册表反转」：公共层不再 include 任何引擎私有头，改为每个引擎导出一个 `mm_<modality>_register()`，由构建期按 profile 生成的 `mm_builtin_engines.c` 集中调用。profile 在配置期做闭包校验，非法组合直接 `FATAL_ERROR`。

**Tech Stack:** C11 / CMake ≥ 3.20 / Ninja / gtest / MinGW（Windows）+ GCC（Linux）

**Spec:** `docs/superpowers/specs/2026-10-27-per-modality-build-isolation-design.md`

## Global Constraints

- **语言标准**：C11（`CMAKE_C_STANDARD 11`，`C_STANDARD_REQUIRED ON`）。不改。
- **不新增第三方依赖**。可用：gtest、uthash、cjson、libmicrohttpd（均已在仓内）。
- **双平台**：Windows（MinGW + MSVC）与 Linux 都必须能配置、构建、通过冒烟。任何只在一边验证的修改视为未完成。
- **MSVC 开了 `/W4 /WX`**：新增任何告警都会变成错误。新增代码不得引入未使用参数/未使用变量/隐式转换告警。
- **公开 API 签名不得变**：`storage_ops_t`、`vdb_api.h`、`mm_storage.h` 中已有函数签名保持原样。
- **Python 脚本只用标准库**（环境无 PyYAML）。
- **commit message 用中文**，格式沿用仓内既有：`feat(scope): …` / `refactor: …` / `fix(scope): …` / `chore: …` / `docs: …`。
- **禁止** `git submodule update --init --recursive`（会拉 25 GB）。
- **每次 commit 前** `git status --short` 必须干净；每完成一个 Task 追加一条 `## Task N: complete (commits <base7>..<head7>)` 到 `.superpowers/sdd/` 下的进度台账（若该目录不存在则跳过本条）。

## Review Focus

这五类是设计隐含、但没有哪个 Task 的测试天然覆盖的失效模式。每个 Task 的测试里已经钉住对应的一条。

1. **单模态构建里注册了不该有的引擎**——`MMDB_MODALITY=kv` 时 `mm_storage_init()` 之后 `storage_get_engine(MODEL_VECTOR)` 必须为 `NULL`。生成文件漏了 profile 过滤，这条就会静默成立。
2. **枚举空槽导致的名称/目录错位**——`DataModel` 的 10 号槽是空的，`MODEL_COUNT = 13`。任何用位置式初始化或 `for (i < MODEL_COUNT)` 全量遍历的新代码都会踩到。
3. **手动 `MMDB_ENABLE_*` 与 profile 混用静默绕过闭包校验**——用户同时给 `-DMMDB_MODALITY=kv -DMMDB_ENABLE_VECTOR=ON` 时必须报错，而不是悄悄打开 vector。
4. **测试静默不跑**——`full` 与 `legacy` 的 `ctest -N` 用例数必须相等（新增 doc/ts 等模态的测试除外，需逐条列出）。重排测试最容易把某些目标从 ctest 里漏掉。
5. **`st`/`rdf` 的 Windows 修复在 Linux 上引入回归**——这两个模态的错误几乎全是 Windows 特有（`mkdir` 两参、`WIN32_FIND_DATAW`）。改完必须两边各跑一次语法检查。

---

## 文件结构

**新建**

| 路径 | 职责 |
|---|---|
| `scripts/measure_build.py` | 统计给定构建目录的编译单元数、静态库数量与体积、可执行文件数量与体积 |
| `scripts/check_layering.py` | 校验 `src/db/<L0/L1>` 不 include 任何模态私有头（去环门禁） |
| `cmake/ModalityProfiles.cmake` | profile 定义 + 闭包校验 + `MMDB_ENABLE_*` 推导 |
| `cmake/gen_builtin_engines.cmake` | 按 profile 生成 `mm_builtin_engines.c` |
| `cmake/mm_builtin_engines.c.in` | 生成模板 |
| `include/db/mm_engine_register.h` | `mm_register_builtin_engines()` / `mm_register_builtin_sql_functions()` 声明 |
| `src/db/storage/mm/mm_builtin_engines_guard.c` | 空 profile 的兜底（仅当 profile 为空时使用） |
| `test/smoke/mm_<modality>_smoke.c` | 每模态一个，链接期独立性证明 |
| `docs/arch/db/modality-build.md` | 依赖白名单、新增同层边登记、实测基线数字 |
| `docs/arch/db/build-baseline-2026-10-27.md` | 改造前的 ctest 失败用例名集合 |

**主要修改**

| 路径 | 改什么 |
|---|---|
| `src/db/core/engine_registry.c` | 拆成每引擎一个 `mm_<modality>_register()`，`engine_registry_init` 改为调用生成的 `mm_register_builtin_engines()` |
| `src/db/core/func_registry.c` | 按模态拆出注册函数，移除对 5 个引擎私有头的 include |
| `src/db/CMakeLists.txt` | 去掉 `db_core ↔ db_storage` 循环链接；各子目录改为按 profile 条件引入 |
| `src/db/storage/CMakeLists.txt` | 每个模态一个独立 `STATIC` 库；删掉 `IMPORTED` 兜底分支 |
| `src/db/index/CMakeLists.txt` | `vector_index` 挂到 VECTOR 下 |
| `cmake/ProjectUtils.cmake` | `add_project_test` 支持 `MM_TEST_MODALITY`，只链该模态的库 |

---

## Task 0: 构建测量工具与基线快照

一切改动的判据都建立在「改造前是什么样」上，所以先把它量出来。这个 Task 不改任何产品代码。

**Files:**
- Create: `scripts/measure_build.py`
- Create: `docs/arch/db/build-baseline-2026-10-27.md`

**Interfaces:**
- Consumes: 无
- Produces: `measure_build.py <build_dir>` → stdout 输出 JSON，字段
  `build_dir` / `compile_units` / `static_libs` / `static_lib_bytes` / `executables` / `executable_bytes`。
  后续每个 Task 的验收都用它。

- [ ] **Step 1: 写 `scripts/measure_build.py`**

统计口径（写进脚本 docstring，后续所有数字都按这个口径）：
- `compile_units`：优先解析 `<build_dir>/build.ninja` 中形如 `build …/x.o: C_COMPILER …` 的构建行并计数；
  没有 `build.ninja` 时退回统计磁盘上 `*.o` 与 `*.obj` 的数量。
- `static_libs` / `static_lib_bytes`：递归找 `*.a` 与 `*.lib`。
- `executables` / `executable_bytes`：递归找 `*.exe`，以及 Unix 上 `os.access(p, os.X_OK)` 且非 `.a/.so/.o` 的文件。
- 只用标准库（`os`、`sys`、`json`、`re`）。

- [ ] **Step 2: 在现有构建树上验证脚本**

Run:
```bash
python scripts/measure_build.py ../build/verify-debt
```
Expected: JSON 中 `static_libs == 101`、`executables == 165`。
（这是已知事实，用来证明脚本口径正确。`compile_units` 该树为 0，因为产物已清理，属正常。）

- [ ] **Step 3: 配置并构建改造前的基线**

Run:
```bash
cmake -B build/base -S . -G Ninja -DBUILD_TESTING=ON
ninja -C build/base -k 0
```
Expected: 构建完成，失败目标数为 0（该仓当前状态是「全新构建零失败」，只有测试会失败）。

- [ ] **Step 4: 记录编译单元基线**

Run: `python scripts/measure_build.py build/base`
Expected: `compile_units` 落在 440–500 区间。把实际数字记进 `docs/arch/db/modality-build.md` 的「改造前基线」表。

- [ ] **Step 5: 采集 ctest 失败用例名集合（跑两次取交集）**

Run:
```bash
ctest --test-dir build/base -j8 --output-junit base-run1.xml
ctest --test-dir build/base -j8 --output-junit base-run2.xml
```
Expected: 两次的失败数不同（该套件噪声大，历史观测 61–68）。把两次**都失败**的用例名写入
`docs/arch/db/build-baseline-2026-10-27.md` 的「稳定失败」表；只失败一次的写入「flaky」表。
文档里必须写明：**比较的是失败用例名集合，不是失败总数**。

- [ ] **Step 6: Commit**

```bash
git add scripts/measure_build.py docs/arch/db/modality-build.md docs/arch/db/build-baseline-2026-10-27.md
git commit -m "chore(build): 新增构建产物测量脚本与改造前基线快照"
```

---

## Task 1: 搬迁两个越界文件

纯移动 + 改 include，不改逻辑。目的是把 `core/` 里两个指向模态的 include 拔掉。

**Files:**
- Move: `src/db/core/vector_query.c` → `src/db/storage/vector/vector_query.c`
- Move: `src/db/core/vector_exec.c` → `src/db/vectorized/vector_exec.c`
- Modify: `src/db/core/CMakeLists.txt`（移除这两个源文件）
- Modify: `src/db/storage/vector/CMakeLists.txt`（加入 `vector_query.c`）
- Modify: `src/db/vectorized/CMakeLists.txt`（加入 `vector_exec.c`）

**Interfaces:**
- Consumes: 无
- Produces: `core/` 不再 include `db/index/vector_index/*` 与 `db/sql/sql_executor.h`。
  `vector_exec.c` 归入 `vectorized` 后，其导出的 `vector_block_*` / `vector_batch_*` 符号位置不变，
  `executor/{framework,operators,parallel}` 的 10+ 个调用点无需改动。

- [ ] **Step 1: 用 `git mv` 移动（保留历史）**

```bash
git mv src/db/core/vector_query.c src/db/storage/vector/vector_query.c
git mv src/db/core/vector_exec.c src/db/vectorized/vector_exec.c
```

- [ ] **Step 2: 修正两个文件里的相对 include**

`vector_exec.c` 原来在 `src/db/core/`，搬到 `src/db/vectorized/` 后，任何 `#include "xxx.h"`（同目录相对）
都可能失效。逐个改成 `#include "db/..."` 全路径形式。`vector_query.c` 同理。

- [ ] **Step 3: 更新三个 CMakeLists**

- `src/db/core/CMakeLists.txt`：删掉 `vector_query.c` 与 `vector_exec.c` 两行。
- `src/db/storage/vector/CMakeLists.txt`：`VECTOR_PERSIST_SOURCES` 里加 `vector_query.c`。
- `src/db/vectorized/CMakeLists.txt`：源文件列表加 `vector_exec.c`。

- [ ] **Step 4: 验证 core 的两个越界 include 已消失**

Run:
```bash
grep -rn 'index/vector_index\|db/sql/' src/db/core/
```
Expected: 无输出。

- [ ] **Step 5: 全量构建**

Run: `cmake -B build/t1 -S . -G Ninja -DBUILD_TESTING=ON && ninja -C build/t1 -k 0`
Expected: 0 个失败目标。若出现 `undefined reference to vector_block_create` 之类，说明 Step 3 的
CMakeLists 没加对，回到 Step 3。

- [ ] **Step 6: 编译单元数不增加**

Run: `python scripts/measure_build.py build/t1`
Expected: `compile_units` 与基线相同（只搬家，不增不减）。

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "refactor(core): 把 vector_query.c 与 vector_exec.c 迁出 core"
```

---

## Task 2: 注册表反转

本 Task 是整条链的关键：让 `core` 不再直接引用任何引擎符号，改为「引擎向注册表登记」。

**Files:**
- Create: `include/db/mm_engine_register.h`
- Create: `cmake/mm_builtin_engines.c.in`
- Create: `cmake/gen_builtin_engines.cmake`
- Modify: `src/db/core/engine_registry.c`
- Modify: `src/db/storage/mm/mm_storage.c`
- Modify: `src/db/CMakeLists.txt`（引入生成逻辑与 `mm_builtin_engines` 目标）

**Interfaces:**
- Consumes: 各引擎已有的 `const storage_ops_t *<modality>_engine_get_ops(void)`
- Produces:
  - `void mm_register_builtin_engines(void)` —— 由生成文件定义，**每次构建都存在**
  - `int mm_<modality>_register(void)` —— 每个启用的模态一个，成功返回 0
  - `int engine_registry_init(void)` 行为不变（返回已注册引擎数）

- [ ] **Step 1: 新建 `include/db/mm_engine_register.h`**

```c
#ifndef DB_MM_ENGINE_REGISTER_H
#define DB_MM_ENGINE_REGISTER_H
#ifdef __cplusplus
extern "C" {
#endif

/* 由构建期按 MMDB_MODALITY profile 生成的 mm_builtin_engines.c 提供。
 * 每次构建都产出定义（空 profile 产出空函数体），因此不需要弱符号。 */
void mm_register_builtin_engines(void);
void mm_register_builtin_sql_functions(void);

#ifdef __cplusplus
}
#endif
#endif
```

- [ ] **Step 2: 新建生成模板 `cmake/mm_builtin_engines.c.in`**

模板里只有一个占位符 `@MMDB_ENGINE_REGISTRATION_BODY@`，以及两个函数骨架：
`mm_register_builtin_engines()` 调用 `mm_<modality>_register()`；
`mm_register_builtin_sql_functions()` 调用 `mm_<modality>_register_sql_functions()`（Task 3 才产出，本 Task 先留空）。

- [ ] **Step 3: 新建 `cmake/gen_builtin_engines.cmake`**

提供一个函数 `mmdb_generate_builtin_engines()`：
遍历当前 `MMDB_ENABLE_*` 为 ON 的模态，拼出调用语句，`configure_file` 到
`${CMAKE_BINARY_DIR}/generated/src/mm_builtin_engines.c`，并
`add_library(mm_builtin_engines STATIC <生成的文件>)`，
`target_link_libraries(mm_builtin_engines PUBLIC mm_storage_api)`。

**这个库不能放进任何 `if()` 分支**——空 profile 也必须产出（产出空函数体），
否则 `mm_storage_init` 链接失败。

- [ ] **Step 4: 改 `engine_registry.c`**

把现有的 11 个 `#ifdef MMDB_ENABLE_*` 注册块，逐个改成一个独立函数，放在文件末尾：

```c
#ifdef MMDB_ENABLE_KV
int mm_kv_register(void) {
    const storage_ops_t *ops = kv_engine_get_ops();
    return (ops && register_storage_engine(MODEL_KV, ops) == 0) ? 0 : -1;
}
#endif
```

`engine_registry_init()` 的函数体缩成：
```c
int engine_registry_init(void) {
    mm_register_builtin_engines();
    g_registered_count = /* 遍历 g_engines 统计非 NULL 数量 */;
    return g_registered_count;
}
```
**统计方式必须遍历 `0..MODEL_COUNT-1` 且跳过 10 号空槽**——不要用计数器累加，
否则与 `storage_register_engine` 的幂等语义冲突（重复注册返回 0 但不新增）。

- [ ] **Step 5: 在 `mm_storage.c` 中补 `#include "db/mm_engine_register.h"`**

`mm_storage_init` 现在通过 `engine_registry_init()` 间接调用生成的注册函数，本身不用改逻辑。

- [ ] **Step 6: 写失败测试——单模态构建只注册一个引擎**

Create: `test/db/core/engine_register_isolation_test.cpp`

```cpp
#include <gtest/gtest.h>
extern "C" {
#include "db/mm_storage.h"
#include "db/storage_engine.h"
}

TEST(EngineRegisterIsolation, OnlyEnabledModalitiesRegistered) {
    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m) {
        if (storage_get_engine((DataModel)m) != nullptr) ++n;
    }
    /* 全量构建下应等于 profile 声明的模态数；kv profile 下应为 1。
     * 由 CMake 通过 MMDB_EXPECTED_ENGINE_COUNT 注入期望值。 */
    EXPECT_EQ(n, MMDB_EXPECTED_ENGINE_COUNT);
}

TEST(EngineRegisterIsolation, EmptySlotIsNotAModel) {
    /* 10 号槽是保留空槽：storage_model_name 对它返回 "unknown"，
     * 而不是 NULL —— 该函数对越界返回 "unknown"，对空槽同样返回 "unknown"。 */
    EXPECT_STREQ(storage_model_name((DataModel)10), "unknown");
}
```

在 `test/db/core/CMakeLists.txt` 里给这个目标注入期望值。**不能**依赖 Task 5 的
`MMDB_ENABLED_MODALITY_COUNT`（那时还没定义），就地算：

```cmake
# 只有实现 storage_ops_t 的模态才计入注册数。
# RELATIONAL 不走 storage_ops_t；SPARSE 明确不实现（见 engine_registry.c 注释）。
set(MMDB_REGISTERING_MODALITIES KV VECTOR GRAPH DOCUMENT TIMESERIES
                                SPATIAL TREE STREAM COLUMNAR ST RDF)
set(_expected 0)
foreach(_m ${MMDB_REGISTERING_MODALITIES})
    if(MMDB_ENABLE_${_m})
        math(EXPR _expected "${_expected}+1")
    endif()
endforeach()
target_compile_definitions(engine_register_isolation_test
    PRIVATE MMDB_EXPECTED_ENGINE_COUNT=${_expected})
```

- [ ] **Step 7: 运行测试确认失败**

Run: `ninja -C build/t2 engine_register_isolation_test && ./build/t2/engine_register_isolation_test`
Expected: FAIL —— 生成逻辑还没接上，或 `mm_register_builtin_engines` 未定义（链接错误）。

- [ ] **Step 8: 接上生成逻辑并重新构建**

Run: `cmake -B build/t2 -S . -G Ninja -DBUILD_TESTING=ON && ninja -C build/t2 -k 0`
Expected: 0 失败目标。

- [ ] **Step 9: 运行测试确认通过**

Run: `./build/t2/engine_register_isolation_test`
Expected: PASS，2 个用例。

- [ ] **Step 10: 去环门禁**

Run: `grep -rn 'storage/\(kv\|vector\|graph\|doc\|ts\|spatial\|yang\)/\|index/vector_index\|db/sql/' src/db/core/`
Expected: 只剩 `func_registry.c` 的 4 行（Task 3 处理）。

- [ ] **Step 11: Commit**

```bash
git add -A
git commit -m "refactor(core): 注册表反转为按 profile 生成的内置引擎注册"
```

---

## Task 3: 拆分 `func_registry.c`

**Files:**
- Modify: `src/db/core/func_registry.c`（405 行，移除 5 个引擎私有头的 include）
- Create: `src/db/storage/doc/doc_sql_functions_register.c`
- Create: `src/db/storage/ts/ts_sql_functions_register.c`
- Create: `src/db/graph/graph_sql_functions_register.c`
- Create: `src/db/storage/spatial/spatial_sql_functions_register.c`
- Create: `src/db/storage/yang/yang_sql_functions_register.c`
- Modify: `cmake/gen_builtin_engines.cmake`（补 `mm_register_builtin_sql_functions` 的函数体生成）

**Interfaces:**
- Consumes: `mm_register_builtin_sql_functions(void)`（Task 2 已声明）
- Produces: `int mm_<modality>_register_sql_functions(void)`，每个模态一个；
  失败返回 -1，成功返回 0。注册顺序必须与改造前一致。

- [ ] **Step 1: 先记录当前的注册顺序**

在 `func_registry.c` 里找到现有的注册调用序列，把顺序抄进本 Task 的 commit message 或
`docs/arch/db/modality-build.md`。**顺序必须保留**——某些函数可能依赖先注册的辅助函数。

- [ ] **Step 2: 写失败测试——所有内置函数仍可见**

Create: `test/db/core/sql_function_registry_test.cpp`

```cpp
TEST(SqlFunctionRegistry, BuiltinFunctionsVisibleAfterInit) {
    ASSERT_EQ(mm_storage_init("test_data/fnreg"), 0);
    /* 改造前后都必须能查到这些函数；数量下限由 profile 决定 */
    EXPECT_NE(sql_function_lookup("now"), nullptr);
    EXPECT_NE(sql_function_lookup("upper"), nullptr);
    EXPECT_GE(sql_function_count(), MMDB_EXPECTED_FUNCTION_COUNT_MIN);
    mm_storage_shutdown();
}
```

`sql_function_lookup` / `sql_function_count` 若不存在，先用 `grep` 找到现有的查询入口并改用之；
**不要新造 API**。

- [ ] **Step 3: 运行测试确认失败**

Run: `ninja -C build/t3 sql_function_registry_test && ./build/t3/sql_function_registry_test`
Expected: FAIL —— 拆分后注册还没接上，函数查不到。

- [ ] **Step 4: 把 `func_registry.c` 里 5 段注册代码分别搬到 5 个新文件**

每段改成 `int mm_<modality>_register_sql_functions(void)`，内部保留原顺序。
`func_registry.c` 只留下与模态无关的部分（若有），并删掉 5 行私有头 include。

- [ ] **Step 5: 生成器补上 sql 函数注册体**

`cmake/mm_builtin_engines.cmake` 里同时拼 `mm_register_builtin_sql_functions()` 的调用列表，
顺序 = Step 1 记录的原始顺序（doc → ts → graph → spatial → yang 之外的顺序以原文件为准）。

- [ ] **Step 6: 构建并跑测试**

Run: `cmake -B build/t3 -S . -G Ninja -DBUILD_TESTING=ON && ninja -C build/t3 -k 0 && ./build/t3/sql_function_registry_test`
Expected: PASS。

- [ ] **Step 7: 去环门禁必须完全为空**

Run: `python scripts/check_layering.py src/db/core`
Expected: 输出 `OK: no modality-private includes in core`。这是本计划的里程碑——core 至此与模态彻底解耦。

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "refactor(core): func_registry 按模态拆分，core 不再依赖引擎私有头"
```

---

## Task 4: 清理 CMake 循环链接与 IMPORTED 兜底

**Files:**
- Modify: `src/db/CMakeLists.txt:46-48`
- Modify: `src/db/storage/CMakeLists.txt:233`、`:248-258`

**Interfaces:**
- Consumes: 无
- Produces: `db_storage` 单向依赖 `db_core`；无 `IMPORTED` 兜底分支。

- [ ] **Step 1: 删除 `db_core → db_storage` 的 PUBLIC 链接**

`src/db/CMakeLists.txt:46-48` 整段删除（含注释）。此时 `db_core` 已不含任何引擎引用，
删除后应仍能链接。

- [ ] **Step 2: 把 `IMPORTED` 兜底改成 `FATAL_ERROR`**

`src/db/storage/CMakeLists.txt:248-258` 的 `else()` 分支整体替换为：
```cmake
else()
    message(FATAL_ERROR "db_storage 没有任何源文件：检查 MMDB_MODALITY profile 与 storage 子目录")
endif()
```

- [ ] **Step 3: 构建验证**

Run: `cmake -B build/t4 -S . -G Ninja -DBUILD_TESTING=ON && ninja -C build/t4 -k 0`
Expected: 0 失败目标。若出现 `undefined reference`，说明 Step 1 删早了——回退 Step 1，
把 `db_core` 需要但 `db_storage` 提供的符号逐条列出（用 `nm -u`），确认后再删。

- [ ] **Step 4: 确认没有残留的循环链接**

Run: `grep -n "db_core PUBLIC db_storage\|db_storage PUBLIC db_core" src/db/CMakeLists.txt src/db/storage/CMakeLists.txt`
Expected: 无输出。

- [ ] **Step 5: Commit**

```bash
git add -A
git commit -m "refactor(build): 打断 db_core 与 db_storage 的循环链接，移除 IMPORTED 兜底"
```

---

## Task 5: `ModalityProfiles.cmake` 与闭包校验

**Files:**
- Create: `cmake/ModalityProfiles.cmake`
- Modify: `engine/CMakeLists.txt`（在 `include(MultimodalConfig.cmake)` 之后引入）
- Modify: `cmake/MultimodalConfig.cmake`（`MMDB_ENABLE_*` 改为由 profile 推导；保留手动覆盖）

**Interfaces:**
- Consumes: `MMDB_MODALITY`（cache 变量，默认 `legacy`）
- Produces:
  - `MMDB_ENABLED_MODALITY_COUNT`（整数，供 Task 2 的测试注入）
  - `MMDB_ENABLED_MODALITIES`（分号分隔列表）
  - `mmdb_profile(<name> MODALITIES <list>)` 函数

- [ ] **Step 1: 定义 profile 表**

```cmake
# 单模态 profile：每个模态一个，供冒烟与裁剪构建使用
mmdb_profile(kv         MODALITIES kv)
mmdb_profile(vector     MODALITIES vector)
mmdb_profile(graph      MODALITIES graph kv)
mmdb_profile(spatial    MODALITIES spatial)
mmdb_profile(tree       MODALITIES tree)
mmdb_profile(sparse     MODALITIES sparse vector)
mmdb_profile(doc        MODALITIES doc vector)
mmdb_profile(ts         MODALITIES ts)
mmdb_profile(stream     MODALITIES stream)
mmdb_profile(columnar   MODALITIES columnar)
mmdb_profile(st         MODALITIES st spatial)
mmdb_profile(rdf        MODALITIES rdf)
mmdb_profile(relational MODALITIES relational)

# 组合 profile
mmdb_profile(legacy     MODALITIES relational kv graph vector spatial tree sparse)
mmdb_profile(full       MODALITIES relational kv graph vector spatial tree sparse
                                  doc ts stream columnar st rdf)
```

依赖闭包（供校验用，硬编码成一张表即可，不必推导）：
`graph→kv`、`st→spatial`、`sparse→vector`、`doc→vector`。

**注意**：`tree` profile 对应 `MMDB_ENABLE_TREE`（yang 引擎），
`sparse` 对应 `MMDB_ENABLE_SPARSE`。profile 名与 `MMDB_ENABLE_*` 后缀并非一一对应，
映射表要显式写出来。

- [ ] **Step 2: 写配置期负向测试脚本**

Create: `scripts/test_profiles.sh`

```bash
#!/usr/bin/env bash
# 期望失败的配置：非法组合必须 FATAL_ERROR
expect_fail() {
  if cmake -B build/probe -S . -G Ninja "$@" >/dev/null 2>&1; then
    echo "FAIL: 本应报错却配置成功: $*"; exit 1
  fi
  rm -rf build/probe
}
expect_fail -DMMDB_MODALITY=kv -DMMDB_ENABLE_VECTOR=ON
expect_fail -DMMDB_MODALITY=nonexistent
expect_fail -DMMDB_MODALITY=graph -DMMDB_ENABLE_KV=OFF
echo "OK: 三个非法组合都被拒绝"
```

- [ ] **Step 3: 运行负向测试确认失败**

Run: `bash scripts/test_profiles.sh`
Expected: FAIL —— profile 校验还没实现，非法组合配置成功。

- [ ] **Step 4: 实现闭包校验与手动开关拦截**

在 `ModalityProfiles.cmake` 中：
1. 若 `MMDB_ALLOW_MANUAL_SWITCHES` 未设，且用户显式给了任一 `MMDB_ENABLE_*`，
   `message(FATAL_ERROR "请用 -DMMDB_MODALITY=<profile>；要手动组合请加 -DMMDB_ALLOW_MANUAL_SWITCHES=ON")`。
   判断「用户显式给了」用 `if(DEFINED CACHE{MMDB_ENABLE_KV})`——**不能**用 `if(MMDB_ENABLE_KV)`，
   因为后者恒真（option 默认值）。
2. 校验 profile 的模态集合对依赖闭包封闭，否则 `FATAL_ERROR` 并列出缺哪个。
3. 比对 `MMDB_PROFILE_APPLIED` 与本次 `MMDB_MODALITY`，不一致则 `FATAL_ERROR`
   要求换构建目录。

- [ ] **Step 5: 运行负向测试确认通过**

Run: `bash scripts/test_profiles.sh`
Expected: `OK: 三个非法组合都被拒绝`。

- [ ] **Step 6: 正向验证每个 profile 都能配置**

```bash
for p in kv vector graph relational legacy full; do
  cmake -B build/p_$p -S . -G Ninja -DMMDB_MODALITY=$p >/dev/null || { echo "FAIL $p"; exit 1; }
done
echo "OK: 全部 profile 可配置"
```

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat(build): 新增 MMDB_MODALITY profile 入口与依赖闭包校验"
```

---

## Task 6: 拆出 `mm_base` 与 `mm_storage_api`

**Files:**
- Modify: `src/db/core/CMakeLists.txt`
- Create: `src/db/storage/mm/CMakeLists.txt` 中的 `mm_storage_api` 目标（或新建 `src/db/api/CMakeLists.txt` 片段）

**Interfaces:**
- Consumes: 无
- Produces:
  - `mm_base`（L0）：`errors.c` `error_codes.c` `log.c` `guc.c` `common_rwlock.c` `mmdb_lock.c`
    `metrics.c` `structured_log.c` `sha256.c` `mm_record.c`
  - `mm_storage_api`（L1）：`engine_registry.c` `mm_storage.c` + `mm_builtin_engines`
  - `db_core` 保留其余文件（`initdb.c` `pg_ctl.c` `db_server.c` `query_filter.c` `query_fusion.c`
    `explain_analyze.c` `mvcc_session.c` `mm_storage_blob.c` `cross_model_optimizer.c`
    `hash_index.c` `xml_parser.c` `cjk_tokenizer.c` `columnar_store.c`），
    并 `target_link_libraries(db_core PUBLIC mm_storage_api)`

- [ ] **Step 1: 建 `mm_base` 目标，把 10 个文件搬进去**

`mm_base` 只依赖 `project_includes` + `multimodal_config_gen` + `Threads::Threads`。
**不得**依赖 `db_storage`、`db_core` 或任何模态。

- [ ] **Step 2: 建 `mm_storage_api` 目标**

含 `engine_registry.c` + `mm_storage.c`，`target_link_libraries(mm_storage_api PUBLIC mm_base)`。
把 Task 2 的 `mm_builtin_engines` 改为 `PUBLIC mm_storage_api` 的反向依赖——
即 `mm_storage_api` **不**链接 `mm_builtin_engines`（避免循环），
由最终可执行文件同时链接两者。

- [ ] **Step 3: 构建验证**

Run: `cmake -B build/t6 -S . -G Ninja -DBUILD_TESTING=ON && ninja -C build/t6 -k 0`
Expected: 0 失败目标。

- [ ] **Step 4: 验证 `mm_base` 的依赖纯净**

Run:
```bash
ninja -C build/t6 -t query mm_base
```
Expected: 输出中不出现 `db_storage`、`mm_vector`、`db_core`。

- [ ] **Step 5: 编译单元数不增加**

Run: `python scripts/measure_build.py build/t6`
Expected: 与 Task 0 基线相同（只是重新分组，不增删文件）。

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "refactor(build): 拆出 mm_base 与 mm_storage_api 两层"
```

---

## Task 7: kv 独立库 + 冒烟（模板任务）

这是第一个模态。**后续所有模态任务都照这个模板做**，所以这里写全，后面的任务只写差异。

**Files:**
- Modify: `src/db/storage/kv/CMakeLists.txt`
- Create: `test/smoke/mm_kv_smoke.c`
- Modify: `test/smoke/CMakeLists.txt`（新建）
- Modify: `test/CMakeLists.txt`（`add_subdirectory(smoke)`）
- Create: `cmake/mm_smoke.cmake`（`mm_smoke(<modality>)` 函数）

**Interfaces:**
- Consumes: `mm_base`、`mm_storage_api`、`mm_builtin_engines`、`mm_kv`
- Produces: `mm_kv`（STATIC）、可执行目标 `mm_kv_smoke`、CMake 函数 `mm_smoke(<name>)`

- [ ] **Step 1: 把 `storage_kv` 从 OBJECT 改成 STATIC**

`src/db/storage/kv/CMakeLists.txt`：`add_library(storage_kv OBJECT ...)` → `add_library(mm_kv STATIC ...)`，
加 `target_link_libraries(mm_kv PUBLIC mm_base)`。
`src/db/storage/CMakeLists.txt` 的 `DB_STORAGE_SUBMODULES` 里把 `kv` 换成对新库的链接。

- [ ] **Step 2: 写冒烟测试（会失败）**

Create: `test/smoke/mm_kv_smoke.c`

```c
#include "db/mm_storage.h"
#include "db/storage_engine.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    assert(mm_storage_init("test_data/smoke_kv") == 0);

    int n = 0;
    for (int m = 0; m < MODEL_COUNT; ++m)
        if (storage_get_engine((DataModel)m) != NULL) ++n;

    /* 独立性证明：kv profile 下只应有一个引擎 */
    assert(n == 1);
    assert(storage_get_engine(MODEL_KV) != NULL);
    assert(storage_get_engine(MODEL_VECTOR) == NULL);
    assert(storage_get_engine(MODEL_GRAPH) == NULL);

    assert(mm_storage_shutdown() == 0);
    printf("mm_kv_smoke OK\n");
    return 0;
}
```

- [ ] **Step 3: 建 `cmake/mm_smoke.cmake`**

`mm_smoke(<modality>)` 生成 `mm_<modality>_smoke` 可执行文件，只链
`mm_base mm_storage_api mm_builtin_engines mm_<modality>`（relational 例外，见 Task 15）。
**不得**链 `db_storage` 或任何其他模态库——链不上就说明隔离失败，这正是它的意义。

- [ ] **Step 4: 用 kv profile 配置并构建**

Run:
```bash
cmake -B build/kv -S . -G Ninja -DMMDB_MODALITY=kv
ninja -C build/kv mm_kv_smoke
```
Expected: 链接成功。

- [ ] **Step 5: 运行冒烟**

Run: `./build/kv/mm_kv_smoke`
Expected: 输出 `mm_kv_smoke OK`。

- [ ] **Step 6: 测量并记录**

Run: `python scripts/measure_build.py build/kv`
Expected: `compile_units` 显著低于基线（估算 45–60，见 spec §5.9）。
把实测值写进 `docs/arch/db/modality-build.md` 的「profile 实测」表，并设为该 profile 的门禁值。

- [ ] **Step 7: Commit**

```bash
git add -A
git commit -m "feat(build): kv 拆为独立静态库并新增独立冒烟目标"
```

---

## Task 8: vector 独立库 + 冒烟

**Files:**
- Modify: `src/db/storage/vector/CMakeLists.txt`（`vector_persist` → `mm_vector`）
- Modify: `src/db/index/CMakeLists.txt`（`add_subdirectory(vector_index)` 挂到 `MMDB_ENABLE_VECTOR` 下）
- Modify: `src/db/index/vector_index/CMakeLists.txt`（`vector_index` → `mm_vector_index`）
- Create: `test/smoke/mm_vector_smoke.c`

**Interfaces:**
- Consumes: Task 7 的 `mm_smoke()` 函数
- Produces: `mm_vector`、`mm_vector_index`、`mm_vector_smoke`

- [ ] **Step 1: 重命名两个库并声明依赖**

`mm_vector` PUBLIC 依赖：`mm_base`、`mm_vector_index`、`mm_buffer`、`mm_page`、`mm_wal`、`algo-prod`。
`mm_vector_index` PUBLIC 依赖：`mm_base`、`mm_index_heap`。

- [ ] **Step 2: 给 `vector_index` 加宏守卫**

`src/db/index/CMakeLists.txt` 中：
```cmake
if(MMDB_ENABLE_VECTOR)
    add_subdirectory(vector_index)
endif()
```

- [ ] **Step 3: 写冒烟（`test/smoke/mm_vector_smoke.c`）**

结构同 `mm_kv_smoke.c`，断言 `n == 1` 且 `storage_get_engine(MODEL_VECTOR) != NULL`、
`storage_get_engine(MODEL_KV) == NULL`。再补一条真实读写：
用 `storage_ops_t->table_create` + `tuple_insert` 建一个小集合，断言返回 0。

- [ ] **Step 4: 构建并运行**

```bash
cmake -B build/vector -S . -G Ninja -DMMDB_MODALITY=vector
ninja -C build/vector mm_vector_smoke && ./build/vector/mm_vector_smoke
```
Expected: `mm_vector_smoke OK`。

- [ ] **Step 5: 验证关闭 VECTOR 后向量索引不再编译**

```bash
cmake -B build/novec -S . -G Ninja -DMMDB_MODALITY=kv
python scripts/measure_build.py build/novec
```
Expected: `compile_units` 比 `build/vector` 少约 110（101 个 vector_index + 9 个 vector）。

- [ ] **Step 6: Commit**

```bash
git add -A
git commit -m "feat(build): vector 与 vector_index 拆为独立库，挂到 VECTOR 开关下"
```

---

## Task 9: graph 独立库 + 冒烟

**Files:**
- Modify: `src/db/storage/graph/CMakeLists.txt`
- Create: `test/smoke/mm_graph_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`、`mm_kv`（graph 白名单依赖 kv）
- Produces: `mm_graph`、`mm_graph_smoke`

- [ ] **Step 1: 重命名为 `mm_graph`，声明 `PUBLIC mm_base mm_kv`**

注意 `storage_graph` 的源文件列表里混了 `executor/graph/traverse/traverse.c` 与
`graph/graph_algorithms.c` / `graph/graph_cypher.c` 三个外部文件——一并归入 `mm_graph`，
并在 `docs/arch/db/modality-build.md` 登记这条归属。

- [ ] **Step 2: 写冒烟**（`n == 2`：GRAPH 与 KV 都在）

- [ ] **Step 3: 构建运行**
```bash
cmake -B build/graph -S . -G Ninja -DMMDB_MODALITY=graph
ninja -C build/graph mm_graph_smoke && ./build/graph/mm_graph_smoke
```
Expected: `mm_graph_smoke OK`。

- [ ] **Step 4: Commit**
```bash
git commit -am "feat(build): graph 拆为独立库（依赖 kv）并新增冒烟目标"
```

---

## Task 10: spatial / yang / sparse 独立库 + 冒烟

**Files:**
- Modify: `src/db/storage/spatial/CMakeLists.txt`、`src/db/storage/yang/CMakeLists.txt`、`src/db/storage/sparse/CMakeLists.txt`
- Create: `test/smoke/mm_spatial_smoke.c`、`mm_yang_smoke.c`、`mm_sparse_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`、`index_hilbert`
- Produces: `mm_spatial`、`mm_yang`、`mm_sparse` 及三个冒烟目标

- [ ] **Step 1: 三个库重命名并声明依赖**

`mm_spatial` PUBLIC `mm_base index_hilbert`；`mm_yang` PUBLIC `mm_base`；
`mm_sparse` PUBLIC `mm_base mm_vector`（sparse 依赖 vector，白名单边）。

- [ ] **Step 2: 三个冒烟，期望引擎数分别为 1 / 1 / 2**

`mm_sparse_smoke` 期望 2（VECTOR + SPARSE）。

- [ ] **Step 3: 逐个构建运行**

```bash
for p in spatial tree; do
  cmake -B build/$p -S . -G Ninja -DMMDB_MODALITY=$p
  ninja -C build/$p && ./build/$p/mm_*_smoke
done
```
（`tree` profile 对应 `MMDB_ENABLE_TREE`，即 yang 引擎；若 profile 名与模态名不一致，
在 `ModalityProfiles.cmake` 里加别名。）

- [ ] **Step 4: Commit**
```bash
git commit -am "feat(build): spatial/yang/sparse 拆为独立库并新增冒烟目标"
```

---

## Task 11: stream / columnar 修复并接入

**Files:**
- Modify: `src/db/storage/CMakeLists.txt:111-125`（去掉 `add_subdirectory` 的注释/条件限制）
- Modify: `src/db/storage/columnar/columnar_engine.c`
- Create: `test/smoke/mm_stream_smoke.c`、`mm_columnar_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`
- Produces: `mm_stream`、`mm_columnar`

- [ ] **Step 1: 先复现两个错误**

```bash
gcc -std=c11 -fsyntax-only -I include -I include/db -I include/db/storage \
    -I include/db/core -I ../build/verify-debt/generated/include src/db/storage/columnar/columnar_engine.c
```
Expected: `static declaration of 'columnar_serialize_table' follows non-static declaration` 等 2 个错误。
（`stream` 预期 0 错误。）

- [ ] **Step 2: 修 `columnar_engine.c` 的静态/非静态冲突**

根因是同一函数在头文件里声明为非静态、在 `.c` 里定义成静态。按头文件声明为准，
把定义改成非静态（不要反过来改头文件——头文件是外部调用方看到的契约）。

- [ ] **Step 3: 验证语法检查通过**

重跑 Step 1 的命令，Expected: 0 个 error。

- [ ] **Step 4: 接入构建 + 冒烟**

两个模态各写一个 `mm_<modality>_smoke.c`，期望引擎数均为 1。各自用自己的 profile 构建：

```bash
cmake -B build/stream -S . -G Ninja -DMMDB_MODALITY=stream
ninja -C build/stream mm_stream_smoke && ./build/stream/mm_stream_smoke

cmake -B build/columnar -S . -G Ninja -DMMDB_MODALITY=columnar
ninja -C build/columnar mm_columnar_smoke && ./build/columnar/mm_columnar_smoke
```
Expected: 两个都输出 `... OK`。

- [ ] **Step 5: Commit**
```bash
git commit -am "fix(columnar): 修正 columnar_serialize_table 的静态声明冲突并接入构建"
```

---

## Task 12: doc 修复并接入

**Files:**
- Modify: `src/db/storage/doc/doc_nested.c:189`、`:429`
- Move: `src/db/core/cjk_tokenizer.c` → `src/db/storage/doc/cjk_tokenizer.c`
- Modify: `src/db/storage/CMakeLists.txt:85-88`
- Create: `test/smoke/mm_doc_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`、`mm_vector`（doc 白名单依赖 vector 的 hnsw）
- Produces: `mm_doc`、`mm_doc_smoke`

- [ ] **Step 1: 复现错误**

```bash
gcc -std=c11 -fsyntax-only -I src/db/storage/doc -I include -I include/db \
    -I include/db/storage -I include/db/core -I include/db/index \
    -I ../build/verify-debt/generated/include src/db/storage/doc/doc_nested.c
```
Expected: 2 个 `'union <anonymous>' has no member named 'num_children'`。

- [ ] **Step 2: 修 union 成员**

`doc_nested.c:189` 把 `&node->value.num_children` 当 `int*` 出参传给 `copy_unescaped`，
`:429` 又把 `node->value.num_children` 当长度读。查 `include/db/storage/doc/*.h` 里该 union 的
实际成员，选语义正确的那个（若确实是漏了成员，在 union 里补 `int num_children;`——
**补之前先确认没有别处依赖该 union 的大小**，用 `grep -rn "num_children" src/ include/` 查全）。

- [ ] **Step 3: 验证 0 错误**

重跑 Step 1，Expected: 无 error 输出。

- [ ] **Step 4: 把 `cjk_tokenizer.c` 从 core 搬进 doc**

`git mv src/db/core/cjk_tokenizer.c src/db/storage/doc/cjk_tokenizer.c`，
从 `src/db/core/CMakeLists.txt` 移除，加入 doc 的源文件列表。
理由：中文分词只服务文档模态，留在 core 会让 kv-only 构建也编它。

- [ ] **Step 5: 接入构建 + 冒烟，期望引擎数 2（DOCUMENT + VECTOR）**

```bash
cmake -B build/doc -S . -G Ninja -DMMDB_MODALITY=doc
ninja -C build/doc mm_doc_smoke && ./build/doc/mm_doc_smoke
```
Expected: `mm_doc_smoke OK`。

- [ ] **Step 6: Commit**
```bash
git commit -am "fix(doc): 修正 doc_nested 的 union 成员错误，doc 引擎接入构建"
```

---

## Task 13: ts 修复并接入

**Files:**
- Modify: `include/db/storage/ts/ts_columnar.h`（或对应实现文件）、`include/db/storage/ts/ts_cdc.h`
- Modify: `src/db/storage/ts/CMakeLists.txt`（补 `include/db/storage/ts` 到 include 路径）
- Modify: `src/db/storage/CMakeLists.txt:90-93`
- Create: `test/smoke/mm_ts_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`
- Produces: `mm_ts`、`mm_ts_smoke`

- [ ] **Step 1: 先补 include 路径再复现**

`src/db/storage/ts/CMakeLists.txt` 的 `target_include_directories` 加上
`${ENGINEERING_SOURCE_DIR}/include/db/storage/ts`（头文件实际在这个目录，现在没加，
所以有 4 个 `No such file or directory`）。

- [ ] **Step 2: 复现剩余 22 个错误**

```bash
gcc -std=c11 -fsyntax-only -I src/db/storage/ts -I include/db/storage/ts -I include \
    -I include/db -I include/db/storage -I include/db/core -I include/db/index \
    -I third_part/uthash/src -I ../build/verify-debt/generated/include src/db/storage/ts/*.c
```

- [ ] **Step 3: 修 4 类根因**

1. `DeltaEncoder` / `BitPackingEncoder` / `TsColumnarCompressor` 缺 `capacity`（及 `raw_capacity`）字段。
   **以 `.c` 的用法为准，在头文件的结构体里补字段**——补之前用
   `grep -rn "capacity" src/db/storage/ts/` 确认字段语义（是容量还是长度），
   补错了会让 `realloc` 逻辑失效。
2. `CdcConsumer` 在两头重复定义 → 保留一份，另一份改为 `#include` 前者。
3. 逐个复跑，直到 0 错误。

- [ ] **Step 4: 接入构建 + 冒烟，期望引擎数 1**

```bash
cmake -B build/ts -S . -G Ninja -DMMDB_MODALITY=ts
ninja -C build/ts mm_ts_smoke && ./build/ts/mm_ts_smoke
```
Expected: `mm_ts_smoke OK`。

- [ ] **Step 5: Commit**
```bash
git commit -am "fix(ts): 补齐 columnar 编码器字段与 CdcConsumer 定义，ts 引擎接入构建"
```

---

## Task 14: st / rdf 修复并接入

**Files:**
- Modify: `src/db/storage/st/st_engine.c`、`src/db/storage/rdf/*.c`
- Modify: `src/db/storage/CMakeLists.txt:127-141`
- Create: `test/smoke/mm_st_smoke.c`、`mm_rdf_smoke.c`

**Interfaces:**
- Consumes: `mm_smoke()`、`mm_spatial`（st 白名单依赖 spatial）
- Produces: `mm_st`、`mm_rdf`

- [ ] **Step 1: 复现 st 的 10 个错误**

```bash
gcc -std=c11 -fsyntax-only -I src/db/storage/st -I include -I include/db \
    -I include/db/storage -I include/db/core -I include/db/index \
    -I ../build/verify-debt/generated/include src/db/storage/st/st_engine.c
```
已知根因：`mkdir` 两参调用、`bbox_t` / `point_t` 重复 typedef、`st_engine_aggregate_time` 签名不一致。

- [ ] **Step 2: 修 `mkdir`**

Windows 的 `_mkdir(path)` 只收一个参数。仓内应已有可移植封装，先查：
`grep -rn "define mkdir\|mm_mkdir\|platform_mkdir" include/ src/`。
有封装就用封装；没有就在 `include/db/common.h` 加一个 `mm_mkdir(const char *path)` 内联包装，
**不要**在业务代码里写 `#ifdef _WIN32`。

- [ ] **Step 3: 修 `bbox_t` / `point_t` 重复 typedef 与函数签名不一致**

以 `include/db/spatial_engine.h` 的声明为准（st 依赖 spatial，几何类型应只有一份定义）。

- [ ] **Step 4: 复现 rdf 的 23 个错误**

已知根因：用了 `WIN32_FIND_DATAW` / `HANDLE` / `cFileName` 但没 include `<windows.h>`；
`mkdir` 未声明。

- [ ] **Step 5: 修 rdf 的 Windows 类型**

目录遍历那段代码用了 Win32 API。两个选择：**（a）** 补 `#ifdef _WIN32 #include <windows.h>` 并
用 `opendir/readdir` 做非 Windows 分支；**（b）** 改用仓内已有的目录遍历封装（先 grep 找）。
优先 (b)——减少平台分支。

- [ ] **Step 6: 两个平台各验一次（Review Focus #5）**

Windows（当前环境）：
```bash
gcc -std=c11 -fsyntax-only ... src/db/storage/st/st_engine.c src/db/storage/rdf/*.c
```
Expected: 0 错误。
Linux：把同样的命令交给 CI 或 WSL 跑一次。**只验 Windows 视为未完成**——
这两个模态的报错几乎全是 Windows 特有，Linux 上可能本来就编得过，改动容易在 Linux 引入回归。

- [ ] **Step 7: 接入构建 + 冒烟（st 期望 2：ST+SPATIAL；rdf 期望 1）**

- [ ] **Step 8: Commit**
```bash
git commit -am "fix(st,rdf): 修正 Windows 专有类型与目录遍历，两个引擎接入构建"
```

---

## Task 15: relational 拆库 + `executor_rag` 守卫

**Files:**
- Modify: `src/db/executor/CMakeLists.txt`（拆出 `mm_executor_rag`，挂 `MMDB_ENABLE_VECTOR`）
- Modify: `src/db/CMakeLists.txt`（`sql` / `validator` / `cli` 已在 `MMDB_ENABLE_RELATIONAL` 下，确认 `parser` 的 sql 子目录同样处理）
- Create: `test/smoke/mm_relational_smoke.c`

**Interfaces:**
- Consumes: `mm_base`、`mm_storage_api`、`mm_builtin_engines`
- Produces: `mm_relational`（聚合 sql/parser/optimizer/rewrite/validator/executor/rel/access）、
  `mm_executor_rag`、`mm_relational_smoke`

- [ ] **Step 1: 拆 `db_executor`**

`src/db/executor/rag/*` 单独成 `mm_executor_rag`，`target_link_libraries(... PUBLIC mm_vector)`。
`db_executor` 不再包含 `rag/` 目录，也不再 include `vector_engine.h`。

- [ ] **Step 2: 验证 relational 不再依赖 vector**

Run: `grep -rn "storage/vector\|vector_engine.h" src/db/executor/ --include=*.c --include=*.h | grep -v "/rag/"`
Expected: 无输出。

- [ ] **Step 3: 写 `mm_relational_smoke.c`（与其他模态不同）**

relational 不通过 `storage_ops_t` 注册（`MODEL_RELATIONAL` 槽位本来就是空的），
所以冒烟不能用「数引擎」。改为：解析并执行一条最小语句。

```c
/* 伪代码骨架，具体入口以 src/db/parser/sql/ 与 src/db/sql/ 现有 API 为准 */
int main(void) {
    assert(mm_storage_init("test_data/smoke_rel") == 0);
    void *ast = sql_parse("SELECT 1");
    assert(ast != NULL);
    /* 断言执行结果只有 1 行 1 列 */
    assert(mm_storage_shutdown() == 0);
    printf("mm_relational_smoke OK\n");
    return 0;
}
```
实现前先 `grep -rn "sql_parse\|parse_statement" src/db/parser/ include/db/parser/` 找到真实入口，
**不要新造 API**。

- [ ] **Step 4: 构建运行**

```bash
cmake -B build/rel -S . -G Ninja -DMMDB_MODALITY=relational
ninja -C build/rel mm_relational_smoke && ./build/rel/mm_relational_smoke
```
Expected: `mm_relational_smoke OK`。

- [ ] **Step 5: 确认 relational profile 不含 vector_index**

Run: `python scripts/measure_build.py build/rel`
Expected: 比 `build/vector` 的 `compile_units` 少约 110（不含 101 个 vector_index）。

- [ ] **Step 6: Commit**
```bash
git commit -am "feat(build): relational 拆为独立库，executor/rag 拆出并挂 VECTOR 守卫"
```

---

## Task 16: 测试按模态重排

**Files:**
- Modify: `cmake/ProjectUtils.cmake`（`add_project_test` 支持 `MM_TEST_MODALITY`）
- Modify: 各 `test/db/**/CMakeLists.txt`（约 40 个文件）

**Interfaces:**
- Consumes: 各模态库
- Produces: `add_project_test(<name> <sources_var> LIBS <libs> MODALITY <modality>)`；
  `MODALITY` 缺省时按 `full` 处理（保持向后兼容）。

- [ ] **Step 1: 先记录当前 ctest 用例总数**

Run: `ctest --test-dir build/base -N | tail -1`
把数字写进本 Task 的 commit message。**这是重排后必须对上的基准**（Review Focus #4）。

- [ ] **Step 2: 改 `add_project_test` 支持 `MODALITY`**

`MODALITY` 决定链接哪一组库：`kv` → `mm_kv`，`vector` → `mm_vector mm_vector_index`，
`relational` → `mm_relational`，`full` → 全部。未声明时按 `full`。

- [ ] **Step 3: 按目录逐批加 `MODALITY`**

顺序：`test/db/storage/kv` → `vector` → `graph` → `spatial` → `doc` → `ts` →
`test/db/core` → `test/db/api` → `test/sdk`。**每批之后跑一次 `ctest -N` 比对总数**，
数量下降就说明漏了目标，立刻停下修。

- [ ] **Step 4: 全量构建 + 用例数比对**

```bash
cmake -B build/full -S . -G Ninja -DMMDB_MODALITY=full -DBUILD_TESTING=ON
ninja -C build/full -k 0
ctest --test-dir build/full -N | tail -1
```
Expected: 用例数 ≥ Step 1 记录的数字（新增了 doc/ts 等模态的测试，只许多不许少）。
少了就逐批回查。

- [ ] **Step 5: 单模态构建的产物显著变小**

```bash
python scripts/measure_build.py build/kv
```
Expected: `executables` 远少于 165，`executable_bytes` 远少于 2.2 GB。
把实测值写进 `docs/arch/db/modality-build.md`。

- [ ] **Step 6: Commit**
```bash
git commit -am "refactor(test): 测试按模态归属链接，单模态构建不再产出全量测试镜像"
```

---

## Task 17: full 回归与文档收尾

**Files:**
- Modify: `docs/arch/db/modality-build.md`（补全实测数字与白名单）
- Modify: `docs/arch/db/MULTIMODAL-DB-ARCHITECTURE.md`（补构建章节）

**Interfaces:**
- Consumes: 前 17 个 Task 的全部产物
- Produces: 可发布的验收报告

- [ ] **Step 1: 全量构建**

```bash
cmake -B build/final -S . -G Ninja -DMMDB_MODALITY=full -DBUILD_TESTING=ON
ninja -C build/final -k 0
```
Expected: 0 失败目标。

- [ ] **Step 2: 全量 ctest，与基线比失败用例名集合**

```bash
ctest --test-dir build/final -j8 --output-junit final.xml
```
Expected: 失败用例名集合 ⊆ `build-baseline-2026-10-27.md` 的「稳定失败」表。
新增失败必须逐条归因写进报告；无法归因的视为回归，回到对应 Task。

- [ ] **Step 3: 六个 profile 全部跑一遍冒烟**

```bash
for p in kv vector graph relational legacy full; do
  cmake -B build/smoke_$p -S . -G Ninja -DMMDB_MODALITY=$p
  ninja -C build/smoke_$p && ./build/smoke_$p/mm_*_smoke || { echo "FAIL $p"; exit 1; }
done
```
Expected: 全部输出 `... OK`。

- [ ] **Step 4: 汇总前后对比**

用 `measure_build.py` 对比 `build/base`（改造前）与 `build/kv` / `build/vector` / `build/final`，
把 `compile_units` / `static_lib_bytes` / `executables` / `executable_bytes` 四项写进
`docs/arch/db/modality-build.md` 的对比表。

- [ ] **Step 5: 补 `MULTIMODAL-DB-ARCHITECTURE.md` 的构建章节**

写清楚：四层依赖、白名单、`MMDB_MODALITY` 的六个 profile、如何加一个新模态
（三步：建 `mm_<name>` 库 → 写 `mm_<name>_register()` → 在 profile 表里登记）。

- [ ] **Step 6: Commit**
```bash
git commit -am "docs(build): 补全按模态构建的实测数据与使用说明"
```

---

## Self-Review 记录

**Spec 覆盖**：§5.1 四层 → Task 6；§5.2 去环 → Task 1/2/3；§5.3 profile → Task 5；
§5.4 白名单 → Task 9/10/15 + `modality-build.md`；§5.5 库粒度 → Task 7–15；
§5.6 冒烟 → Task 7（模板）+ 8–15；§5.7 测试归属 → Task 16；§5.8 L4 归属 → Task 5（profile 表）+ 15；
§5.9 数字 → Task 0 测量 + Task 7/16/17 记录；§5.10 五个未构建模态 → Task 11–14；
§7 交付物 → 全部落地；§8 验收 → Task 17。

**类型一致性**：`mm_register_builtin_engines` / `mm_register_builtin_sql_functions` 在 Task 2 声明、
Task 3 补齐实现；`mm_smoke(<modality>)` 在 Task 7 定义、Task 8–15 复用；
`mm_base` / `mm_storage_api` 在 Task 6 定义、Task 7+ 引用。命名无漂移。

**比例**：计划约 700 行，spec 约 450 行，比例 1.5:1，未退化成代码转录。
