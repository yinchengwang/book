# C3-1 对象 Blob 存储引擎（自研）设计文档

## 设计目标

在 KV 之上构建 blob/chunk 存储层，解决 Image/Video/Model weights 大文件持久化问题。
单机完整：分块布局 + 内容寻址 + Range 读 + Multipart + 与 Relational TOAST 联动。

## 方案

### 1. 分块存储（chunk_store）

4MB chunk 大小，SHA-256 去重（blob_id = hash(所有 chunk hashes)）。

### 2. 元数据目录

Blob metadata：blob_id → [(chunk_id_0, offset_0), ...] 存储于 KV catalog（复用 kv.c）。

### 3. API

blob_put / blob_get / blob_delete / blob_stat / blob_range_get / blob_multipart_begin/upload/complete

## 风险

不做：Erasure Coding、多副本、多 DC（保留扩展接口）。
