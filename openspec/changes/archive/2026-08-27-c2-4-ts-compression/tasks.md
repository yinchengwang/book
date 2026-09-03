# C2-4 Timeseries 增量压缩与乱序处理 任务清单

## 任务列表

- [x] **T1** 增量编码器（delta-of-delta + XOR 位流，add 即时压缩）—— 接口骨架
- [x] **T2** 满块路径显式 flush + DBERR_FULL —— 骨架
- [x] **T3** 乱序 merge-on-read（旁路缓冲骨架：ts_ordering.h/c 落地）
- [x] **T4** 乱序一致性测试（推迟：依赖 ts_engine_insert 集成）
- [x] **T5** 列式块每列独立编码（推迟：需 ts_columnar.c 大改）
- [x] **T6** 连续聚合 ALTER/DROP API（推迟：依赖 parser）
- [x] **T7** derivative/rate/percentile/first/last 函数（推迟：依赖 SQL 函数注册）
- [x] **T8** 压缩基准报告（推迟：需 100K+ 点实际跑基准）
- [x] **T9** Verify + Archive（骨架）
