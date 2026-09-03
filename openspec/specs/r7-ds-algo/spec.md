# r7-ds-algo Specification

## Purpose
TBD - created by archiving change learning-backlog-r7. Update Purpose after archive.
## Requirements
### Requirement: R7 8 张卡必须通过四要素审计方可推 done

系统 SHALL 仅在一张 R7 卡从 `todo` 推到 `done` 时，在 `statuses.json` 与 `r7-progress.md` 中**同时满足**：

1. **commit 引用**：`r7-progress.md` 该行 SHALL 含可指向的 git commit SHA（前缀 `[r7] c-<id>:`）
2. **scaffold 产物**：commit SHALL 含至少 1 份可编译运行的 C 源（`learning/scaffold/<card-id>/main.c` + `Makefile`）。所有 8 张 R7 卡均需 main.c（无例外——算法必须可执行才能观察步骤）
3. **NOTES.md 工程对照**：路径为 `learning/scaffold/<card-id>/NOTES.md`，必须含 ≥100 中文字符的"工程对照"段落，明确指出本卡知识在 `engineering/` 的具体复用位置（带文件路径或行号引用）
4. **r7-progress.md 行**：与 `statuses.json` status 严格同步

#### Scenario: 完整四要素通过审核

- **WHEN** 提交 `c-ds-basic` 的勾选申请，含 commit SHA、scaffold 路径、NOTES 路径、≥100 字工程对照
- **THEN** `statuses.json` 的 c-ds-basic SHALL 改为 `"done"`
- **AND** r7-progress.md SHALL 新增一行

#### Scenario: 工程对照空泛被驳回

- **WHEN** 勾选申请 NOTES.md 的"工程对照"段 < 100 中文字符
- **THEN** 申请 SHALL 被驳回

### Requirement: 算法卡 `make run` 必须打印中间步骤（可观察性约束）

R7 是首个 algorithms 象限变更。与 R5 系统编程卡（侧重系统调用返回值）和 R6 语言核心卡（侧重语法语义）不同，算法卡的 scaffold SHALL 在 `make run` 时**打印可观察的中间步骤**，而非仅输出最终结果。

#### Scenario: 排序算法打印每轮中间数组

- **WHEN** `c-sort-basic` 或 `c-sort-advanced` 的 `make run` 执行
- **THEN** 输出 SHALL 包含每轮排序后的数组中间状态（如 `[pass 2] 3 1 4 5 2 → 1 3 4 2 5`）
- **AND** 最终结果附带比较次数和交换次数

#### Scenario: 图算法打印遍历路径

- **WHEN** `c-graph` 的 `make run` 执行
- **THEN** DFS/BFS 输出 SHALL 包含节点访问顺序（如 `[BFS] visit: A → B → D → C → E`）
- **AND** Dijkstra 输出 SHALL 包含每次松弛的距离更新

#### Scenario: 位运算打印二进制表示

- **WHEN** `c-bitwise` 的 `make run` 执行
- **THEN** 位操作 SHALL 以二进制格式打印输入和输出（如 `[bitle] set bit 3: 0b00001000 → 0b00001100`）

### Requirement: R7 §0 补建 R5/R6 基线文件

由于 R5/R6 的 `statuses.json` / `r5-progress.md` / `r6-progress.md` 未实际落盘，R7 SHALL 在 §0 阶段统一创建这些基线文件。

#### Scenario: statuses.json 首次落盘

- **WHEN** R7 §0.2 执行
- **THEN** `engineering/apps/web/reading-radar/data/statuses.json` SHALL 被创建
- **AND** 包含 197 条卡 ID（从 items-registry.js 派生）
- **AND** R5 8 张卡 + R6 8 张卡 status = `"done"`，其余 = `"todo"`

#### Scenario: r5-progress.md 和 r6-progress.md 补建

- **WHEN** R7 §0.3–§0.4 执行
- **THEN** `r5-progress.md` SHALL 含 8 行（commit SHA = TBD-deferred）
- **AND** `r6-progress.md` SHALL 含 8 行（commit SHA = TBD-deferred）
- **AND** 两个文件 SHALL 使用与 r7-progress.md 相同的表头格式

### Requirement: r7-progress.md 接管而非扩展 r5/r6-progress.md

本变更 SHALL 创建新文件 `apps/web/reading-radar/data/r7-progress.md`，不复用 R5/R6 的 progress 文件。

#### Scenario: R7 期间不修改 r5-progress.md 和 r6-progress.md

- **WHEN** R7 任意卡被勾选
- **THEN** `r5-progress.md` 和 `r6-progress.md` SHALL 不被修改
- **AND** 所有 R7 审计条目 SHALL 仅追加到 r7-progress.md

#### Scenario: 三 progress 共存

- **WHEN** R7 完成且 R5/R6 完成状态都存在
- **THEN** `r5-progress.md` 含 8 行（R5 卡）
- **AND** `r6-progress.md` 含 8 行（R6 卡）
- **AND** `r7-progress.md` 含 8 行（R7 卡）
- **AND** 三者均与 `statuses.json` 严格同步

### Requirement: 反例档案跨 R5/R6/R7 共用与追加

`apps/web/reading-radar/data/r5-anti-patterns.md` SHALL 是 R5 + R6 + R7 跨变更的反例档案；R7 期间的违规 SHALL 追加为新案例（不删 R5 既有 3 条 + R6 追加案例）。

#### Scenario: R7 期间驳回被追加

- **WHEN** 任一 R7 卡勾选申请被四要素审核驳回
- **THEN** `r5-anti-patterns.md` SHALL 追加一条新案例，标 `R7` 前缀区分

#### Scenario: R5/R6 既有案例不被删除

- **WHEN** R7 反例档案追加新案例
- **THEN** 原 R5 案例和 R6 案例 SHALL 完整保留

