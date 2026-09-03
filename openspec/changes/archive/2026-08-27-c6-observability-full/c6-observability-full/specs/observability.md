# 可观测日志完整集成规范（新增）

## 目的

完成 log_engine 的标签索引 + WAL + TTL + 高基数保护完整集成路径。

## 要求

### REQ-1：标签倒排索引

`log_label_index_t` 提供 `(label, value) → {stream_id*}` 映射：
- 文件存储于 `data_dir/labels/{label_hash}/{value_hash}.idx`
- 内存缓存 + mmap
- O(1) 命中（读）/ O(N) 写入（N streams）

### REQ-2：LogQL 解析

`log_query_logql(selector_str)` 解析 `{k="v"}` 格式：
- 跳过空白与 `{}`
- 提取 `key="value"` 对
- 返回 key/value 数组

### REQ-3：WAL 接入

`log_push` 后调 `wal_write_log_append(stream_id, lines, n)`：
- WAL-first：写 WAL 成功后才更新 label index
- 崩溃恢复：db_startup_recover 重放 log WAL

### REQ-4：TTL drop

`log_drop_expired(engine, now_ms)` 扫描 stream 文件，删除 mtime 超 TTL 的：
- 删除文件
- 从 label index 移除
- LOG_INFO

### REQ-5：高基数保护

`log_label_index_set_threshold(idx, n)`：
- n=0 不启用
- 单 label 出现 stream 数 > n 时降级为全扫

## 实现文件

- `engineering/src/db/storage/log/log_label_index.{h,c}`（新增）
- `engineering/src/db/storage/log/log_engine.c`（扩展：LogQL 解析 + WAL + TTL）
- `engineering/src/db/storage/log/log_aggregate.c`（rate 实装）
- `engineering/test/db/storage/c6_observability_test.cpp`（新增集成测试）
