# NOTES.md - 无锁编程交叉引用

本文档记录 `learning/scaffold/cpp/lockfree/` 与 `engineering/src/db/concurrency/` 之间的设计关联。

## engineering/src/db/concurrency/ 模块结构

| 文件 | 职责 | 与无锁编程的关联 |
|------|------|------------------|
| `mvcc_version.c` | MVCC 版本链管理（xmin/xmax 可见性） | 版本链头插法即是无锁栈的 push 模式；`mvcc_version_insert()` 与 `LockFreeStack::push()` 逻辑同构 |
| `mvcc_gc.c` | MVCC 垃圾回收（过期版本清理） | 对应无锁栈的内存回收问题——何时可以安全释放已 pop 的节点？MVCC 用 `xmax` 标记删除事务，等所有 reader 可见性检查通过后回收 |
| `mvcc_snapshot.c` | MVCC 快照读取 | 无锁场景下 reader 线程需要一个稳定的"观测快照"，与 Hazard Pointer 思想一致——在操作期间保护节点不被回收 |
| `mvcc_visibility.c` | 元组可见性判断 | `xmin <= snapshot.max_txn && xmax == INVALID` 的判断逻辑，本质上是一种**无锁版本的链表遍历保护** |

## 核心对应关系

### 1. CAS 循环 vs 版本链插入

```cpp
// learning/scaffold/cpp/lockfree/main.cpp - 无锁栈 push
do {
    old_head = head_.load(memory_order_relaxed);
    new_node->next.store(old_head, memory_order_relaxed);
} while (!head_.compare_exchange_weak(old_head, new_node, ...));

// engineering/src/db/concurrency/mvcc_version.c - 版本链插入
new_version->next = *head;
*head = new_version;
// 注意：mvcc_version.c 当前实现不是原子的（未加锁），
// 生产环境应使用类似上方的 CAS 循环来保证并发安全
```

### 2. ABA 问题 vs MVCC 地址复用

在 MVCC 中，ctid（块号+偏移）唯一标识一行物理位置，不会出现 C 指针级别的 ABA 问题。但如果用 raw pointer 做链表，ABA 问题就真实存在。

解决思路：
- **双宽度 CAS**（80-bit on x86-64）：用 pointer + counter 组成 128-bit 值一起比较交换
- **Hazard Pointer**：pop 时标记节点，reclaimer 线程延迟删除
- **引用计数**：每个节点维护原子 ref_count，最后一个释放者负责删除

### 3. 内存回收 vs MVCC GC

MVCC GC (`mvcc_gc.c`) 的策略是"延迟批量回收"：
1. 扫描所有活跃事务的快照，找出不可见的旧版本
2. 将这些版本从版本链中摘链（通过 CAS）
3. 统一释放

这与 Hazard Pointer 队列的思路一致——**不在 pop 时立即 delete，而是在确认无读者后批量回收**。

## 参考资料

- `engineering/include/db/concurrency/mvcc.h` - MVCC 接口定义
- `engineering/include/db/storage/txn/mvcc.h` - 存储层 MVCC 扩展
- 建议结合 `engineering/src/db/concurrency/mvcc_version.c` 阅读，理解版本链的完整生命周期
