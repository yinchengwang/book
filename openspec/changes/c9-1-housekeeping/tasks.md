# C9-1 Housekeeping 任务清单

## 任务列表

### Task #1: 提交 reference 子模块
- **状态**: completed
- **描述**: 添加 17 个新模型目录到 git（共 79 条 submodule 条目）
- **验收**: `git submodule status reference/` 全部 tracked

### Task #2: 提交 meta 文件
- **状态**: completed
- **描述**: 提交 .gitmodules、catalog.yml、diagrams 更新
- **验收**: `git status -s` 无 reference 相关变更

### Task #3: 修复 cjk_tokenizer.c
- **状态**: completed
- **描述**: 加 `#define _GNU_SOURCE` 解决 strndup 隐式声明
- **验收**: `cmake --build db_core` 无该错误

### Task #4: 修复 xml_parser.c
- **状态**: completed
- **描述**: 修正第 79 行 `p->pos - (colon + 1)` 为 `p->pos - colon - 1`
- **验收**: 无 invalid operands 错误

### Task #5: 修复 explain_analyze.c
- **状态**: completed
- **描述**: 加 `#include "db/vacuum_trigger.h"`
- **验收**: 无 implicit declaration 错误

### Task #6: 完整构建验证
- **状态**: completed
- **描述**: `cmake --build build/engineering --target db_core` 成功
- **验收**: 编译通过

## 完成状态

- [x] Task #1: 提交 reference 子模块
- [x] Task #2: 提交 meta 文件
- [x] Task #3: 修复 cjk_tokenizer.c
- [x] Task #4: 修复 xml_parser.c
- [x] Task #5: 修复 explain_analyze.c
- [x] Task #6: 完整构建验证

## 完成记录（2026-09-21）

- **Task #1/#2**: reference 子模块原计划 17 目录/79 条目，实际上按 category 重组为 **71 条 reference 子模块**（vector/key-value/relational/search/graph/… 13 大类），已全部 tracked + mapped。
  额外发现并修复 **3 条孤儿 gitlink**（`third_part/uthash`、`engineering/third_part/googletest`、`engineering/third_part/uthash`）缺 `.gitmodules` 映射导致 `git submodule status` 报 fatal，已补 gitee mirror URL（commit `fb4a93b55`）。
- **Task #3**: 未采用 `#define _GNU_SOURCE`，改用自定义 `my_strndup()`（`cjk_tokenizer.c:12`）跨平台兜底，MinGW/WSL 皆可用。
- **Task #4**: `xml_parser.c:79` 改为偏移量运算 `p->pos - ((size_t)(colon - p->src) + 1)`，等价于原要求的 `p->pos - colon - 1`，消除指针混算。
- **Task #5**: 未直接 include `vacuum_trigger.h`（避免传递依赖 `TransactionId` 未定义），改为前向声明 `extern void vacuum_trigger_check(void)`（`explain_analyze.c:13`）。
- **Task #6**: `ninja -C build/engineering db_core` 编译+链接通过（`libdb_core.a`，exit 0）。