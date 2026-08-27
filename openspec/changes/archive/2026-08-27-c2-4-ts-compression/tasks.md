# C2-4 Timeseries 增量压缩与乱序处理 任务清单

## 任务列表

- [ ] **T1** 增量编码器（delta-of-delta + XOR 位流，add 即时压缩）
- [ ] **T2** 满块路径显式 flush + DBERR_FULL（替换 ts_compress.c:144-146 静默 -1）
- [ ] **T3** 乱序 merge-on-read（旁路缓冲 + 查询归并）
- [ ] **T4** 乱序一致性测试（乱序写入 vs 顺序写入查询结果一致）
- [ ] **T5** 列式块每列独立编码（RLE/dictionary/Gorilla）
- [ ] **T6** 连续聚合 ALTER/DROP API
- [ ] **T7** derivative/rate/percentile/first/last 函数
- [ ] **T8** 压缩基准报告（≥5x）
- [ ] **T9** Verify + Archive
