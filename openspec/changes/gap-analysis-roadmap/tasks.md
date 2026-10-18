# Gap-Analysis 路线图 — 任务清单

> **For agentic workers:** 本 OPSX 变更是**路线图跟踪 + 追溯已交付物**性质，**不实施任何代码**。
> 每个 P0 / P1 / P2 实施时独立开子 OPSX 变更（如 `p1b-checkpoint-manager`），本变更仅做 backlog 登记与进度跟踪。

---

## 阶段 1：追溯已交付物（已完，无工作）

> 13 篇子报告 + 总报告 + README 链接已 commit 到 main 分支，本阶段仅做文档化。

- [x] **Task 1.1** — 13 篇子报告 commit 链追溯登记
  - 13 篇：relational / key-value / document / columnar / time-series / search / vector / graph / stream / embedded / extension / rag / agent
  - 第一篇：`13b5fbd2d` relational (13:54)
  - 末篇：`d583886bf` agent (16:09)
  - 验证：`git log --reverse -- docs/gap-analysis/*.md` 全部 13 行 + 总报告 1 行 + README 链接 1 行 + 清理 1 行

- [x] **Task 1.2** — 总报告 `docs/gap-analysis/README.md`（58KB / 6 节）
  - commit: `e5fd7f029`
  - 6 节：§1 摘要 / §2 差距诊断 / §3 优势亮点 / §4 RAG+Agent / §5 6 月路线图 / §6 风险与陷阱
  - 验收：5-7 条差距诊断 / 3-5 条优势 / 6 月路线图 / 5-7 条风险 — 全部已勾选（README §8 验收清单 7/7 通过）

- [x] **Task 1.3** — README 直达链接追加到 wiki
  - commit: `a31496645`（三个 wiki README 末尾追加 gap-analysis 直达链接）
  - `.gitkeep` 清理：`b333b52fa`

- [x] **Task 1.4** — 关联已合入的 P1-A
  - commit: `3939cc03c`（p1a-vector-wal-complete 合入 main）
  - 首个路线图落地：Vector WAL 完整化 4 个独立测试 + vdb_e2e WALRecovery/CheckpointTest 全绿

## 阶段 2：路线图 backlog 登记

> 25 项 P0 + 30 项 P1 + 10 项 P2 来自 `docs/gap-analysis/README.md` §5.2-5.7 + §5.10。
> 本阶段只登记，**不实施**。实施时开子 OPSX 变更。

### 阶段 2.1：25 项 P0 路线图（按月排程，§5.2-5.7）

> 投入合计：约 80-100 人周 / 6 个月 1 人精排 + 适当加班
> 来源：每个 P0 标注子报告 §7 行号引用

- [ ] **Task 2.1.1 — 第 1 月 P0（基础设施集中：关系 + KV + 文档）**
  - 子报告：relational / key-value / document §7
  - P0 数量：~5 项
  - 投入：~16-20 人周
  - 实施：开 `p1b-infrastructure-batch` 子 OPSX（按月打包）

- [ ] **Task 2.1.2 — 第 2 月 P0（核心协议 + 优化器）**
  - 子报告：relational / sql / key-value §7
  - P0 数量：~4 项
  - 投入：~14-18 人周

- [ ] **Task 2.1.3 — 第 3 月 P0（向量 + 文档协议 + 列存引擎家族）**
  - 子报告：vector / document / columnar §7
  - P0 数量：~4 项
  - 投入：~14-18 人周

- [ ] **Task 2.1.4 — 第 4 月 P0（向量算法 + 搜索 NRT + RAG 编排协议）**
  - 子报告：vector / search / rag §7
  - P0 数量：~4 项
  - 投入：~14-18 人周

- [ ] **Task 2.1.5 — 第 5 月 P0（RAG 框架 + Agent 编排）**
  - 子报告：rag / agent §7
  - P0 数量：~4 项
  - 投入：~12-16 人周

- [ ] **Task 2.1.6 — 第 6 月 P0（生态适配 + 商业观测）**
  - 子报告：rag / agent / extension §7
  - P0 数量：~4 项
  - 投入：~10-14 人周

### 阶段 2.2：候选 P0 子 OPSX 变更（高优先级，已识别）

> 来自 P1-A 验收 result.md + 本路线图 §5.2-5.3

- [ ] **Task 2.2.1 — `p1b-checkpoint-manager`（Vector CheckpointManager 完善）**
  - 背景：vdb_chaos_test 3 个失败（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash）很可能与 CheckpointManager 行为相关
  - 范围：~2 周 / `engineering/src/db/storage/vector/vector_wal.c` + checkpoint_lsn 状态机
  - 验收：vdb_chaos 3 失败用例转绿 + 1 个新测试覆盖 checkpoint_lsn 起点

- [ ] **Task 2.2.2 — `p1c-chaos-recovery`（Chaos 恢复加固）**
  - 背景：vdb_chaos 3 失败（IndexRebuildAfterKill / MultipleCrashRecovery / ConcurrentWriteWithCrash）独立加固
  - 范围：~3 周 / vector 索引重建 / 多崩溃幂等 / 并发写顺序保证
  - 验收：vdb_chaos 6 用例全绿 + chaos_e2e 套件扩展

- [ ] **Task 2.2.3 — pre-existing 修复（vdb_e2e SearchVectors / DropCollection）**
  - 背景：与 WAL 无关，单独登记
  - 范围：~3-5 天
  - 验收：vdb_e2e 13/13 全绿

### 阶段 2.3：30 项 P1 backlog（推迟到下季度，§5.10）

- [ ] **Task 2.3.1 — P1 推迟项登记（30 项 / 80000-120000 行 / 40-60 人周）**
  - 来源：`docs/gap-analysis/README.md` §5.10
  - 暂不实施，按子报告分组：
    - relational P1：~5 项
    - key-value P1：~3 项
    - document P1：~3 项
    - columnar P1：~3 项
    - time-series P1：~2 项
    - search P1：~2 项
    - vector P1：~3 项
    - graph P1：~2 项
    - stream P1：~2 项
    - embedded P1：~2 项
    - extension P1：~1 项
    - rag P1：~1 项
    - agent P1：~1 项

### 阶段 2.4：10 项 P2 backlog（暂放，§5.10 末尾）

- [ ] **Task 2.4.1 — P2 暂放项登记（10 项）**
  - 来源：`docs/gap-analysis/README.md` §5.10
  - 暂不实施，长期 backlog

## 阶段 3：进度跟踪机制

> 本变更作为路线图 tracker，**长期存在**直到所有 P0 完成（或被推迟/取消）

- [ ] **Task 3.1** — 每月底更新 backlog 进度
  - 哪些 P0 已开子 OPSX 变更
  - 哪些 P0 已完成（含 commit hash）
  - 哪些 P0 推迟到下月 / 下季度
  - 更新本 tasks.md 的 `[ ]` → `[x]` 状态

- [ ] **Task 3.2** — 季度回顾
  - P0 完成率统计
  - P1 重新评估
  - 路线图调整（新增 / 删除 / 降级）

## 阶段 4：归档

- [ ] **Task 4.1** — 当所有 P0 完成或路线图整体调整时，本变更归档到 `openspec/changes/archive/`
  - 归档要求：所有 P0 任务有最终状态（done / dropped / deferred）
  - 归档 commit：单独 1 个 commit 移动目录

---

## 完成状态

- [x] Task 1.1-1.4：追溯已交付物（13 篇子报告 + 总报告 + 链接 + P1-A 关联）
- [ ] Task 2.1.1-2.1.6：25 项 P0 路线图登记（待开子 OPSX 实施）
- [ ] Task 2.2.1-2.2.3：高优先级候选 P0（p1b / p1c / pre-existing）
- [ ] Task 2.3.1：30 项 P1 backlog
- [ ] Task 2.4.1：10 项 P2 backlog
- [ ] Task 3.1-3.2：进度跟踪
- [ ] Task 4.1：归档

## 关联 OPSX 变更

- **已完成：** `p1a-vector-wal-complete`（首个落地，main `3939cc03c`）
- **候选 next：** `p1b-checkpoint-manager` / `p1c-chaos-recovery`（按 P0 路线图第 1 月）
