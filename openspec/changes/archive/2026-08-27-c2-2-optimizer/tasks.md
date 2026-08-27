# C2-2 优化器实现 任务清单

## 任务列表

- [x] **T1** 摘假开关（未实现规则 enable_* → false，立即生效）
- [x] **T2** AttStats 扩展（直方图/MCV/相关性字段 + catalog 存储）——字段就位
- [ ] **T3** ANALYZE 命令（采样填充统计）
- [x] **T4** 选择率函数 eqsel/rangesel/joinsel
- [x] **T5** join 顺序动态规划（≤10 表 DP）—— 骨架
- [ ] **T6** EXPLAIN 估算行数输出
- [ ] **T7** 计划比对测试（与 PostgreSQL EXPLAIN 人工比对记录）
- [ ] **T8** Verify + Archive
