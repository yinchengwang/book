# C3-1 对象 Blob 存储引擎（自研）Proposal

## Why

差距分析报告（§5 缺失模态调研）+ 路由决策（全自研）：KV Value 1MB 上限远低于业界（Redis 512MB、RocksDB 无限），图像/音视频/模型权重场景刚需。KV/Relational/Document 页面 8-16KB 无法承载。集成 MinIO 是更省力方案但用户选定全部自研。

## What Changes

- 新引擎 `storage/blob/`，mm_storage 增加 MODEL_BLOB（11 或不注册枚举，通过 mm_open_blob 类 API）
- 分块存储：对象切 4MB chunk（chunk = 文件系统单位），blob-id = SHA-256(内容)（内容寻址 + 去重）
- 元数据走 KV catalog：blob-id → (chunk 列表, 总长, content-type, 时间戳) —— 复用 KV + CF
- 缓存：LRU chunk 缓存挂 Buffer Pool 之上
- API：`blob_put/get/delete/stat` + `blob_range_get(offset, len)` + `blob_multipart_begin/upload/complete`
- 与 Relational TOAST 联动（C3-5 部分）：大元组外存为 blob-id
- 明确不做：Erasure Coding、多副本、多 DC（接口预留）

## Capabilities

| 能力 | 交付 |
|------|------|
| 分块布局 | 任意大小对象（GB 级）流式读写 |
| 内容寻址 | 同内容自动去重 |
| Range GET | 按 offset/len 范围读 |
| Multipart | 大文件分片并发上传 |
| 持久化 | 重启后数据完整（独立集成测试） |

## Impact

- 新增：storage/blob/ 全套（chunk_store.c、blob_engine.c、catalog 层）
- 修改：mm_storage.h、KV、Buffer Pool
- 预计 8-10 个 commit
- 依赖：C0-3
