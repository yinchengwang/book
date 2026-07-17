# c11_features 学习笔记

## 概念地图

C11（C 标准化 2011）是 C 工程的事实基准。本卡覆盖的 C11 关键能力：

- **`_Generic` 编译期类型分发**：编译期根据类型选实现。常用于 `printf` 风格宏，避免 `_Generic 选择错误` 与 ABI 安全性
- **`designated initializer`** `{.field = val}`：让结构体初始化字段顺序自由，对抗代码重构时的字段重排无声破坏
- **`static_assert`**（C11 关键字）：编译期断言；比 `assert.h` 的 runtime `assert()` 更早失败
- **`_Alignas(N)` / `_Alignof`**（C11 关键字）：显式对齐到 N 字节；SIMD 与 cache line 互动
- **匿名 union in struct**：省 `.data.len` 中间层，简化为 `pkt.len`
- **`_Noreturn` / `_Thread_local` / `quick_exit`** 等小特性属于次要 C11 增量
- **C11 atomic** `<stdatomic.h>`：跨平台原子操作；本仓库有 `engineering/include/` 下 atomic 路径

C11 vs C99 vs C17：C99 引入 designated initializer 雏形；C11 标准化 `_Generic` 与 `static_assert`；C17 主要是 bug 修复。本仓库使用 `-std=c11` 与未来升级到 C17/C23。

## 踩坑记录

1. **`_Generic` 分支不匹配仍有 warning**：本 demo `PRINT_TYPE((int)42);` 触达 `int` 分支；用 `PRINT_TYPE(f);`（float 变量）触达 `float` 分支。如果混用类型，编译器仍能编译但 `printf` 走变参会警告。本 demo 故意让编译器对 `(int)42` 报 `format expects double` 不收，演示 _Generic 选择的不是你直觉的路径。
2. **`static_assert` 是关键字** 不是 `<assert.h>` 的 `assert()`：`assert()` 是运行时宏，`static_assert` 是编译时——且 C11 之前不存在。
3. **匿名 union 也允许匿名 struct**：`struct { union { int a; }; };` 直接 `s.a` 访问；本 demo 不演示匿名 struct。
4. **`_Alignas` 必须是 2 的幂**：传 7 编译错；本 demo 用 16。
5. **designated initializer 与 C++ 兼容**：C++20 起 designated initializer 部分支持；本仓库 C 代码与 C++ header 分离避免冲突。
6. **`_Generic` 不能识别 typedef 的复杂类型**：只能用基础类型关键词 `int / double / char * / struct foo` 等；typedef 别名会被识别为基础类型。

## 工程对照（≥100 字硬约束）

C11 特性在工程侧有以下运用：

1. **`engineering/include/` 全部头文件**：都用 `-std=c11` 编译；`designated initializer` 已普及在多处结构体初始化上（如 `bufpage.c` 里 page init）。本卡刷过后读者能识别"哪个版本支持的"。
2. **`engineering/src/algo-prod/quantizer.c`**：码本加载函数 `pq_load(const char *path)` 用 `_Generic` 分发 int8 / float / binary 三种解量化方法——本卡提供 _Generic 模板。
3. **`engineering/src/db/core/guc.c`**：GUC 自定义变量依赖 `static_assert(sizeof(int) >= 4)` 确保跨平台语义一致。本卡模板可迁移。
4. **`engineering/src/db/index/vector_index/hnsw/aligned_alloc.c`**（假设）：HNSW 邻接表需要 64 字节对齐让 SIMD prefetch 工作，用 `_Alignas(64)`。本卡提供 aligned alloc 入门。
5. **`engineering/src/db/consensus/raft.c`**：匿名 union in struct 让 `RaftMessage { kind, {.request_vote: {term, ...}} }` 紧凑表达 4 种 RPC 类型。本卡刷过后读者能识别 this pattern。

列出仓库内 4 个用 designated initializer 的具体文件（按 `git grep "= {.*\." -- '*.c'`）：
- `engineering/src/db/storage/heapam.c`：HeapPageInit
- `engineering/src/db/storage/wal_writer.c`：XLogRecord 头
- `engineering/src/db/api/handlers.c`：VectorAPI request struct
- `learning/scaffold/c11_features/main.c`：本 demo

学完本卡能动手的事：

- 给 `engineering/include/db/packing.h` 添加 `_Static_assert(sizeof(struct foo) == expected, "ABI drift")` 系列断言，让 ABI 漂移在 CI 编译期就被捕获。
- 把 rag server 的 metric 打印从 `printf("%f", v)` 改为 `_Generic` 分派 `int / double / size_t`，避免格式串错位。
