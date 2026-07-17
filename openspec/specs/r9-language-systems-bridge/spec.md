# r9-language-systems-bridge Specification

## Purpose
TBD - created by archiving change r9-language-systems-bridge. Update Purpose after archive.
## Requirements
### Requirement: 四要素审计

每张 R9 卡 MUST 满足四要素审计：从 todo 推到 done 时，必须在 `statuses.json` 和 `r9-progress.md` 中同时满足四个条件：

1. **commit SHA 引用**：r9-progress.md 行中存在 commit SHA（统一为 TBD-deferred，与 R5–R8 一致）
2. **scaffold 产物**：`learning/scaffold/<card-id>/` 目录下存在 main.c + Makefile（c-lkm 用 hello.c，c-ci 含额外 `.github/workflows/ci.yml` + `ci.sh`）
3. **NOTES.md 工程对照**：`learning/scaffold/<card-id>/NOTES.md` 含"工程对照"段，中文内容 ≥100 字符，至少引用一个 `engineering/` 路径
4. **r9-progress.md 行**：每张卡在 `r9-progress.md` 中有一行记录，时间戳、路径与 statuses.json 同步

#### Scenario: 语言/系统卡交付

- **WHEN** c-syntax / c-control-flow / c-array-string / c-preprocessor / c-file-io / c-process / c-lkm 的 scaffold + docs + notes + status 四步完成
- **THEN** statuses.json 中对应卡 ID 值为 "done"
- **AND** r9-progress.md 中对应行的时间戳为当日、commit SHA 为 TBD-deferred

#### Scenario: 工程卡交付

- **WHEN** c-ci 的 scaffold + docs + notes + status 四步完成
- **THEN** 同上，且 `.github/workflows/ci.yml` + `ci.sh` + `CHEATSHEET.md` 存在
- **AND** Makefile 含 `ci-local`/`check-yml` target，不可用工具时 graceful 降级

### Requirement: 语言卡可观察性约束

R9 的 4 张语言卡 MUST 沿用 R7/R8 的 `[tag]` 中间状态打印规约：每张卡通过 `printf("[xxx] ...\n")` 标记每个 demo 段落，让运行结果可观察、可审计。

- **退出码**：执行无错误时 `return 0`，结尾打印 `=== PASS ===`
- **段落数量**：basic 卡 ≥4 段、advanced 卡 ≥6 段

#### Scenario: c-syntax 中间状态

- **WHEN** 运行 `./syntax_demo`（或 `gcc -o test main.c && ./test`）
- **THEN** 标准输出包含 `[types]`/`[ops]`/`[bounds]`/`[cast]` 四种 tag 标记
- **AND** 退出码为 0

#### Scenario: c-preprocessor 中间状态

- **WHEN** 运行 `./preprocessor_demo`
- **THEN** 标准输出包含 `[define]`/`[func]`/`[stringfy]`/`[concat]`/`[ifdef]`/`[once]`/`[do-while]`/`[variadic]` 八种 tag
- **AND** 退出码为 0

### Requirement: CI/LKM 平台分支 Graceful 降级

R9 中 c-ci 与 c-lkm MUST 在非目标平台（Windows MinGW）上 graceful 降级：

- **c-ci**：`check-yml` target 检测 YAML 工具（`python -c "import yaml"`），不可用时打印 `[skip] yaml 模块不可用` 退出 0
- **c-lkm**：`Makefile` 检测 `uname -s`，非 Linux 时打印 `[skip] c-lkm 需要 Linux 内核开发环境` 退出 0

#### Scenario: c-lkm 在 Windows 降级

- **WHEN** 在 MinGW 环境下运行 `cd learning/scaffold/c-lkm && make`
- **THEN** 输出 `[skip] c-lkm 需要 Linux 内核开发环境，当前 OS=<...>`
- **AND** 退出码为 0

#### Scenario: c-ci YAML 校验

- **WHEN** 在 MinGW 环境下运行 `cd learning/scaffold/c-ci && make check-yml`
- **THEN** 若 Python yaml 模块可用则校验 ci.yml 语法，不可用时打印 `[skip] yaml 不可用`
- **AND** 退出码为 0

### Requirement: r9-progress.md 独立性

R9 MUST 创建独立进度文件 `r9-progress.md`，不复用 R5/R6/R7/R8 的 progress.md。R9 实施期间不修改 r5~r8-progress.md。

#### Scenario: progress 文件互不干扰

- **WHEN** R9 执行 8 张卡的 status 更新
- **THEN** r9-progress.md 新增 8 行
- **AND** r5-progress.md 保持 8 行不变
- **AND** r6-progress.md 保持 8 行不变
- **AND** r7-progress.md 保持 8 行不变
- **AND** r8-progress.md 保持 8 行不变

### Requirement: 反例档案跨轮次共用

`r5-anti-patterns.md` 是 R5+R6+R7+R8+R9 MUST 共享的反例档案。R9 如有违规案例（如 NOTES.md 工程对照字数不足），追加到该文件，不删除既有案例。

#### Scenario: 追加 R9 违规案例

- **WHEN** R9 验证中发现某卡 NOTES.md 工程对照 <100 字
- **THEN** 在 r5-anti-patterns.md 底部追加新条目
- **AND** 不修改 R5/R6/R7/R8 的既有案例

