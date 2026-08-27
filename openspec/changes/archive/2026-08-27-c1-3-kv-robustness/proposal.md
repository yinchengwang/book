# C1-3 KV 锁启用与存储健壮性 Proposal

## Why

差距分析 04 卷发现：kv.c 全文件零锁（grep 零命中），`kv_put` 读-改-写序列无原子性——并发 put 同一 key 丢更新（`:383-462`）；结构体 `lock_mgr` 字段存在但未使用（`kv.h:66`）；page full 误用 KV_ERROR 无专用码（`kv.c:453`）；单页无分裂导致批量插入失败；`kv_get` 释放契约未文档化。

## What Changes

- `kv_put/get/delete` 以 mmdb_rwlock 包裹读-改-写序列（启用 `lock_mgr` 字段）
- 错误码扩展：KV_FULL/KV_CONFLICT/KV_LOCKED + page full 路径改用 KV_FULL
- 页分裂：半满分裂 + 父节点上提（复用 index/btree 的 split 逻辑），Value 1MB → 16MB（溢出页）
- `kv_get` 释放契约文档化（头文件注释 + 调用方审计）
- TTL 过期写 tombstone 进 WAL（惰性 + 后台主动双策略基础）

## Capabilities

| 能力 | 交付 |
|------|------|
| 并发安全 | 并发 put/get/delete 压力测试无丢更新 |
| 页分裂 | 批量插入百万键无 KV_FULL 失败 |
| 错误码 | KV_FULL/KV_CONFLICT/KV_LOCKED 定义与测试 |
| 大 Value | 16MB value 写读回一致 |

## Impact

- 修改：kv.c、kv.h、kv_ttl.c、页面层
- 新增：并发压力测试、大 Value 测试
- 预计 5-6 个 commit
- 依赖：C0-1、C0-2
