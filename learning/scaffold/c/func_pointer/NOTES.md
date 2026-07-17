# func_pointer 学习笔记

## 概念地图

C 函数指针是指向函数入口地址的指针。它让 C 拥有"一等函数"的子集——C++ 用 `std::function` / lambda 完整化，C 用函数指针。

- **声明语法**：`int (*fn)(int, int)`；圆括号强制"这是指针"语义，否则 `int *fn(...)` 是返回 int* 的函数声明
- **typedef 提可读**：`typedef int (*cmp_t)(const void *, const void *);` 后只需 `cmp_t my_cmp;`
- **回调（callback）**：库提供函数指针参数，调用方注入——`qsort` / `bsearch` / `pthread_create` 的 `start_routine` / `atexit` / `signal` 全是
- **调度表（dispatch table）**：`arith_t ops[] = {add, mul};` —— 函数指针数组当作"操作表"
- **C++ 升级**：`std::function<>` / `std::bind` + lambda，类型安全更优雅
- **函数指针与 ABI**：跨函数库（dlsym）需 extern "C"；本仓库 C 代码默认使用同 ABI

函数指针比 C++ lambda 弱：不能捕获状态（需 struct 打包传指针）、不能 inline 表达式。但底层 ABI 兼容——`std::function` 内部就是函数指针 + 捕获指针。

## 踩坑记录

1. **MinGW-w64 缺 `<signal.h>` 完整实现**：本 demo 第一次加 sigaction handler，链接报 `struct sigaction has initializer but incomplete type`。已删除信号段，README 把信号用法说明放在脚注（与 signal §6 互引）。
2. **atexit 调用是函数指针**：参数是 `void (*)(void)`；返回 int 注册成功、0 失败；可注册 32 个（实现定义）。
3. **qsort 的"非对称比较"陷阱**：返回 `(a > b) - (a < b)` 而不是 `a - b`——后者在 a,b 接近 INT_MIN/INT_MAX 时溢出。本 demo 用了非对称版本。
4. **desc 比较取负**：直接 `return -cmp_int_asc(a, b);` 不是严谨负值（数值范围可能逼近 INT_MIN），更稳的是 `return cmp_int_asc(b, a);` 调换入参。本 demo 简化用 `-`。
5. **函数指针 NULL 检查**：`if (fp) fp(1, 2);` 比 `if (fp != NULL)` 更常见；NULL 跳调用是 UB。
6. **`atexit` 钩子不能用 `_exit` 跳过**：`_exit` 不触发 atexit 链。本 demo `return 0;` 正常 trigger。

## 工程对照（≥100 字硬约束）

1. **`engineering/src/algo-prod/quantizer.c`**：量化算法族（PQ / OPQ / SQ）通过函数指针数组做"按 metric 切换"——`metric_ops[metric_t]` 是经典 dispatch table 模式。本卡刷过后读者能识别"策略模式"的 C 等价物。
2. **`engineering/src/algo-prod/dist_query.c`**：分布式查询路由用 hook 注册机制——`register_shard_callback(shard_id, hook_fn)` 与 pthread 联合形成"事件 → callback → 工作线程"流。本卡提供 callback 基础。
3. **`engineering/src/db/consensus/raft.c`**：election 超时与 apply 路径走函数指针，配置不同角色（leader/follower/candidate）时切换 handler 集合。本卡读懂 raft_role → handler_table。
4. **`engineering/src/db/index/vector_index/hnsw/*.c`**：HNSW 图层的 distance metric（L2 / IP / COSINE）作为函数指针传入——`hnsw_search(query, k, dist_func, ...)`。本卡提供 dispatch 标准。
5. **`engineering/include/db/core/guc.h`**：GUC 自定义变量支持 `assign_hook` / `show_hook` 回调——本卡的 atexit 是用户级 hook 范式。

学完本卡能动手的事：

- 在 `engineering/src/algo-prod/` 加 `registry.c`：维护 `(name → fn)` 的注册表，供插件动态注册算子或 metric 处理器。
- 与 §6 signal 联合做"信号 → 调度表 handler"——`signal_action[signo] = handler_fn` 注册。
