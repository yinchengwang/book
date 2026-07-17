# P2 — Coverage HTML 报告（lcov 集成）

## What Changes

S17 + S24 已接 CI coverage 工具链（S17 写 CI、aggregate-gcov.py fallback）。P2 升级到**完整 HTML 报告**：

1. **CI Ubuntu job**（已有）安装 lcov
2. **run.sh** 改用 `lcov --capture --genhtml` 流程
3. **本地 Windows** 仍用 gcov-only 模式（备选）
4. **新脚本** `lcov-aggregate.sh`：合并多 .info，按工程层 / 学习层过滤
5. **README 链接** `engineering/coverage-html/index.html`

## Why

**α 价值**：CI 自动产生可查看的覆盖率 HTML，让 reviewer 一目了然

**前置依赖**：
- S24 baseline 已生成（gcov-only）
- aggregate-gcov.py 已运行

## Scope

**包含**：
- 修改 `run.sh`：检测 lcov 可用则用 lcov，fallback gcov
- `lcov-aggregate.sh`：仅 `lcov + genhtml` 路径
- README 链接

**不包含**：
- ❌ genhtml 在 Windows（无 lcov）
- ❌ Codecov 上传
- ❌ PR 评论
