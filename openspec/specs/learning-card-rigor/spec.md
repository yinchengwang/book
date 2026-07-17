# learning-card-rigor Specification

## Purpose
TBD - created by archiving change learning-backlog-r5. Update Purpose after archive.
## Requirements
### Requirement: 四要素齐全方可推 done

系统 SHALL 仅在一张卡从 `todo` 或 `learning` 推到 `done` 时，在 git 跟踪的 `statuses.json` 与 `r5-progress.md` 中**同时满足**以下四要素，缺一则 SHALL NOT 推入 done：

1. **commit 引用**：`r5-progress.md` 该行 SHALL 含可指向的 git commit SHA（短哈希 ≥ 7 位），commit message 前缀须为 `[r5] <card-id>: <topic>`
2. **scaffold 产物**：commit SHALL 含至少 1 份可编译运行的 C/C++ 代码，路径为 `learning/scaffold/<card-id>/main.c` + `Makefile`（Makefile 必须含 `make run` 目标）
3. **NOTES.md 工程对照**：路径为 `learning/scaffold/<card-id>/NOTES.md`，必须含 ≥ 100 字的"工程对照"段落，明确指出本卡知识在 `engineering/` 哪个模块或哪个未来 OPSX 里会被复用
4. **r5-progress.md 行**：`apps/web/reading-radar/data/r5-progress.md` 同步添加一行，含时间戳、卡 ID、commit SHA、scaffold 路径、NOTES 路径，且与 statuses.json 状态一致

#### Scenario: 四要素齐全通过审核

- **WHEN** 用户提交某卡的 done 勾选申请，含 commit=<sha7+>、scaffold 路径、NOTES.md 路径、r5-progress.md 行号
- **AND** 该 NOTES.md "工程对照"段落 ≥ 100 字符
- **THEN** 该卡 SHALL 允许被推入 done，并在 r5-progress.md 留审计条目

#### Scenario: 缺 commit 引用被驳回

- **WHEN** 用户提交勾选申请但未携带 commit SHA 或 SHA 不存在
- **THEN** 申请 SHALL 被驳回，并追加一条到 `apps/web/reading-radar/data/r5-anti-patterns.md`，标注根因为"commit 缺失"

#### Scenario: NOTES.md 工程对照不足 100 字被驳回

- **WHEN** commit 与 scaffold 都齐全但 NOTES.md 的"工程对照"段落 < 100 字符
- **THEN** 申请 SHALL 被驳回，反例档案追加一条，根因为"工程对照空泛"

### Requirement: 反例档案强制维护（v2 强化）

反例档案 `apps/web/reading-radar/data/r5-anti-patterns.md` SHALL 在 R5 第一张卡勾选完成时已存在，且含**至少 3 条过往虚勾案例**剖析：

- 案例须可追溯（基于 `git log --grep="^doc(learning):"` 或 `git log` 中本仓库历史 commit 推断）
- 每条案例 SHALL 含：时间锚点 + 现象描述 + 根因分析 + 防范要点
- 任何后续违规 SHALL 被追加为新案例，标注根因（commit 缺失 / 产物缺失 / 笔记太薄 / 工程对照空泛）

#### Scenario: 反例档案首次建立

- **WHEN** R5 任意一张卡的 §1 都状态完成时
- **THEN** `apps/web/reading-radar/data/r5-anti-patterns.md` SHALL 已存在
- **AND** 含 ≥ 3 条过往虚勾案例剖析
- **AND** 该文件 SHALL 进入 git 跟踪

#### Scenario: 违规累计追加

- **WHEN** 任何 R5 期间的勾选申请在四要素审核中被驳回 ≥ 2 次
- **THEN** 该违规 SHALL 立即追加一条到反例档案
- **AND** 档案与 statuses.json 在同一个 commit 中提交，避免反向"档案快、状态慢"

### Requirement: 进度日志与 statuses.json 严格同步

系统 SHALL 维护 `r5-progress.md` 作为正向审计清单、`statuses.json` 作为权威状态存储；两者 SHALL 保持一致，任何一方变更必须同步另一方。

#### Scenario: 状态变化即追加进度行

- **WHEN** 一张卡在 statuses.json 中的 status 从非 done 改为 done
- **THEN** `r5-progress.md` SHALL 新增一行，列：`时间戳 | 卡 ID | commit SHA | scaffold 路径 | NOTES 路径`
- **AND** 同一 commit 中 statuses.json 与 r5-progress.md SHALL 同时更新

#### Scenario: 不一致时进度日志为准

- **WHEN** r5-progress.md 与 statuses.json 出现状态不一致
- **THEN** 以 r5-progress.md 为准
- **AND** 应在 CI / 提交流程中提示回填或修复（具体实现细节由工程侧决定，**不在本 spec**）

### Requirement: scaffold 工程对照 ≥ 100 字硬约束

系统 SHALL 要求每个 `learning/scaffold/<card-id>/NOTES.md` 的"工程对照"段 ≥ 100 中文字符（含标点），且 SHALL 满足以下三条之一：

- 指出本卡知识在 `engineering/` 当前已使用的具体位置（带文件路径或行号引用）
- 指出本卡知识在未来某个 OPSX 主题里会被复用的方式
- 指出本卡知识解决了某个具体工程问题（带 incident / PR 引用）

#### Scenario: 三种合规工程对照

- **WHEN** NOTES.md 工程对照段 ≥ 100 字且满足上述三选一
- **THEN** 该卡可推 done

#### Scenario: 工程对照合规但其他要素缺失

- **WHEN** 工程对照合格但 commit 缺失或 NOTES.md 太薄（< 80 字总长）
- **THEN** 申请 SHALL 被驳回，反例档案标注具体原因

