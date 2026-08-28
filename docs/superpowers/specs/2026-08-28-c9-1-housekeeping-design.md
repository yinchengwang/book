# C9-1 Housekeeping 设计

> 日期：2026-08-28
> 目标：提交 git 工作区遗留，修复 db_core 三个预存编译错误

## 一、背景

本会话已完成大量 OPSX 变更归档，但 git 工作区仍有遗留：
- 7 个已修改未提交文件（含 `.gitmodules`、`reference/catalog.yml`、4 个 diagrams）
- 39 个未跟踪文件（reference/ 下 17 个新克隆的子模块目录 + scripts/ + third_part/uthash）
- `engineering/src/db/core/` 三个预存编译错误阻塞所有依赖 db_core 的构建

这些遗留阻止了：
1. `git status` 显示干净工作区
2. `cmake --build build/engineering --target db_core` 成功
3. `ctest` 完整运行（db_core 失败导致后续所有测试无法链接）

## 二、任务分解

### Task 1: 提交 reference 子模块

**范围**：
- `reference/` 下 17 个新模型目录：benchmark、columnar、distributed-sql、document、embedded、extension、graph、key-value、relational、search、stream、time-series、vector
- 每个目录下已克隆多个子模块（共 79 条 `.gitmodules` 条目）
- `reference/catalog.yml` 已更新（含 74 个项目元数据）
- `scripts/clone_retry.sh`、`scripts/clone_with_retry.sh` 工具脚本

**验证**：
- `git status` 无 reference 相关未跟踪文件
- `git submodule status` 显示所有子模块已跟踪

### Task 2: 提交 meta 文件修改

**范围**：
- `.gitmodules` — 新增 79 条 submodule 条目（与 reference 子模块对应）
- `docs/diagrams/level2-storage/L2-010-sql-execution-flow.md` — 反映最新架构
- `docs/diagrams/level3-vector/L3-*.md`（3 个文件）— 反映最新 HNSW/R-Tree 设计
- `reference/catalog.yml` — 已包含 74 个项目

**注意**：diagrams 目录中的修改是 reference 子模块克隆触发的文档同步更新。

### Task 3: 修复 cjk_tokenizer.c strndup 隐式声明

**根因**：`strndup` 是 POSIX.1-2008 / GNU 扩展，非 ISO C 标准函数。当前代码：
```c
#include <string.h>   // 仅 C99 标准，不含 strndup
```

**修复**：在文件顶部加 `#define _GNU_SOURCE` 或 `#define _POSIX_C_SOURCE 200809L`。

**位置**：`engineering/src/db/core/cjk_tokenizer.c` 第 1 行之前。

**验证**：`gcc -std=c11 ... cjk_tokenizer.c` 不再报 implicit declaration。

### Task 4: 修复 xml_parser.c 类型不匹配

**根因**：第 79 行 `p->pos - (colon + 1)` 中：
- `p->pos` 类型为 `size_t`（无符号整数）
- `colon + 1` 类型为 `const char *`（指针）
- 两者不能做减法

**预期意图**：计算字符串长度。正确表达式应为：
```c
p->pos - colon - 1   // 指针相减得到字节数（size_t）
```

**位置**：`engineering/src/db/core/xml_parser.c` 第 79 行。

**验证**：重新编译无 invalid operands 错误。

### Task 5: 修复 explain_analyze.c vacuum_trigger_check 未声明

**根因**：`vacuum_trigger_check` 函数定义在 `engineering/src/db/storage/txn/vacuum_trigger.c`，声明在 `engineering/include/db/vacuum_trigger.h`，但 `explain_analyze.c` 未 include 该头文件。

**修复**：在 `explain_analyze.c` 顶部加：
```c
#include "db/vacuum_trigger.h"
```

**验证**：重新编译无 implicit declaration 错误。

## 三、验收标准

1. `cmake --build build/engineering --target db_core` 成功（无 error，仅有 warning）
2. `git status -s` 无 `reference/` 相关的 `??` 或 ` M`
3. `git submodule status reference/` 全部显示 `+` 前缀（子模块已正确跟踪）
4. 不影响其他已工作的模块（sha256、blob_engine、cf_engine、yang/netconf 等编译结果不变）

## 四、风险与缓解

| 风险 | 缓解 |
|------|------|
| _GNU_SOURCE 可能改变 string.h 暴露的函数集 | 只用于 cjk_tokenizer.c，不影响其他模块 |
| xml_parser.c 表达式修改影响其他路径 | 先读完整上下文再修改，测试 xml_parser_test 回归 |
| vacuum_trigger.h include 引入额外依赖 | 只在 explain_analyze.c 中加一行 include，无传递依赖 |

## 五、文件清单

修改：
- `engineering/src/db/core/cjk_tokenizer.c`（加 #define _GNU_SOURCE）
- `engineering/src/db/core/xml_parser.c`（修正算术表达式）
- `engineering/src/db/core/explain_analyze.c`（加 include "db/vacuum_trigger.h"）
- `.gitmodules`（含 submodule 新增条目）
- `reference/catalog.yml`（含新项目元数据）
- `docs/diagrams/level2-storage/L2-010-sql-execution-flow.md`
- `docs/diagrams/level3-vector/L3-001-index-selection-tree.md`
- `docs/diagrams/level3-vector/L3-002-hnsw-structure.md`
- `docs/diagrams/level3-vector/L3-010-rtree-structure.md`
- `scripts/clone_retry.sh`
- `scripts/clone_with_retry.sh`

新增（git add）：
- `reference/benchmark/{db-bench,tpcds,tpch,vectordbbench}/`
- `reference/columnar/`
- `reference/distributed-sql/`
- `reference/document/`
- `reference/embedded/`
- `reference/extension/{arrow,datafusion,hnswlib,lance,velox}/`
- `reference/graph/`
- `reference/key-value/{badger,dragonfly,foundationdb,leveldb,pebble,rocksdb}/`
- `reference/relational/{firebird,h2,mariadb}/`（mysql/postgres/sqlite3/openGauss 之前已跟踪）
- `reference/search/`
- `reference/stream/`
- `reference/time-series/`
- `reference/vector/{chroma,lancedb,milvus,qdrant,usearch,vald,vespa,weaviate}/`
- `third_part/uthash/`
