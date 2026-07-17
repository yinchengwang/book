## ADDED Requirements

### Requirement: 渐进搜索扩宽 — 结果不足时翻倍重新搜索

`diskann_index_search` SHALL 支持渐进搜索扩宽机制。新增 `max_iterations` 参数：

```c
int32_t diskann_index_search(diskann_t *index, const float *query, int32_t k,
                             int32_t search_list_size, int32_t max_iterations,
                             float *distances, int32_t *labels);
```

当 `max_iterations <= 0` 时，系统 SHALL 执行一次搜索（向后兼容现有行为）。当 `max_iterations > 0` 且首轮搜索返回的有效结果数 < `k` 时，系统 SHALL 将 `search_list_size` 翻倍后重新执行图搜索，最多重复 `max_iterations` 轮。

#### Scenario: 首轮搜索返回足够结果时不进行扩宽

- **WHEN** `max_iterations = 3`，首轮搜索返回 ≥ `k` 个有效结果
- **THEN** 系统不进行第二轮搜索，直接返回首轮结果

#### Scenario: 首轮不足但翻倍后足够

- **WHEN** `max_iterations = 3`，首轮搜索返回 < `k` 个结果，翻倍 `search_list_size` 后第二轮返回 ≥ `k` 个结果
- **THEN** 系统使用第二轮的结果，不再进行第三轮

#### Scenario: 达到最大轮次仍不足

- **WHEN** `max_iterations = 3`，三轮搜索后有效结果仍 < `k`
- **THEN** 系统返回当前收集到的最优结果（可能少于 `k`），不足位置填充 `FLT_MAX` 距离和 `-1` 标签

#### Scenario: search_list_size 超出活跃节点数时不再翻倍

- **WHEN** 翻倍后的 `search_list_size >= index->active_count`
- **THEN** 系统将 `search_list_size` clamp 为 `index->active_count`，执行最后一轮搜索后停止（不再继续翻倍）

#### Scenario: 向后兼容 — max_iterations <= 0

- **WHEN** `max_iterations <= 0`
- **THEN** 系统执行恰好一轮搜索，行为与旧版 `diskann_index_search` 完全一致

### Requirement: 搜索结果去重

渐进扩宽的多轮搜索可能产生重复候选。最终 top-k 结果 SHALL 按 id 去重（保留距离更小的版本），保证返回的 `k` 个标签互不相同。

#### Scenario: 两轮搜索重叠候选去重

- **WHEN** 第一轮和第二轮搜索的候选集中存在相同的 `id`
- **THEN** 系统在精确重排排序后，按 id 去重保留距离最小的实例
