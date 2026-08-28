# KV 健壮性规范（新增）

## 目的

消除 KV 模态的并发丢更新、错误码粒度粗、Value 容量受限等缺陷。

## 要求

### REQ-1：读-改-写原子性

`kv_put` / `kv_delete` 必须用 `mmdb_rwlock_wrlock` 包裹读-改-写序列；`kv_get` 用 `mmdb_rwlock_rdlock`。

### REQ-2：专用错误码

`KV_FULL` 替代 `KV_ERROR`（page full 场景）；新增 `KV_CONFLICT`（CAS 失败）和 `KV_LOCKED`（锁等待超时）。

### REQ-3：页分裂

`kv_page_split()` 提供页满时半满分裂逻辑，避免批量插入因 page full 失败。

### REQ-4：Value 容量

支持 Value 大小扩展到 16MB（单页链或多页）。

### REQ-5：调用方契约

`kv_get` 返回的 value 缓冲区由调用方 `free()`；头文件注释明确。

### REQ-6：TTL WAL

TTL 过期删除写 WAL tombstone 记录（KV_LOG_DELETE），崩溃后可恢复过期删除。
