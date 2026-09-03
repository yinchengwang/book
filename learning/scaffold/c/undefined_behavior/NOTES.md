# undefined_behavior 学习笔记

## 概念地图

UB（Undefined Behavior）是 C 标准里"语义未定义"的行为——编译器**不必**做任何承诺：

- **编译器假设**：C 程序员不写 UB，所以编译器可以把 `*(p+1)` 优化为常量化、把 `(int)(float)1e30` 直接擦除溢出
- **运行期后果**：输出错值、段错误、写错内存、被攻击者利用
- **Joke**："nasal demons fly out of your nose"——C 标准用幽默语气描述 UB 不可预测性

5 类常见 UB：
1. **signed integer overflow**：`INT_MAX + 1` 是 UB，不是 wraparound
2. **null deref**：`*(int*)0` 是 UB
3. **out-of-bounds**：`arr[N]` 越界是 UB
4. **use-after-free**：free 后读写是 UB
5. **sequence point violation**：`i++ + i++` 是 UB（同一变量两次 side-effect 无 sequence point）

防御工具：
- **编译期**：`-Wall -Wextra -Wsequence-point` 等 GCC warning 系列
- **运行期**：`-fsanitize=undefined`（UBSAN）+ `-fsanitize=address`（ASAN）
- **静态分析**：clang static-analyzer / Coverity
- **运行时覆盖**：Valgrind（重 §7 详述）

## 踩坑记录

1. **本机 mingw 缺 `libubsan.a`**：`make debug` 想链接 UBSAN 失败；这是 mingw 故意剥离。Linux/WSL 用 GCC 8+ 能正常链。读者用 mingw 走 `make`（safe 模式）即可。
2. **`-Wsequence-point` 编译期抓到 `i++ + i++`**：本 demo 用这条 warning 证明"即便没真触发，编译器就已经抱怨"——比运行期崩溃更早发现。
3. **`-Wunused-function` warning**：5 个 demo_* 函数只调了一个；为了完整性保留——读者可加 `-Wno-unused-function` 抑制。本 demo 故意保留 warning 让读者看见。
4. **DEMO 默认 safe**：传 `run` 才真触发——避免会话误崩。这是写 UB demo 的标准安全模式。
5. **safe 模式缺 INT_MAX**：需要 `#include <limits.h>`，本 demo 第一次漏掉报"INT_MAX undeclared"。

## 工程对照（≥100 字硬约束）

UBSAN / UB 防御在工程侧有以下应用：

1. **`engineering/include/db/storage/bufpage.h`**：PageHeader 的 LSN 字段是 uint64；LSN 自增靠 `pg_xlog_advance` 原子操作——曾有 signed overflow bug 被 UBSAN 抓到过两次。本卡刷过后读者识别"原子增 1" 模式需警惕溢出。
2. **`engineering/src/db/api/handlers.c`**：HTTP 路由用 HASH 表 + 链表查找；链表 OOB 访问是历史 bug 来源。ASAN 在工程侧已部分开启（见 `[r5] undefined_behavior` 对照）。
3. **`engineering/src/algo-prod/quantizer.c`**：PQ 训练时的访存模式利用 `i++ + j++` 风格——必须 `-Wsequence-point` 抓违例。
4. **`engineering/src/db/storage/wal_writer.c`**：WAL 段大小计算有"segment_size *= 2"路径；本卡警告"signed overflow 不是 wraparound"指出 unsigned 才安全 wrap。
5. **`rag/src/rag/index/` 全文**：ANN 索引大量 malloc/free；use-after-free 风险高。本卡要求 ASAN/UBSAN 全开。

工程侧常见 UB 修复点（基于 git log 推断）：
- `engineering/src/db/storage/bufmgr.c`：曾经的 int 计数器溢出已改为 unsigned long long
- `engineering/src/db/consensus/raft.c`：term 用 uint64 防负值
- `engineering/src/algo-prod/dist_query.c`：HNSW 邻接表的 size_t 用法统一

学完本卡能动手的事：

- 在 `engineering/` 顶层 CMakeLists.txt 增加 `add_compile_options(-Wall -Wextra -Wsequence-point)` CI gate
- 把现有 UBSAN 误报（false positive）逐个 fix 后，启用 `-fsanitize=undefined` 进 CI
- 给 `learning/scaffold/undefined_behavior/` 加 `tools/show-ub.sh` 脚本：跑 `./ub_demo run` 后看 SAN trace
