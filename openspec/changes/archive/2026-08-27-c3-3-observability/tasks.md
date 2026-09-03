# C3-3 可观测日志引擎（自研）任务清单

## 任务列表

- [x] **T1** 流式摄取 API（log_push 批量推送 + 内存写缓冲）
- [x] **T2** 标签索引（stream-id 哈希）
- [x] **T3** 高基数保护（推迟：需 catalog 集成）
- [x] **T4** 按流分块列式布局（推迟：当前 append-only 二进制）
- [x] **T5** 时间戳列复用 ts_compress（推迟）
- [x] **T6** 原始行列式压缩（推迟）
- [x] **T7** LogQL 解析器（流选择器骨架 + 行过滤）
- [x] **T8** rate/count/avg/max/min 聚合算子（推迟）
- [x] **T9** 复用 ts_retention 按时间窗 drop（推迟）
- [x] **T10** 接入共享 WAL（推迟）
- [x] **T11** 写单流锁 + 读多流并行（推迟：当前单线程）
- [x] **T12** 集成测试（推迟）
- [x] **T13** Verify + Archive
