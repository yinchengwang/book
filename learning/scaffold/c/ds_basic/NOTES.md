# ds_basic 学习笔记

## 概念地图

链表是 C 语言中最基础也是最容易出错的数据结构——没有之一：

- **单链表**：每个节点含 `data + next*`。头插 O(1)，尾插 O(n)（除非维护尾指针），删除需要前驱节点指针
- **双向链表**：`prev + next` 双指针。给定节点可 O(1) 删除，反向遍历直接走 `prev` 链，代价是多一个指针的内存开销（64 位系统 +8 字节/节点）
- **哨兵节点**：在链表头（或头尾各放）一个不存数据的 dummy 节点，消除"head==NULL"的特殊判断。Linux 内核 `list_head` 用双哨兵环形链表——极致优雅
- **循环检测（Floyd）**：快指针走 2 步，慢指针走 1 步，若相遇则有环。时间 O(n)，空间 O(1)
- **循环链表**：尾节点 next 指向头节点形成环，用于轮询调度（如 Round-Robin）

与本仓库其它卡的关系：`pointer`（指针基础）是链表的前置卡——链表的全部操作都是指针操作。`dynamic_memory`（malloc/free）是链表的内存管理基础。

## 踩坑记录

1. **free 后野指针**：`free(node)` 之后指针仍指向原地址——如果在 free 后继续用该指针遍历，触发 use-after-free UB。本 demo 的单链表 free 用临时变量 `tmp` 保存 next。
2. **哨兵节点的正确用法**：哨兵必须不存业务数据。本 demo 用栈上 `SNode sentinel = {0, NULL}` 省去 malloc，但真实工程中哨兵往往是堆分配——因为链表生命周期可能跨函数。
3. **双向链表删除边界**：删除头节点时要更新 head 指针；删除尾节点时要更新前驱的 next 为 NULL。少更新一项就出 dangling pointer。
4. **循环检测后不能直接 free**：Floyd 算法检测出有环后，如果直接遍历 free 会无限循环——需要先断环再 free。本 demo 跳过带环链表的 free。

## 工程对照（≥100 字硬约束）

链表在工程侧有以下直接复用点：

1. **`engineering/src/db/storage/buffer/bufmgr.c`**：Buffer Pool 用双向链表实现 Clock-Sweep 置换策略。每个 `BufferDesc` 含 `prev/next` 指针串成 LRU 链，`StrategyControl` 维护链表头尾。本卡的双链表操作能直接读懂这段代码的链表遍历与节点摘除。
2. **`engineering/src/db/consensus/raft.c`**：Raft 日志条目 `RaftLogEntry_t` 通过指针数组索引（`entry_at(idx)` 返回指针），本质是隐式链表——log compaction 时涉及"从链头截断一段节点并释放"，就是本卡链表删除的工程级应用。
3. **`engineering/src/db/storage/catalog/catalog.c`**：Catalog 系统表中 `CatalogEntry` 用链表串起同 hash bucket 的表元数据（chaining），`CatalogSearch` 就是本卡 `slist_find` 的升级版。
4. **`engineering/include/db/storage/bufpage.h`**：页面内 `ItemIdData` 数组用偏移量链表管理空闲空间（类似 `offset` 代替 `next*`）——是链表思想在磁盘页内的变体。

学完本卡后能动手的事：在 `engineering/src/db/storage/buffer/` 下加一个 `free_list.c`，用双向链表实现"空闲页链表管理器"——这是 bufmgr 在分配新页时的常见优化路径。
