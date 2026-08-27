# C3-2 全文搜索引擎增强（自研）任务清单

## 任务列表

- [ ] **T1** 字段加权（索引 + 查询双层 boost 乘子）
- [ ] **T2** function_score 钩子（回调 + 表达式 DSL 子集）
- [ ] **T3** postings 记录 token offset
- [ ] **T4** 统一高亮器（unified 风格）
- [ ] **T5** segment 抽象（内存 segment + 磁盘 segment）
- [ ] **T6** 多 segment 搜索归并（OR/AND/打分合并）
- [ ] **T7** 后台 refresh 线程（毫秒级近实时）
- [ ] **T8** 链式分析器接入（C2-6 中文/英文词干）
- [ ] **T9** 高亮 + 加权 + function_score 集成测试
- [ ] **T10** Verify + Archive
