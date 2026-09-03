# C2-4 Timeseries 增量压缩与乱序 设计文档

## 设计目标

修复 05 卷识别的两个核心缺陷：
1. 热路径不压缩：`ts_compress.c:148-149` 写入 16B/点（int64+double），flush 才压缩
2. 静默丢数据：满块路径 `:144-146` 返回 -1 不告警
3. 乱序写入未处理：IoT 时钟漂移常态

## 方案

### 1. 增量编码器（T1）

每点即时编码：
- 时间戳：delta-of-delta（DOD）：`dod = (cur_ts - prev_ts) - (prev_ts - prev_prev_ts)`
  → 64-bit → 紧凑位宽（VLE）
- 值：XOR with previous value → 紧凑位宽（leading/trailing zero 截位）

实现 `ts_compress_add_xor(stream, ts, value)`：
- block flush 时按 DOD + XOR 输出位流
- 读取端按相同规则解

### 2. 满块显式 flush（T2）

`ts_compress.c:144-146` 当前静默 -1。改为：
- 显式调 `ts_compress_flush()` 腾空间
- 若仍失败返回 `DBERR_FULL`

### 3. 乱序 merge-on-read（T3 骨架）

`ts_engine_insert` 维护 `out_of_order_buffer`（红黑树或简单排序数组）。
查询时归并 in-order 数据 + 排序后的 out-of-order 数据。

### 4. 乱序一致性测试（T4）

测试：插入 (ts,val) 乱序序列 → 查询 topK 应与顺序插入结果一致。

### 5. 列式块（T5 推迟）

每列独立编码（RLE/dictionary/Gorilla）涉及 ts_columnar.c 大改，单独变更推进。

### 6. 连续聚合管理（T6 推迟）

cagg 的 ALTER/DROP API 涉及 parser 改动，单独变更推进。
