# C2-4 Timeseries 增量压缩与乱序处理 Proposal

## Why

差距分析 05 卷发现：`ts_compress_add` 在 add 阶段存原始 16B/点（`ts_compress.c:148-149`），压缩只在 flush（`:175`）——热路径内存无压缩收益；满块路径 `:144-146` 静默返回 -1；乱序写入处理未核（IoT 时钟漂移是常态）；无 WAL 覆盖（C0-2 处理）；连续聚合无 DROP/ALTER 入口。

## What Changes

- 增量编码器：每点即时 delta-of-delta（时间戳）+ XOR（值）追加位流，热路径即时压缩
- 满块路径：显式 flush + retry，失败返回 DBERR_FULL
- 乱序处理：按时间桶分段 merge-on-read——迟到数据进旁路缓冲，查询时归并
- 列式块优化：每列独立编码（RLE/dictionary/Gorilla 按列型选择）
- 连续聚合 ALTER/DROP API
- 专用函数：derivative/rate/percentile/first/last

## Capabilities

| 能力 | 交付 |
|------|------|
| 热路径压缩 | 内存占用 ≥5x 下降（基准测试） |
| 乱序容忍 | 迟到数据查询结果与顺序写入一致 |
| 连续聚合 | ALTER/DROP 生命周期完整 |
| 专用函数 | derivative/rate/percentile/first/last |

## Impact

- 修改：ts_compress.c、ts_segment.c、ts_columnar.c、ts_continuous_agg.c、ts_sql_functions.c
- 新增：乱序测试、压缩基准
- 预计 5-6 个 commit
- 依赖：C0-2
