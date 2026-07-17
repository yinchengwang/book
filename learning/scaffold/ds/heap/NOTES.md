# 工程对照说明：二叉堆实现

## 概述

本 scaffold 实现了一个教学版的二叉堆，与工程轨 `engineering/src/algo-prod/priority_queue/` 中的生产级实现对照学习。

## 工程实现要点

`engineering/src/algo-prod/priority_queue/priority_queue.c` 是生产级优先级队列实现，其设计要点如下：

### 1. 泛型设计

工程实现通过函数指针实现完全泛型：
- `element_size`：支持任意大小元素的存储
- `compare`：用户传入比较函数，支持最大堆/最小堆灵活切换
- `free_element`：可选的元素析构函数，支持复杂对象的生命周期管理

本 scaffold 采用相同设计，展示了核心模式。

### 2. 交换优化

工程实现使用栈缓冲区优化小元素交换：
```c
#define PQ_SWAP_BUF_SIZE 256
char stack_buf[PQ_SWAP_BUF_SIZE];
if (pq->element_size <= PQ_SWAP_BUF_SIZE) {
    tmp = stack_buf;  // 栈上交换，避免 malloc
} else {
    heap_buf = malloc(...);  // 大元素降级到堆
}
```

本 scaffold 采用相同策略，减少小对象场景的内存分配开销。

### 3. Floyd 建堆算法

工程实现的 `pq_heapify` 使用 Floyd 算法，复杂度 O(n)：
```c
/* 从最后一个非叶节点向根逐步下沉 */
i = (count - 2) / 2;
for (;;) {
    pq_sift_down(pq, i);
    if (i == 0) break;
    i -= 1;
}
```

相比逐个 `push` 的 O(n log n)，批量建堆更高效。本 scaffold 的 `heapify` 函数采用相同逻辑。

### 4. 自动扩容

工程实现采用倍增扩容策略：
```c
new_capacity = pq->capacity * PQ_GROWTH_FACTOR;  // 2x
new_data = realloc(pq->data, new_capacity * pq->element_size);
```

本 scaffold 的 `heap_sort` 假设传入缓冲区足够大，生产环境应参考此模式。

### 5. API 对照

| Scaffold API | 工程 API | 说明 |
|-------------|----------|------|
| 手动 swap + sift | `pq_swap` + `pq_sift_up/down` | 工程封装更完整 |
| 内联 heapify | `pq_heapify` | 工程支持从数组批量建堆 |
| 手写 PQ 逻辑 | `pq_push/pq_pop/pq_top` | 工程提供完整 CRUD |
| 无 | `pq_clear/pq_destroy` | 工程提供完整生命周期管理 |

### 6. 与数据库存储的关联

堆（Heap）在数据库系统中有多重含义：
- **本 scaffold 的堆**：数据结构中的二叉堆，用于优先级队列、堆排序
- **数据库的 Heap AM**（`db/storage/access/heap/heapam.c`）：堆表访问方式，无序元组存储

两者虽同名但毫无关系，需注意区分。
