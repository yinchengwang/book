# 堆栈溢出与尾递归

## 核心概念

### 栈帧 (Stack Frame)

每次函数调用时，编译器在栈上分配一块连续内存，称为栈帧。栈帧存储了以下关键信息：

- **返回地址**：函数执行完成后返回的地址
- **局部变量**：函数内定义的自动变量（auto）
- **参数传递**：超出寄存器数量的函数参数
- **保存的寄存器**：调用者保存（caller-saved）和被调用者保存（callee-saved）的寄存器值

x86-64 下使用 RSP（栈顶指针）和 RBP（基址指针）定位栈帧。函数调用时 RSP 递减（栈向低地址增长），返回时递增。

```c
// 每次调用分配约 256+ 字节的栈帧空间
static int normal_recursion(int n) {
    if (n <= 0) return 0;
    char buf[256];               // 局部变量在栈帧内分配
    memset(buf, 0, sizeof(buf));
    return normal_recursion(n - 1) + buf[0];  // 递归后还有操作
}
```

### 尾递归优化 (Tail Call Optimization, TCO)

尾递归是指递归调用是函数执行的最后一个操作，且返回值直接传递，不经过额外计算。编译器（需 `-O2` 或更高）可将尾递归转换为循环，复用当前栈帧：

```c
// 尾递归：编译器可优化为迭代
static int tail_recursion_impl(int n, int acc) {
    if (n <= 0) return acc;
    return tail_recursion_impl(n - 1, acc + n);  // 最后操作，直接返回
}
```

优化后的汇编代码相当于：

```c
static int tail_recursion_opt(int n, int acc) {
    while (n > 0) { acc += n; n--; }
    return acc;
}
```

**关键**：C11 标准未强制要求编译器实现 TCO，GCC/Clang 在 `-O2` 及以上级别才进行优化。MSVC 对尾递归优化支持较弱。

### 栈保护 (Stack Canary)

编译器在栈帧的局部变量与返回地址之间插入一个随机值（canary）。函数返回前检查 canary 是否被覆盖——若被修改则说明发生栈缓冲区溢出，立即调用 `__stack_chk_fail` 终止程序：

```
栈布局（高地址 → 低地址）:
+------------------+
| 返回地址         |
+------------------+
| 保存的 RBP       |
+------------------+
| Canary (随机值)  |  ← 编译器插入
+------------------+
| 局部变量（buffer）|  ← 溢出发生在这里
+------------------+
```

在 `main.c` 中手工模拟了 canary 保护原理：在 buffer 旁放置标记值，溢出后检查标记是否被改写。

### ulimit -s

Unix 系统通过 `ulimit -s` 查看/修改栈大小限制：

```bash
ulimit -s            # 查看当前栈大小（KB），默认 8192（8MB）
ulimit -s unlimited  # 取消栈大小限制（不推荐生产使用）
```

栈大小为 8MB 时，安全递归深度通常为 1 万至 10 万次（取决于栈帧大小）。栈帧越大，可递归次数越少——`main.c` 中每个普通递归栈帧约 256+ 字节，10 万次调用即消耗约 25MB，远超默认限制。

## 关键要点表

| 概念 | 机制 | 影响 | 工程注意事项 |
|------|------|------|-------------|
| **普通递归深度** | 每次调用分配新栈帧，深度过大导致栈溢出 | Linux 8MB 下约 1 万次（256B 栈帧） | 生产中递归深度超过 1000 应考虑改写为迭代 |
| **尾递归 (TCO)** | 编译器复用当前栈帧，不消耗额外栈空间 | 理论上递归深度无上限 | 需 `-O2` 编译，C 标准不强制；MSVC 支持弱 |
| **Canary** | 栈帧中插入随机值，返回前校验 | 检测栈缓冲区溢出，失败时调用 `__stack_chk_fail` | GCC 默认开启 `-fstack-protector-strong` |
| **ASLR** | 栈基址每次加载随机化 | 使攻击者难以预测返回地址位置 | 现代 OS 默认开启（`/proc/sys/kernel/randomize_va_space`） |

### 额外说明

- **ASLR (Address Space Layout Randomization)**：每次进程启动时栈基址随机化，配合 canary 构成双重防护——ASLR 让攻击者无法确定栈地址，canary 检测溢出发生。
- **VLA (Variable Length Arrays)**：C99 引入，分配在栈上。如果传入过大尺寸（如 16MB），会直接导致栈溢出。C11 将 VLA 改为可选特性。

## 工程对照

### 有限资源下的分配与回收策略

栈溢出问题的本质是在**有限栈空间**中，递归调用不加节制地消耗栈帧资源，导致空间耗尽。工程系统中存在大量类似的"有限资源管理"问题，核心思想都是通过合理的分配和回收策略，在受限资源下提供最大处理能力。

**1. Buffer Pool Clock-Sweep 置换算法**

`engineering/src/db/storage/buffer/bufmgr.c` 实现了 Clock-Sweep 置换算法，管理 `shared_buffers = 128MB` 的有限缓冲区空间。当需要加载新页面但 Buffer Pool 已满时，Clock-Sweep 通过环形指针扫描 buffer 描述符，逐轮递减 `usage_count`，选择使用次数最少的页面置换。这与栈帧管理的思路如出一辙——栈帧"分配"对应 buffer 分配，栈帧"回收"对应 buffer 置换。不同之处在于栈帧按 LIFO 顺序自动回收（函数返回），而 Buffer Pool 需要主动的置换策略来应对任意访问模式。

**2. Slab / 伙伴系统分配器**

`engineering/src/db/storage/mm/mm_pool.c` 实现了 Slab 分配器和 Arena 分配器。Slab 分配器将内存划分为固定大小的对象缓存（object cache），避免频繁 malloc/free 产生的碎片和开销。伙伴系统（Buddy System）通过 2 的幂次分割合并管理物理内存。它们的共同点是：预分配 + 复用，减少动态分配的开销。栈帧的管理也是类似——函数调用时栈帧的分配仅需移动 RSP 指针，是最高效的分配方式之一。

**3. GUC work_mem 参数**

工程中 `initdb.c` 默认写入 `work_mem = 4MB`，这是查询排序、哈希连接等操作可使用的工作内存上限。当排序数据量超过 work_mem 时，需要溢出到磁盘临时文件——这与递归超过栈深度时的"栈溢出"异曲同工。`work_mem` 相当于给每个查询操作划定了"栈上限"，超限时换一种策略（磁盘 swap），而非放任不管导致崩溃。

**对照总结**：

| 系统 | 有限资源 | 分配策略 | 回收/降级策略 |
|------|----------|----------|-------------|
| 栈 (Stack Overflow) | 8MB 栈空间 | RSP 自动压栈 | SIGSEGV（溢出即崩溃） |
| Buffer Pool | shared_buffers=128MB | Hash 表 O(1) 定位 | Clock-Sweep 置换 |
| 内存池 (mm_pool) | Chunk 按需增长 | Slab 缓存对象复用 | Chunk 释放，对象回收到 free list |
| 查询执行 | work_mem=4MB | 内存排序/哈希 | 溢出到磁盘临时文件 |

**核心启示**：无论栈、Buffer Pool、内存池还是查询执行，都是在有限资源下做资源调度。栈最简单（LIFO 自动分配回收），也最脆弱（无法降级，溢出即崩溃）。工程系统的成熟度体现在"超出限制时能否优雅降级"——Buffer Pool 找到 victim 页面刷盘、排序溢出到临时文件，都是优雅降级的体现。

## 面试要点

1. **TCO 需要 `-O2` 级别**：GCC/Clang 默认 `-O0` 不进行尾递归优化，未经优化的尾递归同样会栈溢出。面试中可主动提"需要开启编译器优化"。
2. **不是所有递归都能优化**：只有当递归调用是函数最后一个操作且结果直接返回时，编译器才能优化。`fact(n) = n * fact(n-1)` 这种"递归后还有乘法"的代码无法使用 TCO，需要手动改写成累加器风格。
3. **VLA 分配在栈上**：`int arr[n]` 这样的 VLA 数据在栈上分配——如果 n 很大（如百万级）会直接导致栈溢出。C11 已将 VLA 改为可选特性，C++ 不支持 VLA。工程中优先使用堆分配（malloc）处理大数组。
4. **栈保护是编译期机制**：canary 由编译器在**编译时**插入，不开启 `-fstack-protector` 时不会生效。嵌入式或性能敏感场景可能关闭此特性。
5. **`-fstack-check` 与 `-fstack-protector` 的区别**：前者在编译时插入栈边界探测代码，检测硬件栈溢出；后者是 canary 机制，检测软件缓冲区溢出。
