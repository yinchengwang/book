# inline_asm 学习笔记

## 概念地图

GCC 内联汇编是"嵌入 C 代码的汇编片段"——通过它直接控制 CPU 指令、寄存器、内存序。**它是底层优化的最后手段**，99% 的代码不需要：

- **两种 asm 形式**：
  - **基本 asm**：`asm("nop")` —— 单行汇编，编译器看不见操作
  - **扩展 asm**：`asm("指令" : 输出 : 输入 : clobber)` —— 让汇编与 C 变量交互
- **`volatile` 关键字**：
  - `asm` —— 编译器可优化/重排/消除
  - `asm volatile` —— 禁止优化、禁止重排
  - **有副作用必加**：`cpuid`/`rdtsc`/`xchg` 等
- **操作数约束字母表**：
  | 约束 | 含义 |
  |------|------|
  | `r`  | 通用寄存器（eax/ebx/ecx/edx/esi/edi） |
  | `m`  | 内存 |
  | `i`  | 立即数常量 |
  | `g`  | 通用（r/m/i） |
  | `a/b/c/d` | 指定寄存器 eax/ebx/ecx/edx |
  | `=r` | 输出寄存器 |
  | `+r` | 读写寄存器 |
  | `0`-`9` | 引用第 n 个操作数 |
- **clobber list**：末尾逗号分隔的寄存器/内存列表，**告诉编译器"这些值已被破坏"**
  - `"memory"` —— 内存屏障，强制重新加载所有内存
- **AT&T vs Intel 语法**：
  - AT&T：`addl %eax, %ebx`（源在前，后缀 l/w/b/q 标大小）
  - Intel：`add ebx, eax`（目标在前，无后缀）
  - GCC `-masm=intel` 切换

## 踩坑记录

1. **忘记 `volatile`**：编译器把 `asm("nop")` 视为无用指令消除掉，**`cpuid` 之类有副作用的指令必须 `volatile`**
2. **clobber 漏掉寄存器**：汇编修改了 ebx 但 clobber 没列——**编译器认为 ebx 没变，下一次使用就是脏值**
3. **操作数序号错位**：`"r"(a), "1"(b)` 引用第 1 个操作数（a），**不能引用未定义的**
4. **AT&T 操作数顺序写反**：`addl %1, %0` 是 `result += a`，写成 `addl %0, %1` 变成 `a += result`
5. **clobber `"memory"` 漏掉**：汇编修改内存但没声明，**编译器可能缓存旧值到寄存器**
6. **跨平台**：内联汇编是编译器+架构耦合的，**x86 GCC 代码不能在 ARM 上编译**——需 `#ifdef __x86_64__` 隔离

## 工程对照（≥100 字硬约束）

内联汇编在本仓库 `engineering/` 中体现为"性能关键路径的最后优化"：

1. **`engineering/src/algo-prod/quantization/quantization.c` SIMD 路径**：PQ/LVQ 量化用 SSE/AVX2 内联汇编实现距离计算——`asm volatile("..." : ... : : "xmm0", "memory")`。内联汇编让 SIMD 性能比纯 C 高 4-8 倍
2. **`engineering/src/algo-prod/dict/dict_hmm.c` Viterbi 内核**：HMM 中文分词的概率递推用内联汇编预计算常用值，避免重复浮点除法
3. **`engineering/src/db/index/vector_index/faiss/faiss_hnsw_search.c` beam search**：HNSW 图搜索的热路径用内联汇编做 SIMD 距离计算——`asm("vinsertf128 ...")` AVX 指令
4. **`engineering/src/db/storage/bufmgr.c` Clock-Sweep**：`CAS (Compare-And-Swap)` 等无锁原语在 GCC 用 `__atomic_*` intrinsics 实现（不是内联汇编），但底层仍是 `lock cmpxchg` 指令
5. **`engineering/src/algo-prod/sort/sort.c` 排序基元**：某些排序算法用内联汇编做 SIMD 加速——但现代编译器（GCC 14）自动向量化已经很好，**手写汇编只在编译器失败的极端情况**
6. **`engineering/rag/src/rag/persist/hnsw_persist.c` 内存对齐检查**：用 `__attribute__((aligned(16)))` + 内联汇编验证 SIMD 数据对齐
7. **`engineering/src/db/utils/faiss_heap.c` 堆操作**：faiss 优先队列的关键路径用内联汇编做无分支 min/max（`cmov` 指令）——避免分支预测失败

学完本卡能动手的事：扫描 `learning/scaffold/` 下所有 main.c，把性能热路径（例如 sort_basic 的 partition）改为 `-O3 -march=native` 编译，观察编译器自动向量化效果，**只在编译器失败时再考虑手写汇编**。