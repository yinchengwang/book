# C3-3 可观测日志引擎（自研）任务清单

## 任务列表

- [ ] **T1** 流式摄取 API（log_push 批量推送 + 内存写缓冲）
- [ ] **T2** 标签索引（label → stream-id 倒排，复用 doc_inverted）
- [ ] **T3** 高基数保护（阈值 GUC + 降级全扫 + 告警）
- [ ] **T4** 按流分块列式布局（chunk = stream + 时间窗）
- [ ] **T5** 时间戳列复用 ts_compress（Gorilla）
- [ ] **T6** 原始行列式压缩（RLE/zstd-like）
- [ ] **T7** LogQL 解析器（流选择器 + 行过滤 + 字段提取 + 聚合）
- [ ] **T8** rate/count/avg/max/min 聚合算子
- [ ] **T9** 复用 ts_retention 按时间窗 drop
- [ ] **T10** 接入共享 WAL（log append 记录）
- [ ] **T11** 写单流锁 + 读多流并行（mmdb_lock）
- [ ] **T12** 集成测试（吞吐 ≥10K lines/s + 重启恢复）
- [ ] **T13** Verify + Archive
