# C9-1 Housekeeping 任务清单

## 任务列表

### Task #1: 提交 reference 子模块
- **状态**: pending
- **描述**: 添加 17 个新模型目录到 git（共 79 条 submodule 条目）
- **验收**: `git submodule status reference/` 全部 tracked

### Task #2: 提交 meta 文件
- **状态**: pending
- **描述**: 提交 .gitmodules、catalog.yml、diagrams 更新
- **验收**: `git status -s` 无 reference 相关变更

### Task #3: 修复 cjk_tokenizer.c
- **状态**: pending
- **描述**: 加 `#define _GNU_SOURCE` 解决 strndup 隐式声明
- **验收**: `cmake --build db_core` 无该错误

### Task #4: 修复 xml_parser.c
- **状态**: pending
- **描述**: 修正第 79 行 `p->pos - (colon + 1)` 为 `p->pos - colon - 1`
- **验收**: 无 invalid operands 错误

### Task #5: 修复 explain_analyze.c
- **状态**: pending
- **描述**: 加 `#include "db/vacuum_trigger.h"`
- **验收**: 无 implicit declaration 错误

### Task #6: 完整构建验证
- **状态**: pending
- **描述**: `cmake --build build/engineering --target db_core` 成功
- **验收**: 编译通过

## 完成状态

- [ ] Task #1: 提交 reference 子模块
- [ ] Task #2: 提交 meta 文件
- [ ] Task #3: 修复 cjk_tokenizer.c
- [ ] Task #4: 修复 xml_parser.c
- [ ] Task #5: 修复 explain_analyze.c
- [ ] Task #6: 完整构建验证
