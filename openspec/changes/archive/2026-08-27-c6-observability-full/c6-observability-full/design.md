# C6 可观测日志引擎完整路径 设计文档

## 设计目标

完成 log_engine 的完整集成路径：标签索引 + LogQL 解析 + WAL 接入 + TTL drop。

## 方案

### 1. 标签倒排索引（C6.1）

`label_index` 结构：`(label, value) → {stream_id, stream_id, ...}` 二级索引。

存储于 `data_dir/labels/{label_hash}/{value_hash}.idx`：
- 文件读取时加载到内存（mmap 或一次性 read）
- 写入时 append + fsync

### 2. LogQL 流选择器解析（C6.2）

parser `"{k=\"v\",k2=\"v2\"}"` 格式，提取 (k,v) 对。
应用查询时：所有 (k,v) 必须存在于目标 stream（AND 语义）。

### 3. log_rate（C6.3）

与 log_aggregate_count 类似但按时间窗口 rate。
公式：`count / window_size_sec`

### 4. WAL 接入（C6.4）

`log_push` 内部：fwrite 后调 `wal_write`（复用 shared WAL）。`db_startup_recover` 解析 log records 还原。

### 5. TTL drop（C6.5）

后台 thread 周期性扫描 stream 文件，超 TTL 的删除文件 + 更新标签索引。

### 6. 高基数保护（C6.6）

`log_label_index_set_high_cardinality_threshold(threshold)`：
- 超过阈值的 label 降级为全扫
- LOG_WARN 提示
- 全局 GUC `log_label_high_cardinality_threshold`

## 不变项

- log_engine.h 公开 API 不变
- log_query / log_push / log_rate 签名不变
- log_engine_create / open / close 不变
