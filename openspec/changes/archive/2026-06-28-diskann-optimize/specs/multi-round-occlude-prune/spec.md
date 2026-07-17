## ADDED Requirements

### Requirement: 多轮递增 alpha 的 RobustPrune

`diskann_robust_prune` SHALL 支持多轮递增 alpha 的剪枝策略。函数签名变更为：
```
int diskann_robust_prune(const diskann_t *index, int32_t node_id,
                         diskann_scored_t *pool, int32_t pool_count,
                         int32_t *result_ids, int32_t *result_count,
                         int32_t occlude_rounds);
```

当 `occlude_rounds > 0` 时，系统 SHALL 执行指定轮数的递增 alpha 剪枝。当 `occlude_rounds == 0` 时，系统 SHALL 自动计算轮数 = `ceil(log(target_alpha) / log(1.2))`，`target_alpha` 取 `index->alpha`。

每轮的 `cur_alpha` SHALL 从 `1.0f` 开始，每轮乘以 `1.2f`，直至达到或超过 `target_alpha`。最后一轮的 `cur_alpha` SHALL 被 clamp 为 `target_alpha`。

每轮 SHALL 重新初始化 `occlude_factor` 数组（全部设为 `true`/未遮挡），对按距离排序的候选池从头筛选。

#### Scenario: 默认参数下执行多轮剪枝

- **WHEN** 调用 `diskann_robust_prune(index, node_id, pool, pool_count, result_ids, result_count, 0)`，且 `index->alpha = 1.2f`
- **THEN** 系统自动计算轮数 = ceil(log(1.2)/log(1.2)) = 1 轮，执行单轮 alpha=1.2 剪枝

#### Scenario: 高 alpha 值时执行多轮

- **WHEN** `index->alpha = 1.5f` 且 `occlude_rounds = 0`
- **THEN** 系统自动计算轮数 = ceil(log(1.5)/log(1.2)) = 3 轮，依次使用 alpha=1.0, alpha=1.2, alpha=1.5 执行剪枝

#### Scenario: 显式指定轮数

- **WHEN** 调用时 `occlude_rounds = 5`
- **THEN** 系统执行恰好 5 轮，cur_alpha 序列为 1.0, 1.2, 1.44, 1.728，第5轮 clamp 到 `index->alpha`

#### Scenario: 降级到单轮行为保持向后兼容

- **WHEN** `occlude_rounds = 1`
- **THEN** 系统执行恰好 1 轮，cur_alpha = `index->alpha`，行为等同于旧版单轮剪枝

### Requirement: 剪枝后饱和度回填不变

每轮剪枝后，若 `result_count < index->index_size`，系统 SHALL 继续从候选池中按距离顺序选取未被选中且不等于 `node_id` 的节点回填，直到达到 `index->index_size` 或候选池耗尽。此行为与旧版 `diskann_robust_prune` 的回填逻辑一致。

#### Scenario: 多轮剪枝后邻居不足时回填

- **WHEN** 所有轮次执行完毕后 `result_count < index->index_size`
- **THEN** 系统从候选池尾部按距离顺序补充未选中的节点，直到填满或候选池耗尽

### Requirement: 所有调用方适配新签名

所有调用 `diskann_robust_prune` 的位置（`diskann_build.c`、`diskann_insert.c`、`diskann_graph.c`）SHALL 传递 `occlude_rounds = 0`（使用自动计算），保持行为一致性。

#### Scenario: 构建流程中的调用适配

- **WHEN** `diskann_index_build` 执行全量构建
- **THEN** 所有 `diskann_robust_prune` 调用传递 `occlude_rounds = 0`，自动使用多轮剪枝
