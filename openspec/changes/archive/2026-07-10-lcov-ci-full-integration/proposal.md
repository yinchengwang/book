# P6 — lcov CI Full Integration

## What Changes

S17 已写 coverage-ubuntu job（用 lcov 跑），P2 已写 lcov-aggregate.sh + run.sh 双模式。P6 完成**完整 CI 集成**：

1. **CI 中 coverage job** 真正跑 lcov-aggregate.sh（不是 gcov-only）
2. **生成 coverage-current.json**：跑 aggregate.py 输出 `engineering/coverage-current.json`
3. **跑 check-threshold.py**：实施 P0 阈值（80% / 50%）
4. **artifacts**：HTML + JSON + diff + badge

## Why

**α 价值**：CI 自动显示覆盖率变化

**前置依赖**：
- S17 已写 CI
- P2 run.sh 双模式

## Scope

**包含**：
- 修改 `.github/workflows/ci.yml`：coverage job 跑 run.sh
- README 链接
- V1-V3 验证

**不包含**：
- ❌ Codecov 上传
- ❌ PR 评论
