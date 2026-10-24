# 优化器统计与代价规范（新增）

## 目的

让优化器从 TODO 桩转为可工作：选择率函数 + join DP + ANALYZE 统计。

## 要求

### REQ-1：假开关诚实化

未实现的优化规则（join_order/constant_folding 等）默认 `false`，LOG_WARN 提示后续变更接入。

### REQ-2：AttStats 字段

`AttStats` 必须包含：
- `ndistinct`（已有）
- `histogram_bounds`（等深 100 桶左右端点）
- `mcv_freq[]`（前 100 高频值频率）
- `correlation`（-1..1，物理位置与值的相关系数）

### REQ-3：ANALYZE

`ANALYZE [table_name]` 触发统计采样填充 catalog；无指定表时全库所有表采样。

### REQ-4：选择率

`eqsel(stats, "=")` → `min(1/ndistinct, mcv_freq_for_eq_value)`
`rangesel(stats, ">", "<", ...)` → histogram 线性插值
`joinsel(a, b)` → `1 / max(a.ndistinct, b.ndistinct)`

### REQ-5：join DP

≤10 表精确枚举（O(2^n) DP），>10 表退化为输入顺序。

### REQ-6：EXPLAIN

`EXPLAIN` 输出计划节点 + 估算行数 + 估算代价。