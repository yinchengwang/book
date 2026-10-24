# C6 可观测日志引擎完整路径 任务清单

## 任务列表

### 标签倒排 + LogQL（C6.1-C6.2）
- [ ] **C6.1** log_label_index.h/c：标签 → stream 集合映射（文件持久化）
- [ ] **C6.2** LogQL 流选择器解析：`{k="v",k2="v2"}` AND 语义

### 聚合与 WAL（C6.3-C6.4）
- [ ] **C6.3** log_rate 函数：count / window_size_sec
- [ ] **C6.4** WAL 接入：log_push → wal_write_log record

### TTL + 高基数保护（C6.5-C6.6）
- [ ] **C6.5** TTL drop 后台调度（接入 ts_retention）
- [ ] **C6.6** 高基数保护：阈值 + 降级全扫（log_label_high_cardinality_threshold GUC）

### 测试与归档（C6.7-C6.8）
- [ ] **C6.7** log_engine 集成测试（push → query → aggregate → drop）
- [ ] **C6.8** Verify + Archive
