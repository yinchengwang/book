/**
 * @file main.c
 * @brief Linux strace/ptrace 系统调用追踪演示
 *
 * 演示内容：
 *   - ptrace 系统调用原理
 *   - 追踪子进程系统调用
 *   - strace 的实现机制
 *
 * 编译：gcc -std=c11 -Wall -o strace main.c
 * 运行：./strace <目标命令>
 * 示例：./strace /bin/ls
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <errno.h>

/* 系统调用名称映射 */
static const char *syscall_names[] = {
    [0] = "read", [1] = "write", [2] = "open", [3] = "close",
    [4] = "stat", [5] = "fstat", [9] = "mmap", [10] = "mprotect",
    [12] = "brk", [13] = "rt_sigaction", [14] = "rt_sigprocmask",
    [21] = "access", [56] = "clone", [57] = "fork", [59] = "execve",
    [60] = "exit", [61] = "wait4", [62] = "kill",
    [231] = "exit_group"
};

static const char *get_syscall_name(long num) {
    if (num >= 0 && num < (long)(sizeof(syscall_names) / sizeof(syscall_names[0]))
        && syscall_names[num])
        return syscall_names[num];
    return "???";
}

/* ============================================================
 * Demo 1: 显示追踪原理
 * ============================================================ */
static void demo_strace_principle(void) {
    printf("[strace] strace 追踪原理:\n\n");

    printf("[strace] 1. fork() → 子进程\n");
    printf("[strace] 2. 父进程调用 ptrace(PTRACE_TRACEME)\n");
    printf("[strace] 3. 子进程 execve() → 进入被追踪程序\n");
    printf("[strace] 4. 每次系统调用时内核通知父进程:\n");
    printf("[strace]    - syscall-enter-stop（系统调用入口）\n");
    printf("[strace]    - syscall-exit-stop（系统调用返回）\n");
    printf("[strace] 5. 父进程读取寄存器获取系统调用号和参数\n");
    printf("[strace] 6. 父进程继续子进程: ptrace(PTRACE_SYSCALL)\n\n");

    printf("[strace] 性能开销:\n");
    printf("[strace]   - 每个系统调用额外 2 次上下文切换\n");
    printf("[strace]   - 对 I/O 密集型程序影响显著（~20-50%）\n");
    printf("[strace]   - 小技巧: 用 -e 过滤以减少开销\n");
}

/* ============================================================
 * Demo 2: 追踪子进程系统调用
 * ============================================================ */
static void demo_ptrace_child(char *argv[]) {
    printf("[strace] 使用 ptrace 追踪子进程\n");

    pid_t child = fork();
    if (child < 0) {
        perror("fork 失败");
        return;
    }

    if (child == 0) {
        /* 子进程: 允许父进程追踪自己 */
        ptrace(PTRACE_TRACEME, 0, NULL, NULL);
        /* 停止自己，等待父进程 */
        raise(SIGSTOP);
        /* 执行目标程序 */
        execvp(argv[1], &argv[1]);
        perror("execvp 失败");
        _exit(1);
    }

    /* 父进程: 等待子进程停止 */
    int status;
    waitpid(child, &status, 0);

    int in_syscall = 0;
    int count = 0;
    long last_syscall = -1;

    /* 使用 PTRACE_SYSCALL 追踪 */
    while (1) {
        if (ptrace(PTRACE_SYSCALL, child, NULL, NULL) < 0) {
            perror("ptrace 失败");
            break;
        }

        waitpid(child, &status, 0);
        if (WIFEXITED(status)) {
            printf("[strace]   子进程退出 (code=%d), 追踪到 %d 个系统调用\n",
                   WEXITSTATUS(status), count);
            break;
        }
        if (WIFSIGNALED(status)) {
            printf("[strace]   子进程被信号杀死 (signal=%d)\n", WTERMSIG(status));
            break;
        }

        /* 读取系统调用号 */
        long syscall_num = ptrace(PTRACE_PEEKUSER, child,
                                  sizeof(long) * ORIG_RAX, NULL);
        if (syscall_num < 0) continue;

        if (!in_syscall) {
            /* 系统调用入口 */
            if (syscall_num != last_syscall && count < 50) {
                printf("[strace]   [%3d] → %s(%ld)",
                       count, get_syscall_name(syscall_num), syscall_num);
                printf("\n");
                last_syscall = syscall_num;
            }
            count++;
        }
        in_syscall = !in_syscall;
    }
}

/* ============================================================
 * Demo 3: 追踪常用命令
 * ============================================================ */
static void demo_strace_usage(void) {
    printf("[strace] strace 常用命令:\n\n");

    printf("[strace] # 基础追踪\n");
    printf("[strace]   strace ls\n\n");

    printf("[strace] # 按系统调用过滤\n");
    printf("[strace]   strace -e trace=open,read,write ls\n\n");

    printf("[strace] # 统计模式\n");
    printf("[strace]   strace -c ls          # 系统调用统计\n\n");

    printf("[strace] # 追踪运行中进程\n");
    printf("[strace]   strace -p <pid>\n\n");

    printf("[strace] # 追踪多线程\n");
    printf("[strace]   strace -f -p <pid>\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(int argc, char *argv[]) {
    printf("========================================\n");
    printf("  Linux strace/ptrace 系统调用追踪演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: strace 原理 ---\n");
    demo_strace_principle();
    printf("\n");

    if (argc > 1) {
        printf("--- Demo 2: 追踪子进程 ---\n");
        demo_ptrace_child(argv);
    } else {
        printf("--- Demo 2: 跳过（无目标命令）---\n");
        printf("[strace]   用法: %s <command> [args...]\n", argv[0]);
        printf("[strace]   示例: %s /bin/echo hello\n", argv[0]);
    }
    printf("\n");

    printf("--- Demo 3: strace 常用命令 ---\n");
    demo_strace_usage();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
