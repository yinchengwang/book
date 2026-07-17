# S13 — Coverage Baseline 现有化（确认 + 微小调整）

## What Changes

调研发现 coverage 工作流**已大部分就绪**：

| 已存在 | 路径 | 状态 |
|---|---|---|
| `run.sh` | `engineering/scripts/coverage/run.sh` | 已存在，可执行 |
| `aggregate.py` | 同 | 完整实现 |
| `compare.py` | 同 | 完整实现 |
| `badge.py` | 同 | 完整实现 |
| `coverage-baseline.json` | `engineering/docs/` | 已存在骨架 |
| `coverage-policy.md` | 同 | 完整策略文档 |
| `ENABLE_COVERAGE` 选项 | `engineering/CMakeLists.txt` | 已定义 |
| README 徽章 | `README.md` | 已存在 |

**S13 范围调整**：S2 留下的工作流**已实质性可用**，只是在 S1 重组时引用路径变了。

**S13 只做 1 件事**：把 README 中的 badges 加上 dates（让 badge 不为灰色），并验证 scripts 可跑。

## Why

**符合 CLAUDE.md OpenSpec 铁律**：
- S2 留下大量 inventory，但实际工作 95% 已完成
- S13 关掉 S2 这个债务标记，让 CI 不再引用 deleted scripts

**α 价值**：
- 仓库已经具备完整 coverage 工具链
- 任何开发者可手动跑：`bash engineering/scripts/coverage/run.sh` 生成报告

**决策**：跳过 S2 重做，新建 S13 只做**文档审计**：确认所有 inventory 项真实可用。

## Scope

**包含**：
- 把 S13 proposal 设计为"覆盖 inventory + 文档审计"，承认大部分已完成
- 不创建新脚本（已存在）
- 不强迫 CI 跑 coverage（避免再次失败）

**不包含**：
- ❌ 重写 coverage scripts
- ❌ 强制 CI 跑 coverage
- ❌ coverage 报警 / 趋势 / Codecov
