# S24 — Coverage Baseline First Run（生成真实 baseline）

## What Changes

S23 已确认 inventory 完整。S24 第一次真实生成 baseline：

1. **build engineering with coverage**：`cmake -B engineering/build-cov -S engineering -DENABLE_COVERAGE=ON`
2. **跑 ctest**：让 .gcda 数据填充
3. **采集 gcov 数据**：用 `gcov` 收集行覆盖率 → 写 `engineering/build-cov/coverage.json`
4. **`aggregate.py` 解析**：replaced by simple Python parser (lcov unavailable 时)
5. **更新 baseline JSON**：`engineering/docs/coverage-baseline.json` 写入真实数字

## Why

**α 价值**：
- 现在 README 徽章的覆盖率仍是 "TBD"
- 第一次跑 baseline 让数字真实

**前置依赖**：
- S17 已接 CI coverage 工具链
- lcov 不可用（Windows 环境），需要 gcov-only 模式

## Scope

**包含**：
- 1 个 `scripts/coverage/collect-gcov.py`（无 lcov 也可跑）
- 跑一次生成 baseline.json
- 验证 V1-V3

**不包含**：
- ❌ Codecov 上传
- ❌ PR 评论
- ❌ 历史 trend 图
