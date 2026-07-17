# S9 Spec —— RebuildStrategy.decide 决策树

## 1. 决策语义

`rebuild_strategy_decide(strategy, deleted_count, total_count)` 必须返回 `REBUILD_DECISION_*`：

| 触发情况 | 决策 |
|---|---|
| `deleted_count <= 0` 或 `total_count <= 0` | `NONE` |
| `deleted_count/total_count*100 >= deleted_ratio_threshold` | `REBUILD` |
| `deleted_count >= min_deleted_count` | `REBUILD` |
| `prefer_rebuild == true` 且 `deleted_count > 0` | `REBUILD` |
| 其他 | `REPAIR` |

## 2. ABI 兼容

- `rebuild_strategy_*` 函数签名不变
- `rebuild_decision_t` 枚举值不变
- `rebuild_strategy_config_t` 字段不变（`deleted_ratio_threshold` / `min_deleted_count` / `prefer_rebuild`）

## 3. 不做

- ❌ 改测试期望（保持测试反映真实业务语义）
- ❌ 增加新决策枚举值
- ❌ 增加新测试用例
