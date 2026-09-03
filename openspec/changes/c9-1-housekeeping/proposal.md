# C9-1 Housekeeping 提案

## Why

本会话已完成大量 OPSX 归档，但 git 工作区仍有 7 个未提交修改和 40 个未跟踪文件（reference 子模块克隆触发）。
同时 `db_core` 三个预存编译错误阻塞所有依赖该库的测试运行。

这些遗留阻止：
1. `git status` 显示干净工作区
2. `cmake --build build/engineering --target db_core` 成功
3. `ctest` 完整运行

## What Changes

### 修复编译错误（3 处）

1. **cjk_tokenizer.c**：加 `#define _GNU_SOURCE` 让 `strndup` 声明可见
2. **xml_parser.c**：修正第 79 行 `p->pos - (colon + 1)` 为 `p->pos - colon - 1`
3. **explain_analyze.c**：加 `#include "db/vacuum_trigger.h"` 让 `vacuum_trigger_check` 声明可见

### 提交 git 工作区

- `reference/` 下 17 个新模型目录（79 条 submodule 条目）
- `reference/catalog.yml`（74 项项目元数据）
- `.gitmodules`（79 条条目）
- `docs/diagrams/*.md`（4 个架构图更新）
- `scripts/clone_retry.sh`、`scripts/clone_with_retry.sh`

## Capabilities

| 能力 | 交付 |
|------|------|
| db_core 编译通过 | cmake --build db_core 成功，无 error |
| reference 子模块集成 | git submodule status 79 条全部 tracked |
| 测试可链接 | db_core .a 库生成，test_cf_engine 等可链接 |

## Impact

- 修改 3 个源文件（共 ~15 行改动）
- 新增 17 个 reference 子模块目录（只读参考）
- 提交 7 个 meta 文件

## 验收标准

- `cmake --build build/engineering --target db_core` 成功
- `git submodule status reference/` 全部显示 `+` 前缀
- `git status -s` 无 reference/db_core 相关变更