# std::atomic 进阶笔记

## 内存序与工程实践

### release-acquire 的典型应用

生产者和消费者之间的数据同步是 release-acquire 最经典的场景：

```cpp
// 生产者
data = compute();
ready.store(true, std::memory_order_release);

// 消费者
while (!ready.load(std::memory_order_acquire)) {
    std::this_thread::yield();
}
process(data);
```

这种模式保证了：只要消费者看到 ready==true，就能看到之前生产者的所有写入。

### CAS 循环的工程考量

1. **假失败 (spurious failure)**：`compare_exchange_weak` 可能无故返回 false，
   但开销更小，适合循环内使用。
2. **ABA 问题**：CAS 无法检测值从 A→B→A 的变化，需配合引用计数或版本号。
3. **backoff 策略**：高竞争场景下可添加随机退避，减少总线争用。

---

## 与工程代码的交叉引用

### MVCC 版本链中的原子操作

在 `engineering/src/db/concurrency/mvcc_version.c` 中，版本链的插入操作
虽然使用的是普通指针操作（`new_version->next = *head; *head = new_version;`），
但在实际并发环境中，这些操作需要原子化：

```c
// mvcc_version_insert 当前实现（需改进）
void mvcc_version_insert(mvcc_version_t **head, mvcc_version_t *new_version)
{
    new_version->next = *head;  // 非原子！
    *head = new_version;        // 非原子！
}
```

若要支持并发 MVCC，需改用 CAS 循环：

```c
// 改进后的无锁版本链插入
void mvcc_version_insert_atomic(std::atomic<mvcc_version_t*> *head,
                                 mvcc_version_t *new_version)
{
    mvcc_version_t *old_head;
    do {
        old_head = atomic_load(head);
        new_version->next = old_head;
    } while (!atomic_compare_exchange_weak(head, &old_head, new_version));
}
```

### 原子操作在数据库存储中的角色

1. **版本号生成**：事务 ID 的递增必须原子
2. **可见性判断**：`(xmin <= snapshot.xmin && xmax == INVALID) || (xmax > snapshot.xmax)`
3. **垃圾回收**：原子地更新旧版本的 next 指针

---

## 常见误区

| 误区 | 正确做法 |
|------|----------|
| "relaxed 就够用" | 仅当操作间无依赖时才安全 |
| "seq_cst 最安全就用它" | 开销最大，必要时才用 |
| "CAS 万能" | 需处理 ABA 问题和高竞争场景 |
