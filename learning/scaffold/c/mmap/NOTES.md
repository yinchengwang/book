# mmap 学习笔记

## 概念地图

`mmap(2)` 把文件（或匿名内存）映射进进程虚拟地址空间。访问这些内存时内核按 page fault 自动把磁盘 page 调入。机制核心：

- **VA → file offset 映射**：对程序员而言像普通内存；对内核是 page cache 的视图
- **MAP_SHARED**：多个进程共享同一物理页；写回磁盘；适合 IPC（共享内存）
- **MAP_PRIVATE**：COW；写只影响本进程；fork 后父子进程互不影响
- **MAP_ANONYMOUS**：无文件 backing，等价 `malloc` 但 page-aligned、按需分配
- **madvise(2)**：告诉内核预期访问模式（WILLNEED 预读、SEQUENTIAL 顺序读、RANDOM 随机、DONTNEED 释放）

mmap 优势：避免 `read+buffer` 的双重拷贝，对 GB 级数据（向量、模型权重）大文件尤其重要。

## 踩坑记录

1. **MinGW-w64 缺 `<sys/mman.h>`**：本机编译直接 `#error`。Windows 上等价 API 是 `CreateFileMapping` + `MapViewOfFile`，API 哲学不同但能力对等。本卡聚焦 Linux/UNIX。
2. **mmap 大小必须是 page 整数倍**：POSIX 上 mmap 自动补齐；但 `madvise` 段长度应 <= 文件大小。
3. **`MAP_FAILED` 是判断标志**：`void *` 类型不像 malloc 返回 NULL 失败——必须显式判 `MAP_FAILED`（即 `(void*)-1`）。
4. **读时复制 vs 共享**：本 demo 用 MAP_PRIVATE，只读 PROT_READ，不写不触发 COW。读者可改 PROT_WRITE 试 `ptr[0]='x'` 触发 COW——但要小心别污染测试数据。
5. **`madvise(MADV_WILLNEKEN)` 返回值**：POSIX 允许实现忽略（返回 0）+ 但某些 mman 实现会返回 EINVAL 或 ENOSYS；本 demo 对 EINVAL/ENOSYS 不报错。
6. **`ftruncate` 之后 mmap**：匿名 mmap 不能 ftruncate，但文件 mmap 可以靠 `ftruncate(fd, new_len)` 扩展。本 demo 不演示该路径。

## 工程对照（≥100 字硬约束）

mmap 在 rag 服务化与 minivecdb 控制面有以下直接迁移价值：

1. **minivecdb 加载 embedding 大向量**：向量维度 1536 / 2048 float × N=10M 行 ≈ 数十 GB；用 mmap 加载到进程虚拟地址空间，避免 malloc + read 重复一次 page cache 拷贝。这是 milvus 与 chroma 的标准手法。
2. **raft snapshot 落盘**：`engineering/include/db/consensus/raft.h` 触发 snapshot 时，新 segment 可直接 mmap 后 `write`，因为 page cache 与磁盘天然对齐，避免一次用户态 buffer 中转。你的 `engineering/src/db/consensus/raft.c` 当前实现走 `fwrite`+ `fsync`，未来重构可借鉴 RocksDB WAL 的 mmap 路线。
3. **rag-remote-index-backend 的远程索引二进制协议**：当前 SDK 走 JSON + HTTP，未来高吞吐场景可考虑**共享内存 mmap + Unix domain socket**，把远程索引库的 HNSW graph 共享给 RAG Engine，避免序列化与拷贝。本卡刷过后读者能识别 shared memory 路线。
4. **`engineering/src/algo-prod/quantizer.c` 与 PQ 码本**：乘积量化码本加载到内存，mmap 后对 quantization 直接 `madvise(MADV_RANDOM)` 让内核放弃预读——本卡刷过后读者懂得对码本应用随机访问建议。

学完本卡后能动手的事：在 `engineering/src/db/storage/` 下加 `buf_mmap.c`，把 4KB 以上的 vector page 直接 mmap 进 BufMgr 缓存槽位——这是 minivecdb 性能突破的真实杠杆点。
