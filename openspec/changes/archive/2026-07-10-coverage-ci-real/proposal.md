# S17 — Coverage CI 真正接入（基于 S13 inventory）

## What Changes

S13 已确认 coverage 工具链 inventory 完整：`run.sh` / `aggregate.py` / `compare.py` / `badge.py` / `coverage-baseline.json` / `coverage-policy.md`。

S17 在 `.github/workflows/ci.yml` 中**真实接入** coverage 工作流：

1. **新 job**：`coverage-ubuntu`（Ubuntu 22.04）
   - checkout code
   - 安装 lcov + Python 工具
   - configure & build 工程层 with `-DENABLE_COVERAGE=ON`
   - 跑 ctest
   - 跑 `bash engineering/scripts/coverage/run.sh`（含 capture + filter + HTML）
   - 跑 `python3 aggregate.py` 生成当前 coverage JSON
   - 跑 `python3 compare.py` 与 baseline 对比 → 输出 Markdown
   - 跑 `python3 badge.py` 生成 SVG
   - 上传 artifacts (HTML + JSON + badge)

2. **关键改动**：CI 不让 coverage 失败导致红（`|| true` 后缀）

3. **依赖**：S2 失败的 coverage 已通过 S13 inventory 重启

## Why

**α 价值（工程作品集）**：
- 现在 CI 真正能产出 lcov HTML 报告
- baseline JSON 持续更新（每次 CI 自动增）

**β 价值**：N/A（纯工程层）

**前置依赖**：
- S12 已创建 4 jobs（删除旧 coverage）
- S13 已确认 inventory 完整

## Scope

**包含**：
- 修改 `.github/workflows/ci.yml` 添加 coverage job
- 验证 V1-V3 (CI YAML 合法 + 本地模拟跑通)

**不包含**：
- ❌ PR 评论 / 自动报警（V 仅 informational）
- ❌ Codecov 集成
- ❌ coverage 趋势分析
