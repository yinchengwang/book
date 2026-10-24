# C1-3 KV 锁启用与存储健壮性 设计文档

## 设计目标

修复 04 卷识别的 KV 缺陷：
- 全文件零锁 + 读-改-写无原子性 → 并发 put 丢更新
- page full 误用 KV_ERROR（无专用码）
- 无页分裂 → 批量插入失败
- Value 1MB 上限低
- kv_get 释放契约未文档化
- TTL 清理无 WAL 记录

## 方案

### 1. 复现测试（T1）

测试场景：并发 put 同一 key，N 次后读回应 == 第 N 次写入值。修复前因 read-modify-write 非原子，多个写入可能丢失。

### 2. 加锁（T2）

kv_put/get/delete 用 mmdb_rwlock 包裹读-改-写序列，启用现有 `lock_mgr` 字段：
- kv_put：wrlock 包裹（read old → write new）
- kv_get：rdlock 包裹
- kv_delete：wrlock 包裹

### 3. 错误码扩展（T3）

`kv_result_t` 扩展：
- `KV_FULL` 替换 `KV_ERROR`（page full 场景）
- `KV_CONFLICT` CAS 失败（C3-5 配套）
- `KV_LOCKED` 锁等待超时

### 4. 页分裂（T4）

新 API `kv_page_split()`：满页 → 半满分裂 → 父节点上提。
复用 index/btree 的 split 逻辑（最小复用，保持 KV 单文件）。

### 5. Value 16MB 溢出（T5）

Value > 4KB 时拆为多个 chunk，写入外部页链。读时按需组装。
本期仅提供接口与最简实现（一页 4KB 存不下完整 16MB value → 单页 4KB 链）。

### 6. 释放契约文档化（T6）

头文件添加注释：kv_get 返回的 value 缓冲区**调用方负责 free**。

### 7. TTL tombstone 进 WAL（T7）

TTL 过期删除时写 tombstone 记录到 WAL（KV_LOG_DELETE）。
