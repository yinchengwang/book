# pointer 学习笔记

## 概念地图

C 指针是从 C 语义底层贯穿上层的核心抽象：

- **直接 vs 间接访问**：`x` 直接读，`*p` 通过 p 间接读
- **指针算术**：`p + n` 走的是类型步长，非字节偏移；`int *` 加 1 走 4 字节
- **数组退化**：`int arr[5]` 传给函数退化为 `int *`，丢失长度——必须额外传 `n`
- **void \***：通用指针，但**不能解引用，不能做指针算术**
- **const 修饰**：`const int *` 不许通过该指针改 int；`int * const` 不许改指针本身
- **函数指针**：`int (*fn)(int, int)`；语法晦涩，需 typedef 提可读性

与现代 C++ 引用（C++ reference）的对比：引用是别名、不可为空、不可改绑；指针可控但危险。本仓库所有 C 代码都用指针。

## 踩坑记录

1. **`%p` 转换**：`printf("%p", p)` 必须强转 `(void*)`，否则 mingw/gcc 报 `-Wformat`。本 demo Makefile 加了 `-Wno-format` 兜底，但实测前几个非空版本都报一次警告。
2. **NULL 与 `(void*)0`**：C 标准 `NULL` 是实现定义的；本 demo 用 `NULL` 字面常量等价 `(void*)0`。
3. **指针算术的"类型步长"**：`int *` 加 1 = 加 4 字节；`char *` 加 1 = 加 1 字节。如果乱套会读到错误数据。
4. **函数指针声明**：`int (*fn)(int, int)` 不写 typedef 容易看错——本 demo 用 `typedef int (*binop_t)(int, int)` 提可读。
5. **数组退化陷阱**：`void func(int arr[10])` 实际签名是 `void func(int *arr)`，编译器不强制 arr 长度。本 demo 不演示，避免与下张卡混淆。

## 工程对照（≥100 字硬约束）

指针在工程侧有以下直接复用点：

1. **`engineering/src/db/storage/bufmgr.c`**：Buffer Pool 用 `Buffer *` 双向链表，LRU 与 Clock-Sweep 链表指针操作频繁。本卡刷过后能读懂 `StrategyControl` 与 `T` 双向链表的 `prev/next` 操作。
2. **`rag/src/rag/index/hnsw_index.cpp`**（C++ 类）：HNSW 图节点之间的 neighbor 列表是 `std::vector<Node *>`；图遍历靠指针跳。本卡读懂 vector<Node*> 的分配模式。
3. **`engineering/src/db/consensus/raft.h`**：每条 log entry 是 `RaftLogEntry *`，指针串成数组。`raft.c` 中 `entry_at(idx)` 返回指针，避免大片 memcpy。
4. **`engineering/src/algo-prod/dist_query.c`** 与 `engineering/src/algo-prod/quantizer.c`：策略模式用 `Quantizer *current_quant` 类函数指针数组做分派——本 demo 的 `dispatch(ops[i], ...)` 是该模式的最小抽象。
5. **`engineering/include/db/storage/bufpage.h`**：Page Header 是 `PageHeader` 结构体指针，工具函数接受 `PageHeader *ph` 直接对其字段操作。

学完本卡后能动手的事：在 `engineering/src/db/storage/` 下加一个 `slab_allocator.c`，用指针池实现"重复分配/释放同尺寸对象的零开销"——这是 bufmgr 减少 malloc/free 抖动的常见优化路径。
