# Blob 存储引擎完整实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将现有 Blob 骨架升级为可持久化、可校验、支持多 Chunk、Range 读取、Multipart、Catalog WAL 与延迟 GC 的单机 Blob 存储引擎。

**Architecture:** Blob 使用“固定二进制 Manifest + 内容寻址 Chunk + 独立 Catalog”三层布局。所有写入通过统一的流式 Writer 完成，先发布已 fsync 的 Chunk，再通过 `BLOB_PREPARE`、Manifest 原子发布、`BLOB_COMMIT` 两阶段协议使对象可见；读取先加载 Manifest，再按 Chunk 清单组装或定位 Range。Catalog 自己维护内存索引、WAL 和 checkpoint，不依赖当前 KV 事务。

**Tech Stack:** C11、CMake 3.20+、GoogleTest、POSIX `fsync` / Windows `_commit` 与 `FlushFileBuffers` 兼容封装；SHA-256 使用纯 C 实现，不引入 OpenSSL、libxml2 或其他运行时依赖。

## Global Constraints

- 全程使用简体中文；新增代码注释使用中文；Commit Message 使用中文
- 禁止修改 `learning/` 与无关模块；测试产物写入 `test-results/`，不污染源码目录
- 保留 `blob_put/blob_get/blob_delete/blob_stat/blob_range_get` 现有公开 API 签名
- 保留旧 Multipart 符号并让它们包装新流式 API，禁止删除已有 ABI 入口
- Chunk 固定逻辑大小为 `4 * 1024 * 1024` 字节；Manifest 和 Chunk 都必须带 magic/version/checksum
- 正式 Chunk 与 Manifest 禁止覆盖写；必须使用临时文件、`fflush`、跨平台同步、原子 `rename`
- Blob 只有在 `BLOB_COMMIT` 已持久化且 Manifest 与全部 Chunk 校验通过后才可读取
- 所有失败路径释放本任务分配的资源；不使用未检查的整数乘法、文件读写和路径拼接
- 不声称完成未经测试的崩溃恢复；每个验收项必须有对应测试或明确记录为未验证
- 每个任务完成后立即更新 `openspec/changes/archive/2026-08-27-c3-1-blob-engine/tasks.md`，但不得把未通过的功能标成完成

---

## 文件结构

### 创建

- `engineering/include/db/sha256.h`：纯 C SHA-256 上下文与一次性计算 API
- `engineering/src/db/core/sha256.c`：SHA-256 常量、压缩轮函数和流式实现
- `engineering/include/db/blob_manifest.h`：Chunk/Manifest 固定格式、读写与校验接口
- `engineering/src/db/storage/blob/blob_manifest.c`：Manifest 编解码、校验、临时文件发布
- `engineering/include/db/blob_catalog.h`：独立 Catalog 条目、Chunk 引用计数、WAL/checkpoint API
- `engineering/src/db/storage/blob/blob_catalog.c`：Catalog 哈希索引、二进制 WAL、checkpoint、恢复
- `engineering/include/db/blob_upload.h`：统一流式 Upload Writer API
- `engineering/src/db/storage/blob/blob_upload.c`：4MB 分块、哈希、临时文件和两阶段发布
- `engineering/src/db/storage/blob/blob_gc.c`：引用计数为零的 Chunk 延迟 GC
- `engineering/test/db/storage/blob_engine_test.cpp`：Blob、Manifest、Catalog、Multipart 和恢复测试

### 修改

- `engineering/include/db/blob_engine.h`：增加 Upload、metadata、完整读取语义与兼容 Multipart 声明
- `engineering/src/db/storage/blob/blob_engine.c`：改为委托 Upload/Manifest/Catalog，删除 `simple_hash` 和单文件读取假设
- `engineering/src/db/storage/blob/blob_multipart.c`：旧桩改为新 Upload API 包装器
- `engineering/src/db/storage/blob/CMakeLists.txt`：显式包含新源文件与依赖
- `engineering/test/db/storage/CMakeLists.txt`：注册 `blob_engine_test`
- `openspec/changes/archive/2026-08-27-c3-1-blob-engine/tasks.md`：逐项同步真实状态

---

### Task 1: 纯 C SHA-256

**Files:**
- Create: `engineering/include/db/sha256.h`
- Create: `engineering/src/db/core/sha256.c`
- Modify: `engineering/src/db/core/CMakeLists.txt`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Produces `sha256_compute(data, len, digest[32])`、`sha256_init/update/final`
- 后续 Manifest、Chunk Writer、Catalog 均通过这些接口计算摘要

- [ ] **Step 1: 写 SHA-256 标准向量测试**

测试必须覆盖空串、`abc` 和百万个 `a`：

```cpp
TEST(Sha256, StandardVectors) {
    uint8_t digest[32];
    sha256_compute("", 0, digest);
    EXPECT_EQ(hex(digest), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    sha256_compute("abc", 3, digest);
    EXPECT_EQ(hex(digest), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::string million_a(1000000, 'a');
    sha256_compute(million_a.data(), million_a.size(), digest);
    EXPECT_EQ(hex(digest), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}
```

- [ ] **Step 2: 运行测试确认初始失败**

运行：`ctest --test-dir build/engineering -R blob_engine_test --output-on-failure`
预期：因 `sha256.h` 或实现不存在而失败。

- [ ] **Step 3: 实现流式 SHA-256**

`sha256_ctx_t` 至少包含 `state[8]`、`bit_count`、`buffer[64]`、`buffer_len`。`sha256_update()` 支持任意分块大小，`sha256_final()` 按 SHA-256 规范追加 `0x80`、零填充和大端 bit length；所有轮函数使用 `uint32_t` 溢出语义，不使用未对齐指针强转。

- [ ] **Step 4: 注册构建并运行测试**

将 `sha256.c` 加入 `db_core`，运行：`cmake --build build/engineering --target db_core blob_engine_test`，然后运行上述 ctest。预期：3 个标准向量全部通过。

- [ ] **Step 5: 提交**

```bash
git add engineering/include/db/sha256.h engineering/src/db/core/sha256.c engineering/src/db/core/CMakeLists.txt engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 实现纯 C SHA-256"
```

---

### Task 2: Chunk 固定格式与原子发布

**Files:**
- Create: `engineering/include/db/blob_manifest.h`
- Create: `engineering/src/db/storage/blob/blob_manifest.c`
- Modify: `engineering/include/db/blob_engine.h`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Consumes：Task 1 的 `sha256_compute()`
- Produces：`blob_chunk_write_atomic()`、`blob_chunk_read_checked()`、`blob_chunk_exists_checked()`

- [ ] **Step 1: 写 Chunk 格式测试**

测试写入 4MB+17 字节数据，然后重新读取，断言 header、payload、摘要和 checksum 完整一致；另测试篡改 header 与 payload 时读取失败。

- [ ] **Step 2: 定义固定格式**

在 `blob_manifest.h` 定义：

```c
typedef struct blob_chunk_header_s {
    uint32_t magic;              /* BLOB_CHUNK_MAGIC = 0x43484E4B */
    uint32_t version;            /* 1 */
    uint64_t payload_size;
    uint8_t  chunk_sha256[32];
    uint32_t header_checksum;
} blob_chunk_header_t;
```

文件布局必须是 `header + payload + uint32_t payload_checksum`；所有多字节字段按 little-endian 的项目本地二进制约定写入，读入后校验长度上限和摘要。

- [ ] **Step 3: 实现临时文件和不覆盖发布**

`blob_chunk_write_atomic(dir, data, len, upload_id, out_id)`：计算 Chunk ID；创建包含 upload_id 的临时路径；使用 `fwrite` 返回值检查、`fflush` 和 `db_fsync`；发布时使用不覆盖式创建/rename。若正式文件已存在，调用 checked read 验证内容相同，验证成功则删除临时文件并复用，验证失败返回错误且不覆盖正式文件。

- [ ] **Step 4: 实现 checked read**

`blob_chunk_read_checked()` 打开文件后必须依次校验 magic、version、payload_size、header checksum、文件 payload 长度、payload checksum 和 SHA-256；任何失败都返回错误并清零输出长度。

- [ ] **Step 5: 运行测试并提交**

运行：`cmake --build build/engineering --target blob_engine_test && ctest --test-dir build/engineering -R blob_engine_test --output-on-failure`。

```bash
git add engineering/include/db/blob_manifest.h engineering/src/db/storage/blob/blob_manifest.c engineering/include/db/blob_engine.h engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 增加 Chunk 固定格式与原子发布"
```

---

### Task 3: Manifest 编解码与校验

**Files:**
- Modify: `engineering/include/db/blob_manifest.h`
- Modify: `engineering/src/db/storage/blob/blob_manifest.c`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Consumes：Task 1 SHA-256、Task 2 Chunk ID
- Produces：`blob_manifest_t`、`blob_manifest_write_atomic()`、`blob_manifest_load_checked()`、`blob_manifest_free()`

- [ ] **Step 1: 写 Manifest 测试**

构造包含 3 个 Chunk、content-type 和 metadata 的 Manifest，写入后读取并逐字段比较；篡改 magic、version、chunk_count、blob_id 或 checksum 时断言加载失败。

- [ ] **Step 2: 定义内存结构与磁盘格式**

内存结构必须保存 `blob_id[32]`、`blob_size`、`chunk_size`、`chunk_count`、`content_type`、`metadata` 和有序 `blob_manifest_chunk_t[]`。磁盘头部使用设计文档中的固定字段，随后写扩展区和 Chunk 条目数组。

- [ ] **Step 3: 实现安全序列化**

写入前验证：`chunk_count` 与分配大小乘法不溢出、所有 Chunk 总大小等于 `blob_size`、每个 logical offset 单调递增、字符串长度不超过 `uint16_t/uint32_t` 字段上限。使用 `.tmp.<upload_id>` 文件，写完同步后 rename。

- [ ] **Step 4: 实现安全反序列化**

读取文件大小后拒绝小于固定头或超过合理上限的文件；用 checked read 读取每段；拒绝整数溢出、重叠 offset、空 Chunk ID、总长度不匹配和整体 blob SHA-256 不匹配。

- [ ] **Step 5: 运行测试并提交**

运行 Manifest 专项测试后提交：

```bash
git add engineering/include/db/blob_manifest.h engineering/src/db/storage/blob/blob_manifest.c engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 实现 Manifest 编解码与完整性校验"
```

---

### Task 4: 独立 Catalog、WAL 与 checkpoint

**Files:**
- Modify: `engineering/include/db/blob_catalog.h`
- Modify: `engineering/src/db/storage/blob/blob_catalog.c`
- Create: `engineering/src/db/storage/blob/blob_catalog_wal.c`（如实现需要拆分）
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Produces：`blob_catalog_open/close`、`blob_catalog_prepare/commit/delete`、`blob_catalog_ref_inc/ref_dec`、`blob_catalog_checkpoint/recover`、`blob_catalog_find`

- [ ] **Step 1: 写 Catalog 状态测试**

测试 PREPARE→COMMIT、重复 COMMIT 幂等、DELETE、Chunk refcount 增减和 checkpoint 后 reopen；手动截断 WAL 尾部，断言完整前缀可恢复且坏尾记录被丢弃。

- [ ] **Step 2: 替换 KV-only catalog 结构**

在 `blob_catalog.h` 中定义 BlobEntry、ChunkRef、Catalog 句柄和状态枚举；保留旧 `blob_catalog_create(kv_t*, namespace)` API 作为兼容适配层，但新 Blob 主路径使用独立 Catalog。

- [ ] **Step 3: 实现 Catalog WAL**

每条 WAL 记录包含 magic、version、LSN、record type、payload length、payload、CRC32。实现 `BLOB_PREPARE`、`BLOB_COMMIT`、`BLOB_DELETE`、`CHUNK_REF_INC`、`CHUNK_REF_DEC` 和 Upload 状态记录。写入顺序为记录→`fflush`→`db_fsync`；恢复按 LSN 顺序校验并重放。

- [ ] **Step 4: 实现 checkpoint**

把内存索引写到 `catalog.bin.tmp`，写固定 header、Blob 条目、Chunk 引用条目和 checksum，fsync 后 rename；记录 checkpoint LSN 后再安全截断已包含记录的 WAL。截断失败不得破坏现有 checkpoint。

- [ ] **Step 5: 运行测试并提交**

```bash
cmake --build build/engineering --target blob_engine_test
ctest --test-dir build/engineering -R "BlobCatalog|blob_engine_test" --output-on-failure
git add engineering/include/db/blob_catalog.h engineering/src/db/storage/blob/blob_catalog.c engineering/src/db/storage/blob/blob_catalog_wal.c engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 实现独立 Catalog WAL 与 checkpoint"
```

---

### Task 5: 统一流式 Upload Writer 与 Multipart

**Files:**
- Create: `engineering/include/db/blob_upload.h`
- Create: `engineering/src/db/storage/blob/blob_upload.c`
- Modify: `engineering/include/db/blob_engine.h`
- Modify: `engineering/src/db/storage/blob/blob_multipart.c`
- Modify: `engineering/src/db/storage/blob/blob_engine.c`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Consumes：Task 2 Chunk、Task 3 Manifest、Task 4 Catalog
- Produces：`blob_upload_begin/write/finish/abort` 和旧 Multipart 包装器

- [ ] **Step 1: 写 Multipart 测试**

覆盖任意边界写入：一次写入小于 Chunk、跨 Chunk 写入、逐字节写入、abort 后无正式 Manifest、finish 后可 load。

- [ ] **Step 2: 定义 Upload 状态**

`blob_upload_t` 保存 engine、唯一 upload_id、临时目录、整体 SHA-256 上下文、当前 Chunk 缓冲区、Chunk 清单、content-type、metadata 和状态（ACTIVE/COMMITTED/ABORTED）。所有数组采用 checked growth。

- [ ] **Step 3: 实现 `blob_upload_write()`**

允许任意 `data/len` 分块调用；填满 4MB 缓冲就调用 Task 2 的原子 Chunk 发布，并追加 logical offset、chunk size 和 checksum 到内存清单；同时更新整体 SHA-256。零长度写入不产生 Chunk。

- [ ] **Step 4: 实现两阶段 `finish()`**

按设计严格执行：所有 Chunk 完成并 fsync → Catalog PREPARE WAL + fsync → Manifest 临时写入、校验、fsync、rename → Catalog COMMIT WAL + fsync → 内存状态 COMMITTED → 删除 upload 临时目录。任一步骤失败都不写 COMMIT，并保留可恢复/可 GC 的 PREPARED 状态。

- [ ] **Step 5: 让 `blob_put()` 复用 Writer**

`blob_put()` 只创建 Upload、一次或多次调用 write、finish；删除旧 `simple_hash()` 与旧按 Chunk 单文件路径逻辑。旧 `blob_multipart_begin/upload_part/complete/abort` 包装新 Writer，并拒绝重复 part number、超大 part 和非法状态转换。

- [ ] **Step 6: 运行测试并提交**

```bash
cmake --build build/engineering --target blob_engine_test
ctest --test-dir build/engineering -R "BlobUpload|BlobMultipart|blob_engine_test" --output-on-failure
git add engineering/include/db/blob_upload.h engineering/src/db/storage/blob/blob_upload.c engineering/src/db/storage/blob/blob_multipart.c engineering/src/db/storage/blob/blob_engine.c engineering/include/db/blob_engine.h engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 实现统一流式上传与 Multipart"
```

---

### Task 6: Manifest 驱动的完整读取与 Range 读取

**Files:**
- Modify: `engineering/src/db/storage/blob/blob_engine.c`
- Modify: `engineering/include/db/blob_engine.h`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Consumes：Task 3 Manifest、Task 2 checked Chunk reader、Task 4 Catalog visibility
- Produces：修复后的 `blob_get/blob_stat/blob_range_get`

- [ ] **Step 1: 写多 Chunk 读取测试**

生成 `BLOB_MAX_CHUNK_SIZE + 17`、`2 * BLOB_MAX_CHUNK_SIZE + 31` 两种对象，断言完整读回 SHA-256 等于输入；Range 测试覆盖首 Chunk、中间 Chunk、尾 Chunk、跨边界和 offset==blob_size。

- [ ] **Step 2: 实现 `blob_get()`**

根据整体 Blob ID 打开 Manifest；要求 Catalog 状态为 COMMITTED；校验 Manifest；检查 `buf_len >= blob_size`，不足时返回明确错误和所需长度；逐 Chunk checked read 到正确 logical offset，登记活动读取者，结束后注销。

- [ ] **Step 3: 实现 `blob_stat()`**

只读取并校验 Manifest header，返回 Manifest 中的 blob_size；不得通过 Chunk 单文件大小推断对象大小。

- [ ] **Step 4: 实现 `blob_range_get()`**

拒绝 `offset > blob_size`；计算实际读取长度 `min(len, blob_size-offset, buf_len)`；用二分定位第一个覆盖 offset 的 Chunk，只打开覆盖范围的 Chunk，对首尾做局部读取，中间 Chunk 整块读取；返回实际字节数。

- [ ] **Step 5: 运行测试并提交**

```bash
cmake --build build/engineering --target blob_engine_test
ctest --test-dir build/engineering -R "BlobRead|BlobRange|blob_engine_test" --output-on-failure
git add engineering/src/db/storage/blob/blob_engine.c engineering/include/db/blob_engine.h engineering/test/db/storage/blob_engine_test.cpp
git commit -m "fix(blob): 改为 Manifest 驱动的多 Chunk 读取与 Range"
```

---

### Task 7: 引用计数、延迟 GC 与启动恢复

**Files:**
- Create: `engineering/src/db/storage/blob/blob_gc.c`
- Modify: `engineering/include/db/blob_catalog.h`
- Modify: `engineering/src/db/storage/blob/blob_catalog.c`
- Modify: `engineering/src/db/storage/blob/blob_upload.c`
- Modify: `engineering/src/db/storage/blob/blob_engine.c`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Produces：`blob_delete()` 的 refcount 语义、`blob_gc_run()`、`blob_recover()`

- [ ] **Step 1: 写删除与 GC 测试**

两个 Blob 共享至少一个 Chunk：删除其中一个后 Chunk 仍可读；删除第二个后 Chunk 进入 `gc_after`；宽限期未到不能删除，宽限期到且 refcount 仍为零才删除。

- [ ] **Step 2: 实现引用计数更新**

COMMIT 时每个 Chunk 写 `REF_INC`；删除时写 `BLOB_DELETE` 和 `REF_DEC`；任何 refcount 不能下溢。删除 Manifest 采用状态标记或原子 unlink，且先写 WAL 再改变可见状态。

- [ ] **Step 3: 实现活动读取者保护**

Blob Engine 维护活动读取者计数或 ID 集合；`blob_get/range_get` 打开 Chunk 前登记，所有返回路径注销；GC 只有在 refcount==0、宽限期到期且没有活动读取者时删除。

- [ ] **Step 4: 实现启动恢复**

打开 Catalog 时加载 checkpoint、重放 WAL、清理未提交 Upload；PREPARED + Manifest + 全部 Chunk 完整的对象可重建 COMMITTED，否则保留孤立 Chunk 等 GC。对损坏 Manifest/Chunk 记录错误，不将其暴露为可读 Blob。

- [ ] **Step 5: 运行测试并提交**

```bash
cmake --build build/engineering --target blob_engine_test
ctest --test-dir build/engineering -R "BlobDelete|BlobGC|BlobRecovery|blob_engine_test" --output-on-failure
git add engineering/src/db/storage/blob/blob_gc.c engineering/include/db/blob_catalog.h engineering/src/db/storage/blob/blob_catalog.c engineering/src/db/storage/blob/blob_upload.c engineering/src/db/storage/blob/blob_engine.c engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 实现引用计数延迟 GC 与启动恢复"
```

---

### Task 8: Blob 公共 API、构建集成与错误路径审计

**Files:**
- Modify: `engineering/include/db/blob_engine.h`
- Modify: `engineering/src/db/storage/blob/CMakeLists.txt`
- Modify: `engineering/test/db/storage/CMakeLists.txt`
- Modify: `engineering/include/db/mm_record.h` / `engineering/src/db/core/mm_storage_blob.c`
- Test: `engineering/test/db/storage/blob_engine_test.cpp`

**Interfaces:**
- Consumes：Task 1-7 全部 API
- Produces：工程构建可发现的 Blob 库、mm_storage Blob 适配层

- [ ] **Step 1: 补全公共头文件**

在 `blob_engine.h` 声明 Upload options、Upload 生命周期、旧 Multipart 兼容接口和错误语义；所有返回值定义为 0 成功、负值失败，读取接口约定 `out_read` 在失败时为 0。

- [ ] **Step 2: 修复 CMake 源文件发现**

让 `storage_blob` 包含 `blob_engine.c`、`blob_manifest.c`、`blob_catalog.c`、`blob_catalog_wal.c`、`blob_upload.c`、`blob_multipart.c`、`blob_gc.c`；让 `db_core` 包含 `sha256.c`；测试目标链接 `storage_blob`、`db_core`、`project_includes` 和 GoogleTest。

- [ ] **Step 3: 接通 mm_storage Blob 适配层**

`mm_storage_blob_put/get` 不再返回空成功；根据 Blob Engine 句柄执行真实 put/get，句柄不存在或对象不存在返回明确错误。保持 `mm_record_header.h` 的 BLOB 引用只保存 32 字节 blob_id，不复制大对象。

- [ ] **Step 4: 写 API 错误测试**

覆盖 NULL 参数、空对象、Manifest 不存在、Chunk 损坏、输出缓冲区不足、重复 finish、abort 后 write、删除后 get；每个测试断言非零错误码和输出参数状态。

- [ ] **Step 5: 运行专项构建和测试**

```bash
cmake -S . -B build/engineering -G Ninja -DENGINEERING_BUILD=ON
cmake --build build/engineering --target storage_blob blob_engine_test
ctest --test-dir build/engineering -R blob_engine_test --output-on-failure
```

- [ ] **Step 6: 提交**

```bash
git add engineering/include/db/blob_engine.h engineering/src/db/storage/blob/CMakeLists.txt engineering/test/db/storage/CMakeLists.txt engineering/include/db/mm_record.h engineering/src/db/core/mm_storage_blob.c engineering/test/db/storage/blob_engine_test.cpp
git commit -m "feat(blob): 完成公共 API 与构建集成"
```

---

### Task 9: 完整性、并发和崩溃恢复验证

**Files:**
- Modify: `engineering/test/db/storage/blob_engine_test.cpp`
- Modify: `engineering/test/db/storage/CMakeLists.txt`
- Create: `engineering/test/db/storage/blob_recovery_helper.cpp`（若进程级恢复测试需要）
- Modify: `docs/superpowers/specs/2026-08-28-blob-engine-complete-design.md`（记录实际验证结果）

**Interfaces:**
- Consumes：Task 1-8 全部实现
- Produces：可重复的 Blob 专项验收记录

- [ ] **Step 1: 增加完整测试矩阵**

必须包含：标准 SHA-256、4MB 边界、多 Chunk 完整读取、跨 Chunk Range、重复内容去重、Manifest/Chunk 损坏拒绝、Multipart、并发相同内容上传、删除与 GC、Catalog checkpoint/WAL 恢复、所有公共错误路径。

- [ ] **Step 2: 增加并发测试**

启动多个线程同时上传相同数据、不同数据并同时读取已提交 Blob；断言所有返回 Blob ID 正确、Manifest 可校验、没有正式文件被覆盖、没有临时文件被误读。

- [ ] **Step 3: 增加进程级恢复测试**

使用 helper 进程在指定阶段退出，父测试重新打开 Blob Engine 并调用 recover；验证 PREPARE 无 COMMIT 不可见、完整 Manifest 可重建、损坏尾 WAL 不影响已有对象。Windows 使用对应进程退出 API，Linux 使用正常退出码模拟故障点；不把普通测试误称为断电测试。

- [ ] **Step 4: 运行完整验证**

```bash
cmake --build build/engineering --target blob_engine_test
ctest --test-dir build/engineering -R blob_engine_test --output-on-failure --timeout 120
```

预期：所有 Blob 测试通过；失败项记录到 `test-results/engineering/blob/`，不得静默忽略。

- [ ] **Step 5: 更新设计与 tasks 状态**

只把实际完成且测试通过的条目标为 `[x]`；在设计文档附上构建命令、测试命令、测试数量和未覆盖的真实断电场景。

- [ ] **Step 6: 提交**

```bash
git add engineering/test/db/storage/blob_engine_test.cpp engineering/test/db/storage/CMakeLists.txt engineering/test/db/storage/blob_recovery_helper.cpp docs/superpowers/specs/2026-08-28-blob-engine-complete-design.md
git commit -m "test(blob): 完成完整性并发与恢复验证"
```

---

## 计划自检

1. **规格覆盖**：SHA-256、固定 Chunk/Manifest 格式、完整读取、Range、Multipart、独立 Catalog、WAL/checkpoint、两阶段发布、引用计数、延迟 GC、活动读取者、mm_storage、并发与恢复测试分别由 Task 1-9 覆盖。
2. **占位符扫描**：计划中没有要求把未实现代码冒充完成；所有推迟内容都明确为后续任务或测试前置条件。实现过程中禁止使用桩替代验收功能。
3. **类型一致性**：Task 1 的 `sha256_compute` 被 Task 2/3/5 使用；Task 2 的 Chunk checked reader 被 Task 3/5/6/7 使用；Task 3 的 Manifest loader 被 Task 5/6/7 使用；Task 4 的 Catalog 状态被 Task 5/6/7 使用；Task 5 的 Upload API 被 Blob public API 使用。
4. **路径检查**：所有源码位于 `engineering/`，报告位于 `docs/`，测试位于 `engineering/test/`，无根目录业务文件。
5. **兼容性检查**：现有 Blob 基础 API 与旧 Multipart 符号保留；旧 `blob_catalog_create(kv_t*, namespace)` 作为适配层保留，新主路径不依赖 KV 事务。
