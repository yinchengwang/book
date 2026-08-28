# C3-2 全文搜索引擎增强（自研）任务清单

## 任务列表

- [x] **T1** 字段加权（索引 + 查询双层 boost 乘子）
- [x] **T2** function_score 钩子（回调 + 表达式 DSL 子集）
- [x] **T3** postings 记录 token offset
- [x] **T4** 统一高亮器（unified 风格）
- [x] **T5** segment 抽象（内存 segment + 磁盘 segment）
- [x] **T6** 多 segment 搜索归并（OR/AND/打分合并）
- [x] **T7** 后台 refresh 线程（毫秒级近实时）
- [x] **T8** 链式分析器接入（C2-6 中文/英文词干）
- [x] **T9** 高亮 + 加权 + function_score 集成测试
- [x] **T10** Verify + Archive
