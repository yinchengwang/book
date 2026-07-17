## 工程对照

`engineering/src/db/core/guc.c` 中的 GUC 参数解析逻辑复杂，在参数名不匹配时返回 NULL，
但调用方若未检查返回值会导致空指针解引用。用 GDB 条件断点定位：
先 `break guc.c:XXX if param_name != NULL && strlen(param_name) > 0`，然后 `continue`
找到第一个无效参数。Buffer Pool 的 LRU 替换若出现异常驱逐，用 `watch` 监视
`buf_page->refcount` 在多线程同时访问时的变化。`bt`（backtrace）可显示 WAL 刷盘调用的
完整路径：`main` → `exec_simple_query` → `standard_ExecutorRun` → `heap_insert` →
`XLogInsert` → `AsyncWritePage`，用于定位刷盘起点。

在 `bufmgr.c` 的 `BufferAlloc` 函数中，页面置换算法涉及复杂的锁竞争，
当 `clock_sweep` 指针移动异常时，可设置 `watch` 监视 `BufFreelist` 链表长度变化：
`watch BufFreelist->size`，观察是否有非预期的入队/出队操作。
对于 `heapam.c` 中的元组删除，`ItemIdMarkDead` 只标记删除状态，
实际页面 compaction 在 VACUUM 时进行，若发现元组数持续增长但表文件不增长，
可在 `heap_vacuum_rel` 处设断点，追踪 vacuum 的执行频率和影响页面数。

`btreeam.c` 的 BTree 分裂逻辑中，`_bt_split` 函数的递归深度若超过预期，
可能导致栈溢出。可在 `_bt_split` 入口处设置条件断点：
`break btreeam.c:_bt_split if level > 10`，观察 BTree 深度异常的原因。
对于索引扫描结果的正确性，可使用 `print` 命令打印 `_bt_readpage` 返回的
`PageFreeSpaceInfo`，确认页面可用空间计算是否与实际分配一致。

WAL 日志的 `XLogInsert` 函数是高频调用点，若要追踪特定事务的 WAL 记录，
可在 `XLogInsert` 处设置条件断点：
`break XLogInsert if xl_xid == target_xid`，然后 `continue` 直到找到目标事务。
对于 `XLogFlush` 的延迟，可用 `watch` 监视 `WalWrite->write_len`，
观察是否有大块 WAL 数据在积累未刷盘。

`catalog.c` 中的 `CacheInvalidate` 用于通知系统表变更，
若发现缓存失效风暴（invalidation storm），可在 `CacheInvalidate` 处
设置 `condition` 过滤特定表：`break catalog.c:CacheInvalidate if relid == 1259`。
`pg_class` 表的元组查找若频繁失败，可检查 `syscache` 查找逻辑，
用 `bt` 显示调用路径确认是否有不必要的缓存查找。

`wal_buf.c` 的 WAL buffer 是环形缓冲区，当写入位置回到起点时需要刷盘。
可用 `print` 命令打印 `WalBuf->write_pos` 和 `WalBuf->flush_pos` 的差值：
`print WalBuf->write_pos - WalBuf->flush_pos`，观察缓冲区填充速度与刷盘速度的匹配情况。
若差值持续增长，说明刷盘速度跟不上写入速度，可能导致 WAL 阻塞。

在 `db_server.c` 的连接处理中，若发现特定客户端连接后立即断开，
可能是协议解析错误。可在 `pq_getmsg*` 系列函数设断点，
用 `print` 打印已解析的消息内容，确认协议数据是否正确。
对于 session 级别的变量作用域错误，可用 `info locals` 和 `info args`
对比函数入口和当前位置的变量值，观察是否有意外修改。
