# inline_asm scaffold

GCC 内联汇编（AT&T 语法）演示——基本 asm + 扩展 asm + cpuid + rdtsc + xchg 原子操作。

## 复现命令

```bash
cd learning/scaffold/inline_asm

# 内联汇编是 GCC 扩展，用 -std=gnu11（不能 -std=c11）
gcc -Wall -Wextra -O2 -std=gnu11 -o test main.c && ./test

# 用 Intel 语法重编（GCC 加 -masm=intel）
make intel-syntax
```

## 预期输出（节选）

```
[basic] === 基本内联汇编 ===
  3 个 nop 指令
  done.

[extend] === 扩展 asm：操作数约束 ===
  extend_asm_add(3, 4) = 7
  extend_asm_add(100, 200) = 300

[cpuid] === CPUID 指令查询 CPU 厂商 ===
  CPU vendor = "GenuineIntel" (或 "AuthenticAMD")

[rdtsc] === RDTSC 时间戳计数器 ===
  1M 次循环耗时 = 1523847 cycles
  sum = 499999500000

[atomic] === xchg 原子交换 ===
  atomic_xchg(&shared, 100): old=42, new=100

=== PASS ===
```

## 关键点

- **`asm` vs `asm volatile`**：`volatile` 阻止编译器优化/重排内联汇编——**有副作用（I/O、修改内存）必加**
- **AT&T vs Intel 语法**：
  - AT&T：`addl %eax, %ebx`（源在前，目标在后）
  - Intel：`add ebx, eax`（目标在前，源在后）
  - GCC 默认 AT&T，加 `-masm=intel` 切换
- **操作数约束**：
  - `"r"` —— 任意通用寄存器
  - `"m"` —— 内存
  - `"i"` —— 立即数
  - `"=r"` —— 输出寄存器
  - `"+r"` —— 读写寄存器
  - `"0"`~`"9"` —— 引用第 n 个操作数
- **clobber list**：列出汇编破坏的寄存器或内存，**告诉编译器重新加载**这些值
- **`cpuid` / `rdtsc`**：x86 架构指令，**Intel/AMD 独有**，跨平台需 `__cpuid` (MSVC) 或 libc intrinsics

详见 NOTES.md 工程对照段。