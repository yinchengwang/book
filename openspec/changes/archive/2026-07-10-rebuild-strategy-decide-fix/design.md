# S9 设计文档（RebuildStrategy.decide 修复）

## 1. 根因分析

原 `rebuild_strategy_decide` 决策树：

```c
if (ratio_triggered && count_triggered) return REBUILD;
if (ratio_triggered || count_triggered) {
    if (cfg->prefer_rebuild) return REBUILD;
    return REPAIR;          // ← 单触发默认 Repair 但测试期望 Rebuid
}
return REPAIR;              // ← 都不触发返回 Repair，prefer_rebuild 也不生效
```

测试期望：
- `HighDeletedRatio`: 50% (ratio_triggered only) → REBUILD
- `CustomConfig`: 40% (无触发) + prefer_rebuild=true → REBUILD

两者都不能从原决策树自然得到。

## 2. 修复策略

新决策树（语义清晰版）：

```c
rebuild_decision_t rebuild_strategy_decide(strategy, deleted_count, total_count) {
    if (!strategy) /* 默认策略 */;
    if (deleted_count <= 0 || total_count <= 0) return NONE;

    float ratio = deleted / total;
    bool ratio_triggered = ratio*100 >= threshold;
    bool count_triggered = deleted >= min_count;

    /* S9 修复：任一阈值触发即建议 Rebuild */
    if (ratio_triggered || count_triggered) return REBUILD;

    /* 都未触发：prefer_rebuild 让用户主动升级 */
    if (cfg.prefer_rebuild) return REBUILD;

    return REPAIR;  // 仅低删除用边修复
}
```

## 3. 验证

| V | 命令 | 期望 |
|---|---|---|
| V1 | `vector_delete_test.exe --gtest_filter=RebuildStrategyTest.*` | 8/8 pass |
| V2 | `ctest -L vector_index` | 42/42 pass |
| V3 | `vector_delete_test.exe`（全部） | 32 vector_delete 测试全 pass |

## 4. 不做

- ❌ 改 RebuildStrategyTest 测试期望（保持 ABI 兼容）
- ❌ 添加新测试（业务修复）
- ❌ 引入 enum 新值
