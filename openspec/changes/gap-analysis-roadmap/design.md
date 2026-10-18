# Gap-Analysis 路线图 — 设计

**关联 proposal：** `proposal.md`
**关联总报告：** `docs/gap-analysis/README.md`（§1 摘要 / §2 差距 / §3 优势 / §4 RAG+Agent / §5 路线图 / §6 风险）

## 数据契约

### 追溯表结构

13 篇子报告的 OPSX 追溯表采用统一格式：

```markdown
| commit | 子报告 | 大小 | 提交日期 | §7 补齐项数 |
|--------|--------|------|----------|-----------|
| hash   | name   | KB   | YYYY-MM-DD | N         |
```

每行 commit 链必须能在 git log 中定位（`git log --pretty=format:"%h" -- <file>`）。

### P0 / P1 / P2 任务卡格式

```markdown
- [ ] **P0 #1 — <标题>** (子报告 §7, ~X 人周)
  - 来源：`docs/gap-analysis/<子报告>.md` §7
  - 范围：1-2 个文件 / 1 个新组件
  - 验收：1 个新测试 + 1 个 e2e 验证
  - 关联：上游 <commit> / 下游 <下一步>
```

## 组织原则

1. **追溯模式**：13 篇子报告 commit 链**不重写**，仅在 OPSX 中文档化引用（避免 force-push 改写 main 稳定历史）
2. **路线图粒度**：25 项 P0 各为 1 个 task，每项标注子报告 §7 来源 + 投入估算
3. **实施隔离**：每个 P0 实施时**独立建子 OPSX 变更**（`p1b-xxx`, `p1c-xxx`, `p2a-xxx` ...），本变更仅做 backlog 追踪
4. **完成度统计**：每月底更新一次 backlog 进度（哪些 P0 已开子 OPSX、哪些已完成、哪些推迟）

## 风险

- **backlog 膨胀**：P0/P1/P2 共 65 项，列表本身可能成为"挂着不动"的尾巴
  - 缓解：每项标注子报告 §7 来源 + 投入估算，超过 12 个月未动自动降级
- **重复 OPSX 变更**：每个 P0 单独开 OPSX 容易产生 N 个小变更
  - 缓解：1-2 周工作量的 P0 可合并到月级 OPSX 变更（如 `p2-month1-batch`）
- **P1-A 经验教训**：路线图首个落地 P1-A 触发了 6 处 cmake 修复 + gtest submodule init 等基础设施问题
  - 缓解：后续 P0 优先做基础设施（性能分析、覆盖率、CI 矩阵），再上特性

## 与 P1-A 关联

`p1a-vector-wal-complete` 已于 2026-10-16 合入 main（commit `3939cc03c`），是本路线图的**首个落地**。后续 P0 候选：
- `p1b-checkpoint-manager`（Vector CheckpointManager 完善，~2 周）
- `p1c-chaos-recovery`（Chaos 恢复，~3 周）
- 各模态 P0 优化（按子报告优先级 6 个月排程）

## 不在本变更范围

- 任何源代码修改（OPSX 文档管理）
- 任何 commit 重写或 force-push
- 13 篇子报告内容修订
