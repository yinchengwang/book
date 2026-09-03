# 无锁数据结构 (Lock-Free Data Structures)

本目录展示 C++17 无锁编程的核心模式，聚焦数据库/并发系统的底层实现。

## 演示内容

| 文件 | 内容 |
|------|------|
| `main.cpp` | CAS 循环、无锁栈、ABA 问题、Hazard Pointer/引用计数回收、并发压测 |
| `Makefile` | `g++ -std=c++17 -lpthread` 编译 |
| `NOTES.md` | 与 `engineering/src/db/concurrency/` 的交叉引用 |
| `README.md` | 本文件 |

## 快速开始

```bash
make        # 编译
make run    # 运行
make clean  # 清理
```

## 核心概念

### CAS (Compare-And-Swap)

原子指令，在一条指令内完成"比较-交换"：
```cpp
head_.compare_exchange_weak(old, new, mo_release, mo_relaxed);
```
- 成功：old 被替换为 new，返回 true
- 失败：old 被更新为当前值，返回 false，需要重试

### 无锁栈 push/pop

```cpp
void push(const T& value) {
    Node<T>* new_node = new Node<T>(value);
    Node<T>* old_head;
    do {
        old_head = head_.load(memory_order_relaxed);
        new_node->next.store(old_head, memory_order_relaxed);
    } while (!head_.compare_exchange_weak(old_head, new_node, ...));
}
```

### ABA 问题

线程 A 读取 head = X，线程 B pop(X) 后 push(Y) 再 push(X)（地址复用），此时线程 A 的 CAS 成功但栈状态已被篡改。

解决思路：双宽度 CAS（Hazard Pointer 标记）、引用计数、版本号标记。

### 内存回收

- **Hazard Pointer**：每个线程标记正在访问的节点，pop 时不立即删除，而是放入待回收队列
- **引用计数**：每个节点持有一个原子计数器，最后持有者负责释放
- 参考：`engineering/src/db/concurrency/mvcc_version.c` 中的 MVCC 版本链回收策略
