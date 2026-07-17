# r10-c-finale Specification

## Purpose
TBD - created by archiving change r10-c-finale. Update Purpose after archive.
## Requirements
### Requirement: 四要素审计

每张 R10 卡 MUST 满足四要素审计：从 todo 推到 done 时，必须在 `statuses.json` 和 `r10-progress.md` 中同时满足四个条件：

1. **commit SHA 引用**：r10-progress.md 行中存在 commit SHA（统一为 TBD-deferred，与 R5–R9 一致）
2. **scaffold 产物**：`learning/scaffold/<card-id>/` 目录下存在 main.c + Makefile
3. **NOTES.md 工程对照**：`learning/scaffold/<card-id>/NOTES.md` 含"工程对照"段，中文内容 ≥100 字符，至少引用一个 `engineering/` 路径
4. **r10-progress.md 行**：每张卡在 `r10-progress.md` 中有一行记录，时间戳、路径与 statuses.json 同步

#### Scenario: R10 语言/系统卡交付

- **WHEN** c-function-scope / c-stdlib / c-error-handling / c-inline-asm 的 scaffold + docs + notes + status 四步完成
- **THEN** statuses.json 中对应卡 ID 值为 "done"
- **AND** r10-progress.md 中对应行的时间戳为当日、commit SHA 为 TBD-deferred

### Requirement: R10 语言可观察性约束

R10 的 4 张卡 MUST 沿用 R7–R9 的 `[tag]` 中间状态打印规约：每张卡通过 `printf("[xxx] ...\n")` 标记每个 demo 段落，让运行结果可观察、可审计。

- **退出码**：执行无错误时 `return 0`，结尾打印 `=== PASS ===`
- **c-inline-asm 例外**：内联汇编段可能影响输出顺序，但 `=== PASS ===` 仍必须打印

#### Scenario: c-stdlib 13 函数演示

- **WHEN** 运行 `./stdlib_demo`
- **THEN** 标准输出包含 malloc/calloc/realloc/free/atexit/atoi/strtol/qsort/bsearch/rand 等 tag
- **AND** 退出码为 0

#### Scenario: c-error-handling errno 演示

- **WHEN** 运行 `./error_demo`
- **THEN** 标准输出包含 `[errno]`/`[perror]`/`[strerror]`/`[assert]` 四种 tag
- **AND** 退出码为 0

### Requirement: r10-progress.md 独立性

R10 MUST 创建独立进度文件 `r10-progress.md`，不复用 R5–R9 的 progress.md。R10 实施期间不修改 r5~r9-progress.md。

#### Scenario: progress 文件互不干扰

- **WHEN** R10 执行 4 张卡的 status 更新
- **THEN** r10-progress.md 新增 4 行
- **AND** r5~r9-progress.md 各自保持不变

### Requirement: 反例档案跨轮次共用

`r5-anti-patterns.md` 是 R5–R10 MUST 共享的反例档案。R10 如有违规案例（如 NOTES.md 工程对照字数不足），追加到该文件，不删除既有案例。

#### Scenario: 追加 R10 违规案例

- **WHEN** R10 验证中发现某卡 NOTES.md 工程对照 <100 字
- **THEN** 在 r5-anti-patterns.md 底部追加新条目
- **AND** 不修改 R5–R9 的既有案例

