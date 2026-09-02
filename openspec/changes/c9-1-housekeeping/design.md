# C9-1 Housekeeping 设计

> 日期：2026-08-28
> 目标：提交 git 工作区遗留，修复 db_core 三个预存编译错误

## 一、背景

本会话已完成大量 OPSX 归档，但 git 工作区仍有遗留：
- 7 个已修改未提交文件（含 `.gitmodules`、`reference/catalog.yml`、4 个 diagrams）
- 39 个未跟踪文件（reference/ 下 17 个新克隆的子模块目录 + scripts/ + third_part/uthash）
- `engineering/src/db/core/` 三个预存编译错误阻塞所有依赖 db_core 的构建

## 二、任务分解

### Task 1: 提交 reference 子模块

**范围**：
- `reference/` 下 17 个新模型目录（共 79 条 submodule 条目）
- `reference/catalog.yml`（74 个项目元数据）
- `scripts/clone_retry.sh`、`scripts/clone_with_retry.sh`

### Task 2: 提交 meta 文件修改

- `.gitmodules`（79 条 submodule 条目）
- `docs/diagrams/level2-storage/L2-010-sql-execution-flow.md`
- `docs/diagrams/level3-vector/L3-*.md`（3 个文件）

### Task 3: 修复 cjk_tokenizer.c strndup 隐式声明

**根因**：`strndup` 是 POSIX.1-2008 扩展，需 `#define _GNU_SOURCE`。
**修复**：在文件顶部加 `#define _GNU_SOURCE`。

### Task 4: 修复 xml_parser.c 类型不匹配

**根因**：第 79 行 `p->pos - (colon + 1)` 中 `size_t - const char *` 无效。
**修复**：改为 `p->pos - colon - 1`（指针相减）。

### Task 5: 修复 explain_analyze.c vacuum_trigger_check 未声明

**根因**：缺少 `#include "db/vacuum_trigger.h"`。
**修复**：加一行 include。

## 三、验收标准

1. `cmake --build build/engineering --target db_core` 成功
2. `git status -s` 无 reference 相关变更
3. `git submodule status reference/` 全部显示 `+` 前缀
