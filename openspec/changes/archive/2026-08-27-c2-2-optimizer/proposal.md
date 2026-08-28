# C2-2 优化器实现 Proposal

## Why

差距分析 02 卷发现优化器是空壳：`opt_join_order` 等 5 条规则全是 TODO 桩（`optimizer.c:199-304`，递归遍历后原样返回计划）但 `enable_join_order = true` 制造已优化假象（`:83-84`）；选择率估算仅等值 `1/ndistinct`（`cost.c:205-216`），无直方图/MCV/相关性。对标 PostgreSQL 动态规划 + GEQO + selfuncs。

## What Changes

- 摘假开关：未实现的优化规则 `enable_*` 默认 false（诚实化，立即生效）
- join 顺序动态规划：≤10 表精确枚举（自底向上 DP，等价类处理），>10 表维持原始顺序
- AttStats 扩展：直方图（等深 100 桶）+ MCV（最常见值）+ 相关性
- ANALYZE 命令：采样统计填充 AttStats
- 选择率函数：eqsel（直方图+MCV）/rangesel（直方图插值）/joinsel（ndistinct 比）
- EXPLAIN 显示估算行数与真实行数对比

## Capabilities

| 能力 | 交付 |
|------|------|
| join 顺序 | 3-10 表 TPC-H 风格查询计划接近 PG（人工比对 EXPLAIN） |
| 选择率 | 等值/范围/连接三类选择率有统计支撑 |
| 统计 | ANALYZE 命令 + 统计视图 |
| 诚实开关 | enable_* 与实际能力一致 |

## Impact

- 修改：optimizer.c、cost.c、parser/（ANALYZE）、catalog（统计存储）
- 新增：优化器测试（计划比对）
- 预计 6-7 个 commit
- 依赖：C0-3
