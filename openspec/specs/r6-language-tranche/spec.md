# r6-language-tranche Specification

## Purpose
TBD - created by archiving change learning-backlog-r6. Update Purpose after archive.
## Requirements
### Requirement: R6 8 张卡必须通过四要素审计方可推 done

系统 SHALL 仅在一张 R6 卡从 `todo` 推到 `done` 时，在 `statuses.json` 与 `r6-progress.md` 中**同时满足**：

1. **commit 引用**：`r6-progress.md` 该行 SHALL 含可指向的 git commit SHA（前缀 `[r6] c-<id>:`）
2. **scaffold 产物**：commit SHALL 含至少 1 份可编译运行的 C 源（`learning/scaffold/<card-id>/main.c` + `Makefile`）。c-code-style 例外：只 Makefile + NOTES。
3. **NOTES.md 工程对照**：路径为 `learning/scaffold/<card-id>/NOTES.md`，必须含 ≥100 中文字符的"工程对照"段落，明确指出本卡知识在工程侧的具体复用位置（带文件路径或行号引用）
4. **r6-progress.md 行**：与 `statuses.json` status 严格同步

#### Scenario: 完整四要素通过审核

- **WHEN** 提交 `c-pointer` 的勾选申请，含 commit SHA、scaffold 路径、NOTES 路径、≥100 字工程对照
- **THEN** `statuses.json` 的 c-pointer SHALL 改为 `"done"`
- **AND** r6-progress.md SHALL 新增一行

#### Scenario: 工程对照空泛被驳回

- **WHEN** 勾选申请 NOTES.md 的"工程对照"段 < 100 中文字符
- **THEN** 申请 SHALL 被驳回

#### Scenario: c-code-style 的特殊路径

- **WHEN** 勾选 c-code-style 类非代码卡
- **THEN** scaffold 产物为 Makefile + NOTES.md，不要求 main.c

### Requirement: r6-progress.md 接管而非扩展 r5-progress.md

本变更 SHALL 创建新文件 `apps/web/reading-radar/data/r6-progress.md`，不复用 R5 的 `r5-progress.md`。

#### Scenario: R6 期间不修改 r5-progress.md

- **WHEN** R6 任意卡被勾选
- **THEN** `r5-progress.md` SHALL 不被修改
- **AND** 所有 R6 审计条目 SHALL 仅追加到 r6-progress.md

#### Scenario: 双 progress 共存

- **WHEN** R6 完成且 R5 完成状态都存在
- **THEN** `r5-progress.md` 含 8 行（R5 卡）
- **AND** `r6-progress.md` 含 8 行（R6 卡）
- **AND** 两者均与 `statuses.json` 严格同步

### Requirement: 反例档案跨 R5/R6 共用与追加

`apps/web/reading-radar/data/r5-anti-patterns.md` SHALL 是 R5 + R6 跨变更的反例档案；R6 期间的违规 SHALL 追加为新案例（不删 R5 既有 3 条）。

#### Scenario: R6 期间驳回被追加

- **WHEN** 任一 R6 卡勾选申请被四要素审核驳回
- **THEN** `r5-anti-patterns.md` SHALL 追加一条新案例，标 `R6` 前缀区分

#### Scenario: R5 既有 3 条不被删除

- **WHEN** R6 反例档案追加新案例
- **THEN** 原 R5 案例 SHALL 完整保留

