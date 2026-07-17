/* inline_asm scaffold — GCC 内联汇编（AT&T 语法）
 *
 * 复现命令：
 *   gcc -Wall -Wextra -O2 -std=c11 -o test main.c && ./test
 *   # MinGW/GCC 默认 AT&T 语法；Intel 语法加 -masm=intel
 *
 * 演示 5 段：
 *   [basic]   — 基本 asm("nop") 单语句
 *   [extend]  — 扩展 asm volatile("..." : 输出 : 输入 : clobber)
 *   [cpuid]   — cpuid 查询 CPU 厂商（真实硬件指令）
 *   [rdtsc]   — rdtsc 时间戳计数器（性能基准）
 *   [atomic]  — xchg 原子交换（无锁同步原语）
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* === [basic] 基本内联汇编 === */
static inline void basic_asm_demo(void) {
    /* asm volatile 让编译器不能优化掉 */
    asm volatile ("nop");                /* 空指令 */
    asm volatile ("nop");
    asm volatile ("nop");
}

/* === [extend] 扩展 asm：操作数约束 === */
/* 将 C 变量 a 移入 eax，相加后存回 result */
static inline int extend_asm_add(int a, int b) {
    int result;
    /* "=r"(result): 输出到任意通用寄存器
     * "r"(a) "r"(b): 从任意通用寄存器输入
     * 末尾无 clobber 表示不破坏其他寄存器 */
    asm volatile (
        "addl %1, %0"                     /* AT&T 语法: src 在前, dst 在后 */
        : "=r"(result)                    /* 输出 */
        : "r"(a), "0"(b)                  /* 输入 (0 = 与输出 0 同一位置) */
    );
    return result;
}

/* === [cpuid] CPUID 指令查询 CPU 厂商字符串 === */
/* 返回 12 字节厂商字符串（不含 \0） */
static inline void cpuid_vendor(char vendor[13]) {
    uint32_t ebx, ecx, edx;

    asm volatile (
        "cpuid"
        : "=b"(ebx), "=c"(ecx), "=d"(edx)  /* 输出到 ebx/ecx/edx */
        : "a"(0)                            /* 输入 eax=0 表示查询 vendor */
        :                                  /* 无 clobber */
    );

    /* vendor 字符串 = ebx + edx + ecx (12 字节) */
    memcpy(vendor,     &ebx, 4);
    memcpy(vendor + 4, &edx, 4);
    memcpy(vendor + 8, &ecx, 4);
    vendor[12] = '\0';
}

/* === [rdtsc] 时间戳计数器（性能基准） === */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    asm volatile (
        "rdtsc"
        : "=a"(lo), "=d"(hi)               /* 输出：低 32 位在 eax，高 32 位在 edx */
        :                                  /* 无输入 */
        :                                  /* rdtsc 不破坏其他寄存器 */
    );
    return ((uint64_t)hi << 32) | lo;
}

/* === [atomic] xchg 原子交换（无锁原语基础） === */
static inline int atomic_xchg(volatile int *ptr, int new_val) {
    int old_val;
    /* xchg 指令是 x86 的原子交换——lock 前缀隐含 */
    asm volatile (
        "xchgl %0, %1"
        : "=r"(old_val), "+m"(*ptr)        /* "+m" 读写内存 */
        : "0"(new_val)                     /* 输入与输出 0 同寄存器 */
        : "memory"                         /* clobber: 标记内存屏障 */
    );
    return old_val;
}

int main(void) {
    /* === [basic] 基本内联汇编 === */
    printf("[basic] === 基本内联汇编 ===\n");
    printf("  3 个 nop 指令（编译器不能消除）\n");
    basic_asm_demo();
    printf("  done.\n");

    /* === [extend] 扩展 asm：操作数约束 === */
    printf("\n[extend] === 扩展 asm：操作数约束 ===\n");
    int s = extend_asm_add(3, 4);
    printf("  extend_asm_add(3, 4) = %d (C: 3+4=%d)\n", s, 3 + 4);
    int s2 = extend_asm_add(100, 200);
    printf("  extend_asm_add(100, 200) = %d\n", s2);

    /* === [cpuid] CPUID 指令 === */
    printf("\n[cpuid] === CPUID 指令查询 CPU 厂商 ===\n");
    char vendor[13];
    cpuid_vendor(vendor);
    printf("  CPU vendor = \"%s\" (12 字节)\n", vendor);

    /* === [rdtsc] 时间戳计数器 === */
    printf("\n[rdtsc] === RDTSC 时间戳计数器 ===\n");
    uint64_t t1 = rdtsc();
    /* 模拟一段计算 */
    volatile long sum = 0;
    for (int i = 0; i < 1000000; i++) sum += i;
    uint64_t t2 = rdtsc();
    printf("  1M 次循环耗时 = %llu cycles\n", (unsigned long long)(t2 - t1));
    printf("  sum = %ld\n", sum);

    /* === [atomic] xchg 原子交换 === */
    printf("\n[atomic] === xchg 原子交换 ===\n");
    volatile int shared = 42;
    int old = atomic_xchg(&shared, 100);
    printf("  atomic_xchg(&shared, 100): old=%d, new=%d\n", old, shared);

    printf("\n=== PASS ===\n");
    return 0;
}