# S9 — RebuildStrategy.decide 业务逻辑修复

## What Changes

S8 续做激活 vector_delete_test 后，42 个 vector_index 测试中有 2 个失败：

- `RebuildStrategyTest.HighDeletedRatio`：50% 删除比例（5000/10000）期望 `REBUILD_DECISION_REBUILD`，实际 `REBUILD_DECISION_REPAIR`
- `RebuildStrategyTest.CustomConfig`：40% 删除 + prefer_rebuild=true 期望 `REBUILD_DECISION_REBUILD`，实际 `REBUILD_DECISION_REPAIR`

**根因**：`engineering/src/db/index/vector_index/delete/rebuild_strategy.c` 中 `rebuild_strategy_decide` 逻辑：

```c
// 原逻辑
if (ratio_triggered && count_triggered) {
    return REBUILD_DECISION_REBUILD;  // 需要 ratio AND count 双触发
}

if (ratio_triggered || count_triggered) {
    if (cfg->prefer_rebuild) {
        return REBUILD_DECISION_REBUILD;
    }
    return REBUILD_DECISION_REPAIR;  // 单触发默认 Repair
}
```

- `HighDeletedRatio`: ratio=50%>30% 但 count=5000<10000 → 单触发 + 不 prefer → 返回 REPAIR（期望 REBUILD）
- `CustomConfig`: 40%<50% + 400<10000 → 0 触发 → prefer_rebuild 不生效 → 返回 REPAIR（期望 REBUILD）

**变更内容**：

1. **修复决策树**：把"任一条件触发即建议重建"作为标准行为，把 prefer_rebuild 的语义改为"允许在未达阈值时主动升级到 Rebuild"
2. **变更点**：`engineering/src/db/index/vector_index/delete/rebuild_strategy.c` 函数 `rebuild_strategy_decide`

## Why

**α 价值（工程作品集）**：
- vector_index 是 MiniVecDB 核心组件；删除路径的策略决策逻辑应有 100% 测试通过
- 修复让 42/42 tests 通过，CI 绿
- 暴露已存在的业务逻辑 bug 是个隐性问题——S9 让它显式化

**前置依赖**：
- S8 已让 gtest 链路恢复
- S8 已暴露 2 个 RebuildStrategyTest 失败（这就是 S9 修复目标）

## Scope

**包含**：
- 改 `rebuild_strategy_decide` 让测试期望真实反映业务语义
- 保留所有现有 8 个 RebuildStrategyTest 原样不修改
- 验证 42 个 vector_index 测试 100% 通过

**不包含**：
- ❌ 重构 rebuild_strategy 接口（保持 ABI 兼容）
- ❌ 添加新的测试用例（业务修复而非增测试）
- ❌ 修复 vector_index 其他子模块

## 新策略语义

| 触发情况 | 决策 |
|---|---|
| `deleted_count <= 0` 或 `total_count <= 0` | NONE |
| `ratio_triggered`（达 ratio 阈值） | REBUILD |
| `count_triggered`（达 min count） | REBUILD |
| `prefer_rebuild && deleted_count > 0` | REBUILD（即使两个阈值都未达到） |
| 其他 | REPAIR |

这与测试期望语义一致：
- `HighDeletedRatio`: ratio 50%>30% → REBUILD ✓
- `CustomConfig`: 40%<50% + 400<10000 + prefer_rebuild=true → REBUILD ✓
- `LowDeletedRatio`: 1%<30% + 100<10000 + prefer=false → REPAIR ✓
- `NoDeleted`: deleted=0 → NONE ✓
- `NullStrategy`: 用默认策略，100/1000=10%<30% + 100<10000 → REPAIR ✓

## Risk

| 风险 | 概率 | 缓解 |
|---|---|---|
| 修复后影响生产代码的 rebuild 决策路径 | 中 | 测试覆盖了所有测试场景（NoDeleted/LowDeletedRatio/DeletedRatioCalculation/ZeroTotal/NullStrategy），只要测试通过即视为语义正确 |
| 其他模块内部调用 rebuild_strategy_decide 受影响 | 低 | grep 显示只在测试和 rebuild_strategy.c 内部使用 |
