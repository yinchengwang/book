# Linux 内存屏障与 C++ 内存序对照

## 映射关系

| Linux 屏障指令 | C++ 内存序 | 语义说明 |
|----------------|-----------|----------|
| `mb()` (full memory barrier) | `memory_order_seq_cst` | 所有读写操作全局有序，强制 CPU 刷新 Store Buffer |
| `smp_mb()` (SMP 屏障) | `memory_order_seq_cst` | 多核间全序，需配合内存屏障指令（如 x86 MFENCE） |
| `smp_wmb()` (写屏障) | `memory_order_release` | 保证所有 release 前的写操作对其他核可见 |
| `smp_rmb()` (读屏障) | `memory_order_acquire` | 保证所有 acquire 后的读操作看到 release 前的写入 |
| 无屏障（编译器屏障 `barrier()`） | `memory_order_relaxed` | 仅原子性，无内存序保证 |

## 详细说明

### memory_order_seq_cst

对应 Linux 的 `smp_mb()`。在 x86 架构下编译为 `MFENCE` 指令，强制所有未完成的 store 操作 flush 到内存，使所有核看到一致状态。在 ARM/RISC-V 下会插入 `dmb ish`（数据内存屏障）指令。

### memory_order_release

对应 Linux 的 `smp_wmb()`。release 操作确保所有在此之前发生的写入（program order）都对后续的 acquire 操作可见。在 Linux 内核中，通过 `smp_wmb()` 确保设备 I/O 写入在 DMA 传输前完成。

### memory_order_acquire

对应 Linux 的 `smp_rmb()`。acquire 操作确保所有在此之后发生的读取都能看到 release 之前的写入。在 Linux 内核中，用于实现自旋锁的解锁语义：`spin_unlock()` 包含 release，`spin_lock()` 包含 acquire。

### memory_order_consume

Linux 无直接等价物。依赖编译器的 dependency ordering，将数据依赖传播到后续读取。在 Linux 内核中，通常使用 `smp_rmb()` 或显式 `dma_rmb()` 代替，因为 consume 的实现曾存在 bug（部分 GCC 版本忽略 dependency）。

### memory_order_relaxed

仅对应 Linux 的编译器屏障 `barrier()`（`__asm__ __volatile__("" ::: "memory")`），无硬件屏障。保证操作是原子的，但不提供任何跨线程同步保证。适用于计数器等纯本地状态更新。

## 实际案例：Linux 自旋锁

```c
// Linux 内核自旋锁实现（简化）
static inline void spin_lock(spinlock_t *lock) {
    while (atomic_dec_and_test(&lock->counter)) {
        // smp_mb() + yield
    }
    // implicit acquire barrier
}

static inline void spin_unlock(spinlock_t *lock) {
    atomic_set(&lock->counter, 1);
    // implicit release barrier via smp_mb() in atomic_set
}
```

对应 C++ 实现：

```cpp
std::atomic<int> lock_flag{1};

void lock() {
    while (lock_flag.fetch_sub(1, std::memory_order_acquire) != 1) {
        // spin
    }
}

void unlock() {
    lock_flag.store(1, std::memory_order_release);
}
```

## 总结

Linux 内存屏障提供了更底层的控制，C++ 内存序是更高层的抽象。实际系统编程中，Linux 内核使用显式屏障指令确保特定内存序，而 C++ 标准库通过内存序参数将这一能力暴露给用户，同时隐藏了平台差异（如 x86 的隐式屏障 vs ARM 的显式屏障）。
