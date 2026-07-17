# dynamic_memory 学习笔记

## 概念地图

C 标准库动态内存三件套：

- **`malloc(size)`**：分配 size 字节，不初始化；返回 `void *`
- **`calloc(count, size)`**：分配 count*size 字节并清零；适合数组分配
- **`realloc(p, new_size)`**：调整 p 大小；可能移动内存到新地址；失败返回 NULL，p 仍有效
- **`free(p)`**：释放；p 必须是非空、此前由 malloc/calloc/realloc 返回的指针
- **`aligned_alloc(align, size)`**（C11）：对齐分配，SIMD 与硬件 cache 行常用

内存来源：进程堆（heap）；OS 提供 `sbrk` 或 `mmap` 兜底。glibc malloc 走 slab + tree-bin 结构，free list 设计远超教学范围——但知道"分配是耗时的"足够提醒读者避免 hot path malloc。

## 踩坑记录

1. **`realloc` 用错模式**：`p = realloc(p, n)` 直接赋值，失败时 p 指针丢失（原指针仍指向旧内存泄漏）。本 demo 用 `q = realloc(p, n); if (!q && n) exit; p = q;` 是正确模式。
2. **`malloc(0)` 实现定义**：glibc 返回非空指针；其他实现可能返回 NULL。本 demo 不踩。
3. **未初始化堆**：malloc 不会清零；读取未初始化内存是 UB。本 demo 用 calloc 演示对比。
4. **dangling pointer**：`free(p); printf("%d\n", *p);` 是 UB。本 demo 显式 `p = NULL` 防御。
5. **double free**：两次 free 同一指针导致 glibc abort 或堆破坏。本 demo 不演示——UB。
6. **glibc malloc 的 ptmalloc2**：64 位系统对齐 16 字节，metadata 内嵌在 chunk 头；umem / jemalloc 不一样——本仓库工程侧通常假设 glibc。
7. **mmap 阈值**：分配 > 128KB（默认）走 mmap 而非 sbrk；free 时直接 munmap——避免大对象 memory fragmentation。

## 工程对照（≥100 字硬约束）

1. **`engineering/src/db/storage/bufmgr.c` 的 BufferPool**：每页 8KB 用 `palloc(sizeof(Buffer))` 分配 Buffer 节点；num_buffers × page_size 大小决定走 sbrk 还是 mmap。本卡刷过后可读懂 `palloc` wrapper 的副作用。
2. **`engineering/src/algo-prod/quantizer.c`**：PQ 码本加载到 `float *centroids` 后用 `realloc` 扩到 256 × dim；这是 2 倍扩容的标准做法，可平滑处理"训练完成后才知道精确码本大小"的场景。
3. **`engineering/src/db/consensus/raft.c`**：log entries 数组 `entries = malloc(cap * sizeof(RaftLogEntry))`，realloc 扩容；本卡 2 倍扩容策略与 PG WAL 段管理一致。
4. **`rag/src/rag/index/hnsw_index.cpp`**（C++ 但思路相通）：HNSW 图邻接表用 `std::vector<node_id_t>`，新节点 push 时容量自动 2 倍扩张。本卡 2 倍策略可迁移到 C++ reserve()。
5. **`engineering/src/db/storage/slab_allocator.c`**（假设未来添加）：slab 分配器是"同尺寸对象零开销分配"——本卡**直接**是 slab 的核心实现思路（int_vec 的 2 倍扩容本质就是对象池）。

学完本卡能动手的事：

- 在 `engineering/src/db/storage/` 加 `slab_allocator.c`：实现 `slab_create(obj_size)` / `slab_alloc` / `slab_free` 三件，把 `Buffer` 节点从 malloc/free 切到 slab，减少 fragmentation 与 syscall 次数。
- 在 `engineering/include/db/` 加 `xmalloc.h` 工程侧通用 wrapper：所有 malloc/calloc/realloc 用 x 系列替身，OOM 走统一的 `fprintf(stderr, ...); abort();` 路径。
