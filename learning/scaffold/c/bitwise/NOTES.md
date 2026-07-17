# bitwise 学习笔记

## 概念地图

位运算是 C 语言离硬件最近的抽象——直接对应 CPU 的 AND/OR/XOR/SHIFT 指令周期：

- **位掩码四原语**：`set = | (1<<n)`、`clear = & ~(1<<n)`、`toggle = ^ (1<<n)`、`test = & (1<<n)`。这四个操作是 page header 位字段、flags 枚举、bitmap 索引的全部基础
- **左移乘、右移除**：`x << k = x * 2^k`（对无符号数）。编译器 O2 自动做这个优化——但你手写位运算时需要知道它在语义上的等价性。**右移**：逻辑右移（无符号，高位补 0）vs 算术右移（有符号，高位补符号位）——C 标准对负数的右移是"实现定义"
- **Brian Kernighan popcount**：`n & (n-1)` 清除最低位的 1——每次迭代消去一个 1，而不是遍历所有 32/64 位。`__builtin_popcount` 在 x86 上编译为单条 `POPCNT` 指令
- **bitset**：用整数数组按位存储集合——每个 word 代表 32/64 个元素。交集、并集、差集用 `& | &~`——在大数据量场景下比哈希集合快 10×+（CPU 向量化）
- **位字段 vs 位掩码**：C 的 `struct { int a:3; int b:5; }` 是位字段——但内存布局由编译器决定，不可移植。工程代码用位掩码 + `uint16_t`/`uint32_t` 代替，显式可控

## 踩坑记录

1. **移位溢出**：`1 << 31` 在 32 位 int 上触发 UB（溢出）——因为 1 是 `int` 类型，31 位移出符号位。正确写法：`1u << 31`（无符号）
2. **带符号右移陷阱**：`-1 >> 1` 的结果是实现定义的——gcc 用算术右移得 -1，某些编译器用逻辑右移得 `INT_MAX`。工程代码一律用 `uint32_t`/`uint64_t`
3. **bitset 索引计算**：`idx / 32` 找到 word、`idx % 32` 找到 bit——但 `% 32` 可以优化为 `idx & 31`（因为 32 是 2^k）。本 demo 保留 `% 32` 可读性
4. **`~` 的整数提升**：`~(1 << n)` 在 16 位平台上可能把高位也清了——写 `~(1u << n)` 确保无符号提升

## 工程对照（≥100 字硬约束）

位运算在工程侧的直接复用点：

1. **Page Header 位字段**：`engineering/include/db/storage/bufpage.h` 中 `PageHeader` 的 `flags` 字段用位掩码标记页面状态——`PAGE_HAS_FREE_LINES`(1<<0)、`PAGE_FULL`(1<<2) 等。本卡 demo_bitmask 的 set/clear/test 三原语直接对应 `PageSetFlag`/`PageClearFlag`/`PageTestFlag` 宏
2. **Bitmap 索引**：`engineering/src/db/index/bitmap/bitmap.c` 和 `engineering/src/db/index/vector_index/delete/vector_delete_bitmap.c` 用 bitmap 标记被删除的向量——每个 bit 代表一个 vector ID。`WORD_INDEX(id)` 和 `BIT_OFFSET(id)` 宏就是本卡 `idx/32` 和 `idx%32` 的工程版
3. **Bloom Filter**：`engineering/src/db/index/` 中可能的 Bloom Filter 实现用多个 hash 函数 + bitset——`k` 个 hash 函数各 set 一个 bit。本卡的 bitset 是最小实现
4. **对齐与掩码**：`engineering/src/db/storage/buffer/bufmgr.c` 中内存对齐检查用 `addr & (align-1) == 0`——这依赖位运算的"align 必为 2^k"特性。`engineering/include/db/storage/bufpage.h` 中 `BLCKSZ` 对齐类似

学完本卡后能动手的事：在 `engineering/src/db/index/bitmap/bitmap.c` 中找到 `WORD_INDEX` 和 `BIT_OFFSET` 宏定义，用本卡的知识验证 bitmap 扫描逻辑——理解为什么 bitmap 索引的 AND/OR 查询可以向量化（SIMD 一条指令处理 256 个 bit）
