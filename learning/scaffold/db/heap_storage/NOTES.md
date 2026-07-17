# heap_storage 学习笔记

## 对照 engineering/src/db/storage/access/heap/heapam.c 的分析

### 1. 堆表页面布局

参考 `heapam.c` 第 47-65 行的 `heap_page_init` 函数：

```c
void heap_page_init(void *page, size_t size) {
    PageHeader ph = (PageHeader)page;
    ph->pd_lsn = 0;
    ph->pd_checksum = 0;
    ph->pd_flags = 0;
    ph->pd_lower = SizeOfPageHeaderData;  // LinePointer 数组起始
    ph->pd_upper = size;                   // 空闲空间结束
    ph->pd_special = size;
    ph->pd_pagesize_version = 0x0001;
    ph->pd_prune_xid = 0;
    ph->pd_xid_base = 0;
    ph->pd_multi_base = 0;
}
```

页面布局采用经典的 "Zeus" 结构（两端向中间增长）：
- **pd_lower**：LinePointer 数组起始位置，初始为 `SizeOfPageHeaderData`（24字节）
- **pd_upper**：元组数据结束位置，初始为 `HEAP_PAGE_SIZE`（8192字节）
- **空闲空间**：`pd_upper - pd_lower`，元组插入时从此区域分配

### 2. 元组组织方式

参考 `heapam.c` 第 80-118 行的 `heap_page_add_tuple` 函数：

```c
int heap_page_add_tuple(void *page, const void *tuple, size_t len,
                        HeapLinePointer *lp) {
    // 1. 检查空间
    int free_space = ph->pd_upper - ph->pd_lower;
    int needed = len + SizeOfHeapLinePointer;
    if (free_space < needed) return -1;

    // 2. 计算元组位置（在 pd_upper 处向前）
    uint16_t off = ph->pd_upper - len;
    ph->pd_upper = off;

    // 3. 放置 LinePointer（在 pd_lower 处向后）
    HeapLinePointer lp_ptr = (HeapLinePointer)((char *)page + ph->pd_lower);
    lp_ptr->t_off = off;      // 记录元组偏移
    lp_ptr->t_flags = 0;
    lp_ptr->t_xmax = 0;

    // 4. 复制元组数据
    memcpy((char *)page + off, tuple, len);

    // 5. 更新 pd_lower
    ph->pd_lower += SizeOfHeapLinePointer;
}
```

关键点：
- **元组从高地址向低地址增长**（从 `pd_upper` 向 `pd_lower`）
- **LinePointer 从低地址向高地址增长**（从 `pd_lower` 向 `pd_upper`）
- 两者相遇时页面已满，需要页面分裂或创建新页面

### 3. TID 定位机制

TID（Tuple Identifier）由两部分组成：
- **blockno（块号）**：页面在文件中的序号
- **offset（偏移）**：LinePointer 在页面中的偏移

参考 `heapam.c` 第 202-260 行的 `heap_delete` 函数：

```c
// 解析 TID
const uint8_t *tid_data = (const uint8_t *)tid;
uint32_t blocknum = 0;
uint16_t offset = 0;
memcpy(&blocknum, tid_data, sizeof(uint32_t));
memcpy(&offset, tid_data + sizeof(uint32_t), sizeof(uint16_t));

// 读取页面并定位 LinePointer
PageHeader ph = heap_page_get_header(page);
if (offset < SizeOfPageHeaderData || offset >= ph->pd_upper) {
    return -1;
}

HeapLinePointer lp = (HeapLinePointer)(page + offset);
```

定位流程：
1. 根据 `blocknum` 读取对应页面
2. 根据 `offset` 定位到 LinePointer
3. LinePointer 的 `t_off` 字段指向元组数据的实际位置
4. 通过 `t_xmax` 判断元组是否被删除

### 4. 元组删除（打墓碑）

参考 `heapam.c` 第 248-250 行：

```c
/* 设置 t_xmax 标记删除 */
lp->t_xmax = 1;
lp->t_flags |= HEAP_XMAX_INVALID;
```

采用 **墓碑机制**：
- 不立即清理元组数据，只设置 `t_xmax != 0`
- 通过 `HEAP_XMAX_INVALID` 标志标记为无效
- 后续通过 `heap_page_prune` 进行页面清理

### 5. 与原版 PostgreSQL 的差异

| 特性 | 本项目 | PostgreSQL |
|------|--------|------------|
| 页面大小 | 8KB | 8KB |
| PageHeader | 简化版（24字节） | 24字节（对齐） |
| TID 编码 | block(4B) + offset(2B) | block(4B) + offset(2B) |
| MVCC | 简化版 | 完整 MVCC |
| WAL | 待实现 | 完整 WAL |

### 6. 学习要点

1. **双向增长**：理解为何元组和 LinePointer 要从两端向中间增长
2. **空间回收**：墓碑机制如何支持 MVCC 的快照隔离
3. **TID 设计**：为何 TID 需要包含 blockno 和 offset 两部分
4. **页面分裂**：当单页无法容纳元组时如何处理
