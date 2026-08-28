# C3-1 Blob 存储引擎完整实现设计

> 日期：2026-08-28
> 状态：已获用户确认
> 目标：修复现有 Blob 引擎的哈希、分块读取、Multipart、Catalog 和 mm_storage 接入缺陷

## 一、背景与目标

现有 `engineering/src/db/storage/blob/blob_engine.c` 存在以下问题：

- `simple_hash()` 不是 SHA-256，不能作为内容寻址标识
- 写入按 Chunk 哈希保存，读取却按整体 Blob 哈希读取单个文件，超过 4MB 的对象无法正确读取
- `blob_multipart.c` 的接口全部为桩
- `blob_catalog.c` 未接入 Blob 引擎主路径
- Blob 元数据没有独立权威存储
- `mm_storage` Blob 接口仍为骨架

本设计将 Blob 实现为**Blob Manifest + 内容寻址 Chunk + 独立 Catalog**，不依赖尚未完成的 KV 事务。

## 二、目标与非目标

### 目标

- SHA-256 整体 Blob ID 和 Chunk ID
- 4MB Chunk 分块存储
- 任意大小对象的完整读取和跨 Chunk Range 读取
- 一次性 `blob_put()` 与流式 Multipart API
- Manifest 固定二进制格式
- Catalog 独立二进制 checkpoint + WAL
- 两阶段发布协议
- 引用计数、延迟 GC 和崩溃恢复
- 并发上传同一内容时的幂等发布
- Blob API、Catalog、Multipart 和集成测试

### 非目标

- Erasure Coding
- 多副本和多数据中心
- 网络层 S3 REST 协议
- 分布式 Catalog 共识
- 依赖外部加密库

## 三、目录布局

```text
<data_dir>/
├── chunks/
│   └── <chunk_sha256>.chunk
├── manifests/
│   └── <blob_sha256>.manifest
├── uploads/
│   └── <upload_id>/
│       ├── session.meta
│       └── parts/
├── catalog.bin
└── catalog.wal
```

## 四、核心数据结构

### 4.1 SHA-256

新增纯 C 实现：

```text
engineering/include/db/sha256.h
engineering/src/db/core/sha256.c
```

接口：

```c
typedef struct sha256_ctx_s sha256_ctx_t;
void sha256_init(sha256_ctx_t *ctx);
void sha256_update(sha256_ctx_t *ctx, const void *data, size_t len);
void sha256_final(sha256_ctx_t *ctx, uint8_t digest[32]);
void sha256_compute(const void *data, size_t len, uint8_t digest[32]);
```

### 4.2 Chunk 文件

```c
typedef struct blob_chunk_header_s {
    uint32_t magic;          /* 'CHNK' */
    uint32_t version;        /* 1 */
    uint64_t payload_size;
    uint8_t  chunk_sha256[32];
    uint32_t header_checksum;
} blob_chunk_header_t;
```

Chunk 正式文件为：`header + payload + payload_checksum`。

### 4.3 Manifest 文件

```c
typedef struct blob_manifest_header_s {
    uint32_t magic;          /* 'BLMF' */
    uint32_t version;        /* 1 */
    uint32_t flags;
    uint64_t blob_size;
    uint32_t chunk_size;
    uint32_t chunk_count;
    uint16_t content_type_len;
    uint32_t metadata_len;
    uint8_t  blob_sha256[32];
    uint32_t manifest_checksum;
} blob_manifest_header_t;

typedef struct blob_manifest_chunk_s {
    uint8_t  chunk_sha256[32];
    uint64_t logical_offset;
    uint32_t chunk_size;
    uint32_t chunk_checksum;
} blob_manifest_chunk_t;
```

Manifest 后依次保存 content-type、metadata 和有序 Chunk 条目。

### 4.4 Catalog

```c
typedef enum blob_entry_state_e {
    BLOB_STATE_PREPARED = 1,
    BLOB_STATE_COMMITTED = 2,
    BLOB_STATE_DELETED = 3
} blob_entry_state_t;

typedef struct blob_catalog_entry_s {
    uint8_t blob_id[32];
    blob_entry_state_t state;
    uint64_t blob_size;
    uint32_t chunk_count;
    int64_t created_at_ms;
    int64_t deleted_at_ms;
} blob_catalog_entry_t;

typedef struct blob_chunk_ref_s {
    uint8_t chunk_id[32];
    uint64_t ref_count;
    int64_t gc_after_ms;
} blob_chunk_ref_t;
```

Catalog 由内存索引、`catalog.bin` checkpoint 和 `catalog.wal` 组成。

## 五、API 设计

### 5.1 一次性 API

保留现有接口并修正语义：

```c
int blob_put(blob_engine_t *engine,
             const void *data, size_t len,
             uint8_t out_blob_id[32]);

int blob_get(blob_engine_t *engine,
             const uint8_t blob_id[32],
             void *out_buf, size_t buf_len, size_t *out_read);

int blob_range_get(blob_engine_t *engine,
                   const uint8_t blob_id[32],
                   size_t offset, size_t len,
                   void *out_buf, size_t buf_len, size_t *out_read);
```

`blob_put()` 内部复用流式 Writer，避免两套分块逻辑。

### 5.2 Multipart 流式 API

```c
typedef struct blob_upload_options_s {
    const char *upload_id;
    const char *content_type;
    const void *metadata;
    size_t metadata_len;
} blob_upload_options_t;

typedef struct blob_upload_s blob_upload_t;

blob_upload_t *blob_upload_begin(blob_engine_t *engine,
                                 const blob_upload_options_t *options);
int blob_upload_write(blob_upload_t *upload, const void *data, size_t len);
int blob_upload_finish(blob_upload_t *upload,
                       uint8_t out_blob_id[32]);
int blob_upload_abort(blob_upload_t *upload);
```

兼容旧 `blob_multipart_begin/upload_part/complete/abort` 接口时，由旧接口包装新 Writer。

## 六、两阶段发布协议

### 准备阶段

1. 创建唯一 Upload Session
2. 接收数据并累计 SHA-256
3. 每满 4MB 封存 Chunk
4. Chunk 写入临时文件并校验
5. `fflush + fsync`
6. 不覆盖式原子发布，已存在则校验并复用
7. 写 `BLOB_PREPARE` Catalog WAL
8. Catalog WAL `flush + fsync`

### 提交阶段

9. 写 Manifest 临时文件
10. Manifest `flush + fsync`
11. 原子 `rename` 为正式 Manifest
12. 写 `BLOB_COMMIT` Catalog WAL
13. Catalog WAL `flush + fsync`
14. 内存 Catalog 状态改为 COMMITTED
15. 删除 Upload Session 临时文件

只有 COMMITTED Blob 对读取接口可见。

## 七、读取与 Range 算法

### 完整读取

1. 根据 Blob ID 打开 Manifest
2. 校验 Manifest magic/version/checksum/blob_id
3. 分配或检查调用方输出缓冲区
4. 按 Chunk 条目顺序读取
5. 校验每个 Chunk header、ID、payload checksum
6. 拼接到输出缓冲区
7. 校验实际读取长度等于 blob_size

### Range 读取

给定 `[offset, offset + len)`：

1. 校验 offset 不超过 blob_size
2. 二分查找第一个 `logical_offset + chunk_size > offset` 的 Chunk
3. 只打开覆盖范围的 Chunk
4. 对首尾 Chunk 做局部读取
5. 中间 Chunk 整块读取
6. 返回不超过 `min(len, buf_len, blob_size-offset)` 的数据

## 八、Catalog WAL 与恢复

Catalog WAL 记录类型：

```text
CATALOG_BLOB_PREPARE
CATALOG_BLOB_COMMIT
CATALOG_BLOB_DELETE
CATALOG_CHUNK_REF_INC
CATALOG_CHUNK_REF_DEC
CATALOG_UPLOAD_BEGIN
CATALOG_UPLOAD_ABORT
```

启动时：

1. 校验并加载 `catalog.bin`
2. 顺序读取 `catalog.wal`
3. 校验记录 CRC
4. 重放 checkpoint 之后的记录
5. PREPARED 且无 COMMIT 的 Blob 标记清理
6. 有 Manifest 且所有 Chunk 完整的 PREPARED Blob 可重建提交
7. 删除过期 Upload Session
8. 保留孤立 Chunk，等待 GC 宽限期

## 九、删除与 GC

### 删除

1. 加载 Manifest
2. 写 `BLOB_DELETE` WAL 并 fsync
3. 原子删除 Manifest 或标记 DELETED
4. 每个 Chunk 写 `REF_DEC`
5. 引用计数为 0 时设置 `gc_after = now + 1h`

### GC

1. 扫描 ref_count=0 且 gc_after 已到期的 Chunk
2. 重新确认 Catalog 引用计数
3. 确认无活动读取者
4. 原子删除 Chunk
5. 写 Catalog checkpoint/WAL 清理记录

## 十、并发控制

- 每个 Upload Session 独立临时目录
- 正式 Chunk 只创建、不覆盖
- 正式 Manifest 只通过临时文件 + rename 发布
- Catalog 修改使用 Blob 内部互斥锁
- 读操作使用读锁；GC 使用写锁
- 读取者在打开 Chunk 前登记，关闭后注销，避免 GC 误删

## 十一、测试计划

新增：`engineering/test/db/storage/blob_engine_test.cpp`

测试项：

1. SHA-256 标准测试向量
2. 小对象 put/get
3. 4MB 边界对象
4. 大于 4MB 的多 Chunk put/get
5. 跨 Chunk Range 读取
6. Range 越界与输出缓冲区不足
7. 重复内容 Chunk 去重
8. Manifest 校验失败
9. Chunk 校验失败
10. Multipart 任意大小 write
11. Multipart abort 清理临时文件
12. 并发相同内容上传
13. 删除后引用计数与延迟 GC
14. Catalog checkpoint + WAL 恢复
15. 进程崩溃点恢复矩阵

## 十二、验收标准

- SHA-256 测试向量全部通过
- 16MB、64MB 测试对象完整读回且 SHA-256 一致
- Range 读取只访问覆盖范围的 Chunk
- Multipart 不要求一次性持有完整对象
- 正式文件没有覆盖写
- 中断上传不产生可见 Blob
- Catalog WAL 重放后状态一致
- 共享 Chunk 删除不会误删仍被引用内容
- CMake 构建通过，Blob 测试全部通过

## 十三、文件变更清单

创建：

```text
engineering/include/db/sha256.h
engineering/src/db/core/sha256.c
engineering/include/db/blob_manifest.h
engineering/src/db/storage/blob/blob_manifest.c
engineering/include/db/blob_catalog.h
engineering/src/db/storage/blob/blob_catalog.c
engineering/include/db/blob_upload.h
engineering/src/db/storage/blob/blob_upload.c
engineering/src/db/storage/blob/blob_gc.c
engineering/test/db/storage/blob_engine_test.cpp
```

修改：

```text
engineering/include/db/blob_engine.h
engineering/src/db/storage/blob/blob_engine.c
engineering/src/db/storage/blob/CMakeLists.txt
engineering/test/db/storage/CMakeLists.txt
```

删除或替换：

```text
engineering/src/db/storage/blob/blob_multipart.c 中的全部桩逻辑
```
