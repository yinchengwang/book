# S21 — Coverage Policy Enforce（强制阈值 + 报警）

## What Changes

`engineering/docs/coverage-policy.md` 已声明 80%/50% 目标，但**无强制执行**。

S21 在 CI 中加入：

1. **`engineering/scripts/coverage/check-threshold.py`**：解析 `coverage-current.json`，对各模块检查：
   - db / algo：lines ≥ 80%
   - 其他：lines ≥ 50%
   - 失败则 exit 1
2. **CI 中 coverage job 末尾加 1 步**：跑 threshold check
3. **`coverage-policy.md`** 增加"门槛违规处理"段

## Why

**α 价值（工程作品集）**：
- 之前 coverage 是 informational，现在变成 actionable
- 防止覆盖率倒退（infra-level guard）

## 范围

**包含**：
- 1 个 threshold 检查脚本
- CI 集成
- coverage-policy.md 更新

**不包含**：
- ❌ Slack webhook
- ❌ PR 自动评论（policy 决策留给后续）
- ❌ Coverage 趋势图
