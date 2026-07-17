# profiling scaffold

性能剖析实践——三类性能问题（堆分配热点、cache miss、算法复杂度）的代码演示和计时对比。

## 复现命令

```bash
cd learning/scaffold/profiling
gcc -Wall -Wextra -O2 -std=c11 -o prof_demo main.c
./prof_demo
```

## 预期输出

```
--- 1. malloc vs stack（10000 iterations）---
[hot_slow] last hash=767486
[time] hot_slow: 2.84 ms (malloc/free each iteration)
[hot_fast] last hash=767486
[time] hot_fast: 0.52 ms (stack allocation)

--- 2. Matrix multiplication（256x256）---
[time] matmul_naive: 382.15 ms (k-innermost, cache-unfriendly)
[time] matmul_opt:   46.73 ms  (loop interchange, cache-friendly)
[verify] OK (results identical)
```

## Profiling 工具对比

| 工具 | 平台 | 分析维度 | 本机可用 | 输出 |
|------|------|---------|---------|------|
| clock() | 全平台 | wall time | ✅ | 本次运行 |
| gprof | Linux/MinGW | 函数级 CPU time | ❌ (MinGW 不完整) | call graph + 耗时占比 |
| perf stat | Linux | 硬件计数器 | ❌ (Linux only) | cache miss / branch miss / IPC |
| valgrind/cachegrind | Linux | cache 模拟 | ❌ (Linux only) | L1/L2 miss rate |
| VTune | Win/Linux | 全维度 | ❌ (需安装) | 微架构分析 |

## 关键点

- **heap vs stack**：malloc 每次调用涉及 libc 内存管理器锁、freelist 遍历——比栈分配慢 5-10x
- **cache miss 是性能杀手**：CPU cache line 64B——行主序访问时预取器连续加载，列主序每次跳到不同 cache line 导致 miss
- **loop interchange**：把 `i-j-k` 换成 `i-k-j`，让最内层循环遍历行主序的 k 维——不改变结果，只改变内存访问顺序，速度提升 8-10x
- **clock() 是最简陋但最通用的**：不需要任何安装，任何 C 编译器都支持

详见 NOTES.md 工程对照段。
