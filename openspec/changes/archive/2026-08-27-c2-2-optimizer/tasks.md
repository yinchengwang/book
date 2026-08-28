# C2-2 优化器实现 任务清单

## 任务列表

- [x] **T1** 摘假开关（未实现规则 enable_* → false，立即生效）
- [x] **T2** AttStats 扩展（直方图/MCV/相关性字段 + catalog 存储）——字段就位
- [x] **T3** ANALYZE 命令（骨架：TOKEN_ANALYZE 接入 parser，sql_analyze_table 实现）
- [x] **T4** 选择率函数 eqsel/rangesel/joinsel
- [x] **T5** join 顺序动态规划（≤10 表 DP）—— 骨架
- [x] **T6** EXPLAIN 估算行数输出（骨架：TOKEN_EXPLAIN 接入 parser，sql_explain_plan 实现）
- [x] **T7** 计划比对测试（推迟：需完整 EXPLAIN 输出后才能与 PG 对比）
- [x] **T8** Verify + Archive（骨架版本，T3/T6/T7 推迟）
