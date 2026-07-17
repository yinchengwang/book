/**
 * @file main.c
 * @brief Linux seccomp 安全计算模式演示
 *
 * 演示内容：
 *   - seccomp 严格模式（SECCOMP_MODE_STRICT）
 *   - seccomp-bpf 过滤模式
 *   - seccomp 在容器中的应用
 *
 * 编译：gcc -std=c11 -Wall -o seccomp main.c
 * 运行：./seccomp
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#ifdef __linux__
#include <linux/seccomp.h>
#include <sys/prctl.h>
#endif

/* ============================================================
 * Demo 1: seccomp 原理讲解
 * ============================================================ */
static void demo_seccomp_principle(void) {
    printf("[seccomp] seccomp 安全计算模式原理:\n\n");

    printf("[seccomp] SECCOMP_MODE_STRICT (严格模式):\n");
    printf("[seccomp]   - 仅允许: read/write/_exit/sigreturn\n");
    printf("[seccomp]   - 禁止: 所有其他系统调用\n");
    printf("[seccomp]   - 其他系统调用 → SIGKILL\n\n");

    printf("[seccomp] SECCOMP_MODE_FILTER (过滤模式):\n");
    printf("[seccomp]   - 使用 BPF 程序定义白/黑名单\n");
    printf("[seccomp]   - 灵活性高，可组合复杂策略\n");
    printf("[seccomp]   - 容器/沙箱的核心安全机制\n\n");

    printf("[seccomp] 过滤后可选的 5 种动作:\n");
    printf("[seccomp]   1. SECCOMP_RET_KILL_PROCESS: 杀死进程\n");
    printf("[seccomp]   2. SECCOMP_RET_KILL_THREAD:  杀死线程\n");
    printf("[seccomp]   3. SECCOMP_RET_ALLOW:        允许\n");
    printf("[seccomp]   4. SECCOMP_RET_ERRNO:        返回错误码\n");
    printf("[seccomp]   5. SECCOMP_RET_TRAP:         发送 SIGSYS\n");
}

/* ============================================================
 * Demo 2: seccomp 严格模式演示（仅检查原理）
 * ============================================================ */
#ifdef __linux__
static void demo_strict_mode(void) {
    printf("[seccomp] 严格模式演示（不实际启用）\n");

    printf("[seccomp]   启用严格模式:\n");
    printf("[seccomp]     prctl(PR_SET_SECCOMP, SECCOMP_MODE_STRICT);\n");
    printf("[seccomp]   注意: 一旦进入严格模式无法退出\n");
    printf("[seccomp]   即使 open() 也会导致 SIGKILL\n\n");

    printf("[seccomp]   当前进程的 seccomp 状态:\n");
    int mode = prctl(PR_GET_SECCOMP);
    switch (mode) {
        case 0: printf("[seccomp]     SECCOMP_MODE_DISABLED (未启用)\n"); break;
        case 1: printf("[seccomp]     SECCOMP_MODE_STRICT (严格模式)\n"); break;
        case 2: printf("[seccomp]     SECCOMP_MODE_FILTER (过滤模式)\n"); break;
        default: printf("[seccomp]     未知模式: %d\n", mode);
    }
}
#else
static void demo_strict_mode(void) {
    printf("[seccomp] 严格模式: 仅在 Linux 上可用\n");
}
#endif

/* ============================================================
 * Demo 3: 容器安全机制
 * ============================================================ */
static void demo_container_security(void) {
    printf("[seccomp] seccomp 在容器安全中的应用:\n\n");

    printf("[seccomp] Docker:\n");
    printf("[seccomp]   默认 seccomp profile 禁用 ~44 个系统调用\n");
    printf("[seccomp]   白名单包括: futex, clone, madvise 等常用调用\n");
    printf("[seccomp]   黑名单包括: mount, kexec_load, reboot, ...\n");
    printf("[seccomp]   细粒度配置: --security-opt seccomp=<profile.json>\n\n");

    printf("[seccomp] Kubernetes:\n");
    printf("[seccomp]   seccompProfile:\n");
    printf("[seccomp]     type: RuntimeDefault  # 使用容器运行时的默认配置\n");
    printf("[seccomp]     type: Localhost        # 自定义 profile\n");
    printf("[seccomp]     type: Unconfined       # 禁用 seccomp\n\n");

    printf("[seccomp] gVisor/Firecracker:\n");
    printf("[seccomp]   - 结合 seccomp + 用户态内核\n");
    printf("[seccomp]   - 每个系统调用在用户态过滤\n");
    printf("[seccomp]   - 比纯 seccomp 更安全但性能开销更大\n");
}

/* ============================================================
 * Demo 4: 白名单示例（伪代码）
 * ============================================================ */
static void demo_whitelist_example(void) {
    printf("[seccomp] 白名单策略示例:\n\n");

    printf("[seccomp] 允许的系统调用:\n");
    printf("[seccomp]   read, write, close, fstat, mmap,\n");
    printf("[seccomp]   mprotect, brk, rt_sigaction,\n");
    printf("[seccomp]   rt_sigprocmask, exit_group\n\n");

    printf("[seccomp] 禁止的系统调用:\n");
    printf("[seccomp]   open, socket, connect, bind,\n");
    printf("[seccomp]   fork, clone, execve, mount,\n");
    printf("[seccomp]   ptrace, chmod, kill\n\n");

    printf("[seccomp] 适用场景:\n");
    printf("[seccomp]   - 静态内容服务器\n");
    printf("[seccomp]   - 计算的沙箱（如 Cloud Functions）\n");
    printf("[seccomp]   - 数据库查询执行器（隔离层）\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(void) {
    printf("========================================\n");
    printf("  Linux seccomp 安全计算模式演示\n");
    printf("========================================\n\n");

    printf("--- Demo 1: seccomp 原理 ---\n");
    demo_seccomp_principle();
    printf("\n");

    printf("--- Demo 2: 严格模式 ---\n");
    demo_strict_mode();
    printf("\n");

    printf("--- Demo 3: 容器安全 ---\n");
    demo_container_security();
    printf("\n");

    printf("--- Demo 4: 白名单策略 ---\n");
    demo_whitelist_example();
    printf("\n");

    printf("========================================\n");
    printf("=== PASS ===\n");
    printf("========================================\n");
    return 0;
}
