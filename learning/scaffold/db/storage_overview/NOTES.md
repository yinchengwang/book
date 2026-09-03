# 工程对照笔记

基于 `engineering/include/db/` 中的实际头文件实现，描述 Catalog、Buffer Pool、Heap、BTree 四大组件的协作关系。

## Catalog (catalog.h)

Catalog 是数据库的元数据中心，对应 PostgreSQL 的 pg_class、pg_attribute、pg_index 等系统表。

工程实现中，`catalog_create_table()` 创建新表并分配唯一 OID；`catalog_lookup_table()` 按名称查找表，返回 filenode 用于后续存储操作；`catalog_get_indexes()` 获取表关联的索引信息。Catalog 与 Buffer Pool 共享元数据缓存，修改表结构后调用 `catalog_invalidate_table()` 使缓存失效。

## Buffer Pool (buf.h)

Buffer Pool 是数据库的内存缓存层，缓存磁盘页面以减少 I/O。核心接口包括 `buf_read()` 读取页面、`buf_new_page()` 分配新页面、`buf_pin()` / `buf_unpin()` 控制页面引用计数、`buf_dirty()` 标记脏页。

Clock-Sweep 置换算法通过 usage_count 选择牺牲页：每轮扫描清除 refbit=1 的页的引用位，当 refbit=0 且 refcount=0 时选择换出。BufferDesc 结构包含 buf_id、relfilenode、blocknum、usage_count、refcount、is_dirty 等字段。工程实现中通过 Hash 表加速页面查找，与 WAL 协同确保修改前先记录日志。

## Heap (heapam.h)

Heap AM 负责表数据的存储和访问。页面大小固定为 8KB，页面头部(PageHeader)包含 pd_lsn、pd_lower、pd_upper、pd_special 等字段。LinePointer 数组指向实际元组，通过 t_flags 标志区分 LIVE/DEAD 元组。

`heap_insert()` 调用 `heap_page_add_tuple()` 在页面中插入元组，更新 pd_lower（分配 LinePointer）和 pd_upper（写入元组数据）。`heap_page_prune()` 清理死亡元组，回收页面空间。Heap 通过 BufferDesc 获取页面内容，修改后调用 `buf_dirty()` 标记脏页。

## BTree (btreeam.h)

BTree AM 负责索引的存储和访问，对应 PostgreSQL 的 nbtree 实现。BTPageHeader 包含 btpo_flags（页面类型）、btpo_level（层级）、btpo_prev/btpo_next（双向链表指针）、btpo_count（条目数）。

`btinsert()` 定位叶子页面，插入键值和 heap_ptr；如页面已满则触发分裂，更新父节点 downlink。`bt_beginscan()` / `bt_getnext()` 实现索引扫描。内部节点存储 downlink 指向子页面，叶子节点存储 heap_ptr 指向堆表元组。

## 组件协作流程

一次 INSERT 操作的数据流：

1. SQL 层调用 `catalog_lookup_table()` 查找表元数据，获取 filenode
2. `buf_new_page(filenode)` 从 Buffer Pool 分配/复用页面，`buf_pin()` 固定页面
3. `heap_page_add_tuple()` 在 Heap 页面中插入元组，`buf_dirty()` 标记脏页
4. `btinsert()` 在 BTree 索引中插入键值对，可能触发页面分裂
5. 事务提交时，`buf_unpin()` 释放页面引用，WAL 确保日志先落盘
6. Checkpoint 时脏页刷盘，Catalog 缓存按需失效并重新加载

## 关键数据结构对照

| 组件 | 工程结构体 | 核心字段 |
|------|-----------|---------|
| Catalog | table_info_s | oid, name, filenode, npages |
| Buffer | BufferDesc_s | buf_id, relfilenode, blocknum, usage_count, refcount |
| Heap | PageHeaderData | pd_lsn, pd_lower, pd_upper, pd_special |
| BTree | BTPageHeaderData | btpo_flags, btpo_level, btpo_count |

## 真实源码路径

- Catalog: `engineering/include/db/catalog.h`
- Buffer Pool: `engineering/include/db/buf.h`
- Heap: `engineering/include/db/heapam.h`
- BTree: `engineering/include/db/btreeam.h`
- Relation 抽象: `engineering/include/db/rel.h`

参考这些头文件可以深入理解各组件的 API 设计、并发控制策略和持久化机制。
