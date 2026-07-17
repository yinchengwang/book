## 工程对照

在 DB 存储引擎中，`perf record -g` 配合 FlameGraph 可精确定位 Buffer Pool
的热点路径。例如：`buf_get_page@bufmgr.c` → `hash_search@htab.c` →
`malloc@sys` 的调用栈，在火焰图上表现为从 `buf_get_page` 向外辐射的
多个分支。WAL 刷盘（`wal_flush` → `disk_write_page` → `fsync`）的
火焰图顶端通常在 `fsync` 处突然变宽，说明 IO 等待是主要瓶颈。
BTree 插入时 `btree_insert` → `buf_page_get` → `lock_acquire` 的
锁等待会在火焰图上表现为 `lock_acquire` 方块异常宽大。
