# std::atomic 内存序演示

本模块演示 C++ std::atomic 的核心概念与内存序语义。

## 演示内容

| 示例 | 说明 |
|------|------|
| `demo_basic_atomic` | 基本原子操作：fetch_add, load, store |
| `demo_acquire_release` | release-acquire 同步语义 |
| `demo_seq_cst` | seq_cst 顺序一致性 |
| `demo_cas_loop` | CAS 循环实现无锁栈 |

## 编译运行

```bash
make        # 编译
make run    # 编译并运行
make clean  # 清理
```

## 核心概念

### 内存序等级（从弱到强）

1. **memory_order_relaxed** - 仅保证原子性，无同步
2. **memory_order_acquire** - 读取端，获取之前所有写入
3. **memory_order_release** - 写入端，之后所有读取可见
4. **memory_order_seq_cst** - 全局顺序一致（默认、最安全）

### CAS 操作

- `compare_exchange_weak` - 可能假失败（spurious failure）
- `compare_exchange_strong` - 不会假失败
- 典型模式：`while (!atomic.compare_exchange_weak(...)) {}`

## 参考资料

- C++17 标准 [atomics] 章节
- 《C++ Concurrency in Action》Anthony Williams
