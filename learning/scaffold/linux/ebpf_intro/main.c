/**
 * @file main.c
 * @brief eBPF 入门演示
 *
 * 演示 BCC 工具使用方法，展示 eBPF 探针机制
 * 使用 perf_event 子系统演示性能计数
 *
 * 编译：gcc -std=c11 -o ebpf_intro main.c
 * 运行：./ebpf_intro
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>

/* 模拟多种系统调用用于 eBPF 追踪 */
static volatile int g_running = 1;

static void sig_handler(int sig) {
    (void)sig;
    g_running = 0;
}

/* 写文件（open + write 组合）*/
static void do_file_io(void) {
    FILE *fp = fopen("/tmp/ebpf_test.log", "w");
    if (!fp) return;

    for (int i = 0; i < 10; i++) {
        fprintf(fp, "line %d: eBPF tracing test\n", i);
    }
    fclose(fp);
}

/* 分配内存（malloc + free）*/
static void do_memory_allocation(void) {
    const int count = 100;
    void *ptrs[count];

    for (int i = 0; i < count; i++) {
        ptrs[i] = malloc(1024);
    }
    for (int i = 0; i < count; i++) {
        free(ptrs[i]);
    }
}

/* 系统调用（getpid/getuid）*/
static void do_syscalls(void) {
    volatile pid_t pid = getpid();
    volatile uid_t uid = getuid();
    (void)pid;
    (void)uid;
}

/* TCP 操作（socket + connect，仅本地 loopback）*/
static void do_network_work(void) {
    /* 模拟连接操作 */
    printf("  [模拟] 发起 TCP 连接...\n");
    usleep(10000);  /* 10ms */
}

int main(void) {
    printf("===========================================\n");
    printf("  eBPF 入门演示\n");
    printf("===========================================\n");

    signal(SIGINT, sig_handler);

    /* 1. eBPF 概述 */
    printf("\n\n=== 1. eBPF 概述 ===\n");
    printf(
        "eBPF (extended Berkeley Packet Filter) 是 Linux 内核中的一个\n"
        "可编程虚拟机，允许在不需要内核改动的情况下插入运行时代码。\n\n"
        "核心组件：\n"
        "  - eBPF 程序：运行在内核中的沙盒字节码\n"
        "  - Maps：内核与用户空间共享的数据结构\n"
        "  - Helper 函数：内核提供的安全 API\n"
        "  - 验证器：确保 eBPF 程序安全（无循环、无越界）\n");

    /* 2. BCC 工具介绍 */
    printf("\n\n=== 2. BCC 工具集合 ===\n");
    printf("\nBCC (BPF Compiler Collection) 提供开箱即用的诊断工具:\n");
    printf("  execsnoop  - 追踪新进程创建\n");
    printf("  opensnoop  - 追踪文件打开\n");
    printf("  tcptop     - 追踪 TCP 连接\n");
    printf("  biolatency - 分析 I/O 延迟分布\n");
    printf("  cachestat  - 追踪页缓存命中率\n");
    printf("  profile    - 采样内核栈火焰图\n");

    /* 3. 演示工作负载 */
    printf("\n\n=== 3. 运行工作负载（供 eBPF 追踪）===\n");

    for (int round = 0; round < 3 && g_running; round++) {
        printf("\n--- 第 %d 轮 ---\n", round + 1);

        printf("[文件 I/O]\n");
        do_file_io();

        printf("[内存分配]\n");
        do_memory_allocation();

        printf("[系统调用]\n");
        do_syscalls();

        printf("[网络操作]\n");
        do_network_work();

        usleep(500000);  /* 500ms */
    }

    /* 4. eBPF 追踪命令速查 */
    printf("\n\n=== 4. eBPF 追踪命令速查 ===\n");
    printf("\n# 系统调用追踪\n");
    printf("sudo bpftrace -e 'tracepoint:syscalls:sys_enter_* { @[probe] = count(); }'\n");
    printf("\n# 文件打开追踪\n");
    printf("sudo bpftrace -e 'tracepoint:syscalls:sys_enter_openat "
           "{ printf(\"%%s %%s\\n\", comm, str(args->filename)); }'\n");
    printf("\n# 进程创建追踪\n");
    printf("sudo bpftrace -e 'tracepoint:syscalls:sys_enter_execve "
           "{ join(args->argv); }'\n");
    printf("\n# 内存分配追踪（需要 uprobe）\n");
    printf("sudo bpftrace -e 'uprobe:/lib/libc.so.6:malloc "
           "{ printf(\"malloc: %%d bytes\\n\", arg0); }'\n");
    printf("\n# 函数调用火焰图\n");
    printf("sudo profile -F 99 -f 30 > /tmp/ebpf_folded.stacks\n");

    /* 5. 内核事件统计 */
    printf("\n\n=== 5. 内核事件统计 ===\n");

    printf("\nperf_event 可观测事件：\n");
    printf("  PERF_COUNT_SW_CPU_CLOCK\n");
    printf("  PERF_COUNT_SW_TASK_CLOCK\n");
    printf("  PERF_COUNT_SW_PAGE_FAULTS\n");
    printf("  PERF_COUNT_SW_CONTEXT_SWITCHES\n");
    printf("  PERF_COUNT_SW_CPU_MIGRATIONS\n");
    printf("  PERF_COUNT_HW_CPU_CYCLES\n");
    printf("  PERF_COUNT_HW_INSTRUCTIONS\n");

    /* 6. BCC 安装说明 */
    printf("\n\n=== 6. BCC/BPF 安装 ===\n");
    printf("\n# Ubuntu/Debian:\n");
    printf("sudo apt install bpfcc-tools linux-headers-$(uname -r)\n");
    printf("\n# 检查 eBPF 支持:\n");
    printf("sudo bpftrace -e 'BEGIN { printf(\"hello eBPF!\\n\"); exit(); }'\n");

    printf("\n===========================================\n");
    printf("  eBPF 入门演示完成\n");
    printf("===========================================\n");

    return 0;
}
