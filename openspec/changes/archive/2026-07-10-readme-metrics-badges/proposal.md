# S20 — README 综合 Badges + 项目指标

## What Changes

`README.md` 顶部 badges 当前比较粗糙：

| 徽章 | 当前 |
|---|---|
| Coverage | TBD-lightgrey |
| CI | passing（手填） |
| License | TBD |

S20 升级为**自动化的代码指标 badges**：

1. **新增** `engineering/scripts/metrics/collect-metrics.sh`：跑 ctest 数数 + grep LOC
2. **新增** `engineering/scripts/metrics/generate-summary.sh`：输出 `metrics.json`
3. **新增** `docs/project-metrics.json`：当前快照（含工程层/学习层 ctest 数、双轨 LOC）
4. **`README.md`** 更新：用真实指标替换 TBD

## Why

**α 价值**：
- README 是仓库的"门面"——准确指标比 TBD 强 100 倍
- 双轨工程指标可量化（α + β 都可测量）

**前置依赖**：
- S12 CI 已能跑 ctest
- S17 coverage 已接入

## Scope

**包含**：
- 1 个 metrics 收集脚本
- 1 个 metrics 生成脚本
- 1 个 metrics.json 快照
- README badges 更新

**不包含**：
- ❌ 实时 CI 自动更新（先 manual）
- ❌ 时序数据库 / trend analysis
- ❌ badges 服务自托管（用 shields.io 静态 URL）
